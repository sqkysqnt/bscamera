import socket
import struct
import time
import json
import os
import threading
import logging
from flask import Flask, request, render_template, jsonify
from . import panlogic
from flask import Blueprint

x32_bp = Blueprint('x32', __name__, template_folder='templates')

# Configure logging
logging.basicConfig(level=logging.INFO)  # Set log level as needed (DEBUG, INFO, WARNING, ERROR)

# JSON configuration file for settings persistence
CONFIG_FILE = "config.json"

# Load default settings or existing settings from the config file
default_config = {
    "x32_ip": "192.168.1.23",
    "channel_targets": {
        # Add your channel targets here
    },
    "osc_messages": {
        "mic_ready": "/micReady",
        "mic_on": "/micOn",
        "mic_off": "/micOff"
    }
}


def load_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                return json.load(f)
        except json.JSONDecodeError:
            logging.error("Invalid JSON in config file. Falling back to default configuration.")
            return default_config
    else:
        return default_config

def save_config(config):
    with open(CONFIG_FILE, "w") as f:
        json.dump(config, f, indent=4)
        logging.info("Configuration saved to %s", CONFIG_FILE)

# Global configuration
config_lock = threading.Lock()
config = load_config()

# Utility functions for OSC communication
def osc_encode_string(s):
    s += '\0'
    padding = (4 - (len(s) % 4)) % 4
    s += '\0' * padding
    return s.encode('ascii')

def osc_build_message(address, arguments=None):
    address_encoded = osc_encode_string(address)
    if arguments:
        typetag = ',' + ''.join(['f' if isinstance(arg, float) else 'i' for arg in arguments])
        typetag_encoded = osc_encode_string(typetag)
        args_encoded = b''
        for arg in arguments:
            if isinstance(arg, float):
                args_encoded += struct.pack('>f', arg)
            elif isinstance(arg, int):
                args_encoded += struct.pack('>i', arg)
        return address_encoded + typetag_encoded + args_encoded
    else:
        typetag_encoded = osc_encode_string(',')  # No arguments
        return address_encoded + typetag_encoded

def send_osc_message(sock, message, ip, port):
    try:
        sock.sendto(message, (ip, port))
        logging.info("OSC message sent to %s:%d", ip, port)
    except OSError as e:
        logging.error("Failed to send message to %s:%d - %s", ip, port, e)

def receive_response(sock):
    try:
        data, addr = sock.recvfrom(1024)
        if data:
            address_end = data.find(b'\0')
            address = data[:address_end].decode('ascii')
            i = (address_end + 4) & ~0x03
            typetag_end = data.find(b'\0', i)
            typetag = data[i:typetag_end].decode('ascii')
            i = (typetag_end + 4) & ~0x03
            args = []
            for tag in typetag[1:]:
                if tag == 'f':
                    arg = struct.unpack('>f', data[i:i+4])[0]
                    i += 4
                    args.append(arg)
                elif tag == 'i':
                    arg = struct.unpack('>i', data[i:i+4])[0]
                    i += 4
                    args.append(arg)
                else:
                    logging.warning("Unhandled OSC type tag: %s", tag)
            return address, args
    except socket.timeout:
        logging.warning("No response received from X32 within timeout.")
    return None, None

# Periodic checking logic
def periodic_check():
    local_ip = "0.0.0.0"
    local_port = 55000  # Use an available port number

    x32_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    x32_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    x32_sock.bind((local_ip, local_port))
    x32_sock.settimeout(5)  # Set a timeout for receiving data

    target_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    logging.info("Starting periodic_check thread for X32 monitoring.")

    try:
        while True:
            with config_lock:
                current_config = json.loads(json.dumps(config))  # Deep copy of config

            x32_ip = current_config["x32_ip"]
            osc_messages = current_config["osc_messages"]
            channel_targets = current_config["channel_targets"]

            for channel_str, targets in channel_targets.items():
                channel = int(channel_str)
                channel_formatted = f"{channel:02d}"

                fader_address = f"/ch/{channel_formatted}/mix/fader"
                mute_address = f"/ch/{channel_formatted}/mix/on"

                # Retrieve fader level
                logging.info("Requesting fader level for channel %s...", channel_formatted)
                message = osc_build_message(fader_address)
                send_osc_message(x32_sock, message, x32_ip, 10023)
                address, args = receive_response(x32_sock)
                if address == fader_address and args:
                    fader_level = args[0]
                    logging.info("Fader level for channel %s: %s", channel_formatted, fader_level)
                else:
                    logging.warning("Failed to retrieve fader level for channel %s.", channel_formatted)
                    fader_level = None

                # Retrieve mute status
                logging.info("Requesting mute status for channel %s...", channel_formatted)
                message = osc_build_message(mute_address)
                send_osc_message(x32_sock, message, x32_ip, 10023)
                address, args = receive_response(x32_sock)
                if address == mute_address and args:
                    mute_status = args[0]
                    status_str = "Unmuted" if mute_status == 1 or mute_status == 1.0 else "Muted"
                    logging.info("Mute status for channel %s: %s", channel_formatted, status_str)
                else:
                    logging.warning("Failed to retrieve mute status for channel %s.", channel_formatted)
                    mute_status = None

                # Evaluate conditions and send OSC messages
                if mute_status == 1 or mute_status == 1.0:
                    # Channel is unmuted
                    if fader_level is None:
                        logging.warning("Fader level is None for channel %s, skipping condition evaluation.", channel_formatted)
                        continue

                    if fader_level == 0.0:
                        logging.info("Condition met for channel %s: Unmuted but fader is at 0.", channel_formatted)
                        message = osc_build_message(osc_messages["mic_ready"])
                    elif fader_level > 0.0:
                        logging.info("Condition met for channel %s: Unmuted and fader is above 0.", channel_formatted)
                        message = osc_build_message(osc_messages["mic_on"])
                    else:
                        logging.info("Invalid fader level for channel %s (below 0), skipping...", channel_formatted)
                        continue
                else:
                    logging.info("Condition met for channel %s: Muted or mute status unknown.", channel_formatted)
                    message = osc_build_message(osc_messages["mic_off"])

                # Send the message to the target devices
                for target in targets:
                    if target.get("auto_pan", False):
                        logging.info("Auto Pan enabled for channel %s. Processing pan logic.", channel_formatted)
                        panlogic.process_pan(channel_formatted, x32_ip, x32_sock, osc_build_message, send_osc_message, receive_response)
                    else:
                        logging.info("Auto Pan disabled for channel %s.", channel_formatted)

                    send_osc_message(target_sock, message, target["ip"], target["port"])
                    logging.info("OSC message sent to %s:%d for channel %s.", target["ip"], target["port"], channel_formatted)

            time.sleep(2.5)

    except Exception as e:
        logging.error("An error occurred in periodic_check: %s", e)
    finally:
        x32_sock.close()
        target_sock.close()
        logging.info("Periodic_check thread terminated. Sockets closed.")


@x32_bp.route('/')
def index():
    return render_template('x32_channel.html', config=config)

@x32_bp.route('/get_channel_targets')
def get_channel_targets():
    with config_lock:
        return jsonify(config["channel_targets"])

@x32_bp.route('/remove_channel_target', methods=['POST'])
def remove_channel_target():
    channel = int(request.form['channel'])
    target_ip = request.form['target_ip']
    target_port = int(request.form['target_port'])

    with config_lock:
        if str(channel) in config["channel_targets"]:
            original_count = len(config["channel_targets"][str(channel)])
            config["channel_targets"][str(channel)] = [
                target for target in config["channel_targets"][str(channel)]
                if target["ip"] != target_ip or target["port"] != target_port
            ]
            if not config["channel_targets"][str(channel)]:
                del config["channel_targets"][str(channel)]
            removed_count = original_count - len(config["channel_targets"].get(str(channel), []))
            if removed_count > 0:
                logging.info("Removed target %s:%d from channel %d", target_ip, target_port, channel)
            else:
                logging.warning("No matching target to remove for channel %d.", channel)

            save_config(config)
            return jsonify({"status": "success", "message": f"Target {target_ip}:{target_port} removed from channel {channel}."})
        else:
            logging.warning("Channel %d does not exist. Cannot remove target %s:%d.", channel, target_ip, target_port)
            return jsonify({"status": "error", "message": f"Channel {channel} does not exist."})

@x32_bp.route('/update_x32_ip', methods=['POST'])
def update_x32_ip():
    new_ip = request.form['x32_ip']
    with config_lock:
        config["x32_ip"] = new_ip
        save_config(config)
        logging.info("X32 IP updated to %s", new_ip)
    return jsonify({"status": "success", "message": "X32 IP updated!"})

@x32_bp.route('/add_channel_target', methods=['POST'])
def add_channel_target():
    channel = int(request.form['channel'])
    target_ip = request.form['target_ip']
    target_port = int(request.form['target_port'])
    auto_pan = 'auto_pan' in request.form

    with config_lock:
        if str(channel) not in config["channel_targets"]:
            config["channel_targets"][str(channel)] = []
        
        config["channel_targets"][str(channel)].append({
            "ip": target_ip,
            "port": target_port,
            "auto_pan": auto_pan
        })
        save_config(config)
        logging.info("Added target %s:%d (Auto Pan: %s) to channel %d", target_ip, target_port, auto_pan, channel)

    return jsonify({"status": "success", "message": f"Target added to channel {channel} with Auto Pan set to {auto_pan}!"})

@x32_bp.route('/update_osc_messages', methods=['POST'])
def update_osc_messages():
    with config_lock:
        config["osc_messages"]["mic_ready"] = request.form['mic_ready']
        config["osc_messages"]["mic_on"] = request.form['mic_on']
        config["osc_messages"]["mic_off"] = request.form['mic_off']
        save_config(config)
        logging.info("OSC messages updated: mic_ready=%s, mic_on=%s, mic_off=%s",
                     config["osc_messages"]["mic_ready"],
                     config["osc_messages"]["mic_on"],
                     config["osc_messages"]["mic_off"])
    return jsonify({"status": "success", "message": "OSC messages updated!"})

@x32_bp.route('/send_test_message', methods=['POST'])
def send_test_message():
    channel = int(request.form['channel'])
    message_type = request.form['message_type']

    with config_lock:
        message_address = config["osc_messages"].get(message_type)
        if not message_address:
            logging.error("Invalid message type requested: %s", message_type)
            return jsonify({"status": "error", "message": "Invalid message type!"})

        message = osc_build_message(message_address)

        if str(channel) in config["channel_targets"]:
            channel_formatted = f"{channel:02d}"
            target_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            for target in config["channel_targets"][str(channel)]:
                send_osc_message(target_sock, message, target["ip"], target["port"])
                logging.info("Test message '%s' sent to channel %s target %s:%d",
                             message_type, channel_formatted, target["ip"], target["port"])
            target_sock.close()
            return jsonify({"status": "success", "message": f"Test message '{message_type}' sent to channel {channel_formatted} targets."})
        else:
            logging.warning("No targets found for channel %d to send test message '%s'.", channel, message_type)
            return jsonify({"status": "error", "message": f"No targets found for channel {channel:02d}."})
