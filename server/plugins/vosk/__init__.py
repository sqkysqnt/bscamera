from plugins.plugin_interface import PluginInterface
from .plugin import VoskTranscriptionPlugin

class Plugin(PluginInterface):
    def __init__(self):
        # No arguments, we just do:
        self.vosk_plugin = VoskTranscriptionPlugin()

    def register(self, app, socketio, dispatcher):
        self.vosk_plugin.register(app, socketio, dispatcher)

    def get_plugin_info(self):
        return self.vosk_plugin.get_plugin_info()
