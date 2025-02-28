# In plugin.py:

import socket
import struct
import time
import json
import os
import threading
import logging
import requests
from flask import Flask, request, render_template, jsonify
from . import panlogic
from flask import Blueprint

ms_bp = Blueprint('MixingStation', __name__, template_folder='templates')

logging.basicConfig(level=logging.INFO)

CONFIG_FILE = "plugins/mixingstation/config.json"

default_config = {
    "x32_ip": "192.168.1.23",
    "enabled": True,  # Add an enabled flag
    "channel_targets": {
        # Add your channel targets here
    },
    "osc_messages": {
        "mic_ready": "/micReady",
        "mic_on": "/micOn",
        "mic_off": "/micOff"
    }
}

config_lock = threading.Lock()
config = {}

print("Looking for config.json at:", os.path.abspath(CONFIG_FILE))


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

config = load_config()

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

import requests
import json
import logging
import time

import requests
import json
import logging
import time

def periodic_check():
    logging.info("Starting periodic_check thread for Mixing Station monitoring.")

    try:
        while True:
            # Check if functionality is enabled
            with config_lock:
                if not config.get("enabled", True):
                    time.sleep(2.5)
                    continue

                current_config = json.loads(json.dumps(config))  # Deep copy of config

            x32_ip = current_config["x32_ip"]
            osc_messages = current_config["osc_messages"]
            channel_targets = current_config["channel_targets"]

            for channel_str, targets in channel_targets.items():
                channel = int(channel_str)

                # Correct API Endpoints
                fader_url = f"http://{x32_ip}:8080/console/data/get/ch.{channel}.mix.lvl/val"
                mute_url = f"http://{x32_ip}:8080/console/data/get/ch.{channel}.mix.on/val"

                # Retrieve fader level
                try:
                    logging.info("Requesting fader level for channel %s...", channel)
                    response = requests.get(fader_url, timeout=5)
                    response.raise_for_status()
                    fader_level = response.json().get("value")
                    logging.info("Fader level for channel %s: %s", channel, fader_level)
                except requests.RequestException as e:
                    logging.warning("Failed to retrieve fader level for channel %s: %s", channel, e)
                    fader_level = None

                # Retrieve mute status
                try:
                    logging.info("Requesting mute status for channel %s...", channel)
                    response = requests.get(mute_url, timeout=5)
                    response.raise_for_status()
                    mute_status = response.json().get("value")
                    status_str = "Unmuted" if mute_status else "Muted"
                    logging.info("Mute status for channel %s: %s", channel, status_str)
                except requests.RequestException as e:
                    logging.warning("Failed to retrieve mute status for channel %s: %s", channel, e)
                    mute_status = None

                # Evaluate conditions and send OSC messages
                if mute_status:  # Unmuted (true)
                    if fader_level is None:
                        logging.warning("Fader level is None for channel %s, skipping condition evaluation.", channel)
                        continue

                    if fader_level <= -90:  # Assuming -90 is equivalent to "off"
                        logging.info("Condition met for channel %s: Unmuted but fader is at -90 (off).", channel)
                        status = "Mic Ready"
                        message = osc_build_message(osc_messages["mic_ready"])
                    elif fader_level > -90:
                        logging.info("Condition met for channel %s: Unmuted and fader is above -90.", channel)
                        status = "Mic On"
                        message = osc_build_message(osc_messages["mic_on"])
                    else:
                        logging.info("Invalid fader level for channel %s, skipping...", channel)
                        continue
                else:  # Muted or unknown
                    logging.info("Condition met for channel %s: Muted or mute status unknown.", channel)
                    status = "Mic Off"
                    message = osc_build_message(osc_messages["mic_off"])

                # Send the message to the target devices
                for target in targets:
                    if target.get("auto_pan", False):
                        logging.info("Auto Pan enabled for channel %s. Processing pan logic.", channel)
                        panlogic.process_pan(channel, x32_ip, osc_build_message)

                    logging.info("OSC message sent to %s:%d for channel %s.", target["ip"], target["port"], channel)
                    send_osc_message(target["ip"], target["port"], message)

            time.sleep(2.5)

    except Exception as e:
        logging.error("An error occurred in periodic_check: %s", e)
    finally:
        logging.info("Periodic_check thread terminated.")


@ms_bp.route('/get_channel_status', methods=['GET'])
def get_channel_status():
    """Retrieve status for all configured channel targets and send OSC messages."""
    with config_lock:
        mixer_ip = config.get("x32_ip", "")
        channel_targets = config.get("channel_targets", {})
        osc_messages = config.get("osc_messages", {})

    if not mixer_ip:
        return jsonify({"status": "error", "message": "No mixer IP configured."})

    channel_status_list = {}

    for channel_str, targets in channel_targets.items():
        channel = int(channel_str) - 1  # **FIX: Adjust for zero-based indexing**
        if channel < 0:
            logging.warning(f"Invalid channel number: {channel + 1} (Converted to {channel})")
            continue

        # Retrieve mute status
        try:
            mute_response = requests.get(f"http://{mixer_ip}:8080/console/data/get/ch.{channel}.mix.on/val", timeout=3)
            mute_response.raise_for_status()
            mute_status = not mute_response.json().get("value", None)  # **FIX: Invert Boolean**
            logging.info(f"Channel {channel + 1}: Processed mute status (after fix): {'Unmuted' if mute_status else 'Muted'}")
        except requests.RequestException:
            logging.error(f"Channel {channel + 1}: Failed to retrieve mute status.")
            mute_status = None

        # Retrieve fader level
        try:
            fader_response = requests.get(f"http://{mixer_ip}:8080/console/data/get/ch.{channel}.mix.lvl/val", timeout=3)
            fader_response.raise_for_status()
            fader_level = fader_response.json().get("value", None)
            logging.info(f"Channel {channel + 1}: Raw fader level from API: {fader_level}")
        except requests.RequestException:
            logging.error(f"Channel {channel + 1}: Failed to retrieve fader level.")
            fader_level = None

        # Handle missing values
        if mute_status is None or fader_level is None:
            status = "Not Reachable"
            message = None
        else:
            # **New Inverted Logic:**
            if mute_status:  # **True = Unmuted**
                status = "Mic Off"
                message = osc_build_message(osc_messages["mic_off"])
            else:  # **False = Muted**
                if fader_level <= -90:
                    status = "Mic Ready"
                    message = osc_build_message(osc_messages["mic_ready"])
                else:
                    status = "Mic On"
                    message = osc_build_message(osc_messages["mic_on"])


        # Store status
        channel_status_list[str(channel + 1)] = status  # **Convert back for user display**
        logging.info(f"Channel {channel + 1}: Final Status = {status}")

        # Send OSC messages
        if message:
            for target in targets:
                send_osc_message(socket.socket(socket.AF_INET, socket.SOCK_DGRAM), message, target["ip"], target["port"])
                logging.info(f"Sent OSC to {target['ip']}:{target['port']} for channel {channel + 1}: {status}")

    return jsonify({"status": "success", "channel_status": channel_status_list})





@ms_bp.route('/')
def index():
    return render_template('x32_channel.html', config=config)

@ms_bp.route('/get_channel_targets')
def get_channel_targets():
    with config_lock:
        return jsonify(config["channel_targets"])

@ms_bp.route('/remove_channel_target', methods=['POST'])
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

@ms_bp.route('/update_x32_ip', methods=['POST'])
def update_x32_ip():
    new_ip = request.form['x32_ip']
    with config_lock:
        config["x32_ip"] = new_ip
        save_config(config)
        logging.info("X32 IP updated to %s", new_ip)
    return jsonify({"status": "success", "message": "X32 IP updated!"})

@ms_bp.route('/add_channel_target', methods=['POST'])
def add_channel_target():
    channel = request.form.get('channel')
    target_ip = request.form.get('target_ip')
    target_port = request.form.get('target_port')
    auto_pan = 'auto_pan' in request.form

    if not channel or not target_ip or not target_port:
        return jsonify({"status": "error", "message": "Missing required fields."})

    try:
        channel = int(channel)
        target_port = int(target_port)
    except ValueError:
        return jsonify({"status": "error", "message": "Invalid channel or port number."})

    with config_lock:
        if str(channel) not in config["channel_targets"]:
            config["channel_targets"][str(channel)] = []

        config["channel_targets"][str(channel)].append({
            "ip": target_ip,
            "port": target_port,
            "auto_pan": auto_pan
        })
        save_config(config)
        logging.info(f"Added target {target_ip}:{target_port} (Auto Pan: {auto_pan}) to channel {channel}")

    return jsonify({"status": "success", "message": f"Target added to channel {channel}."})


@ms_bp.route('/update_osc_messages', methods=['POST'])
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

@ms_bp.route('/send_test_message', methods=['POST'])
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

# New route to toggle the periodic check on/off
@ms_bp.route('/toggle_enabled', methods=['POST'])
def toggle_enabled():
    with config_lock:
        current_state = config.get("enabled", True)
        config["enabled"] = not current_state
        save_config(config)
        state_str = "enabled" if config["enabled"] else "disabled"
        logging.info("Periodic check has been %s", state_str)
    return jsonify({"status": "success", "message": f"Periodic check is now {state_str}."})

@ms_bp.route('/check_mixer_connection', methods=['POST'])
def check_mixer_connection():
    """Check if the mixer IP is accessible"""
    with config_lock:
        mixer_ip = config.get("x32_ip", "")

    if not mixer_ip:
        return jsonify({"status": "error", "reachable": False, "message": "No mixer IP configured."})

    try:
        response = requests.get(f"http://{mixer_ip}:8080/app/state", timeout=3)
        response.raise_for_status()
        return jsonify({"status": "success", "reachable": True, "message": "Mixer is reachable."})
    except requests.RequestException:
        return jsonify({"status": "error", "reachable": False, "message": "Mixer is not reachable."})


@ms_bp.route('/get_mixer_channels', methods=['GET'])
def get_mixer_channels():
    """Retrieve total number of channels and fetch their names."""
    with config_lock:
        mixer_ip = config.get("x32_ip", "")

    if not mixer_ip:
        return jsonify({"status": "error", "message": "No mixer IP configured."})

    # Step 1: Get the total number of channels
    try:
        response = requests.get(f"http://{mixer_ip}:8080/console/information", timeout=3)
        response.raise_for_status()
        total_channels = response.json().get("totalChannels", 0)
    except requests.RequestException as e:
        logging.error("Failed to retrieve total channels: %s", e)
        return jsonify({"status": "error", "message": "Failed to retrieve total channels."})

    if total_channels == 0:
        return jsonify({"status": "error", "message": "No channels available."})

    # Step 2: Fetch channel names
    channels = []
    for index in range(total_channels):
        try:
            response = requests.get(f"http://{mixer_ip}:8080/console/data/get/ch.{index}.cfg.name/val", timeout=3)
            response.raise_for_status()
            channel_name = response.json().get("value", f"Channel {index+1}")  # Default to generic name
            channels.append({"index": index + 1, "name": channel_name})
        except requests.RequestException:
            logging.warning("Failed to fetch name for channel %d, using default.", index + 1)
            channels.append({"index": index + 1, "name": f"Channel {index+1}"})

    return jsonify({"status": "success", "channels": channels})
