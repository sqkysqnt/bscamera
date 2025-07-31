# plugins/qlab_bridge/plugin.py

from flask import Blueprint, jsonify, render_template, request
from plugins.plugin_interface import PluginInterface
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import ThreadingOSCUDPServer
from pythonosc.udp_client import SimpleUDPClient
import threading
import logging
import json
import os
import socket
import time

qlab_bp = Blueprint('qlab_plugin', __name__, template_folder='templates')

CONFIG_FILE = "plugins/qlab_bridge/config.json"

default_config = {
    "qlab_ip": "127.0.0.1",
    "qlab_port": 53000,
    "local_port": 53001,
    "chat_channel": "qlab",
    "chat_sender": "qlab",
    "enable_theatrechat": True,
    "filters": ["go", "start", "stop"]
}

config_lock = threading.Lock()
config = default_config.copy()

#latest_cue_lock = threading.Lock()
#latest_cue = ""

def load_config():
    global config
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, "r") as f:
            file_config = json.load(f)
        with config_lock:
            for key, value in file_config.items():
                config[key] = value
    else:
        save_config()
        logging.info(f"[QLAB] Created default config at {CONFIG_FILE}")

def save_config():
    with config_lock:
        with open(CONFIG_FILE, "w") as f:
            json.dump(config, f, indent=4)


@qlab_bp.route('/')
def qlab_ui():
    overlay_path = os.path.join(os.path.dirname(__file__), 'templates', 'qlab_overlay.html')
    overlay_content = ""
    if os.path.exists(overlay_path):
        with open(overlay_path, 'r') as f:
            overlay_content = f.read()
    return render_template('qlab_interface.html', config=config, overlay_content=overlay_content)


@qlab_bp.route('/update_config', methods=['POST'])
def update_config():
    with config_lock:
        config['qlab_ip'] = request.form['qlab_ip']
        config['qlab_port'] = int(request.form['qlab_port'])
        config['chat_channel'] = request.form['chat_channel']
        config['chat_sender'] = request.form['chat_sender']
        config['enable_theatrechat'] = request.form['enable_theatrechat']
        config['filters'] = request.form.getlist('filters')
        save_config()

    if hasattr(qlab_bp, 'plugin_instance'):
        qlab_bp.plugin_instance.apply_config()

    return jsonify({"status": "success"})


@qlab_bp.route('/overlay')
def qlab_overlay():
    return render_template('qlab_overlay.html')

@qlab_bp.route('/cue_text')
def get_latest_cue():
    plugin = getattr(qlab_bp, 'plugin_instance', None)
    if plugin:
        return jsonify({
            "cue": plugin.latest_cue,
            "light_active": plugin.light_active_cue,
            "light_pending": plugin.light_pending_cue
        })
    return jsonify({
        "cue": "",
        "light_active": "",
        "light_pending": ""
    })  


@qlab_bp.route('/save_overlay', methods=['POST'])
def save_overlay():
    data = request.get_json()
    html_content = data.get('html', '')
    try:
        overlay_path = os.path.join(os.path.dirname(__file__), 'templates', 'qlab_overlay.html')
        with open(overlay_path, 'w') as f:
            f.write(html_content)
        return jsonify({"status": "success"})
    except Exception as e:
        logging.error(f"[QLAB] Failed to save overlay: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500



class QlabPlugin(PluginInterface):
    def __init__(self):
        self.client = None
        self.server = None
        self.server_thread = None
        self.theatrechat_client = None
        self.chat_channel = config["chat_channel"]
        self.chat_sender = config["chat_sender"]
        self.latest_cue = ""
        self.listen_thread = None
        self.listen_running = False
        self.current_cue_data = {}  # Initialize cue data storage
        self.pending_cue = None     # Store pending cue for reply handling
        self.active_cues = {}  # Track running cues by uniqueID
        self.current_timer = None  # Track the active timer
        self.active_cue_id = None  # Track currently displayed cue
        self.light_active_cue = ""
        self.light_pending_cue = ""


    def start_listen_heartbeat(self):
        def send_listen_forever():
            while self.listen_running:
                try:
                    if self.client:
                        self.client.send_message("/listen", [])
                        logging.debug("[QLAB] Sent /listen heartbeat")
                except Exception as e:
                    logging.error(f"[QLAB] Heartbeat /listen failed: {e}")
                # Wait 10 seconds
                time.sleep(10)

        self.listen_running = True
        self.listen_thread = threading.Thread(target=send_listen_forever, daemon=True)
        self.listen_thread.start()
        logging.info("[QLAB] Started /listen heartbeat thread")

    def clear_overlay(self):
        """Safely clear the overlay and cancel any timers"""
        if self.current_timer:
            self.current_timer.cancel()
            self.current_timer = None
        self.latest_cue = ""
        self.active_cue_id = None


    def register(self, app, socketio=None, dispatcher=None):
        load_config()
        qlab_bp.plugin_instance = self

        if 'qlab_plugin' not in [bp.name for bp in app.blueprints.values()]:
            app.register_blueprint(qlab_bp, url_prefix='/qlab')

        self.setup_theatrechat_sender()
        self.start_osc_server()
        self.apply_config()

    def get_plugin_info(self):
        return {
            "name": "QLab → TheatreChat Bridge",
            "url": "/qlab",
            "description": "Forwards QLab show control messages to TheatreChat.",
            "ignore": False
        }

    def apply_config(self):
        with config_lock:
            qlab_ip = config.get("qlab_ip", "127.0.0.1")
            qlab_port = config.get("qlab_port", 53000)
            self.chat_channel = config.get("chat_channel", "qlab")
            self.chat_sender = config.get("chat_sender", "qlab")
            self.start_listen_heartbeat()


        try:
            self.client = SimpleUDPClient(qlab_ip, qlab_port)
            self.client.send_message("/listen", [])
            self.start_listen_heartbeat()
            logging.info(f"[QLAB] Connected and sent /listen to {qlab_ip}:{qlab_port}")
        except Exception as e:
            logging.error(f"[QLAB] Failed to connect/send /listen: {e}")


    def start_osc_server(self):
        # Only start in the reloader child process
        if os.environ.get("WERKZEUG_RUN_MAIN") != "true":
            logging.info("[QLAB] Skipping OSC server in main loader process.")
            return

        dispatcher = Dispatcher()
        dispatcher.map("/reply/*", self.handle_reply)
        dispatcher.set_default_handler(self.handle_osc)


        try:
            self.server = ThreadingOSCUDPServer(("0.0.0.0", config["local_port"]), dispatcher)
            self.server_thread = threading.Thread(target=self.server.serve_forever, daemon=True)
            self.server_thread.start()
            logging.info(f"[QLAB] OSC server listening on port {config['local_port']}")
        except OSError as e:
            logging.error(f"[QLAB] Could not start OSC server: {e}")


    def setup_theatrechat_sender(self):
        try:
            self.theatrechat_client = SimpleUDPClient("255.255.255.255", 27900)
            self.theatrechat_client._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            logging.info("[QLAB] TheatreChat OSC broadcast client initialized.")
        except Exception as e:
            logging.error(f"[QLAB] Failed to initialize TheatreChat client: {e}")

    def shutdown(self):
        self.listen_running = False
        if self.listen_thread:
            self.listen_thread.join(timeout=1)

    def set_cue_timer(self, msg_str, cue_id, cue_type):
        """Handle cue display timing based on cue type"""
        # First clear any existing overlay/timer
        self.clear_overlay()
        
        # Set the new cue
        self.latest_cue = msg_str
        self.active_cue_id = cue_id
        
        if cue_type in ["Audio", "Video", "Fade"]:
            # For timed cues, request duration
            try:
                self.client.send_message(f"/cue/{cue_id}/duration", ["get"])
                logging.debug(f"[QLAB] Requested duration for cue {cue_id}")
            except Exception as e:
                logging.error(f"[QLAB] Failed to request duration: {e}")
                # Fallback to default duration
                self.set_fallback_timer()
        else:
            # For Group/other cues, just set fallback timer
            self.set_fallback_timer()

    def set_fallback_timer(self, duration=10.0):
        """Set a fallback timer to clear overlay"""
        def clear():
            if self.current_timer:  # Only clear if this timer is still active
                self.clear_overlay()
                logging.debug("[QLAB] Cleared overlay after fallback timeout")
        
        self.current_timer = threading.Timer(duration, clear)
        self.current_timer.start()            

    def handle_reply(self, address, *args):
        """Handle duration replies from QLab"""
        try:
            if "/duration" in address and self.active_cue_id:
                try:
                    # Handle JSON error responses
                    if args and isinstance(args[0], str) and args[0].startswith('{'):
                        try:
                            error_data = json.loads(args[0])
                            if error_data.get("status") == "error":
                                logging.debug(f"[QLAB] QLab error response: {error_data}")
                                return  # Keep existing timer
                        except json.JSONDecodeError:
                            pass  # Not JSON, continue
                    
                    # Process duration value
                    duration = float(args[0])
                    logging.debug(f"[QLAB] Received duration: {duration} seconds")
                    
                    # Replace fallback timer with actual duration
                    if self.current_timer:
                        self.current_timer.cancel()
                    self.set_fallback_timer(duration)
                    
                except (ValueError, IndexError) as e:
                    logging.error(f"[QLAB] Invalid duration reply: {e}")
                except Exception as e:
                    logging.error(f"[QLAB] Error processing reply: {e}")
        except Exception as e:
            logging.error(f"[QLAB] Reply handler error: {e}")

    def is_cue_running(self, cue_id):
        """Check if a cue is currently running"""
        return cue_id in self.active_cues


    def handle_osc(self, address, *args):
        logging.debug(f"[OSC] Received: {address} {' '.join(map(str, args))}")

        try:
            parts = address.strip("/").split("/")
            
            # Track cue start/stop states
            if len(parts) >= 5 and parts[:2] == ["qlab", "event"]:
                event_type = parts[3]
                detail = parts[4] if len(parts) > 4 else ""
                
                # Handle cue stop events
                if event_type == "stop" and detail == "uniqueID" and args:
                    cue_id = args[0]
                    if cue_id == self.active_cue_id:
                        self.clear_overlay()
                        logging.debug(f"[QLAB] Cleared overlay for stopped cue: {cue_id}")
            elif address.startswith("/eos/out/active/cue/text") and args:
                self.light_active_cue = f"LQ Current: {args[0]}"
                logging.info(f"[ETC] Active cue: {self.light_active_cue}")

            elif address.startswith("/eos/out/pending/cue/text") and args:
                self.light_pending_cue = f"LQ Pending: {args[0]}"
                logging.info(f"[ETC] Pending cue: {self.light_pending_cue}")



            # Process cue events
            filters = config.get("filters", [])
            if len(parts) >= 5 and parts[:2] == ["qlab", "event"]:
                event_type = parts[3]
                detail = parts[4] if len(parts) > 4 else ""
                
                # Update cue data
                if event_type not in self.current_cue_data:
                    self.current_cue_data[event_type] = {}
                if detail in ["number", "name", "uniqueID", "type"]:
                    self.current_cue_data[event_type][detail] = args[0] if args else ""
                
                # Process when we have complete data
                if (event_type in filters and 
                    "name" in self.current_cue_data[event_type] and 
                    "uniqueID" in self.current_cue_data[event_type]):
                    
                    cue_info = self.current_cue_data[event_type]
                    cue_name = cue_info["name"]
                    cue_id = cue_info["uniqueID"]
                    cue_type = cue_info.get("type", "")
                    msg_str = f"SQ - {cue_name} ({cue_type}) - {event_type}"
                    
                    # Immediately display new cue
                    self.set_cue_timer(msg_str, cue_id, cue_type)
                    logging.info(f"[QLAB] Displaying cue: {msg_str}")
                    
                    # Send to TheatreChat if enabled
                    if config.get("enable_theatrechat", True):
                        try:
                            self.theatrechat_client.send_message(
                                f"/theatrechat/message/{self.chat_channel}",
                                [self.chat_sender, msg_str]
                            )
                        except Exception as e:
                            logging.error(f"[QLAB] TheatreChat send failed: {e}")
                    
                    # Clean up
                    del self.current_cue_data[event_type]
                    
        except Exception as e:
            logging.error(f"[QLAB] OSC handler error: {e}")
