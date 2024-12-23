# plugins/x32_app/__init__.py

import threading
from plugins.plugin_interface import PluginInterface
from .x32_channel import x32_bp, periodic_check
import os
import logging

class Plugin(PluginInterface):
    def register(self, app, socketio, dispatcher):
        # Register the blueprint
        app.register_blueprint(x32_bp, url_prefix='/x32')

        # Only start the thread in the true main process
        if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
            thread = threading.Thread(target=periodic_check, daemon=True)
            thread.start()
            logging.info("periodic_check thread started in main process.")

    def get_plugin_info(self):
        return {
            "name": "X32 Channel Config",
            "url": "/x32",
            "description": "Configure X32 mixer channels and send OSC messages.",
            "ignore": False
        }
