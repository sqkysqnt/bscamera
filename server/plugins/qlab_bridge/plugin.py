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

qlab_bp = Blueprint('qlab_plugin', __name__, template_folder='templates')

CONFIG_FILE = "plugins/qlab_bridge/config.json"

default_config = {
    "qlab_ip": "127.0.0.1",
    "qlab_port": 53000,
    "local_port": 53001,
    "chat_channel": "qlab",
    "chat_sender": "qlab",
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
    return render_template('qlab_interface.html', config=config)

@qlab_bp.route('/update_config', methods=['POST'])
def update_config():
    with config_lock:
        config['qlab_ip'] = request.form['qlab_ip']
        config['qlab_port'] = int(request.form['qlab_port'])
        config['chat_channel'] = request.form['chat_channel']
        config['chat_sender'] = request.form['chat_sender']
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
        return jsonify({"cue": plugin.latest_cue})
    return jsonify({"cue": ""})




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


    def handle_osc(self, address, *args):
        msg_str = None
        filters = config.get("filters", [])

        if address.startswith("/qlab/event/workspace/go") and "go" in filters and len(args) >= 2:
            cue_number = args[0]
            cue_name = args[1]
            msg_str = f"Qlab Cue #{cue_number} - {cue_name} - go"

        elif address.startswith("/qlab/event/workspace/cue/start") and "start" in filters and len(args) >= 2:
            cue_number = args[0]
            cue_name = args[1]
            msg_str = f"Qlab Cue #{cue_number} - {cue_name} - start"

        elif address.startswith("/qlab/event/workspace/cue/stop") and "stop" in filters and len(args) >= 2:
            cue_number = args[0]
            cue_name = args[1]
            msg_str = f"Qlab Cue #{cue_number} - {cue_name} - stop"

        else:
            logging.debug(f"[QLAB] Ignored: {address} {' '.join(str(a) for a in args)}")
            return

        # Store the cue first
        self.latest_cue = msg_str


        logging.info(f"[QLAB] {msg_str}")
        try:
            self.theatrechat_client.send_message(
                f"/theatrechat/message/{self.chat_channel}",
                [self.chat_sender, msg_str]
            )
            logging.debug(f"[QLAB] Sent to TheatreChat: {msg_str}")

        except Exception as e:
            logging.error(f"[QLAB] Failed to send to TheatreChat: {e}")
