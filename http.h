#ifndef HTTP_H
#define HTTP_H

#include "index.h"
#include "littlefs_functions.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>  // Include AsyncWebServer library
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

volatile uint32_t idleCounter = 0;

extern "C" void vApplicationIdleHook(void) {
    idleCounter++;
}

unsigned long previousMillis = 0;
unsigned long previousIdleCounter = 0;
int cpuUsage = 0;

// External declarations
extern AsyncWebServer server;  // Update to AsyncWebServer
extern bool debugMode;
extern bool cameraFlip;
extern bool cameraMirror;
extern int cameraBrightness;
extern int cameraContrast;
extern int cameraQuality;
extern bool irEnabled;
extern int irBrightness;
extern bool oscReceiveEnabled;
extern int oscPort;
extern bool theatreChatEnabled;
extern int theatreChatPort;
extern String theatreChatChannel;
extern String theatreChatName;
extern String theatreChatMessage;
extern int deviceOrientation;
extern bool backstageMode;
extern Preferences preferences;
extern String cameraResolution;
extern SemaphoreHandle_t cameraMutex;  // Declare the mutex
//extern bool isStreaming;               // Declare isStreaming as extern
//extern WiFiClient streamingClient;     // Declare streamingClient as extern
extern camera_config_t camera_config;  // Declare camera_config as extern
extern int frameRate;
extern int cameraSaturation;
extern int cameraSpecialEffect;
extern bool cameraWhiteBalance;
extern bool cameraAwbGain;
extern int cameraWbMode;


void saveSettings();
void loadSettings();
void logSerial(String message);
void sendTheatreChatOscMessage(String messageToSend);

// Camera Mirror Handler
void handleSetCameraMirror(AsyncWebServerRequest *request) {
    logSerial("handleSetCameraMirror Called");
    if (!request->hasParam("mirror", true)) {
        request->send(400, "text/plain", "Missing 'mirror' argument");
        return;
    }

    String mirrorParam = request->getParam("mirror", true)->value();
    logSerial("Received mirror param: " + mirrorParam);

    cameraMirror = (mirrorParam == "1");
    logSerial("Camera mirror set to: " + String(cameraMirror ? "Enabled" : "Disabled"));

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *s = esp_camera_sensor_get();
        
        // Check if the sensor is NULL and handle the error
        if (s == NULL) {
            logSerial("Error: Camera sensor not initialized");
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera sensor not initialized");
            return;
        }

        s->set_hmirror(s, cameraMirror);
        xSemaphoreGive(cameraMutex);
        saveSettings();
        request->send(200, "text/plain", "OK");
    } else {
        logSerial("Error: Failed to acquire camera mutex");
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}


// IR On/Off Handler
void handleSetIr(AsyncWebServerRequest *request) {
    if (!request->hasParam("ir", true)) {
        request->send(400, "text/plain", "Missing 'ir' argument");
        return;
    }

    irEnabled = (request->getParam("ir", true)->value() == "1");

    if (irEnabled) {
        digitalWrite(IR_LED_PIN, HIGH);
        logSerial("IR Enabled");
    } else {
        digitalWrite(IR_LED_PIN, LOW);
        logSerial("IR Disabled");
    }
    saveSettings();
    request->send(200, "text/plain", "OK");
}

// OSC Receive On/Off Handler
void handleSetOscReceive(AsyncWebServerRequest *request) {
    if (!request->hasParam("oscReceive", true)) {
        request->send(400, "text/plain", "Missing 'oscReceive' argument");
        return;
    }

    oscReceiveEnabled = (request->getParam("oscReceive", true)->value() == "1");
    logSerial("OSC Receive set to: " + String(oscReceiveEnabled ? "ON" : "OFF"));

    saveSettings();
    request->send(200, "text/plain", "OK");
}

// OSC Port Handler
void handleSetOscPort(AsyncWebServerRequest *request) {
  logSerial("handleSetOscPort function");
    if (!request->hasParam("port", true)) {
        request->send(400, "text/plain", "Missing 'port' argument");
        return;
    }

    int port = request->getParam("port", true)->value().toInt();
    if (port <= 0) {
        request->send(400, "text/plain", "Invalid port number");
        return;
    }

    oscPort = port;
    saveSettings();
    logSerial("OSC Port set to: " + String(oscPort));

    request->send(200, "text/plain", "OK");
}

void applyOrientation() {
    // Acquire the camera mutex before applying orientation settings
    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *camera_sensor = esp_camera_sensor_get();  // Get the camera sensor object
        if (camera_sensor == NULL) {
            logSerial("Failed to get camera sensor for orientation adjustment.");
            xSemaphoreGive(cameraMutex);  // Release the mutex
            return;
        }

        // Apply orientation settings based on deviceOrientation
        switch (deviceOrientation) {
            case 90:
                // Camera: Rotate by 90 degrees using a combination of horizontal mirror and vertical flip
                camera_sensor->set_hmirror(camera_sensor, 1);  // Enable horizontal mirroring
                camera_sensor->set_vflip(camera_sensor, 0);    // Disable vertical flip
                
                logSerial("Applied 90 degree rotation for camera.");
                break;

            case 180:
                // Camera: Rotate by 180 degrees using vertical flip only
                camera_sensor->set_hmirror(camera_sensor, 0);  // Disable horizontal mirroring
                camera_sensor->set_vflip(camera_sensor, 1);    // Enable vertical flip

                logSerial("Applied 180 degree rotation for camera.");
                break;

            case 270:
                // Camera: Rotate by 270 degrees using a combination of horizontal mirror and vertical flip
                camera_sensor->set_hmirror(camera_sensor, 1);  // Enable horizontal mirroring
                camera_sensor->set_vflip(camera_sensor, 1);    // Enable vertical flip

                logSerial("Applied 270 degree rotation for camera.");
                break;

            default:
                // Default (0 degrees), no rotation
                camera_sensor->set_hmirror(camera_sensor, 0);  // Disable horizontal mirroring
                camera_sensor->set_vflip(camera_sensor, 0);    // Disable vertical flip

                logSerial("Applied 0 degree (default) rotation for camera.");
                break;
        }

        // Release the camera mutex after the settings have been applied
        xSemaphoreGive(cameraMutex);

    } else {
        logSerial("Failed to acquire camera mutex for orientation adjustment.");
    }

    // Rotate the display based on deviceOrientation
    switch (deviceOrientation) {
        case 90:
            u8g2.setDisplayRotation(U8G2_R3);  // Rotate screen by 90 degrees
            logSerial("Applied 90 degree rotation for screen.");
            break;
        case 180:
            u8g2.setDisplayRotation(U8G2_R0);  // Rotate screen by 180 degrees
            logSerial("Applied 180 degree rotation for screen.");
            break;
        case 270:
            u8g2.setDisplayRotation(U8G2_R1);  // Rotate screen by 270 degrees
            logSerial("Applied 270 degree rotation for screen.");
            break;
        default:
            u8g2.setDisplayRotation(U8G2_R2);  // No rotation (0 degrees)
            logSerial("Applied 0 degree (default) rotation for screen.");
            break;
    }

    // After setting the orientation, update the screen content
    u8g2.clearBuffer();    // Clear the screen
    displayScreen("Orientation: " + String(deviceOrientation) + "°");  // Example message
    u8g2.sendBuffer();     // Push the content to the display
}

// Device Orientation Handler
void handleSetDeviceOrientation(AsyncWebServerRequest *request) {
    if (!request->hasParam("orientation", true)) {
        request->send(400, "text/plain", "Missing 'orientation' argument");
        return;
    }

    // Get the orientation value from the request and convert it to an integer
    deviceOrientation = request->getParam("orientation", true)->value().toInt();
    
    // Log the new orientation value
    logSerial("Device Orientation set to: " + String(deviceOrientation) + " degrees");

    // Save the updated settings (e.g., to Preferences or SPIFFS)
    saveSettings();  // Assuming you have a saveSettings() function to store preferences
    applyOrientation();
    // Send a response back to the client confirming the change
    request->send(200, "text/plain", "OK");
}





bool validateOscConfig(int port, String channel, String message, String name) {
    // Validate port (must be between 1 and 65535)
    if (port < 1 || port > 65535) {
        logSerial("Error: Invalid port number. Must be between 1 and 65535.");
        return false;
    }

    // Validate channel (must be a non-empty string with alphanumeric characters)
    if (channel.length() == 0) {
        logSerial("Error: Channel ID is empty.");
        return false;
    }
    // Check for invalid characters in the channel (OSC paths generally allow alphanumeric, "_", "-", ".")
    for (int i = 0; i < channel.length(); i++) {
        if (!isalnum(channel[i]) && channel[i] != '_' && channel[i] != '-' && channel[i] != '.') {
            logSerial("Error: Invalid character in channel ID.");
            return false;
        }
    }

    // Validate message (should be non-empty and within reasonable size)
    if (message.length() == 0) {
        logSerial("Error: Message is empty.");
        return false;
    }
    if (message.length() > 128) { // Arbitrary limit for message length
        logSerial("Error: Message too long.");
        return false;
    }

    // Validate name (should be non-empty and within reasonable size)
    if (name.length() == 0) {
        logSerial("Error: Sender name is empty.");
        return false;
    }
    if (name.length() > 32) { // Arbitrary limit for name length
        logSerial("Error: Sender name too long.");
        return false;
    }

    // All validations passed
    logSerial("OSC Config validated successfully.");
    return true;
}

// Theatre Chat Configuration Handler
void handleSetTheatreChatConfig(AsyncWebServerRequest *request) {
    if (!request->hasParam("port", true) || !request->hasParam("channel", true) ||
        !request->hasParam("name", true) || !request->hasParam("message", true)) {
        request->send(400, "text/plain", "Missing 'port', 'channel', 'name', or 'message' arguments");
        return;
    }

    theatreChatPort = request->getParam("port", true)->value().toInt();
    theatreChatChannel = request->getParam("channel", true)->value();
    theatreChatName = request->getParam("name", true)->value();
    theatreChatMessage = request->getParam("message", true)->value();

    // Validate OSC config before sending the message
    if (!validateOscConfig(theatreChatPort, theatreChatChannel, theatreChatMessage, theatreChatName)) {
        request->send(400, "text/plain", "Invalid OSC configuration.");
        return;
    }

    logSerial("TheatreChat Config - Port: " + String(theatreChatPort) +
              ", Channel: " + theatreChatChannel + ", Name: " + theatreChatName +
              ", Message: " + theatreChatMessage);

    saveSettings();
    request->send(200, "text/plain", "OK");
}

// Function to handle device restart
void handleRestartDevice(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Device is restarting...");
    delay(1000);  // Give time for response to be sent
    ESP.restart();  // Restart the device
}


// Server-side handler to set the new device name
void handleSetDeviceName(AsyncWebServerRequest *request) {
    if (!request->hasParam("deviceName", true)) {
        request->send(400, "text/plain", "Missing 'deviceName' argument");
        return;
    }

    String newDeviceName = request->getParam("deviceName", true)->value();
    theatreChatName = newDeviceName;  // Update the global device name variable
    logSerial("New Device Name: "+ theatreChatName);
    saveSettings();  // Save to preferences or storage
    request->send(200, "text/plain", "Device name updated");
}

void handleSetSoundThreshold(AsyncWebServerRequest *request) {
    if (!request->hasParam("threshold", true)) {
        request->send(400, "text/plain", "Missing 'threshold' argument");
        return;
    }

    // Update the global variable
    SOUND_THRESHOLD = request->getParam("threshold", true)->value().toInt();

    logSerial("Sound Threshold set to: " + String(SOUND_THRESHOLD));
    saveSettings();  // Save to preferences or storage
    request->send(200, "text/plain", "OK");
}


void handleSetSoundDebounceDelay(AsyncWebServerRequest *request) {
    if (!request->hasParam("debounceDelay", true)) {
        request->send(400, "text/plain", "Missing 'debounceDelay' argument");
        return;
    }

    // Update the global variable
    SOUND_DEBOUNCE_DELAY = request->getParam("debounceDelay", true)->value().toInt();

    // Save to preferences
    logSerial("Sound Debounce Delay set to: " + String(SOUND_DEBOUNCE_DELAY) + " ms");
    saveSettings();  // Save to preferences or storage
    request->send(200, "text/plain", "OK");
}

void handleSetPirDebounceDelay(AsyncWebServerRequest *request) {
    if (!request->hasParam("pirDebounceDelay", true)) {
        request->send(400, "text/plain", "Missing 'pirDebounceDelay' argument");
        return;
    }

    // Update the global variable
    PIR_DEBOUNCE_DELAY = request->getParam("pirDebounceDelay", true)->value().toInt();

    // Save to preferences
    logSerial("PIR Debounce Delay set to: " + String(PIR_DEBOUNCE_DELAY) + " ms");
    saveSettings();  // Save to preferences or storage
    request->send(200, "text/plain", "OK");
}

void handleSetCameraSaturation(AsyncWebServerRequest *request) {
    if (request->hasParam("saturation", true)) {
        const AsyncWebParameter* p = request->getParam("saturation", true);
        cameraSaturation = p->value().toInt();

        // Apply to camera
        sensor_t * s = esp_camera_sensor_get();
        s->set_saturation(s, cameraSaturation);

        // Save to preferences
        saveSettings();

        request->send(200, "text/plain", "Saturation set to: " + String(cameraSaturation));
    } else {
        request->send(400, "text/plain", "Saturation value not provided.");
    }
}

void handleSetCameraSpecialEffect(AsyncWebServerRequest *request) {
    if (request->hasParam("specialEffect", true)) {
        const AsyncWebParameter* p = request->getParam("specialEffect", true);
        cameraSpecialEffect = p->value().toInt();

        // Apply to camera
        sensor_t * s = esp_camera_sensor_get();
        s->set_special_effect(s, cameraSpecialEffect);

        // Save to preferences
        saveSettings();

        request->send(200, "text/plain", "Special effect set to: " + String(cameraSpecialEffect));
    } else {
        request->send(400, "text/plain", "Special effect value not provided.");
    }
}

void handleSetCameraWhiteBalance(AsyncWebServerRequest *request) {
    if (request->hasParam("whitebal", true)) {
        const AsyncWebParameter* p = request->getParam("whitebal", true);
        cameraWhiteBalance = p->value().toInt() == 1;

        // Apply to camera
        sensor_t * s = esp_camera_sensor_get();
        s->set_whitebal(s, cameraWhiteBalance);

        // Save to preferences
        saveSettings();

        request->send(200, "text/plain", "White balance " + String(cameraWhiteBalance ? "enabled" : "disabled"));
    } else {
        request->send(400, "text/plain", "White balance value not provided.");
    }
}

void handleSetCameraAwbGain(AsyncWebServerRequest *request) {
    if (request->hasParam("awbGain", true)) {
        const AsyncWebParameter* p = request->getParam("awbGain", true);
        cameraAwbGain = p->value().toInt() == 1;

        // Apply to camera
        sensor_t * s = esp_camera_sensor_get();
        s->set_awb_gain(s, cameraAwbGain);

        // Save to preferences
        saveSettings();

        request->send(200, "text/plain", "AWB gain " + String(cameraAwbGain ? "enabled" : "disabled"));
    } else {
        request->send(400, "text/plain", "AWB gain value not provided.");
    }
}

void handleSetCameraWbMode(AsyncWebServerRequest *request) {
    if (request->hasParam("wbMode", true)) {
        const AsyncWebParameter* p = request->getParam("wbMode", true);
        cameraWbMode = p->value().toInt();

        // Apply to camera
        sensor_t * s = esp_camera_sensor_get();
        s->set_wb_mode(s, cameraWbMode);

        // Save to preferences
        saveSettings();

        request->send(200, "text/plain", "White balance mode set to: " + String(cameraWbMode));
    } else {
        request->send(400, "text/plain", "White balance mode value not provided.");
    }
}


void handleSetFrameRate(AsyncWebServerRequest *request) {
    if (request->hasParam("frameRate", true)) {
        const AsyncWebParameter* p = request->getParam("frameRate", true);
        frameRate = p->value().toInt();  // Update the global variable


        logSerial("Frame rate updated to: " + String(frameRate) + " FPS");

        // Send response back to client
        saveSettings();  // Save to preferences or storage
        request->send(200, "text/plain", "Frame rate set to: " + String(frameRate) + " FPS");
    } else {
        request->send(400, "text/plain", "Frame rate not provided.");
    }
}






// Message Sending Handler
void handleSetMessageSending(AsyncWebServerRequest *request) {
    if (!request->hasParam("messageSending", true)) {
        request->send(400, "text/plain", "Missing 'messageSending' argument");
        return;
    }

    theatreChatEnabled = (request->getParam("messageSending", true)->value() == "1");
    logSerial("theatreChatEnabled Receive set to: " + String(theatreChatEnabled ? "ON" : "OFF"));

    saveSettings();
    request->send(200, "text/plain", "OK");
}


// Camera Quality Handler
void handleSetCameraQuality(AsyncWebServerRequest *request) {
    if (!request->hasParam("quality", true)) {
        request->send(400, "text/plain", "Missing 'quality' argument");
        return;
    }

    cameraQuality = request->getParam("quality", true)->value().toInt();
    logSerial("Camera quality set to: " + String(cameraQuality));

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_quality(s, cameraQuality);
            xSemaphoreGive(cameraMutex);
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera not initialized");
        }
    } else {
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}

// Camera Contrast Handler
void handleSetCameraContrast(AsyncWebServerRequest *request) {
    if (!request->hasParam("contrast", true)) {
        request->send(400, "text/plain", "Missing 'contrast' argument");
        return;
    }

    cameraContrast = request->getParam("contrast", true)->value().toInt();
    logSerial("Camera contrast set to: " + String(cameraContrast));

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_contrast(s, cameraContrast);
            xSemaphoreGive(cameraMutex);
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera not initialized");
        }
    } else {
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}

// Camera Brightness Handler
void handleSetCameraBrightness(AsyncWebServerRequest *request) {
    if (!request->hasParam("brightness", true)) {
        request->send(400, "text/plain", "Missing 'brightness' argument");
        return;
    }

    cameraBrightness = request->getParam("brightness", true)->value().toInt();
    logSerial("Camera brightness set to: " + String(cameraBrightness));

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_brightness(s, cameraBrightness);
            xSemaphoreGive(cameraMutex);
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera not initialized");
        }
    } else {
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}

// Debug Mode Handler
void handleSetDebugMode(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(405, "text/plain", "Method Not Allowed");
        return;
    }

    if (!request->hasParam("mode", true)) {
        request->send(400, "text/plain", "Bad Request");
        return;
    }

    String mode = request->getParam("mode", true)->value();
    if (mode == "1") {
        debugMode = true;
        logSerial("Debug mode enabled via web interface.");
    } else if (mode == "0") {
        debugMode = false;
        logSerial("Debug mode disabled via web interface.");
    } else {
        request->send(400, "text/plain", "Invalid Mode");
        return;
    }

    saveSettings();
    request->send(200, "text/plain", "OK");
}

// Root Handler
void handleRoot(AsyncWebServerRequest *request) {
    // Start with the index_html template
    String html = index_html;

    // Replace placeholders with actual values
    html.replace("%DEBUG_MODE%", (debugMode) ? "Enabled" : "Disabled");
    html.replace("%BUTTON_CLASS%", (debugMode) ? "disabled" : "enabled");
    html.replace("%BUTTON_TEXT%", (debugMode) ? "Disable" : "Enable");
    html.replace("%FLIP_CHECKED%", (cameraFlip) ? "checked" : "");

    // Set the correct selected option in the resolution dropdown
    html.replace("%SELECTED_QVGA%", (cameraResolution == "QVGA") ? "selected" : "");
    html.replace("%SELECTED_VGA%", (cameraResolution == "VGA") ? "selected" : "");
    html.replace("%SELECTED_SVGA%", (cameraResolution == "SVGA") ? "selected" : "");

    // Send the HTML response
    request->send(200, "text/html", html);
}

// Camera Flip Handler
void handleSetCameraFlip(AsyncWebServerRequest *request) {
    if (!request->hasParam("flip", true)) {
        request->send(400, "text/plain", "Missing 'flip' argument");
        return;
    }

    cameraFlip = (request->getParam("flip", true)->value() == "1");
    logSerial("Camera flip set to: " + String(cameraFlip ? "Enabled" : "Disabled"));

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_vflip(s, cameraFlip);
            s->set_hmirror(s, cameraFlip);
            xSemaphoreGive(cameraMutex);
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera not initialized");
        }
    } else {
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}

// Camera Resolution Handler (Requires Re-initialization)
void handleSetCameraResolution(AsyncWebServerRequest *request) {
    if (!request->hasParam("resolution", true)) {
        request->send(400, "text/plain", "Missing 'resolution' argument");
        return;
    }

    String resolution = request->getParam("resolution", true)->value();
    framesize_t newFrameSize;

    if (resolution == "QVGA") {
        newFrameSize = FRAMESIZE_QVGA;
    } else if (resolution == "VGA") {
        newFrameSize = FRAMESIZE_VGA;
    } else if (resolution == "SVGA") {
        newFrameSize = FRAMESIZE_SVGA;
    } else {
        request->send(400, "text/plain", "Invalid Resolution");
        return;
    }

    if (xSemaphoreTake(cameraMutex, portMAX_DELAY) == pdTRUE) {
        // Deinitialize the camera
        esp_camera_deinit();

        // Update camera config
        camera_config.frame_size = newFrameSize;

        // Re-initialize the camera
        esp_err_t err = esp_camera_init(&camera_config);
        if (err != ESP_OK) {
            xSemaphoreGive(cameraMutex);
            logSerial("Camera re-init failed with error 0x" + String(err, HEX));
            request->send(500, "text/plain", "Camera re-initialization failed");
            return;
        }

        // Re-apply sensor settings
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL) {
            s->set_vflip(s, cameraFlip);
            s->set_hmirror(s, cameraFlip);
            s->set_brightness(s, cameraBrightness);
            s->set_contrast(s, cameraContrast);
            s->set_quality(s, cameraQuality);
            // Add other sensor settings if needed
        }

        xSemaphoreGive(cameraMutex);
        cameraResolution = resolution;
        saveSettings();
        request->send(200, "text/plain", "Camera resolution updated");
    } else {
        request->send(500, "text/plain", "Failed to acquire camera mutex");
    }
}

// Memory and CPU Usage Handler

// This function calculates the CPU usage
void calculateCPUUsage() {
    unsigned long currentMillis = millis();
    unsigned long elapsedMillis = currentMillis - previousMillis;

    if (elapsedMillis >= 1000) {  // Calculate every second
        unsigned long currentIdleCounter = idleCounter;
        unsigned long idleTime = currentIdleCounter - previousIdleCounter;

        cpuUsage = 100 - ((idleTime * 100) / elapsedMillis);

        previousMillis = currentMillis;
        previousIdleCounter = currentIdleCounter;
    }
}

// Return the calculated CPU usage
int getCPUUsage() {
    calculateCPUUsage();
    return cpuUsage;
}

void handleGetUsage(AsyncWebServerRequest *request) {
    // Get memory usage (free heap)
    int freeHeap = ESP.getFreeHeap();
    int totalHeap = ESP.getHeapSize();  // Total heap size (if supported)
    
    // Calculate memory usage percentage
    int memoryUsage = 100 - ((freeHeap * 100) / totalHeap);

    // Get CPU usage
    int cpuUsage = getCPUUsage();

    // Prepare JSON response
    String jsonResponse = "{\"memoryUsage\": " + String(memoryUsage) + ", \"cpuUsage\": " + String(cpuUsage) + "}";

    // Send the response
    request->send(200, "application/json", jsonResponse);
}


void handleSendTestOscMessage(AsyncWebServerRequest *request) {
    if (!request->hasParam("message", true)) {
        request->send(400, "text/plain", "Missing 'message' parameter");
        return;
    }

    String messageToSend = request->getParam("message", true)->value();
    
    // Send the OSC message using the custom function
    sendTheatreChatOscMessage(messageToSend);

    // Log and respond to the request
    Serial.println("Test OSC Message Sent: " + messageToSend);
    request->send(200, "text/plain", "OSC Message Sent");
}


// Handler to Serve Stored Settings as JSON
void handleGetSettings(AsyncWebServerRequest *request) {
    loadSettings();

    // Create a JSON document
    StaticJsonDocument<512> doc;

    // Add key-value pairs
    doc["debugMode"] = debugMode;
    doc["cameraFlip"] = cameraFlip;
    doc["cameraMirror"] = cameraMirror;
    doc["brightness"] = cameraBrightness;
    doc["contrast"] = cameraContrast;
    doc["quality"] = cameraQuality;
    doc["irEnabled"] = irEnabled;
    doc["irBrightness"] = irBrightness;
    doc["oscReceive"] = oscReceiveEnabled;
    doc["oscPort"] = oscPort;
    doc["theatreChatEnabled"] = theatreChatEnabled;
    doc["theatreChatPort"] = theatreChatPort;
    doc["theatreChatChannel"] = theatreChatChannel;
    doc["theatreChatName"] = theatreChatName;
    doc["theatreChatMessage"] = theatreChatMessage;
    doc["deviceOrientation"] = deviceOrientation;
    doc["backstageMode"] = backstageMode;
    doc["SOUND_THRESHOLD"] = SOUND_THRESHOLD; // Add sound threshold
    doc["SOUND_DEBOUNCE_DELAY"] = SOUND_DEBOUNCE_DELAY; // Add sound debounce delay
    doc["PIR_DEBOUNCE_DELAY"] = PIR_DEBOUNCE_DELAY; // Add PIR debounce delay
    doc["frameRate"] = frameRate;
    doc["saturation"] = cameraSaturation;
    doc["specialEffect"] = cameraSpecialEffect;
    doc["whitebal"] = cameraWhiteBalance;
    doc["awbGain"] = cameraAwbGain;
    doc["wbMode"] = cameraWbMode;

    // Serialize the JSON document to a string and send it
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

#endif  // HTTP_H
