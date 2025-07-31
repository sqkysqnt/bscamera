# QLab → TheatreChat Bridge Plugin

This plugin bridges **QLab** (a macOS-based cueing system) with **TheatreChat** by:

- Subscribing to QLab's OSC event stream using `/listen`
- Filtering and formatting QLab cue events (`go`, `start`, `stop`)
- Broadcasting formatted messages to TheatreChat via UDP
- Displaying a live overlay webpage showing the current cue

## Features

- ✅ Receives OSC messages from QLab
- ✅ Filters specific cue types via configurable `config.json`
- ✅ Forwards cue events to TheatreChat using OSC UDP broadcast
- ✅ Exposes a live overlay at `/qlab/overlay` to display cue text
- ✅ Sends `/listen` heartbeat every 10 seconds to keep QLab stream alive
- ✅ Configurable via web interface at `/qlab`

## Configuration

Edit `plugins/qlab_bridge/config.json`:

```json
{
  "qlab_ip": "192.168.11.128",
  "qlab_port": 53000,
  "local_port": 53001,
  "chat_channel": "qlab",
  "chat_sender": "qlab",
  "filters": ["go", "start", "stop"]
}
```

Or use the web interface at `/qlab` to update settings dynamically.

## Routes

- `/qlab` — Configuration UI
- `/qlab/overlay` — Overlay webpage displaying the current cue
- `/qlab/cue_text` — Returns the latest cue text as JSON

## Notes

- Only starts the OSC server in the Werkzeug reloader child process to avoid port conflicts during development.
- Automatically sends a `/listen` OSC message every 10 seconds to prevent QLab from timing out.

## Requirements

- Python 3.7+
- `python-osc`
- QLab with OSC enabled and permissions to send messages

---

Developed as part of the `bscamera` server plugin suite.
