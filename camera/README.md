# BSCamera (Backstage Camera) Readme

## Overview

This project utilizes the T-Camera S3 to provide a flexible and customizable camera solution for backstage use in musical theater productions. It includes OSC messaging for mic status updates, IR control, low-latency streaming, and on-screen display capabilities. It was originally created as a way for Audio Engineers to see the cast utilizing out of sightline microphones and as a way for the cast to see if the mic was on or not.

## Features

- **WiFi Connectivity**: The device tries to connect to the last known WiFi network on boot. If the network is unavailable, it creates a WiFi access point named "AutoConnectAP" with the password "hector1995". If that fails, it will create an AP named "MST_BS_CAM" also with the password "hector1995".
- **OSC Communication**: Receives commands to handle microphone statuses (`/micOn`, `/micOff`, `/micReady`), camera settings, and TheatreChat messaging.
- **LED Control**: WS2812B LEDs connected to the device are controlled via OSC commands or HTTP endpoints.
- **Web Configuration**: The device hosts a web server that allows configuring various device settings through HTTP POST endpoints.

## Web Endpoints

[For a full list of available web endpoints and usage examples, please refer to the device communication documentation](https://github.com/sqkysqnt/bscamera/blob/main/docs/device_comm_documentation.md).


- `/` - Main configuration page
- `/setCameraMirror` - Sets the camera mirror setting
- `/setIr` - Turns the IR on/off
- `/setOscReceive` - Enables or disables OSC message reception
- `/setOscPort` - Sets the port for incoming OSC communication
- `/setDeviceOrientation` - Adjusts the device orientation
- `/setSoundThreshold` - Sets the sound threshold for detection
- `/setTheatreChatConfig` - Configures TheatreChat OSC settings
- `/setMessageSending` - Enables or disables message sending
- `/setCameraQuality` - Sets the camera quality level
- `/setCameraContrast` - Sets the camera contrast level
- `/setCameraBrightness` - Sets the camera brightness level
- `/setCameraFlip` - Enables or disables camera flip
- `/setCameraResolution` - Sets the camera resolution
- `/getSettings` - Retrieves the current settings
- `/getBatteryPercentage` - Gets the current battery percentage
- `/setDeviceName` - Sets the device name
- `/getUptime` - Gets the device uptime
- `/hello` - Test endpoint returning "Hello World"
- `/capture` - Captures an image and returns it as JPEG
- `/getUsage` - Gets memory and CPU usage statistics
- `/sendTestOscMessage` - Sends a test OSC message
- `/setDebugMode` - Enables/disables debug mode
- `/restartDevice` - Restarts the device
- `/setSoundDebounceDelay` - Sets sound debounce delay
- `/setPirDebounceDelay` - Sets PIR debounce delay
- `/setFrameRate` - Sets the camera frame rate
- `/setCameraSaturation` - Sets the camera saturation level
- `/setCameraSpecialEffect` - Sets the camera special effect
- `/setCameraWhiteBalance` - Enables/disables white balance
- `/setCameraAwbGain` - Enables/disables auto white balance gain
- `/setCameraWbMode` - Sets the white balance mode
- `/setMicOnColor` - Sets the LED color when mic is on
- `/setMicOffColor` - Sets the LED color when mic is off
- `/setMicReadyColor` - Sets the LED color when mic is ready
- `/setLedState` - Turns the LED on/off
- `/setLedBrightness` - Sets the LED brightness

## OSC Endpoints

- `/micOn` - Indicates the mic is on
- `/micOff` - Indicates the mic is off
- `/micReady` - Indicates the mic is ready
- `/display` - Displays a given message on the screen
- `/clear` - Clears the display
- `/ledOn` - Turns on the LED with a specified color
- `/theatrechat/message/*` - Receives TheatreChat messages and processes commands

[More about how to communicate with the device using OSC](https://github.com/sqkysqnt/bscamera/blob/main/docs/theatrechat.md)

## Default Settings

- **Frame Rate**: 30 FPS
- **IR Enabled**: `false`
- **IR Brightness**: 50
- **LED Brightness**: 10
- **LED State**: Off
- **Mic On Color**: `#00FF00` (Green)
- **Mic Off Color**: `#FF0000` (Red)
- **Mic Ready Color**: `#0000FF` (Blue)
- **Camera Brightness**: 0
- **Camera Contrast**: 0
- **Camera Quality**: 10
- **Camera Resolution**: VGA
- **Camera Mirror**: `false`
- **Camera Flip**: `true`
- **Sound Threshold**: 6000
- **Sound Debounce Delay**: 500 ms
- **PIR Debounce Delay**: 5000 ms
- **OSC Receive**: `false`
- **OSC Port**: 8000
- **TheatreChat Enabled**: `true`
- **TheatreChat Port**: 27900
- **TheatreChat Channel**: "cameras"
- **TheatreChat Name**: "TCameraS3"
- **Device Orientation**: 0 degrees
- **Backstage Mode**: `false`
- **Debug Mode**: `true`

## Included Libraries

- WiFiManager
- WiFi
- WiFiUdp
- Arduino
- XPowersLib
- Preferences
- Wire
- U8g2lib
- esp_camera
- ESPAsyncWebServer
- AsyncTCP
- driver/i2s
- esp_vad
- Adafruit_NeoPixel
- LittleFS
- ArduinoJson
- freertos/FreeRTOS
- freertos/task
- freertos/timers
- FFat
- ArduinoOSCWiFi
- HTTPClient
- HTTPUpdate
d
