# QLab + ETC → TheatreChat Bridge Plugin

This plugin bridges **QLab** (macOS-based cueing system) and **ETC Lighting Consoles** with **TheatreChat**, offering live display and cue monitoring:

- Subscribes to QLab's OSC event stream using `/listen`
- Subscribes to ETC's lighting cue output (e.g., `/eos/out/active/cue`)
- Filters and formats cue events
- Broadcasts structured messages to TheatreChat via UDP
- Displays a live web overlay with:
  - ✅ QLab cue info
  - ✅ ETC active and pending light cue info

---

## ✅ Features

- 🎭 **QLab integration**
  - Receives OSC messages from QLab (`go`, `start`, `stop`)
  - Requests cue durations and auto-clears overlay on cue stop or timeout
  - Sends `/listen` heartbeat every 10 seconds
- 💡 **ETC lighting integration**
  - Handles active and pending lighting cues
  - Displays:  
    - `LQ Current: [cue info]`  
    - `LQ Pending: [cue info]`
- 🌐 **Overlay display**
  - Exposed at `/qlab/overlay`
  - Left-justified, single-line-per-cue display
- ⚙️ **Dynamic configuration**
  - Web interface at `/qlab`
  - Or via `config.json` file
- 📡 **Broadcasts to TheatreChat**
  - Sends formatted cue strings over UDP (port 27900 by default)

---

## Configuration

Edit `plugins/qlab_bridge/config.json`:

```json
{
  "qlab_ip": "192.168.11.128",
  "qlab_port": 53000,
  "local_port": 53001,
  "chat_channel": "qlab",
  "chat_sender": "qlab",
  "enable_theatrechat": true,
  "filters": ["go", "start", "stop"]
}
```

Or open the web UI at [`/qlab`](http://localhost:yourport/qlab) to modify settings live.

---

## Routes

| Route                | Description                                 |
|---------------------|---------------------------------------------|
| `/qlab`             | Configuration web interface                 |
| `/qlab/overlay`     | Live overlay display (QLab + ETC cues)      |
| `/qlab/cue_text`    | Returns latest cue JSON:                    |
|                     | → `cue` (QLab), `light_active`, `light_pending` |

Example response from `/qlab/cue_text`:
```json
{
  "cue": "Qlab Cue - Music (Audio) - go",
  "light_active": "LQ Current: 1/1 Label 5.00",
  "light_pending": "LQ Pending: 1/1.5 cue Label 5.00"
}
```

---

## Notes

- The OSC server only starts in the Werkzeug reloader child process (avoids port bind issues during development).
- ETC messages are parsed from common EOS output paths like:
  - `/eos/out/active/cue`
  - `/eos/out/active/cue/text`
  - `/eos/out/pending/cue`
  - `/eos/out/pending/cue/text`
- Overlay display auto-clears when a QLab cue finishes or after fallback timeout.

---

## Requirements

- Python 3.7+
- [`python-osc`](https://pypi.org/project/python-osc/)
- QLab with OSC enabled (receive & reply)
- ETC console sending OSC messages
- Optional: TheatreChat running and listening for OSC UDP on port `27900`

---

Developed as part of the `bscamera` server plugin suite.
