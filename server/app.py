import eventlet
eventlet.monkey_patch()

from flask import Flask, render_template, request, redirect, url_for, Response, stream_with_context, jsonify

import requests
import threading
import json
from filelock import FileLock
import logging
import sys
import cv2
from pythonosc import udp_client
from pythonosc.dispatcher import Dispatcher
from pythonosc import osc_server
import socket
import re

# Set up the dispatcher to handle specific OSC addresses
dispatcher = Dispatcher()

app = Flask(__name__)

logging.basicConfig(level=logging.INFO)
logging.getLogger('eventlet.wsgi').setLevel(logging.ERROR)

CAMERAS_FILE = 'cameras.json'
LOCK_FILE = 'cameras.lock'
SCENES_FILE = 'scenes.json'
BROADCAST_IP = "192.168.1.255"
OSC_PORT = 27900
SENDER_NAME = "CameraServer"
DEFAULT_CHANNEL = "cameras"

last_loaded_scene = None  # Global variable to track last loaded scene


def get_broadcast_address():
    # You can set this manually if automatic detection doesn't work
    return '192.168.1.255'  # Replace with your network's broadcast IP

BROADCAST_IP = get_broadcast_address()
osc_client = udp_client.SimpleUDPClient(BROADCAST_IP, OSC_PORT)
osc_client._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

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

            # Send OSC message using global sender name and channel
            message_text = f"Loaded Scene {scene['sceneNumber']} - {scene['sceneName']}"
            send_osc_message(message_text)  # Use global sender name and channel

            # Notify the front-end to refresh the camera layout
            logging.info("Notifying front-end to refresh camera layout.")
            # Front-end should be polling or use another notification mechanism here
            notify_frontend_scene_loaded()

            logging.info(f"Scene {scene_number} loaded via OSC.")
        else:
            logging.error(f"Scene {scene_number} not found.")
    else:
        logging.error(f"Invalid OSC address: {address}. No scene number found.")





def notify_frontend_scene_loaded():
    # This function sends a message to the front-end to refresh the camera layout.
    # If you use WebSockets, emit the event here.
    # Alternatively, make an HTTP call to an endpoint that the front-end listens to.
    
    logging.info("Notifying front-end to refresh camera layout.")
    
    # Example: If your front-end polls this endpoint, you can call it here.
    # Alternatively, you can implement WebSockets for real-time updates.
    # This is a placeholder function for the notification logic.



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

        # Send OSC message to notify camera was added
        message_text = f"Camera {ip_address} added to scene."
        send_osc_message(message_text)

# Register the handlers with the dispatcher
dispatcher.map("/cameraserver/loadscene/*", osc_load_scene_handler)
dispatcher.map("/cameraserver/addcamera/*", osc_add_camera_handler)

# Function to start the OSC server
def start_osc_server():
    osc_server_address = (BROADCAST_IP, OSC_PORT)
    server = osc_server.ThreadingOSCUDPServer(osc_server_address, dispatcher)
    print(f"OSC Server started and listening on {osc_server_address}")
    server.serve_forever()

# Start the OSC server in a separate thread
osc_thread = threading.Thread(target=start_osc_server)
osc_thread.daemon = True
osc_thread.start()

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

@app.route('/')
def index():
    scenes = load_scenes()
    if scenes['lastScene']:
        last_scene_number = scenes['lastScene']
    else:
        last_scene_number = None
    return render_template('index.html', last_scene_number=last_scene_number)

@app.route('/add_camera', methods=['POST'])
def add_camera():
    ip_address = request.form.get('ip_address')
    cameras = load_current_scene_cameras()

    # Check if the camera is already added
    if ip_address and not any(camera['ip'] == ip_address for camera in cameras):
        # Assign default size, position, and name (if not provided)
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

        # Send OSC message using global sender name and channel
        message_text = f"Camera {ip_address} added to scene."
        send_osc_message(message_text)  # Channel and sender are handled globally

    return '', 204





@app.route('/remove_camera/<ip_address>')
def remove_camera(ip_address):
    cameras = load_current_scene_cameras()
    cameras = [camera for camera in cameras if camera['ip'] != ip_address]
    save_current_scene_cameras(cameras)

    # Send OSC message using global sender name and channel
    message_text = f"Camera {ip_address} removed from scene."
    send_osc_message(message_text)  # Channel and sender are handled globally

    return '', 204




@app.route('/get_cameras')
def get_cameras():
    cameras = load_current_scene_cameras()
    return jsonify(cameras)



@app.route('/camera_stream/<ip_address>')
def camera_stream(ip_address):
    stream_url = f'http://{ip_address}:81/'  # Camera MJPEG stream endpoint
    
    def generate():
        try:
            with requests.get(stream_url, stream=True, timeout=10) as r:
                for chunk in r.iter_content(chunk_size=1024):
                    if chunk:
                        yield chunk  # Stream the MJPEG chunks to the client
        except requests.exceptions.RequestException as e:
            print(f"Error proxying camera {ip_address}: {e}")
        except Exception as e:
            print(f"Unexpected error in proxy_stream: {e}")
    
    return Response(stream_with_context(generate()), mimetype='multipart/x-mixed-replace; boundary=frame')



from flask import jsonify

@app.route('/camera_settings/<ip_address>')
def camera_settings(ip_address):
    settings_url = f'http://{ip_address}/getSettings'
    try:
        response = requests.get(settings_url, timeout=5)
        response.raise_for_status()
        settings = response.json()
        return jsonify(settings)
    except requests.exceptions.RequestException as e:
        print(f"Error fetching settings from camera {ip_address}: {e}")
        return jsonify({'error': 'Unable to fetch settings'}), 500

@app.route('/update_camera', methods=['POST'])
def update_camera():
    data = request.json
    ip = data['ip']
    cameras = load_current_scene_cameras()

    # Find the camera and update position and size
    for camera in cameras:
        if camera['ip'] == ip:
            camera['position'] = data['position']
            camera['size'] = data['size']
            break

    save_current_scene_cameras(cameras)
    return '', 204  # No content response


def proxy_stream():
    try:
        with requests.get(stream_url, stream=True, timeout=10) as r:
            headers = {}
            content_type = r.headers.get('Content-Type')
            if content_type:
                headers['Content-Type'] = content_type
            for chunk in r.iter_content(chunk_size=1024):
                if chunk:
                    yield chunk
    except requests.exceptions.RequestException as e:
        print(f"Error proxying camera {ip_address}: {e}")
    except BrokenPipeError:
        print(f"BrokenPipeError: Client disconnected from stream {ip_address}")
    except GeneratorExit:
        print(f"Client disconnected from stream {ip_address}")
    except Exception as e:
        print(f"Unexpected error in proxy_stream: {e}")


    return Response(stream_with_context(proxy_stream()), mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/camera_snapshot/<ip_address>')
def camera_snapshot(ip_address):
    snapshot_url = f'http://{ip_address}/capture'  # Adjust the URL if necessary
    try:
        response = requests.get(snapshot_url, timeout=5)
        response.raise_for_status()
        return Response(response.content, mimetype='image/jpeg')
    except requests.exceptions.RequestException as e:
        print(f"Error fetching snapshot from camera {ip_address}: {e}")
        # Return a placeholder image or an error image
        return Response(status=404)

def load_scenes():
    try:
        with open(SCENES_FILE, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
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
        # If there's no last scene, create one
        last_scene_number = 1
        scenes['lastScene'] = last_scene_number
        scenes['scenes'].append({
            'sceneNumber': last_scene_number,
            'sceneName': f"Scene {last_scene_number}",
            'cameras': cameras
        })
    else:
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

    scenes = load_scenes()

    # Check for duplicate and prompt for overwrite
    existing_scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)
    if existing_scene:
        existing_scene.update(scene_data)
    else:
        scenes['scenes'].append(scene_data)

    scenes['lastScene'] = scene_number
    save_scenes(scenes)

    # Send OSC message using global sender name and channel
    message_text = f"Scene {scene_number} - {scene_name} saved."
    send_osc_message(message_text)  # Channel and sender are handled globally

    return jsonify({"status": "success"})

@app.route('/update_camera_visibility', methods=['POST'])
def update_camera_visibility():
    data = request.json
    ip = data['ip']
    is_visible = data['visible']

    # Load current scene cameras
    cameras = load_current_scene_cameras()

    # Update the visibility of the camera
    for camera in cameras:
        if camera['ip'] == ip:
            camera['visible'] = is_visible
            break

    # Save the updated scene
    save_current_scene_cameras(cameras)

    return '', 204



last_loaded_scene = None  # Global variable to track the last loaded scene

@app.route('/load_scene/<int:scene_number>')
def load_scene(scene_number):
    global last_loaded_scene  # Access the global variable
    
    scenes = load_scenes()
    scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)
    
    if scene:
        # Check if the scene being loaded is different from the last loaded scene
        if last_loaded_scene != scene_number:
            # Update lastScene in scenes.json and the global variable
            scenes['lastScene'] = scene_number
            save_scenes(scenes)
            last_loaded_scene = scene_number  # Track the last loaded scene

            # Send OSC message using global sender name and channel
            message_text = f"Loaded Scene {scene['sceneNumber']} - {scene['sceneName']}"
            send_osc_message(message_text)  # Use global sender name and channel

            return jsonify(scene)
        else:
            logging.info(f"Scene {scene_number} is already loaded. Skipping reload.")
            return jsonify(scene)  # Return the scene but skip re-sending OSC message
    else:
        return jsonify({"error": "Scene not found"}), 404





@app.route('/delete_scene/<int:scene_number>', methods=['DELETE'])
def delete_scene(scene_number):
    scenes = load_scenes()
    scenes['scenes'] = [scene for scene in scenes['scenes'] if scene['sceneNumber'] != scene_number]
    save_scenes(scenes)

    # Send OSC message using global sender name and channel
    message_text = f"Scene {scene_number} deleted."
    send_osc_message(message_text)  # Use global sender name and channel

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




def send_osc_message(message, channel=DEFAULT_CHANNEL, sender=SENDER_NAME):
    try:
        # Send OSC message using global channel and sender
        osc_client.send_message(f"/theatrechat/message/{channel}", [sender, message])
        
        logging.info(f"Sending OSC message to {BROADCAST_IP}:{OSC_PORT}")
        logging.info(f"OSC Address: /theatrechat/message/{channel}")
        logging.info(f"Arguments: Sender: {sender}, Message: {message}")
        
    except Exception as e:
        logging.error(f"Failed to send OSC message: {e}")



if __name__ == '__main__':
    import eventlet.wsgi
    app.debug = True  # Enable debug mode
    eventlet.wsgi.server(eventlet.listen(('0.0.0.0', 5000)), app)

