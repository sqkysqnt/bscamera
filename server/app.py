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

app = Flask(__name__)



logging.basicConfig(level=logging.INFO)
logging.getLogger('eventlet.wsgi').setLevel(logging.ERROR)

CAMERAS_FILE = 'cameras.json'
LOCK_FILE = 'cameras.lock'
SCENES_FILE = 'scenes.json'

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
    return render_template('index.html')

@app.route('/add_camera', methods=['POST'])
def add_camera():
    ip_address = request.form.get('ip_address')
    cameras = load_cameras()

    # Check if the camera is already added
    if ip_address and not any(camera['ip'] == ip_address for camera in cameras):
        # Assign default size, position, and name (if not provided)
        new_camera = {
            "ip": ip_address,
            "name": "Unnamed Camera",  # Default camera name
            "position": {
                "left": len(cameras) * 350,  # Adjust left based on number of cameras
                "top": len(cameras) * 100   # Adjust top based on number of cameras
            },
            "size": {
                "width": 320,   # Default width
                "height": 240   # Default height
            }
        }
        cameras.append(new_camera)
        save_cameras(cameras)
    return '', 204  # Return no content since it's an AJAX request



@app.route('/remove_camera/<ip_address>')
def remove_camera(ip_address):
    cameras = load_cameras()
    if ip_address in cameras:
        cameras.remove(ip_address)
        save_cameras(cameras)
    return '', 204  # Return no content for AJAX request



@app.route('/get_cameras')
def get_cameras():
    cameras = load_cameras()
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
    cameras = load_cameras()

    # Find the camera and update position and size
    for camera in cameras:
        if camera['ip'] == ip:
            camera['position'] = data['position']
            camera['size'] = data['size']
            break

    save_cameras(cameras)
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


@app.route('/save_scene', methods=['POST'])
def save_scene():
    scene_data = request.json
    scene_number = scene_data['sceneNumber']
    scene_name = scene_data.get('sceneName', f"Scene {scene_number}")

    scenes = load_scenes()

    # Check for duplicate and prompt for overwrite
    existing_scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)
    if existing_scene:
        # If overwrite confirmation needed, handle it on the frontend
        existing_scene.update(scene_data)
    else:
        scenes['scenes'].append(scene_data)

    scenes['lastScene'] = scene_number
    save_scenes(scenes)
    return jsonify({"status": "success"})


@app.route('/load_scene/<int:scene_number>')
def load_scene(scene_number):
    scenes = load_scenes()
    scene = next((scene for scene in scenes['scenes'] if scene['sceneNumber'] == scene_number), None)
    if scene:
        return jsonify(scene)
    else:
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


@app.route('/get_last_scene')
def get_last_scene():
    scenes = load_scenes()
    if scenes['lastScene']:
        return load_scene(scenes['lastScene'])
    else:
        return jsonify({"error": "No last scene found"}), 404


if __name__ == '__main__':
    import eventlet.wsgi
    app.debug = True  # Enable debug mode
    eventlet.wsgi.server(eventlet.listen(('0.0.0.0', 5000)), app)
