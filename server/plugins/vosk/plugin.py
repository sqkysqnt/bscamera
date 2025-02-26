import os
import queue
import wave
import datetime
import threading
import numpy as np
import sounddevice as sd
from flask import Blueprint, render_template, jsonify
from flask_socketio import Namespace, emit
from vosk import Model, KaldiRecognizer
import json

###############################################################################
# Blueprint & Vosk Model Setup
###############################################################################
vosk_bp = Blueprint('vosk', __name__, template_folder='templates')

# Path to small Vosk model (customize if needed)
MODEL_PATH = "vosk-model-small-en-us-0.15"

# Load model once at module level
vosk_model = Model(MODEL_PATH)

# Create a recognizer at 44100 sample rate (we'll do the same for capturing)
recognizer = KaldiRecognizer(vosk_model, 44100)

# Ensure 'recordings' directory exists
AUDIO_DIR = "recordings"
os.makedirs(AUDIO_DIR, exist_ok=True)

###############################################################################
# Socket.IO Namespace
###############################################################################
class VoskNamespace(Namespace):
    def on_connect(self):
        print("[VOSK] Client connected to /vosk namespace")

    def on_disconnect(self):
        print("[VOSK] Client disconnected from /vosk namespace")

###############################################################################
# Vosk Plugin Class
###############################################################################
class VoskTranscriptionPlugin:
    _is_recording = False
    transcription = ""

    # We'll store incoming float32 blocks in a queue
    audio_queue = queue.Queue()

    # Worker threads
    capture_thread = None
    processing_thread = None

    # SoundDevice stream and WAV file
    stream = None
    wav_file = None

    # Vosk model references
    model = vosk_model
    recognizer = recognizer

    # Socket.IO reference
    socketio = None

    # Hardware / Sounddevice config
    device_index = "hw:2,0"
    device_samplerate = 41000
    channels = 1

    def __init__(self):
        print("[VOSK] Initializing plugin with model:", MODEL_PATH)
        self.model = Model(MODEL_PATH)
        self.recognizer = KaldiRecognizer(self.model, self.device_samplerate)

    def register(self, app, socketio, dispatcher):
        # Register blueprint if not already
        if 'vosk' not in app.blueprints:
            app.register_blueprint(vosk_bp, url_prefix='/vosk')

        # Register the custom Socket.IO namespace
        socketio.on_namespace(VoskNamespace('/vosk'))

        # Keep references
        self.socketio = socketio
        VoskTranscriptionPlugin.socketio = socketio

        print("[VOSK] Plugin registered successfully!")

    def get_plugin_info(self):
        return {
            "name": "Vosk Transcription",
            "url": "/vosk",
            "description": "Real-time transcription using the Vosk STT engine",
            "ignore": True
        }

    ############################################################################
    # Recording Start / Stop
    ############################################################################
    @classmethod
    def start_recording(cls):
        if cls._is_recording:
            print("[VOSK] Already recording. No action taken.")
            return

        print("[VOSK] Starting recording...")
        cls._is_recording = True
        cls.transcription = ""  # reset transcription

        # Prepare WAV filename
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = os.path.join(AUDIO_DIR, f"vosk_{timestamp}.wav")
        print(f"[VOSK] Recording to file: {filename}")

        cls.wav_file = wave.open(filename, 'wb')
        cls.wav_file.setnchannels(cls.channels)
        cls.wav_file.setsampwidth(2)  # 16-bit
        cls.wav_file.setframerate(cls.device_samplerate)

        # Start capture and processing in separate threads
        cls.capture_thread = threading.Thread(target=cls.capture_audio, daemon=True)
        cls.capture_thread.start()

        cls.processing_thread = threading.Thread(target=cls.process_audio, daemon=True)
        cls.processing_thread.start()

    @classmethod
    def stop_recording(cls):
        if not cls._is_recording:
            print("[VOSK] stop_recording called but we are not recording.")
            return

        print("[VOSK] Stopping recording...")
        cls._is_recording = False

        # If we have a live stream, stop & close it
        if cls.stream is not None:
            try:
                cls.stream.stop()
                cls.stream.close()
                cls.stream = None
                print("[VOSK] Stream closed.")
            except Exception as e:
                print(f"[VOSK] Error stopping stream: {e}")

        # Close WAV file
        if cls.wav_file:
            cls.wav_file.close()
            cls.wav_file = None
            print("[VOSK] WAV file closed.")

    ############################################################################
    # Audio Capture & STT Processing
    ############################################################################
    @classmethod
    def capture_audio(cls):
        """
        Continuously capture from the microphone at device_samplerate.
        We'll store raw float32 blocks in a queue for Vosk,
        and also write them as 16-bit to a WAV file if desired.
        """
        def audio_callback(indata, frames, time, status):
            if status:
                print(f"[VOSK] Audio error: {status}")
            if cls._is_recording:
                block_f32 = np.frombuffer(indata, dtype=np.float32)

                # Write to WAV as 16-bit
                if cls.wav_file:
                    pcm_data = (block_f32 * 32767).astype(np.int16).tobytes()
                    cls.wav_file.writeframes(pcm_data)

                # Optionally skip if queue is too big to avoid backlog
                if cls.audio_queue.qsize() > 50:
                    # e.g., skip this block to stay "real-time"
                    # or store it in a ring buffer approach
                    print("[VOSK] audio_queue is large, skipping block to avoid backlog.")
                    return

                # Push float32 block onto queue
                cls.audio_queue.put(block_f32)

        print(f"[VOSK] Opening sounddevice.InputStream @ {cls.device_samplerate} Hz, device={cls.device_index}")
        try:
            cls.stream = sd.InputStream(
                device=cls.device_index,
                samplerate=cls.device_samplerate,
                channels=cls.channels,
                blocksize=8192,  # smaller blocksize for more frequent partial updates
                latency='high',
                dtype='float32',
                callback=audio_callback
            )
            cls.stream.start()
            print("[VOSK] Audio stream started. Listening...")

            # Keep capturing until _is_recording is set to False
            while cls._is_recording:
                sd.sleep(200)  # Sleep 200ms
            print("[VOSK] capture_audio ended.")
        except Exception as e:
            print(f"[VOSK] Error in capture_audio: {e}")

    @classmethod
    def process_audio(cls):
        """
        Processes audio blocks from the queue in near real-time,
        sending them to the Vosk recognizer for transcription.
        """
        print("[VOSK] Processing thread started.")
        cls.recognizer.Reset()

        while cls._is_recording or not cls.audio_queue.empty():
            try:
                block = cls.audio_queue.get(timeout=1)
            except queue.Empty:
                continue
            except Exception as ex:
                print(f"[VOSK] Error retrieving from queue: {ex}")
                continue

            # Convert float32 -> int16
            block_int16 = (block * 32767).astype(np.int16).tobytes()

            # Feed Vosk
            got_final = cls.recognizer.AcceptWaveform(block_int16)
            if got_final:
                # We got a final recognized chunk
                res_str = cls.recognizer.Result()  # JSON string
                cls.handle_vosk_result(res_str)
            else:
                # partial
                partial_str = cls.recognizer.PartialResult()
                cls.handle_vosk_partial(partial_str)

        # Final result after the loop
        final_res_str = cls.recognizer.FinalResult()
        cls.handle_vosk_result(final_res_str)

        print("[VOSK] Processing thread stopped.")

    ############################################################################
    # Handling Recognition Output
    ############################################################################
    @classmethod
    def handle_vosk_result(cls, json_str):
        data = json.loads(json_str)
        text = data.get('text', '').strip()
        if text:
            cls.transcription += (text + " ")
            print(f"[VOSK] Final: {text}")
            if cls.socketio:
                cls.socketio.emit(
                    'vosk_transcription',
                    {"transcription": cls.transcription},
                    namespace='/vosk'
                )

    @classmethod
    def handle_vosk_partial(cls, json_str):
        data = json.loads(json_str)
        partial = data.get('partial', '').strip()
        if partial:
            # If you want partial updates, you can uncomment:
            # print(f"[VOSK partial] {partial}")
            # if cls.socketio:
            #     cls.socketio.emit(
            #         'vosk_transcription',
            #         {"transcription": cls.transcription + " (partial) " + partial},
            #         namespace='/vosk'
            #     )
            pass

###############################################################################
# Blueprint Routes
###############################################################################
@vosk_bp.route('/')
def vosk_interface():
    """
    A simple HTML page that starts/stops recording 
    and displays transcription in real time via Socket.IO.
    """
    return """
    <html>
      <head><title>VOSK Transcription</title></head>
      <body>
        <h1>VOSK Plugin</h1>
        <button onclick="startRecording()">Start</button>
        <button onclick="stopRecording()">Stop</button>
        <p id='text'></p>

        <script src="https://cdn.socket.io/4.7.2/socket.io.min.js"></script>
        <script>
          const socket = io('/vosk');
          socket.on('vosk_transcription', (data) => {
              document.getElementById('text').textContent = data.transcription;
              console.log("Transcription event:", data);
          });

          function startRecording() {
            fetch('/vosk/start', { method:'POST' })
              .then(r => r.json())
              .then(data => console.log("Start response:", data))
              .catch(e => console.error("Start error:", e));
          }
          function stopRecording() {
            fetch('/vosk/stop', { method:'POST' })
              .then(r => r.json())
              .then(data => {
                console.log("Stop response:", data);
                alert("Stopped. Transcription so far: " + data.transcription);
              })
              .catch(e => console.error("Stop error:", e));
          }
        </script>
      </body>
    </html>
    """

@vosk_bp.route('/start', methods=['POST'])
def start_vosk():
    print("** /start route called **")
    VoskTranscriptionPlugin.start_recording()
    return jsonify({"status": "started"})

@vosk_bp.route('/stop', methods=['POST'])
def stop_vosk():
    print("** /stop route was actually called! **")
    VoskTranscriptionPlugin.stop_recording()
    print("** about to return JSON from stop_vosk **")
    return jsonify({"status": "stopped", "transcription": VoskTranscriptionPlugin.transcription})
