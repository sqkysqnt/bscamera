
# Device Communication Documentation

This documentation outlines the HTTP endpoints available for communication with the device. It includes GET and POST endpoints that handle configuration, camera settings, and status monitoring, including OSC message handling.

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
