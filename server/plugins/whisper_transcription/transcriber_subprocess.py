import os
import queue
import wave
import datetime
import threading
import numpy as np
import sounddevice as sd
import librosa
import whisper
from flask import Flask, jsonify, request
import requests

app = Flask(__name__)

# Global state
is_recording = False
transcription = ""
audio_queue = queue.Queue()
stream = None
wav_file = None

print(sd.query_devices())

# Audio settings
DEVICE_INDEX = 0            # or "hw:2,0" if you want to specify directly
DEVICE_SR = 44100           # We capture at 44.1 kHz
CHANNELS = 1

# Minimum number of samples after we resample to 16 kHz
# e.g. 300 samples is ~0.018s at 16k. If smaller, skip.
MIN_SAMPLES_16K = 300

# Whisper model
model = whisper.load_model("base.en", device="cpu", in_memory=True)

@app.route('/start', methods=['POST'])
def start_recording():
    global is_recording, transcription
    if is_recording:
        return jsonify({"status": "already_recording"})
    is_recording = True
    transcription = ""

    # Start audio capture & processing in background threads
    threading.Thread(target=capture_audio, daemon=True).start()
    threading.Thread(target=process_audio, daemon=True).start()

    return jsonify({"status": "started"})

@app.route('/stop', methods=['POST'])
def stop_recording():
    global is_recording
    if not is_recording:
        return jsonify({"status": "not_recording"})
    is_recording = False
    return jsonify({"status": "stopped", "transcription": transcription})

@app.route('/transcription', methods=['GET'])
def get_transcription():
    return jsonify({"transcription": transcription})

def capture_audio():
    """
    Continuously captures audio at 44.1 kHz, storing float32 blocks in a queue.
    Also writes to a WAV if desired.
    """
    global is_recording, stream, wav_file
    os.makedirs("recordings", exist_ok=True)
    filename = f"recordings/whisper_{datetime.datetime.now().strftime('%Y%m%d_%H-%M-%S')}.wav"
    wav_file = wave.open(filename, 'wb')
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(2)  # 16-bit PCM
    wav_file.setframerate(DEVICE_SR)
    print(f"[Subprocess] Recording to: {filename}")

    try:
        with sd.InputStream(device=DEVICE_INDEX,
                            samplerate=DEVICE_SR,
                            channels=CHANNELS,
                            dtype='float32',
                            callback=audio_callback):
            while is_recording:
                # Sleep briefly, let callback fill queue
                sd.sleep(200)  
    except Exception as e:
        print("[Subprocess] capture_audio error:", e)
    finally:
        if wav_file:
            wav_file.close()
            wav_file = None
        print("[Subprocess] capture_audio finished.")

def audio_callback(indata, frames, time_info, status):
    if status:
        print("[Subprocess] Audio status:", status)
    block_f32 = np.frombuffer(indata, dtype=np.float32)
    # (Optional) Write to WAV if desired
    # Then quickly push to queue
    audio_queue.put(block_f32)

def process_audio():
    """
    Processes audio in near real-time:
      - Accumulates float32 data in 'buffer'.
      - Every time we have at least 44,100 samples, we slice 1 second of audio.
      - We resample from 44.1k -> 16k, ensuring we have enough samples.
      - We run whisper on that chunk, and append text to `transcription`.
    """
    global is_recording, transcription
    buffer = np.array([], dtype=np.float32)

    while is_recording or not audio_queue.empty():
        try:
            block = audio_queue.get(timeout=1)
        except queue.Empty:
            continue

        # Append new audio block
        buffer = np.append(buffer, block)
        print(f"[Subprocess] buffer has {len(buffer)} samples, need {DEVICE_SR} for 1 sec")

        # While we have at least one second at 44.1 kHz, process a chunk
        while len(buffer) >= DEVICE_SR:
            chunk = buffer[:DEVICE_SR]
            buffer = buffer[DEVICE_SR:]

            print("[Subprocess] Got 1 second of audio. Resampling to 16k...")
            chunk_16k = librosa.resample(chunk, orig_sr=DEVICE_SR, target_sr=16000)

            if len(chunk_16k) < MIN_SAMPLES_16K:
                print(f"[Subprocess] Skipping chunk_16k (too small: {len(chunk_16k)} samples).")
                continue

            # Attempt transcription
            try:
                result = model.transcribe(chunk_16k, fp16=False)
            except RuntimeError as e:
                print("[Subprocess] Caught an error in transcribe:", e)
                continue

            text_chunk = result.get("text", "").strip()
            if text_chunk:
                transcription += text_chunk + " "
                print("[Subprocess] Partial transcription:", text_chunk)

                # Optionally push partial text to main app
                try:
                    requests.post(
                        "http://127.0.0.1:15000/whisper/push_partial",
                        json={"partial": text_chunk},
                        timeout=2
                    )
                except Exception as e:
                    print("[Subprocess] Error pushing partial text:", e)

    # Final flush for leftover
    leftover_len = len(buffer)
    if leftover_len > 0:
        print(f"[Subprocess] Final leftover in buffer: {leftover_len} samples")
        chunk_16k = librosa.resample(buffer, orig_sr=DEVICE_SR, target_sr=16000)
        if len(chunk_16k) >= MIN_SAMPLES_16K:
            try:
                result = model.transcribe(chunk_16k, fp16=False)
                text_chunk = result.get("text", "").strip()
                if text_chunk:
                    transcription += text_chunk + " "
                    print("[Subprocess] Final leftover transcription:", text_chunk)
            except RuntimeError as e:
                print("[Subprocess] Error in final leftover transcribe:", e)
        else:
            print(f"[Subprocess] Skipping leftover chunk_16k (too small: {len(chunk_16k)} samples).")

    print("[Subprocess] process_audio finished.")

if __name__ == '__main__':
    print("[Subprocess] Starting transcriber on port 7000...")
    app.run(host='127.0.0.1', port=7000, debug=False)
