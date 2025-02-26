from flask import Blueprint, jsonify
from plugins.plugin_interface import PluginInterface

# Define the blueprint outside the class
test_bp = Blueprint('test_plugin', __name__)

@test_bp.route('/')
def test_home():
    return jsonify({"message": "Test Plugin is working!"})

class TestPlugin(PluginInterface):
    def register(self, app, socketio, dispatcher):
        """
        Register the test plugin with the main Flask app.
        """
        if 'test_plugin' not in [bp.name for bp in app.blueprints.values()]:
            app.register_blueprint(test_bp, url_prefix='/test')

    def get_plugin_info(self):
        return {
            "name": "Test Plugin",
            "url": "/test",
            "description": "A simple test plugin to verify loading.",
            "ignore": True
        }
