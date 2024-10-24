import eventlet
eventlet.monkey_patch()

from flask import Flask, render_template, request, redirect, url_for, Response, stream_with_context, jsonify
import requests
import threading
import json
from filelock import FileLock

app = Flask(__name__)

CAMERAS_FILE = 'cameras.json'
LOCK_FILE = 'cameras.lock'

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
            json.dump(cameras, f)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/add_camera', methods=['POST'])
def add_camera():
    ip_address = request.form.get('ip_address')
    cameras = load_cameras()
    if ip_address and ip_address not in cameras:
        cameras.append(ip_address)
        save_cameras(cameras)
    return '', 204  # Return no content since it's an AJAX request

@app.route('/remove_camera/<ip_address>')
def remove_camera(ip_address):
    cameras = load_cameras()
    if ip_address in cameras:
        cameras.remove(ip_address)
        save_cameras(cameras)
    return redirect(url_for('index'))

@app.route('/get_cameras')
def get_cameras():
    cameras = load_cameras()
    return jsonify(cameras)

@app.route('/camera_stream/<ip_address>')
def camera_stream(ip_address):
    stream_url = f'http://{ip_address}:81/'

    def proxy_stream():
        try:
            with requests.get(stream_url, stream=True, timeout=5) as r:
                headers = {}
                content_type = r.headers.get('Content-Type')
                if content_type:
                    headers['Content-Type'] = content_type
                for chunk in r.iter_content(chunk_size=1024):
                    if chunk:
                        yield chunk
        except requests.exceptions.RequestException as e:
            print(f"Error proxying camera {ip_address}: {e}")
        except GeneratorExit:
            print(f"Client disconnected from stream {ip_address}")
        except Exception as e:
            print(f"Unexpected error in proxy_stream: {e}")

    return Response(stream_with_context(proxy_stream()), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    import eventlet.wsgi
    app.debug = True  # Enable debug mode
    eventlet.wsgi.server(eventlet.listen(('0.0.0.0', 5000)), app)
