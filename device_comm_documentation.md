
# Device Communication Documentation (GET Endpoints Only)

This section outlines the GET endpoints available for communication with the device. These endpoints allow for retrieving various status information and device settings.

## GET Endpoints

### 1. `/getSettings`
- **Description**: Retrieves the current device settings in JSON format.
- **Example Request**:
  ```
  GET /getSettings HTTP/1.1
  Host: [device_ip]
  ```
- **Example Response**:
  ```json
  {
    "debugMode": false,
    "cameraFlip": false,
    "cameraMirror": false,
    "brightness": 2,
    "contrast": 2,
    "quality": 10,
    "irEnabled": false,
    "irBrightness": 100,
    "oscReceive": true,
    "oscPort": 8000,
    "theatreChatEnabled": true,
    "theatreChatPort": 9000,
    "theatreChatChannel": "cameras",
    "theatreChatName": "NewDeviceName",
    "theatreChatMessage": "Hello World!",
    "deviceOrientation": 0,
    "backstageMode": false,
    "SOUND_THRESHOLD": 6000,
    "SOUND_DEBOUNCE_DELAY": 500,
    "PIR_DEBOUNCE_DELAY": 999999,
    "frameRate": 30,
    "saturation": 2,
    "specialEffect": 3,
    "whitebal": true,
    "awbGain": true,
    "wbMode": 2
  }
  ```

### 2. `/getBatteryPercentage`
- **Description**: Retrieves the battery percentage (returns 'N/A' when no battery is connected).
- **Example Request**:
  ```
  GET /getBatteryPercentage HTTP/1.1
  Host: [device_ip]
  ```
- **Example Response**:
  ```json
  {
    "batteryPercentage": "N/A"
  }
  ```

### 3. `/getUptime`
- **Description**: Retrieves the device uptime in a human-readable format.
- **Example Request**:
  ```
  GET /getUptime HTTP/1.1
  Host: [device_ip]
  ```
- **Example Response**:
  ```
  0 hrs 0 min 11 sec
  ```

### 4. `/getUsage`
- **Description**: Retrieves memory and CPU usage of the device.
- **Example Request**:
  ```
  GET /getUsage HTTP/1.1
  Host: [device_ip]
  ```
- **Example Response**:
  ```json
  {
    "memoryUsage": 65,
    "cpuUsage": 34
  }
  ```


# Device Communication Documentation (POST Endpoints Only)

This section outlines the POST endpoints available for communication with the device. These endpoints allow for configuring various settings like camera parameters, OSC communication, and device status.

## POST Endpoints

### 1. `/setCameraSaturation`
- **Description**: Sets the camera saturation level.
- **Required Parameter**:
  - `saturation` (Integer): The saturation level.
- **Example Request**:
  ```
  POST /setCameraSaturation HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  saturation=5
  ```
- **Example Response**: `Saturation set to: 5`

### 2. `/setCameraSpecialEffect`
- **Description**: Sets the special effect for the camera.
- **Required Parameter**:
  - `specialEffect` (Integer): The special effect setting.
- **Example Request**:
  ```
  POST /setCameraSpecialEffect HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  specialEffect=3
  ```
- **Example Response**: `Special effect set to: 3`

### 3. `/setCameraWhiteBalance`
- **Description**: Enables or disables the camera's white balance.
- **Required Parameter**:
  - `whitebal` (Integer): `1` to enable, `0` to disable.
- **Example Request**:
  ```
  POST /setCameraWhiteBalance HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  whitebal=1
  ```
- **Example Response**: `White balance enabled`

### 4. `/setCameraAwbGain`
- **Description**: Enables or disables the AWB gain.
- **Required Parameter**:
  - `awbGain` (Integer): `1` to enable, `0` to disable.
- **Example Request**:
  ```
  POST /setCameraAwbGain HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  awbGain=1
  ```
- **Example Response**: `AWB gain enabled`

### 5. `/setCameraWbMode`
- **Description**: Sets the white balance mode for the camera.
- **Required Parameter**:
  - `wbMode` (Integer): White balance mode value.
- **Example Request**:
  ```
  POST /setCameraWbMode HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  wbMode=2
  ```
- **Example Response**: `White balance mode set to: 2`

### 6. `/setDeviceName`
- **Description**: Sets the name of the device.
- **Required Parameter**:
  - `deviceName` (String): New device name.
- **Example Request**:
  ```
  POST /setDeviceName HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  deviceName=NewDeviceName
  ```
- **Example Response**: `Device name updated`

### 7. `/setCameraQuality`
- **Description**: Sets the quality of the camera.
- **Required Parameter**:
  - `quality` (Integer): Quality setting.
- **Example Request**:
  ```
  POST /setCameraQuality HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  quality=10
  ```
- **Example Response**: `OK`

### 8. `/setCameraContrast`
- **Description**: Sets the camera's contrast.
- **Required Parameter**:
  - `contrast` (Integer): Contrast value.
- **Example Request**:
  ```
  POST /setCameraContrast HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  contrast=2
  ```
- **Example Response**: `OK`

### 9. `/setCameraBrightness`
- **Description**: Sets the brightness level for the camera.
- **Required Parameter**:
  - `brightness` (Integer): Brightness value.
- **Example Request**:
  ```
  POST /setCameraBrightness HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  brightness=2
  ```
- **Example Response**: `OK`

### 10. `/setMessageSending`
- **Description**: Enables or disables message sending.
- **Required Parameter**:
  - `messageSending` (Integer): Set to `1` to enable message sending or `0` to disable.
- **Example Request**:
  ```
  POST /setMessageSending HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  messageSending=1
  ```
- **Example Response**: `OK`

### 11. `/setSoundThreshold`
- **Description**: Sets the sound threshold that the microphone will use to trigger an OSC message.
- **Required Parameter**:
  - `threshold` (Integer): Sound threshold in arbitrary units.
- **Example Request**:
  ```
  POST /setSoundThreshold HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  threshold=6000
  ```
- **Example Response**: `OK`

### 12. `/setSoundDebounceDelay`
- **Description**: Sets the debounce delay for sound input.
- **Required Parameter**:
  - `debounceDelay` (Integer): Debounce delay in milliseconds.
- **Example Request**:
  ```
  POST /setSoundDebounceDelay HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  debounceDelay=500
  ```
- **Example Response**: `OK`

### 13. `/setPirDebounceDelay`
- **Description**: Sets the debounce delay for PIR (motion sensor) input.
- **Required Parameter**:
  - `pirDebounceDelay` (Integer): PIR debounce delay in milliseconds.
- **Example Request**:
  ```
  POST /setPirDebounceDelay HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  pirDebounceDelay=999999
  ```
- **Example Response**: `OK`

### 14. `/setOscPort`
- **Description**: Sets the port number for incoming OSC messages.
- **Required Parameter**:
  - `port` (Integer): OSC port number.
- **Example Request**:
  ```
  POST /setOscPort HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  port=8000
  ```
- **Example Response**: `OK`

### 15. `/sendTestOscMessage`
- **Description**: Sends a test OSC message using the configured settings.
- **Required Parameter**:
  - `message` (String): The message to be sent.
- **Example Request**:
  ```
  POST /sendTestOscMessage HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  message=TestMessage
  ```
- **Example Response**: `OSC Message Sent`

### 16. `/restartDevice`
- **Description**: Restarts the device.
- **Example Request**:
  ```
  POST /restartDevice HTTP/1.1
  Host: [device_ip]
  ```
- **Example Response**: `Device is restarting...`

### 17. `/setTheatreChatConfig`
- **Description**: Configures TheatreChat OSC settings (port, channel, name, and message).
- **Required Parameters**:
  - `port` (Integer): The OSC port number for TheatreChat communication.
  - `channel` (String): The channel ID for TheatreChat.
  - `name` (String): The name of the TheatreChat device.
  - `message` (String): A test message to send.
- **Example Request**:
  ```
  POST /setTheatreChatConfig HTTP/1.1
  Host: [device_ip]
  Content-Type: application/x-www-form-urlencoded

  port=9000&channel=Main&name=Device1&message=TestMessage
  ```
- **Example Response**: `OK`
