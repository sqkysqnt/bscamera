# plugin.py
import os
import subprocess
import signal
import time
import requests
from flask import Blueprint, jsonify
from flask_socketio import emit
from . import transcriber_subprocess  # If you want to check constants, or you can just refer by name.
from flask import request
from flask import render_template
from flask_socketio import Namespace

# We'll run the microservice on this port
TRANSCRIBER_PORT = 7000

whisper_bp = Blueprint('whisper', __name__, template_folder='templates')



class WhisperTranscriptionPlugin:
    """
    A plugin that spawns transcriber_subprocess.py as a separate process and
    provides routes to start/stop transcription, proxying calls to that microservice.
    """

    microservice_process = None  # We'll store the subprocess.Popen object here.

    def __init__(self):
        print("[WhisperPlugin] __init__ called.")
        # Optionally start the microservice right away:
        self.ensure_microservice_running()

    def register(self, app, socketio, dispatcher):
        """
        Called by the main app to register. We'll attach the blueprint to the app
        so we have /whisper routes.
        """
        if 'whisper' not in app.blueprints:
            app.register_blueprint(whisper_bp, url_prefix='/whisper')
        
        # Use the socketio instance provided by the main app
        socketio.on_namespace(WhisperNamespace('/whisper'))

        print("[WhisperPlugin] Plugin registered successfully!")

    def get_plugin_info(self):
        return {
            "name": "Whisper Transcription (via Subprocess)",
            "url": "/whisper",
            "description": "Real-time transcription using a separate process with sounddevice + Whisper.",
            "ignore": True
        }

    def ensure_microservice_running(self):
        """
        If not already running, start transcriber_subprocess.py in the background.
        """
        if not self.is_microservice_alive():
            print("[WhisperPlugin] Microservice not running; starting...")
            self.start_microservice()
        else:
            print("[WhisperPlugin] Microservice already running.")

    def is_microservice_alive(self):
        """
        Quick check if something is responding on port 7000 (/transcription).
        """
        try:
            r = requests.get(f"http://127.0.0.1:{TRANSCRIBER_PORT}/transcription", timeout=1)
            return (r.status_code == 200)
        except Exception:
            return False

    def start_microservice(self):
        """
        Launch transcriber_subprocess.py via subprocess.Popen.
        """
        script_path = os.path.join(os.path.dirname(__file__), 'transcriber_subprocess.py')
        print(f"[WhisperPlugin] Launching {script_path} ...")

        with open("transcriber.log", "w") as log_file:
            self.microservice_process = subprocess.Popen(
                ["python3", script_path],
                stdout=log_file,
                stderr=subprocess.STDOUT,
                preexec_fn=os.setpgrp
            )

        # Start in new process group so we can kill it cleanly
        with open("transcriber.log", "w") as log_file:
            self.microservice_process = subprocess.Popen(
                ["python3", script_path],
                stdout=log_file,
                stderr=log_file,
                preexec_fn=os.setpgrp
            )
        #time.sleep(3)  # Let it bind the port

        # In start_microservice() 
        time.sleep(3)  # Increase from 1 second
        for _ in range(5):  # Retry port checks
            if self.is_microservice_alive():
                break
            time.sleep(1)
        

        # Check if it came up
        if not self.is_microservice_alive():
            print("[WhisperPlugin] Warning: Microservice didn't start properly (port check failed).")
        else:
            print(f"[WhisperPlugin] Microservice started with PID={self.microservice_process.pid}")

    def stop_microservice(self):
        """
        Kill the microservice if running.
        """
        if self.microservice_process and self.microservice_process.poll() is None:
            print("[WhisperPlugin] Killing microservice process...")
            import os
            os.killpg(os.getpgid(self.microservice_process.pid), signal.SIGTERM)
            self.microservice_process = None

    @classmethod
    def forward_to_microservice(cls, method, endpoint):
        """
        Helper to call the microservice e.g. POST to /start or /stop,
        or GET /transcription.
        """
        url = f"http://127.0.0.1:{TRANSCRIBER_PORT}{endpoint}"
        try:
            if method == 'POST':
                r = requests.post(url, timeout=5)
            else:
                r = requests.get(url, timeout=5)
            return r.json()
        except Exception as e:
            return {"error": str(e)}

class WhisperNamespace(Namespace):
    def on_connect(self):
        print("[WebSocket] Client connected to Whisper transcription stream.")

    def on_disconnect(self):
        print("[WebSocket] Client disconnected.")


##############################################################################
# Blueprint routes that proxy to the microservice
##############################################################################
@whisper_bp.route('/start', methods=['POST'])
def start_transcription():
    """
    POST /whisper/start -> Start capturing audio in the microservice.
    """
    data = WhisperTranscriptionPlugin.forward_to_microservice('POST', '/start')
    return jsonify(data)

@whisper_bp.route('/stop', methods=['POST'])
def stop_transcription():
    """
    POST /whisper/stop -> Stop capturing audio, return final transcription.
    """
    data = WhisperTranscriptionPlugin.forward_to_microservice('POST', '/stop')
    return jsonify(data)

@whisper_bp.route('/transcription', methods=['GET'])
def get_transcription():
    """
    GET /whisper/transcription -> Current partial or final transcription text.
    """
    data = WhisperTranscriptionPlugin.forward_to_microservice('GET', '/transcription')
    return jsonify(data)

@whisper_bp.route('/push_partial', methods=['POST'])
def push_partial():
    """
    Receives partial text from the microservice 
    and broadcasts it via Socket.IO.
    """
    data = request.json or {}
    partial_text = data.get("partial", "").strip()
    if partial_text:
        print(f"[WhisperPlugin] Received partial: {partial_text}")

        # Emit the partial result to all connected clients
        emit('whisper_transcription', {'transcription': partial_text}, namespace='/whisper', broadcast=True)

    return jsonify({"status": "ok"})

@whisper_bp.route('/')
def whisper_interface():
    """
    Render the HTML template from 'templates/whisper_interface.html'.
    """
    return render_template('whisper_interface.html')
