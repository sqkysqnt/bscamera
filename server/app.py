import eventlet
eventlet.monkey_patch()

from flask import Flask, render_template, request, redirect, url_for, Response, stream_with_context, jsonify
from flask_socketio import SocketIO
import os
import requests
import threading
import json
from filelock import FileLock
import logging
import sys
import struct
import cv2
from pythonosc import udp_client
from pythonosc.dispatcher import Dispatcher
from pythonosc import osc_server
import socket
import re
import netifaces
import time
import uuid
from werkzeug.utils import secure_filename
import numpy as np
from datetime import datetime, timedelta
import atexit
import urllib3
import base64
import subprocess
#from x32_app.x32_channel import x32_bp, periodic_check
from flask import session
from functools import wraps
import importlib
from plugins.plugin_interface import PluginInterface



# Store login credentials on the server (for demo only; consider environment variables or a separate config file)
VALID_USERNAME = "hector"
VALID_PASSWORD = "hector"

http = urllib3.PoolManager()

# Initialize Flask app and SocketIO
app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet')
#app.register_blueprint(x32_bp, url_prefix='/x32')

# Set a secret key for session:
app.secret_key = "yoursecretkey"  # Change this to a secure value in production.

# Set the plugin directory
PLUGIN_DIR = "plugins"

# Set up the dispatcher to handle specific OSC addresses
dispatcher = Dispatcher()

#check_thread = threading.Thread(target=periodic_check, daemon=True)
#check_thread.start()

camera_cache = {}  # Keyed by IP address
CACHE_DURATION = 300  # 5 minutes

logging.basicConfig(level=logging.DEBUG)
logging.getLogger('eventlet.wsgi').setLevel(logging.ERROR)

CAMERAS_FILE = 'cameras.json'
LOCK_FILE = 'cameras.lock'
SCENES_FILE = 'scenes.json'
OSC_PORT = 27900

UPLOAD_FOLDER = os.path.join(app.static_folder, 'uploads')
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

last_loaded_scene = None  # Global variable to track last loaded scene

loaded_plugins_info = []

# Import the theatrechat module after initializing app, socketio, and dispatcher
import theatrechat

def get_num_cameras():
    cameras = load_current_scene_cameras()
    return len(cameras)

# Initialize TheatreChat with necessary objects
theatrechat.init_theatrechat(app, socketio, dispatcher, OSC_PORT, get_num_cameras)



camera_streams = {}  # Global dictionary to store camera streams
recorders = {}


class CameraStream:
    def __init__(self, ip_address):
        self.ip_address = ip_address
        self.stream_url = f"http://{ip_address}:81/"
        self.current_frame = None
        self.running = True
        self.recording = False
        self.last_frame = None
        self.last_frame_time = 0  # Ensure this is present
        self.frame_lock = threading.Lock()
        # Use eventlet's green thread
        self.thread = eventlet.spawn(self.fetch_frames)
        self.start_info_thread()

    def fetch_frames(self):
        last_emit_time = 0
        frame_interval = 1 / 30  # Limit to 30 FPS
        while self.running:
            try:
                response = requests.get(self.stream_url, stream=True, timeout=2)
                bytes_buffer = b''
                for chunk in response.iter_content(chunk_size=1024):
                    if not self.running:
                        break
                    bytes_buffer += chunk
                    a = bytes_buffer.find(b'\xff\xd8')  # Start of JPEG
                    b = bytes_buffer.find(b'\xff\xd9')  # End of JPEG
                    if a != -1 and b != -1:
                        jpg = bytes_buffer[a:b+2]
                        bytes_buffer = bytes_buffer[b+2:]
                        with self.frame_lock:
                            self.current_frame = jpg
                            self.last_frame_time = time.time()

                        # Emit the frame over Socket.IO
                        current_time = time.time()
                        if (current_time - last_emit_time) >= frame_interval:
                            socketio.emit('frame', {
                                'ip': self.ip_address,
                                'frame': base64.b64encode(jpg).decode('utf-8')
                            }, namespace='/')
                            last_emit_time = current_time

                        # If recording, send frame to recorder
                        if self.recording and self.ip_address in recorders:
                            recorders[self.ip_address].write_frame(jpg)

            except requests.exceptions.RequestException as e:
                logging.error(f"Error fetching frames from camera {self.ip_address}: {e}")
                time.sleep(2)  # Slight delay before retrying

    def login_required(f):
        @wraps(f)
        def decorated_function(*args, **kwargs):
            if 'logged_in' not in session or not session['logged_in']:
                return render_template("login.html")  # or return a redirect if preferred
            return f(*args, **kwargs)
        return decorated_function


    def get_frame(self):
        with self.frame_lock:
            return self.current_frame
            
    def stop(self):
        self.running = False
        if self.thread:
            self.thread.kill()
        if hasattr(self, 'info_thread') and self.info_thread:
            self.info_thread.kill()
         

    def reinitialize_capture(self):
        """Attempt to reinitialize the VideoCapture object."""
        with self.capture_lock:
            self.capture.release()
            time.sleep(1)  # Wait before attempting to reconnect
            self.capture = cv2.VideoCapture(self.stream_url)
            if not self.capture.isOpened():
                print(f"Failed to re-open video capture for camera {self.ip_address}")
                return False
            else:
                print(f"Successfully re-opened video capture for camera {self.ip_address}")
                return True

    def distribute_frame(self, frame):
        """Send a frame to all connected clients and handle recording."""
        with self.clients_lock:
            for q in self.clients[:]:
                try:
                    q.put(frame, timeout=0.1)
                except:
                    self.clients.remove(q)
        # If recording, save the frame
        with self.recording_lock:
            if self.recording and self.current_frame is not None:
                self.record_frame()

    def record_frame(self):
        """Record the current frame to the video file."""
        try:
            # Decode the JPEG image
            np_arr = np.frombuffer(self.current_frame, np.uint8)
            frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if frame is None:
                return
            # Initialize video writer if not already initialized
            if self.video_writer is None:
                height, width, _ = frame.shape
                fourcc = cv2.VideoWriter_fourcc(*'mp4v')
                fps = 30.0  # Adjust the frame rate as needed
                self.video_writer = cv2.VideoWriter(self.recording_filename, fourcc, fps, (width, height))
                if not self.video_writer.isOpened():
                    print(f"Failed to open video writer for {self.recording_filename}")
                    self.video_writer = None
                    return
            # Write frame to video
            self.video_writer.write(frame)
        except Exception as e:
            print(f"Error recording frame for camera {self.ip_address}: {e}")

    def start_recording(self, filename):
        """Start recording video to the specified filename."""
        with self.recording_lock:
            if not self.recording:
                self.recording = True
                self.recording_filename = filename
                self.video_writer = None  # Will be initialized when the first frame is recorded
                print(f"Started recording for camera {self.ip_address} to file {filename}")

    def stop_recording(self):
        """Stop recording video."""
        with self.recording_lock:
            if self.recording:
                self.recording = False
                # Release the video writer
                if self.video_writer is not None:
                    self.video_writer.release()
                    self.video_writer = None
                print(f"Stopped recording for camera {self.ip_address}")

    def client_generator(self):
        """Generator function to yield frames to a client."""
        from queue import Queue
        q = Queue(maxsize=10)
        with self.clients_lock:
            self.clients.append(q)
        try:
            while True:
                frame = q.get()
                yield frame
        except GeneratorExit:
            with self.clients_lock:
                if q in self.clients:
                    self.clients.remove(q)
        except Exception as e:
            print(f"Error in client_generator for camera {self.ip_address}: {e}")

    @staticmethod
    def create_placeholder_frame():
        """Create a placeholder frame indicating the camera is not available."""
        text = "Camera not available"
        img = np.zeros((240, 320, 3), np.uint8)
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 0.7
        color = (255, 255, 255)
        thickness = 2
        text_size = cv2.getTextSize(text, font, font_scale, thickness)[0]
        text_x = (img.shape[1] - text_size[0]) // 2
        text_y = (img.shape[0] + text_size[1]) // 2
        cv2.putText(img, text, (text_x, text_y), font, font_scale, color, thickness)
        _, jpeg = cv2.imencode('.jpg', img)
        frame = jpeg.tobytes()
        return frame

    def frame_generator(self):
        while self.running:
            try:
                with self.frame_lock:
                    frame = self.current_frame
                    # If no frame is available, use the last valid frame within the timeout window
                    if frame is None or (time.time() - self.last_frame_time) > 5:  # Reduce timeout
                        frame = self.create_placeholder_frame()
                yield (b'--frame\r\n'
                    b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
            except Exception as e:
                logging.error(f"Error in frame generator for camera {self.ip_address}: {e}")
                frame = self.create_placeholder_frame()
                yield (b'--frame\r\n'
                    b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

    def start_info_thread(self):
        self.info_thread = eventlet.spawn(self.fetch_info)
        #self.info_thread.daemon = True
        #self.info_thread.start()

    def fetch_info(self):
        while self.running:
            try:
                # Fetch camera settings
                response = requests.get(f'http://{self.ip_address}/getSettings', timeout=3)
                if response.status_code == 200:
                    settings = response.json()
                    self.settings = settings
                    # Emit the camera settings via Socket.IO
                    socketio.emit('camera_settings', {
                        'ip': self.ip_address,
                        'settings': settings
                    }, namespace='/')
                else:
                    logging.error(f"Failed to fetch settings from {self.ip_address}")
                # Fetch battery percentage as before...
                # Emit battery status as before...
            except Exception as e:
                logging.error(f"Error fetching info for camera {self.ip_address}: {e}")
            eventlet.sleep(60)  # Use eventlet.sleep


class CameraRecorder(threading.Thread):
    def __init__(self, ip_address, filename):
        super().__init__()
        self.ip_address = ip_address
        self.filename = filename
        self.running = False
        self.process = None
        self.frame_queue = eventlet.Queue()

    def run(self):
        self.running = True
        command = [
            'ffmpeg',
            '-y',
            '-f', 'image2pipe',     # or 'mjpeg' if that works better
            '-framerate', '30',      # Input frame rate
            '-video_size', '640x480', # Input frame size
            '-i', '-',               # Input from stdin
            '-c:v', 'libx264',       # Output video codec
            '-preset', 'veryfast',
            '-pix_fmt', 'yuv420p',
            self.filename
        ]
        logging.info(f"Starting FFmpeg with command: {' '.join(command)}")
        try:
            self.process = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stderr=subprocess.PIPE,
                stdout=subprocess.PIPE
            )
            while self.running:
                frame = self.frame_queue.get()
                if frame is None:  # Stop signal
                    break
                self.process.stdin.write(frame)

            # Read FFmpeg stderr for errors
            stderr_output = self.process.stderr.read().decode('utf-8')
            if stderr_output:
                logging.error(f"FFmpeg stderr for {self.ip_address}: {stderr_output}")
        except Exception as e:
            logging.error(f"Error recording from camera {self.ip_address}: {e}")
        finally:
            if self.process:
                self.process.stdin.close()
                self.process.terminate()
                self.process.wait()

    def write_frame(self, frame):
        if self.running:
            if frame.startswith(b'\xff\xd8') and frame.endswith(b'\xff\xd9'):
                self.frame_queue.put(frame)
            else:
                logging.warning(f"Invalid frame received for {self.ip_address}")


    def stop(self):
        self.running = False
        self.frame_queue.put(None)  # Signal to stop
        if self.process:
            self.process.stdin.close()
            if self.process.poll() is None:
                self.process.terminate()
                self.process.wait()


def get_broadcast_ip():
    # Find the primary network interface
    interfaces = netifaces.interfaces()
    for interface in interfaces:
        addrs = netifaces.ifaddresses(interface)
        if netifaces.AF_INET in addrs:
            for link in addrs[netifaces.AF_INET]:
                if 'broadcast' in link:
                    return link['broadcast']
    return None


BROADCAST_IP = get_broadcast_ip()
print(f"BROADCAST_IP = {BROADCAST_IP}")

# Initialize OSC client
osc_client = udp_client.SimpleUDPClient(BROADCAST_IP, OSC_PORT)
osc_client._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)


def load_plugins(app, socketio, dispatcher):
    """
    Dynamically load plugins from the plugins directory.
    """
    for plugin_name in os.listdir(PLUGIN_DIR):
        plugin_path = os.path.join(PLUGIN_DIR, plugin_name)
        if os.path.isdir(plugin_path) and "__init__.py" in os.listdir(plugin_path):
            try:
                # Import the plugin module
                module = importlib.import_module(f"{PLUGIN_DIR}.{plugin_name}")
                # Ensure the plugin implements the interface
                if hasattr(module, "Plugin") and issubclass(module.Plugin, PluginInterface):
                    plugin_instance = module.Plugin()
                    
                    # First, retrieve the plugin info:
                    info = plugin_instance.get_plugin_info()
                    
                    # Check if "ignore" is True
                    if info.get("ignore") is True:
                        print(f"Skipping plugin {plugin_name} because ignore=True.")
                        continue
                    
                    # Otherwise, go ahead and register it
                    plugin_instance.register(app, socketio, dispatcher)
                    
                    # Add the plugin info to our loaded_plugins_info
                    loaded_plugins_info.append(info)

                    print(f"Loaded plugin: {plugin_name}")
                else:
                    print(f"Skipping invalid plugin: {plugin_name}")
            except Exception as e:
                print(f"Error loading plugin {plugin_name}: {e}")


# Load plugins
load_plugins(app, socketio, dispatcher)


# Handler for loading a scene
def osc_load_scene_handler(address, *args):
    logging.info(f"OSC message received at {address} with args: {args}")

    # Extract scene number from the OSC address using regex
    match = re.match(r"/cameraserver/loadscene/(\d+)", address)

    if match:
        scene_number = int(match.group(1))  # Extract the scene number from the address

        # Load scene but do not return a Flask response, just get the scene data
        scenes = load_scenes()
        scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)

        if scene:
            # Update lastScene in scenes.json
            scenes['lastScene'] = scene_number
            save_scenes(scenes)

            # Notify the front-end to refresh the camera layout
            logging.info("Notifying front-end to refresh camera layout.")
            socketio.emit('scene_loaded', scene)

            logging.info(f"Scene {scene_number} loaded via OSC.")
        else:
            logging.error(f"Scene {scene_number} not found.")
    else:
        logging.error(f"Invalid OSC address: {address}. No scene number found.")

@app.route('/get_plugins', methods=['GET'])
def get_plugins():
    # Return the dynamic list of plugin info
    return jsonify({"plugins": loaded_plugins_info})


def notify_frontend_scene_loaded():
    # This function sends a message to the front-end to refresh the camera layout.
    # Implement the notification logic here
    logging.info("Notifying front-end to refresh camera layout.")


# Handler for adding a camera
def osc_add_camera_handler(address, *args):
    ip_address = args[0]
    cameras = load_current_scene_cameras()

    # Check if the camera is already added
    if ip_address and not any(camera['ip'] == ip_address for camera in cameras):
        new_camera = {
            "ip": ip_address,
            "name": "Unnamed Camera",  # Default camera name
            "position": {
                "left": len(cameras) * 350,
                "top": len(cameras) * 100
            },
            "size": {
                "width": 320,
                "height": 240
            },
            "visible": True
        }
        cameras.append(new_camera)
        save_current_scene_cameras(cameras)


# Register the handlers with the dispatcher
dispatcher.map("/cameraserver/loadscene/*", osc_load_scene_handler)
dispatcher.map("/cameraserver/addcamera/*", osc_add_camera_handler)


# Function to start the OSC server
def start_osc_server():
    osc_server_address = ('0.0.0.0', OSC_PORT)
    server = osc_server.ThreadingOSCUDPServer(osc_server_address, dispatcher)
    print(f"OSC Server started and listening on {osc_server_address}")
    server.serve_forever()  # This will run in the background task


# Start the OSC server using Flask-SocketIO's background task
# socketio.start_background_task(start_osc_server)

# Start the OSC server in a separate thread
# osc_thread = threading.Thread(target=start_osc_server)
# osc_thread.daemon = True
# osc_thread.start()




def load_cameras():
    with FileLock(LOCK_FILE):
        try:
            with open(CAMERAS_FILE, 'r') as f:
                cameras = json.load(f)
        except FileNotFoundError:
            cameras = []
    return cameras


def save_cameras(cameras):
    with FileLock(LOCK_FILE):
        with open(CAMERAS_FILE, 'w') as f:
            json.dump(cameras, f, indent=4)

@app.route('/is_logged_in')
def is_logged_in():
    return jsonify({'logged_in': session.get('logged_in', False)})


@app.route('/')
def index():
    # If not logged in, show just a placeholder or login page:
    if 'logged_in' not in session or not session['logged_in']:
        return render_template('login.html')

    # If logged in, proceed as normal
    scenes = load_scenes()
    if scenes['lastScene']:
        last_scene_number = scenes['lastScene']
    else:
        last_scene_number = None
    return render_template('index.html', last_scene_number=last_scene_number)

@app.route('/login', methods=['POST'])
def login():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')

    if username == VALID_USERNAME and password == VALID_PASSWORD:
        session['logged_in'] = True
        return jsonify({'status': 'success'})
    else:
        return jsonify({'status': 'fail'}), 401

@app.route('/logout', methods=['POST'])
def logout():
    session.clear()
    return jsonify({'status': 'logged_out'})


@app.route('/add_camera', methods=['POST'])
def add_camera():
    ip_address = request.form.get('ip_address')
    if not ip_address:
        app.logger.error("No IP address provided when adding camera.")
        return jsonify({'error': 'No IP provided'}), 400
    # Load all scenes and identify the current scene
    scenes = load_scenes()
    last_scene_number = scenes.get('lastScene')
    if last_scene_number is None:
        # If there's no current scene, create one
        last_scene_number = 1
        scenes['lastScene'] = last_scene_number
        scenes['scenes'].append({
            'sceneNumber': last_scene_number,
            'sceneName': f"Scene {last_scene_number}",
            'cameras': []
        })

    # Find the current scene
    current_scene = next((s for s in scenes['scenes'] if s['sceneNumber'] == last_scene_number), None)
    if current_scene is None:
        # If no current scene found, create it
        current_scene = {
            'sceneNumber': last_scene_number,
            'sceneName': f"Scene {last_scene_number}",
            'cameras': []
        }
        scenes['scenes'].append(current_scene)

    # Check if camera already exists in any scene
    camera_already_exists = any(
        any(cam['ip'] == ip_address for cam in scene['cameras'])
        for scene in scenes['scenes']
    )

    if not camera_already_exists and ip_address:
        # Create the new camera with default properties
        new_camera = {
            "ip": ip_address,
            "name": "Unnamed Camera",  # Default camera name
            "position": {
                "left": len(current_scene['cameras']) * 350,
                "top": len(current_scene['cameras']) * 100
            },
            "size": {
                "width": 320,
                "height": 240
            },
            "visible": True  # Visible in the current scene
        }

        # Add the camera to the current scene
        current_scene['cameras'].append(new_camera)

        # Add this camera to all other scenes, but hidden
        for scene in scenes['scenes']:
            if scene['sceneNumber'] != last_scene_number:
                # If the camera doesn't exist in this scene, add it as hidden
                if not any(cam['ip'] == ip_address for cam in scene['cameras']):
                    hidden_camera = new_camera.copy()
                    hidden_camera['visible'] = False
                    # Make sure to not copy by reference (dict), do a shallow copy and change fields if needed
                    hidden_camera['position'] = hidden_camera['position'].copy()
                    hidden_camera['size'] = hidden_camera['size'].copy()
                    scene['cameras'].append(hidden_camera)

        # Save changes to all scenes
        save_scenes(scenes)

    return '', 204



@app.route('/remove_camera/<ip_address>')
def remove_camera(ip_address):
    # Remove from the current scene cameras
    cameras = load_current_scene_cameras()
    cameras = [camera for camera in cameras if camera['ip'] != ip_address]
    save_current_scene_cameras(cameras)

    # Stop and remove the CameraStream instance if it exists
    if ip_address in camera_streams:
        try:
            camera_streams[ip_address].stop()
            del camera_streams[ip_address]
            logging.info(f"Camera {ip_address} removed and stream stopped.")
        except Exception as e:
            logging.error(f"Error stopping camera stream for {ip_address}: {e}")

    # Now remove the camera from EVERY scene
    scenes = load_scenes()
    for scene in scenes['scenes']:
        original_count = len(scene['cameras'])
        scene['cameras'] = [cam for cam in scene['cameras'] if cam['ip'] != ip_address]
        if len(scene['cameras']) < original_count:
            logging.info(f"Camera {ip_address} removed from scene {scene['sceneNumber']}")

    save_scenes(scenes)

    return '', 204




@app.route('/get_cameras')
def get_cameras():
    cameras = load_current_scene_cameras()
    # Update cameras with recording state
    for camera in cameras:
        ip = camera['ip']
        # Ensure the CameraStream instance exists
        if ip not in camera_streams:
            camera_streams[ip] = CameraStream(ip)
        # Add the recording state
        camera['recording'] = camera_streams[ip].recording
    return jsonify(cameras)


@app.route('/camera_stream/<ip_address>')
def camera_stream(ip_address):
    stream_url = f"http://{ip_address}:81/"  # Use the correct stream URL

    try:
        # Fetch the camera's stream
        response = http.request('GET', stream_url, preload_content=False, timeout=urllib3.Timeout(connect=5.0, read=10.0))
        
        # Get the Content-Type header from the camera's response
        content_type = response.headers.get('Content-Type')
        if content_type is None:
            logging.error("No Content-Type header in response from camera")
            return Response("Camera stream not available", status=503)
        
        # Return a Response that streams data from the camera to the client
        return Response(
            response,
            content_type=content_type,
            direct_passthrough=True
        )

    except Exception as e:
        logging.error(f"Error accessing camera {ip_address}: {e}")
        return Response("Camera stream not available", status=503)


@app.route('/get_battery_percentage/<ip_address>')
def get_battery_percentage(ip_address):
    refresh = request.args.get('refresh', '0') == '1'
    now = datetime.now()
    if not refresh and ip_address in camera_cache:
        cached_data = camera_cache[ip_address]
        if 'battery' in cached_data:
            if now - cached_data['battery']['timestamp'] < timedelta(seconds=CACHE_DURATION):
                # Return cached battery status
                return cached_data['battery']['data']
    # Fetch new data
    try:
        response = requests.get(f'http://{ip_address}/getBatteryPercentage', timeout=3)
        if response.status_code == 200:
            battery_percentage = response.text.strip()
            # Update cache
            camera_cache.setdefault(ip_address, {})['battery'] = {
                'data': battery_percentage,
                'timestamp': now
            }
            return battery_percentage
        else:
            return "N/A"
    except requests.exceptions.RequestException:
        return "N/A"


@app.route('/getBatteryPercentage/<ip_address>')
def getBatteryPercentage(ip_address):
    return get_battery_percentage(ip_address)  # Call the existing function


@app.route('/camera_settings/<ip_address>')
def camera_settings(ip_address):
    refresh = request.args.get('refresh', '0') == '1'
    now = datetime.now()
    if not refresh and ip_address in camera_cache:
        cached_data = camera_cache[ip_address]
        if 'settings' in cached_data:
            if now - cached_data['settings']['timestamp'] < timedelta(seconds=CACHE_DURATION):
                # Return cached settings
                return jsonify(cached_data['settings']['data'])
    # Fetch new data
    try:
        response = requests.get(f'http://{ip_address}/getSettings', timeout=3)
        if response.status_code == 200:
            settings = response.json()
            # Update cache
            camera_cache.setdefault(ip_address, {})['settings'] = {
                'data': settings,
                'timestamp': now
            }
            return jsonify(settings)
        else:
            return jsonify({"error": "Camera not reachable"}), 500
    except requests.exceptions.RequestException:
        return jsonify({"error": "Camera not reachable"}), 500


@app.route('/update_camera', methods=['POST'])
def update_camera():
    data = request.get_json()
    if data is None:
        app.logger.error("No JSON data received in update_camera")
        return jsonify({'error': 'No data received'}), 400

    ip = data.get('ip')
    if not ip or not isinstance(ip, str):
        app.logger.error("Invalid or missing 'ip' in update_camera")
        return jsonify({'error': 'No IP address provided'}), 400

    position = data.get('position', {})
    size = data.get('size', {})

    if not (isinstance(position.get('left'), int) and isinstance(position.get('top'), int) and 
            isinstance(size.get('width'), int) and isinstance(size.get('height'), int)):
        app.logger.error("Invalid position/size data in update_camera")
        return jsonify({'error': 'Invalid position/size values'}), 400

    cameras = load_current_scene_cameras()
    camera_found = False
    for camera in cameras:
        if camera['ip'] == ip:
            camera['position'] = position
            camera['size'] = size
            camera_found = True
            break

    if not camera_found:
        app.logger.error(f"No camera found for IP {ip} in current scene")
        return jsonify({'error': 'Camera not found'}), 400

    save_current_scene_cameras(cameras)
    return '', 204






def load_scenes():
    try:
        with open(SCENES_FILE, 'r') as f:
            data = f.read().strip()
            if not data:
                # File is empty, initialize default structure
                return {"scenes": [], "lastScene": None}
            return json.loads(data)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        logging.error(f"Error loading scenes: {e}")
        # Return a default structure if file not found or invalid JSON
        return {"scenes": [], "lastScene": None}


def save_scenes(data):
    with open(SCENES_FILE, 'w') as f:
        json.dump(data, f, indent=4)


def load_current_scene_cameras():
    scenes = load_scenes()
    last_scene_number = scenes.get('lastScene')
    if last_scene_number is None:
        return []
    scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == last_scene_number), None)
    if scene:
        return scene.get('cameras', [])
    return []


def save_current_scene_cameras(cameras):
    scenes = load_scenes()
    last_scene_number = scenes.get('lastScene')
    if last_scene_number is None:
        last_scene_number = 1
        scenes['lastScene'] = last_scene_number
        scenes['scenes'].append({
            'sceneNumber': last_scene_number,
            'sceneName': f"Scene {last_scene_number}",
            'cameras': []
        })

    # Validate cameras and ensure all fields are populated
    for camera in cameras:
        if not camera.get("ip"):
            app.logger.warning("Camera with missing IP found. Assigning default 0.0.0.0.")
            camera["ip"] = "0.0.0.0"
        camera.setdefault("name", "Unnamed Camera")
        camera.setdefault("position", {"left": 0, "top": 0})
        camera.setdefault("size", {"width": 320, "height": 240})
        camera.setdefault("visible", True)

    # Update the cameras in the current scene
    for scene in scenes['scenes']:
        if scene['sceneNumber'] == last_scene_number:
            scene['cameras'] = cameras
            break

    save_scenes(scenes)



@app.route('/save_scene', methods=['POST'])
def save_scene():
    scene_data = request.json
    scene_number = scene_data['sceneNumber']
    scene_name = scene_data.get('sceneName', f"Scene {scene_number}")

    # Validate and ensure all required fields in cameras
    for camera in scene_data.get('cameras', []):
        camera.setdefault("ip", "0.0.0.0")  # Provide a default IP if missing
        camera.setdefault("name", "Unnamed Camera")  # Default name
        camera.setdefault("position", {"left": 0, "top": 0})
        camera.setdefault("size", {"width": 320, "height": 240})
        camera.setdefault("visible", True)

    scenes = load_scenes()

    # Check for duplicate and prompt for overwrite
    existing_scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)
    if existing_scene:
        existing_scene.update(scene_data)
    else:
        scenes['scenes'].append(scene_data)

    scenes['lastScene'] = scene_number
    save_scenes(scenes)

    return jsonify({"status": "success"})


@app.route('/update_camera_visibility', methods=['POST'])
def update_camera_visibility():
    data = request.get_json()
    if not data or 'ip' not in data:
        return jsonify({'error': 'IP address not provided'}), 400

    ip = data['ip']
    is_visible = data.get('visible', True)

    cameras = load_current_scene_cameras()

    # Update the visibility of the camera
    for camera in cameras:
        if camera['ip'] == ip:
            camera['visible'] = is_visible
            break

    save_current_scene_cameras(cameras)
    return '', 204


@app.route('/upload_image', methods=['POST'])
def upload_image():
    global UPLOAD_FOLDER

    if 'image' not in request.files:
        return jsonify({"error": "No file part"}), 400

    file = request.files['image']
    if file.filename == '':
        return jsonify({"error": "No selected file"}), 400

    unique_name = str(uuid.uuid4()) + "_" + secure_filename(file.filename)
    save_path = os.path.join(UPLOAD_FOLDER, unique_name)
    file.save(save_path)

    file_url = url_for('static', filename=f'uploads/{unique_name}', _external=True)
    return jsonify({"imageUrl": file_url})


last_loaded_scene = None  # Global variable to track the last loaded scene


@socketio.on('load_scene')
def handle_load_scene(data):
    scene_number = data.get('sceneNumber')
    if scene_number is None:
        emit('error', {'message': 'Scene number not provided'})
        return

    # Load the scene as before
    scene = load_scene_by_number(scene_number)
    if scene:
        # Emit the scene data to the client
        emit('scene_loaded', scene, broadcast=True)
    else:
        emit('error', {'message': 'Scene not found'})

@app.route('/load_scene/<int:scene_number>')
def load_scene(scene_number):
    global last_loaded_scene

    scenes = load_scenes()
    scene = next((s for s in scenes['scenes'] if s['sceneNumber'] == scene_number), None)

    if scene:
        if last_loaded_scene != scene_number:
            scenes['lastScene'] = scene_number
            save_scenes(scenes)
            last_loaded_scene = scene_number
            # Emit the scene data via Socket.IO
            socketio.emit('scene_loaded', scene)
            logging.info(f"Scene {scene_number} loaded and emitted via Socket.IO.")
        else:
            logging.info(f"Scene {scene_number} is already loaded.")
        return jsonify(scene)
    else:
        logging.error(f"Scene {scene_number} not found.")
        return jsonify({"error": "Scene not found"}), 404



@app.route('/delete_scene/<int:scene_number>', methods=['DELETE'])
def delete_scene(scene_number):
    scenes = load_scenes()
    scenes['scenes'] = [scene for scene in scenes['scenes'] if scene['sceneNumber'] != scene_number]
    save_scenes(scenes)

    return jsonify({"status": "success"})


@app.route('/get_scenes')
def get_scenes():
    scenes = load_scenes()
    return jsonify(scenes['scenes'])


@app.route('/get_last_scene', methods=['GET'])
def get_last_scene():
    scenes = load_scenes()
    last_scene_number = scenes.get('lastScene')
    if last_scene_number is None:
        return jsonify({"error": "No last scene found"})

    scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == last_scene_number), None)
    if scene:
        return jsonify(scene)
    else:
        return jsonify({"error": "Scene not found"})


@app.route('/start_recording/<ip_address>')
def start_recording(ip_address):
    if ip_address in recorders:
        return jsonify({'status': f'Recording already in progress for camera {ip_address}'}), 400

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f'/home/jeremy/Videos/recordings/{ip_address}_{timestamp}.mp4'
    os.makedirs(os.path.dirname(filename), exist_ok=True)

    recorder = CameraRecorder(ip_address, filename)
    recorder.start()
    recorders[ip_address] = recorder

    return jsonify({'status': f'Started recording for camera {ip_address}', 'filename': filename})




@app.route('/stop_recording/<ip_address>')
def stop_recording(ip_address):
    if ip_address not in recorders:
        return jsonify({'status': f'No recording in progress for camera {ip_address}'}), 400

    recorder = recorders[ip_address]
    recorder.stop()
    recorder.join()
    del recorders[ip_address]

    return jsonify({'status': f'Stopped recording for camera {ip_address}'})



@app.route('/static/images/camera_unavailable.jpg')
def dynamic_placeholder():
    return Response(CameraStream.create_placeholder_frame(), mimetype='image/jpeg')



# Include the messages page route from theatrechat
@app.route('/messages')
def messages_page():
    return theatrechat.messages_page()

def cleanup():
    for stream in camera_streams.values():
        stream.stop()
    print("Cleanup completed. All camera streams have been stopped.")
    for recorder in recorders.values():
        recorder.stop()
        recorder.join()
    print("Cleanup completed. All camera recorders have been stopped.")


@socketio.on('disconnect')
def handle_disconnect():
    logging.info('Client disconnected')
    # Implement any cleanup if necessary

atexit.register(cleanup)


import os

if __name__ == '__main__':
    app.debug = True  # Enable debug mode
    # Only start the OSC server if this is the main process (not the reloader)
    if os.environ.get('WERKZEUG_RUN_MAIN') == 'true':
        # Start the OSC server using Eventlet's green thread
        socketio.start_background_task(start_osc_server)
    try:
        socketio.run(app, host='0.0.0.0', port=15000)
    except KeyboardInterrupt:
        logging.info("Application interrupted by user. Shutting down...")
        cleanup()

