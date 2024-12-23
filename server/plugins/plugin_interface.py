# plugins/plugin_interface.py
class PluginInterface:
    def register(self, app, socketio, dispatcher):
        """
        Register the plugin with the main app.
        """
        raise NotImplementedError("Plugins must implement the `register` method.")
    
    def get_plugin_info(self):
        """
        Return a dict with keys like:
          - "name": str (Plugin display name)
          - "url": str (Main route link, e.g. "/x32")
          - "description": str (Short description)
        """
        raise NotImplementedError("Plugins must implement the `get_plugin_info` method.")
