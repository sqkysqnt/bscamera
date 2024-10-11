
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

