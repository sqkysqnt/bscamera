import os
import subprocess
import time
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.chrome.options import Options

ffmpeg_process = None

def open_website_browser(url):
    chrome_options = Options()
    # chrome_options.add_argument("--headless")  # Removed headless
    chrome_options.add_argument("--no-sandbox")
    chrome_options.add_argument("--disable-dev-shm-usage")
    chrome_options.add_argument("--disable-extensions")
    chrome_options.add_argument("--disable-popup-blocking")
    chrome_options.add_argument("--disable-gpu")
    chrome_options.add_argument("--disable-software-rasterizer")
    chrome_options.add_argument("--window-size=1920,1080")

    driver = webdriver.Chrome(service=Service("/usr/bin/chromedriver"), options=chrome_options)
    driver.get(url)
    return driver

def start_recording(output_file):
    display_env = os.environ.get("DISPLAY", ":1")
    print(f"Recording from display: {display_env}")

    ffmpeg_cmd = [
        "ffmpeg",
        "-y",
        "-f", "x11grab",
        "-r", "30",
        "-s", "1920x1080",
        "-i", display_env,
        "-preset", "ultrafast",
        output_file
    ]
    print(f"FFmpeg command: {' '.join(ffmpeg_cmd)}")
    process = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return process

def stop_recording(ffmpeg_process):
    ffmpeg_process.terminate()
    ffmpeg_process.wait()

if __name__ == "__main__":
    website_url = "http://192.168.8.238:5000"
    output_video = "headless_recording.mp4"
    driver = None

    try:
        print("Opening website in browser...")
        driver = open_website_browser(website_url)
        
        print("Starting recording...")
        ffmpeg_process = start_recording(output_video)

        # Read FFmpeg logs for a short period to confirm it's running
        # (Non-blocking method: read lines in a loop.)
        start_time = time.time()
        record_duration = 60  # Recording duration in seconds
        while time.time() - start_time < record_duration:
            line = ffmpeg_process.stderr.readline()
            if line:
                print("FFmpeg:", line.strip())
            time.sleep(1)

    except Exception as e:
        print(f"An error occurred: {e}")

    finally:
        if ffmpeg_process:
            print("Stopping recording...")
            stop_recording(ffmpeg_process)

        print("Closing browser...")
        if driver:
            driver.quit()

        print(f"Recording saved to {output_video}")
