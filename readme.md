T-Camera S3 Project

Overview

This project utilizes the LilyGO T-Camera S3, leveraging a combination of WiFi, OSC messaging, web server functionality, and NeoPixel LEDs to provide an interactive system that handles various inputs, visual indications, and OSC-based communication. The T-Camera S3 also features an onboard camera module, which streams video over a local network and provides various options for interaction.

Features

WiFi Connectivity: Connects to a specified WiFi network using WiFiManager. If the network is unavailable, the device creates an access point named "MST_BS_CAM".

Web Server: Provides a web-based interface to control and configure various settings. Hosted endpoints allow the user to control the camera and LED features.

OSC Messaging: Supports incoming and outgoing OSC messages to integrate with theatrical and show control systems.

Camera Module: Streams video content over the network, providing a live video feed.

LED Indicators: NeoPixel LEDs for visual indications based on OSC commands.

Onboard Display: Displays current status and various device settings.

Hardware Requirements

LilyGO T-Camera S3 Board

Micro-USB Cable for power and programming

NeoPixel WS2812B LED Strip (optional)

External power source (if required)

Setup Instructions

Install PlatformIO or Arduino IDE to compile the code.

Clone this repository to get the project code.

Configure the WiFi credentials via WiFiManager or by connecting to the AutoConnect network to set them.

Connect the hardware components:

Plug in the LilyGO T-Camera S3 via USB.

Connect the WS2812B LED strip data pin to IO15.

Compile and Upload the code to the board.

Default Settings

WiFi AutoConnect AP: AutoConnectAP, Password: hector1995

Secondary AP Mode SSID: MST_BS_CAM, Password: hector1995

OSC Listening Port: 8000

Theatre Chat Port: 27900

Camera Orientation: Flipped by default

Camera Quality: 10

LED State: Off

LED Brightness: 10

IR: Disabled

Sound Threshold: 6000

Web Endpoints

Some of the available web endpoints include:

/ - Device Dashboard page to configure various settings.

/getSettings - Retrieves the current device settings.

/setCameraMirror - Configures the camera mirror setting.

/setOscPort - Sets the OSC port used for receiving messages.

/getBatteryPercentage - Returns the battery level if connected.

/setSoundThreshold - Configures the microphone sensitivity.

/sendTestOscMessage - Sends a test OSC message.

/restartDevice - Restarts the device.

For a full list of web endpoints and usage examples, refer to the documentation.

OSC Endpoints

/micOn - Displays "Mic On" and sets LEDs to green.

/micOff - Displays "Mic Off" and sets LEDs to red.

/micReady - Displays "Mic Ready" and sets LEDs to blue.

/standby - Displays "Standby".

/go - Displays "Go".

/warning - Displays "Warning".

/display - Updates the display with a specified message.

/clear - Clears the display.

/ledOn [color] - Turns LEDs on with the specified color.

/ledOff - Turns LEDs off.

Usage

The device is intended to be used in a theatrical environment to monitor microphone status and integrate with other show control systems. The onboard display provides status updates, and the web server allows further control of the camera, LED states, and network settings.

License

This project is licensed under the MIT License. See the LICENSE file for more details.
