# __init__.py in whisper_transcription folder
from plugins.plugin_interface import PluginInterface
from .plugin import WhisperTranscriptionPlugin

class Plugin(PluginInterface):
    def __init__(self):
        self.whisper_plugin = WhisperTranscriptionPlugin()

    def register(self, app, socketio, dispatcher):
        self.whisper_plugin.register(app, socketio, dispatcher)

    def get_plugin_info(self):
        return self.whisper_plugin.get_plugin_info()
