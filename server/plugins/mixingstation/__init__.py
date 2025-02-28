# plugins/mixingstation/__init__.py

import threading
from plugins.plugin_interface import PluginInterface
from .plugin import ms_bp, periodic_check
import os
import logging

class Plugin(PluginInterface):
    def register(self, app, socketio, dispatcher):
        # Register the blueprint
        app.register_blueprint(ms_bp, url_prefix='/mixingstation')

        # Only start the thread in the true main process
        if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
            thread = threading.Thread(target=periodic_check, daemon=True)
            thread.start()
            logging.info("periodic_check thread started in main process.")

    def get_plugin_info(self):
        return {
            "name": "Mixing Station Channel Monitor",
            "url": "/mixingstation",
            "description": "Configure Mixing Station channels and send OSC messages.",
            "ignore": False
        }
