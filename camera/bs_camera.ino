#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"   // Include utilities.h for pin definitions
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "led.h"
#include "osc.h"         // Include the OSC header (contains logSerial and displayScreen)
#include "esp_camera.h"
#include <ESPAsyncWebServer.h>  // Include AsyncWebServer library
#include <AsyncTCP.h>           // Include AsyncTCP library
#include "http.h"
#include "driver/i2s.h"
#include "esp_vad.h"
#include <Adafruit_NeoPixel.h>
#include "httpupdate.h"
#include <vector>
#include <nvs_flash.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Global objects and variables
QueueHandle_t sensorEventQueue;
SemaphoreHandle_t cameraMutex; 
SemaphoreHandle_t soundSemaphore;
SemaphoreHandle_t pirSemaphore;
TaskHandle_t streamTaskHandle = NULL;


WiFiUDP udp;
WiFiClient streamingClient;
Preferences preferences;
XPowersPMU PMU;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

int frameRate = 30;  // Default frame rate of 30 FPS
int currentScreen = 0;
String screenState= "Boot";
bool isStreaming = false;

volatile unsigned long lastDebounceTime = 0;  // Timestamp of the last valid button press
const unsigned long debounceDelay = 100;  // 50 milliseconds debounce delay

// Incoming OSC
bool oscReceiveEnabled = false;
int oscPort = 8000;

String firmwareUrl = "";
String currentVersion = "1.1.1";

bool confirmDHCPRenewal = false;
bool confirmFactoryReset = false;

bool mdnsEnabled = true;

// TheatreChat OSC Configuration
bool theatreChatEnabled = true;
int theatreChatPort = 27900;
String theatreChatChannel = "cameras";
String theatreChatName = "TCameraS3";
String theatreChatMessage = "default message";

// Device Orientation
int deviceOrientation = 0;
bool backstageMode = false;
bool debugMode = true;    // Variable to track debug mode
bool chargingActive = false;          // Indicates we’re showing charging mode
bool chargingScreenDisplayed = false; // Helps prevent re-displaying “Charging” over and over

// Sound Detection Parameters
int SOUND_THRESHOLD = 6000; // Adjust based on testing
unsigned long SOUND_DEBOUNCE_DELAY = 500; // .5 seconds debounce
unsigned long PIR_DEBOUNCE_DELAY = 5000; // 5 seconds debounce
bool PIRDetectionToggle = false;
bool soundDetectionToggle = true;
bool soundDebounceDebug = false;
bool pirFlag = false;
volatile bool pirTriggered = false;
bool micFlag = false;
unsigned long micTimestamp = 0;
vad_handle_t vad_inst;
int16_t *vad_buff;

// IR Settings variables
bool irEnabled = false;
int irBrightness = 50;

TaskHandle_t otaTaskHandle = NULL;  // NULL if not running
bool otaActive = false;
bool otaModeState = false;

// LED state and colors
String micOnColor = "#00FF00";   // Default green
String micOffColor = "#FF0000";  // Default red
String micReadyColor = "#0000FF"; // Default blue
bool ledState = false;  // Default to LEDs off
int ledBrightness = 255;
String ledColor = "#FFFFFF";

// Define debounce delay

String currentPirStateString = ""; // Initialize as an empty string
String serialBuffer = ""; // Buffer to store the serial output

// Camera Related variables
camera_config_t camera_config;
bool cameraMirror = false;
bool cameraFlip = true;  // Default: camera is flipped
int cameraBrightness = 0;
int cameraContrast = 0;
int cameraQuality = 10;
String cameraResolution = "VGA";  // Default resolution
int cameraSaturation = 0; // Range: -2 to 2
int cameraSpecialEffect = 0; // Range: 0 to 6
bool cameraWhiteBalance = true; // 0 (false) or 1 (true)
bool cameraAwbGain = true; // 0 (false) or 1 (true)
int cameraWbMode = 0; // Range: 0 to 4

int batteryPercent = -1;  // Global variable to store battery percentage
bool batteryConnected = false;  // Global variable to store battery connection status
bool chargingDismissed = false; 

unsigned long lastExecutionTime = 0; // Keeps track of the last execution time
const unsigned long interval = 33;   // 33 milliseconds for 30 executions per second

String getDefaultHostname() {
    String mac = WiFi.macAddress();  // Get full MAC address (format: "XX:XX:XX:XX:XX:XX")
    String lastFour = mac.substring(mac.length() - 5);  // Extract last 5 characters
    lastFour.replace(":", "");  // Remove colons
    return "BSCam" + lastFour;
}


int screenPress = 1;

#include "http.h"

AsyncWebServer server(80);     // Async web server on port 80
WiFiServer streamServer(81);  // Different port for the stream


// Camera and Streaming constants
#define PART_BOUNDARY "123456789000000000000987654321"
const char* streamBoundary = "\r\n--" PART_BOUNDARY "\r\n";
const char* streamContentType = "Content-Type: image/jpeg\r\nContent-Length: ";
const char* streamEnd = "\r\n";

volatile bool pmu_flag = false;  // Declare the flag variable as volatile since it's used in an interrupt
volatile bool userButtonFlag = false; 

void setFlag() {
    pmu_flag = true;  // Set the flag to true when the interrupt is triggered
}

/*
void userButtonISR() {
    unsigned long currentTime = millis();
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        userButtonFlag = true;  // Set the flag when the button is pressed
        lastDebounceTime = currentTime;  // Update the debounce time
    }
}
*/


unsigned long loopMillis = 0;  // Initialize it globally

void handlePMUButtonPress() {
    if (chargingActive) {
        chargingActive    = false;
        chargingDismissed = true;  // So we won't re-enter until next USB removal
        chargingClear();
        logSerial("User dismissed charging screen.");
        // return; // Optionally bail out of the rest
    }
    screenPress++;  // Cycle through menu items
    if (screenPress > 20) {  // Assuming you have 6 screens
        screenPress = 1;  // Loop back to the first menu item
    }
    handleScreenSwitch();  // Update the display based on the new screen
}

// Button press handler to cycle screens
void handleButtonPress() {
    if (chargingActive) {
        chargingActive    = false;
        chargingDismissed = true;  // So we won't re-enter until next USB removal
        chargingClear();
        logSerial("User dismissed charging screen.");
        // return; // Optionally bail out of the rest
    }
    screenPress++;
    if (screenPress > 20) {
        screenPress = 1;
    }
    handleUserButtonPress();
}

void handleScreenSwitch() {
    switch (screenPress) {
        case 1:
            //displayScreen("Main Screen", true);  // Save the screen buffer for screen 1
            displayScreen("1)" + theatreChatName);
            break;
        case 2:
            displayScreen("2) IP: " + WiFi.localIP().toString());
            break;
        case 3:
            if (batteryPercent == -1){
              displayScreen("3) Battery Not Connected");
            } else {
              displayScreen("3) Battery: " + String(batteryPercent) + "%");
            }
            
            break;
        case 4:
            displayScreen("4) Amplitude:");
            soundDebounceDebug = true;
            break;
        case 5:
            soundDebounceDebug = false;
            //turn IR on or off using the other button            
            //handleUserButtonPress();
            if(irEnabled) {
              displayScreen("5) IR On");
            } else {
              displayScreen("5) IR Off");
            }
            break;    
        case 6:
            //handleUserButtonPress();
            if(theatreChatEnabled) {
              displayScreen("6) OSC Send On");
            } else {
              displayScreen("6) OSC Send Off");
            }
            break;            
        case 7:
            //handleUserButtonPress();
            if (oscReceiveEnabled) {
                displayScreen("7) OSC Receive On");  // Update screen to show "IR On"
            } else {
                displayScreen("7) OSC Receive Off");  // Update screen to show "IR Off"
            }

            break;  
        case 8:
            //handleUserButtonPress();
            if (ledState) {
                displayScreen("8) LEDs On");  // Update screen to show "IR On"
            } else {
                displayScreen("8) LEDs Off");  // Update screen to show "IR Off"
            }
            break;  
        case 9:
            displayScreen("9) Check for update?");
            break;  
        case 10:
            displayScreen("10) Current Firmware: " + currentVersion);
            break;
        case 11:
            if (soundDetectionToggle) {
                displayScreen("11) Sound Detection On");
            } else {
                displayScreen("11) Sound Detection Off");
            }
            break;
        case 12:
            if (PIRDetectionToggle) {
                displayScreen("12) Motion Detection On");
            } else {
                displayScreen("12) Motion Detection Off");
            }
            break;
        case 13:
            if (WiFi.status() == WL_CONNECTED) {
                displayScreen("13) SSID: " + WiFi.SSID());
            } else {
                displayScreen("13) Not connected to WiFi");
            }
            break;   
        case 14:
            if (WiFi.status() == WL_CONNECTED) {
                displayScreen("14) Renew DHCP?");
                confirmDHCPRenewal = true; // Set confirmation state
            } else {
                displayScreen("14) Not connected to WiFi");
                confirmDHCPRenewal = false; // No action needed if not connected
            }
            break;                                               
        case 15:
            displayScreen("15) Reset to factory?");
            confirmFactoryReset = true; // Set confirmation state
            break;  
        case 16:
            if (otaModeState) {
                displayScreen("16) OTA On");
            } else {
                displayScreen("16) OTA Off");
            }
            break;  
        case 17:

            break;     
        case 18:

            break;  
        case 19:

            break;  
        case 20:
            u8g2.clearBuffer();
            u8g2.sendBuffer();
            screenState = "Manually cleared";
            break;                                               
        default:
            screenPress = 1;  // Loop back to screen 1
            handleScreenSwitch();  // Recall screen 1
            break;
    }
}

void handleUserButtonPress() {
    switch (screenPress) {
        case 1:
            // Displays device name
            break;
        case 2:
            // Displays device IP
            break;
        case 3:
            // Displays device battery level
            break;
        case 4:
            // Displays device amplitude max
            break;
        case 5:
            // Toggle IR on/off
            toggleIr();
            break;
        case 6:
            // Toggles the device SENDING OSC messages
            toggleOSCSend();
            break;            
        case 7:
            // Toggles the device RECEIVING OSC messages
            toggleOSCReceive();
            break;  
        case 8:
            // Toggles the LEDs On/Off
            toggleLEDs();
            break;  
        case 9:
            logSerial("Checking for update");
            firmwareUrl = checkForUpdate();
            break;  
        case 10:
            // Displays current firmware version
            break;
        case 11:
            togglesoundDetectionToggle();
            break;
        case 12:
            togglePIRDetectionToggle();
            break;
        case 13:

            break;   
        case 14:
            if (confirmDHCPRenewal) {
                logSerial("Renewing DHCP...");
                WiFi.disconnect();
                delay(100); // Small delay to ensure disconnect
                WiFi.reconnect();
                logSerial("DHCP Renewed");
                displayScreen("DHCP Renewed");
                confirmDHCPRenewal = false; // Reset the state
            } else {
                logSerial("No action, waiting for confirmation.");
            }
            break;                                               
        case 15:
            if (confirmFactoryReset) {
                resetToFactoryDefaults(); // Call the function to reset settings
                confirmFactoryReset = false; // Reset the confirmation state
            } else {
                logSerial("No action, waiting for confirmation.");
            }
            break;  
        case 16:
            handleOTAModeToggle();
            break;  
        case 17:

            break;     
        case 18:

            break;  
        case 19:

            break;  
        case 20:

            break;  
        default:
            break;
    }
}





void startOtaUpdateMode(const char* hostname = nullptr, const char* otaPassword = "hector1995") {
    // 1. Configure ArduinoOTA
    String defaultHostname = getDefaultHostname();  // Get "BSCam[last 4]"
    if (!hostname) {
        hostname = defaultHostname.c_str();  // Use default if no hostname provided
    }

    ArduinoOTA.setHostname(hostname);
    if (otaPassword != nullptr) {
      ArduinoOTA.setPassword(otaPassword);
    }

    // Optional callbacks
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r\n", (progress * 100U) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    });

    // 2. Begin ArduinoOTA
    ArduinoOTA.begin();
    otaActive = true;
    Serial.println("OTA Mode Enabled. Ready for remote upload.");

    // 3. Create a FreeRTOS task that calls ArduinoOTA.handle() repeatedly
    if (otaTaskHandle == NULL) {
      xTaskCreatePinnedToCore(
        otaTask,            // Task function
        "OtaTask",          // Name
        4096,               // Stack size
        NULL,               // Parameter
        1,                  // Priority
        &otaTaskHandle,     // Task handle
        1                   // Core (optional; set to 0 or 1 on dual-core)
      );
    }
}

void otaTask(void* parameter) {
    while (otaActive) {
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay to avoid hogging CPU
    }
    Serial.println("Exiting OTA Task...");
    vTaskDelete(NULL);  // Delete this task once otaActive is false
}

void stopOtaUpdateMode() {
    if (!otaActive) {
      return; // Already stopped
    }
    // 1. Indicate we’re no longer active
    otaActive = false;

    // 2. End ArduinoOTA so it won’t accept new connections
    ArduinoOTA.end();

    // 3. Let the task exit gracefully
    //    The task will see otaActive==false, break out of its loop, and call vTaskDelete(NULL).
    //    If you want to forcibly delete, do:
    //      if (otaTaskHandle) vTaskDelete(otaTaskHandle);
    //      otaTaskHandle = NULL;

    Serial.println("OTA Mode Disabled.");
}

void handleOTAModeToggle() {
    otaModeState = !otaModeState; 

    if (otaModeState) {
        logSerial("OTA On");
        displayScreen("OTA On"); 
        startOtaUpdateMode();
    } else {
        logSerial("OTA Off");
        displayScreen("OTA Off");
        stopOtaUpdateMode();
    }

    saveSettings();  // Save the state (if needed)
}


void resetToFactoryDefaults() {
    logSerial("Resetting to factory defaults...");

    // Clear NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK || err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); // Erase entire NVS flash partition
        nvs_flash_init();  // Reinitialize NVS
        logSerial("NVS partition erased");
    } else {
        logSerial("Failed to initialize NVS");
    }

    // Optionally erase Wi-Fi credentials
    WiFi.disconnect(true);
    logSerial("Wi-Fi credentials cleared");

    // Reset other states or variables here

    logSerial("Factory reset complete. Restarting...");
    displayScreen("Factory reset done");
    delay(2000);
    ESP.restart();
}

void toggleIr() {
    irEnabled = !irEnabled;  // Toggle the current IR state

    if (irEnabled) {
        digitalWrite(IR_LED_PIN, HIGH);  // Turn IR on
        logSerial("IR Enabled");
        displayScreen("IR On");  // Update screen to show "IR On"
    } else {
        digitalWrite(IR_LED_PIN, LOW);  // Turn IR off
        logSerial("IR Disabled");
        displayScreen("IR Off");  // Update screen to show "IR Off"
    }

    saveSettings();  // Save the state (if needed)
}

void chargingClear(){
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    if (ledState) {
      currentMode = LED_OFF;
    }
}

void togglesoundDetectionToggle() {
    soundDetectionToggle = !soundDetectionToggle; 

    if (soundDetectionToggle) {
        logSerial("Sound Detection On");
        displayScreen("Sound Detection On"); 
    } else {
        logSerial("Sound Detection Off");
        displayScreen("Sound Detection Off");
    }

    saveSettings();  // Save the state (if needed)
}

void togglePIRDetectionToggle() {
    PIRDetectionToggle = !PIRDetectionToggle;  // Toggle the current IR state

    if (PIRDetectionToggle) {
        logSerial("Motion Detection On");
        displayScreen("Motion Detection On");  // Update screen to show "IR On"
    } else {
        logSerial("Motion Detection Off");
        displayScreen("Motion Detection Off");  // Update screen to show "IR Off"
    }

    saveSettings();  // Save the state (if needed)
}

void toggleOSCSend() {
    theatreChatEnabled = !theatreChatEnabled;  // Toggle the current IR state


    if (theatreChatEnabled) {
        logSerial("OSC Send On");
        displayScreen("OSC Send On");  // Update screen to show "IR On"
    } else {
        logSerial("OSC Send Off");
        displayScreen("OSC Send Off");  // Update screen to show "IR Off"
    }

    saveSettings();  // Save the state (if needed)
}

void toggleOSCReceive() {
    oscReceiveEnabled = !oscReceiveEnabled;  // Toggle the current IR state


    if (oscReceiveEnabled) {
        logSerial("OSC Receive On");
        displayScreen("OSC Receive On");  // Update screen to show "IR On"
    } else {
        logSerial("OSC Receive Off");
        displayScreen("OSC Receive Off");  // Update screen to show "IR Off"
    }

    saveSettings();  // Save the state (if needed)
}

void toggleLEDs() {
    ledState = !ledState;  // Toggle the current IR state
    //ledBrightness

    if (ledState) {
        displayScreen("LEDs On");  // Update screen to show "IR On"
        handleLedOn("clear");
    } else {
        displayScreen("LEDs Off");  // Update screen to show "IR Off"
        currentMode = LED_OFF;
    }

    saveSettings();  // Save the state (if needed)
}
/*
// streamTask does this now
// Function to handle the camera stream
void handleStream() {
    WiFiClient client = streamServer.available();  // Listen for incoming clients
    if (client) {
        // Start sending the headers for the multipart JPEG stream
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
        client.println();

        // Stream JPEG images from the camera
        while (client.connected()) {
            camera_fb_t * fb = esp_camera_fb_get();
            if (!fb) {
                client.println("Content-Length: 0\r\n\r\n");
                continue;
            }

            // Send JPEG frame as part of the multipart stream
            client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
            client.write(fb->buf, fb->len);
            client.println("\r\n");

            esp_camera_fb_return(fb);
            delay(30);  // Adjust frame rate by adding a delay
        }

        client.stop();  // Close connection when client disconnects
    }
}
*/


void streamTask(void *pvParameters) {
    WiFiClient client;

    while (1) {
        client = streamServer.available();  // Listen for incoming clients
        if (client) {
            logSerial("Client connected, starting stream...");
            // Send the multipart HTTP header with the correct boundary
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
            client.println("Connection: keep-alive");  // Prevents premature client disconnection
            client.println();

            while (client.connected()) {
                camera_fb_t * fb = esp_camera_fb_get();
                if (!fb) {
                    logSerial("Camera capture failed");
                    client.stop();
                    break;
                }

                // Send the image frame as part of the multipart stream
                client.printf("--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", "frame", fb->len);
                client.write(fb->buf, fb->len);
                client.println();  // Send an empty line after the frame

                esp_camera_fb_return(fb);

                // Calculate delay based on the frame rate (in milliseconds per frame)
                int frameDelayMs = 1000 / frameRate;

                // Optionally add a delay to control frame rate without blocking main loop
                vTaskDelay(pdMS_TO_TICKS(frameDelayMs));  // Run at desired FPS
            }

            logSerial("Client disconnected, ending stream.");
            client.stop();
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to prevent CPU hogging
    }
}



// Function to log messages
void logSerial(String message) {
    if (debugMode) {
        Serial.println(message);  // Print to the serial monitor if in debug mode
    }
    serialBuffer += message + "\n";  // Add message to the serial buffer

    // Limit the size of the serial buffer to prevent overflow
    if (serialBuffer.length() > 2048) {  // Keep only the last 2048 characters
        serialBuffer.remove(0, serialBuffer.length() - 2048);
    }

    Serial.println(message);  // Print to the serial monitor if in debug mode
}

void initI2SMicrophone() {
    i2s_config_t i2s_config_mic = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
    };

    i2s_pin_config_t pin_config_mic = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = IIS_SCLK_PIN,
        .ws_io_num  = IIS_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = IIS_DIN_PIN
    };

    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config_mic, 0, NULL);
    if (err != ESP_OK) {
        logSerial("Failed to install I2S driver: " + String(err));
        return;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config_mic);
    if (err != ESP_OK) {
        logSerial("Failed to set I2S pin configuration: " + String(err));
        return;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    logSerial("I2S Microphone initialized successfully.");
}

// Function to calculate the broadcast IP address
IPAddress getBroadcastAddress() {
    IPAddress ip = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();
    IPAddress broadcast;
    for (int i = 0; i < 4; i++) {
        broadcast[i] = ip[i] | ~subnet[i];
    }
    return broadcast;
}

bool detectSound() {
    static int16_t dma_buffer[256];
    size_t bytes_read;
    int maxAmplitude = 0;
    
    // Read multiple samples at once
    esp_err_t result = i2s_read(I2S_NUM_0, dma_buffer, sizeof(dma_buffer), &bytes_read, 0);
    
    if (result != ESP_OK || bytes_read == 0) {
        return false;
    }
    
    int samples_read = bytes_read / sizeof(int16_t);
    for (int i = 0; i < samples_read; i++) {
        int amplitude = abs(dma_buffer[i]);
        if (amplitude > maxAmplitude) {
            maxAmplitude = amplitude;
        }
    }
    //logSerial("Max amplitude detected: " + String(maxAmplitude));
    if (soundDebounceDebug && (maxAmplitude > SOUND_THRESHOLD)) {
        logSerial("Max amplitude detected: " + String(maxAmplitude));
        displayScreen("Amplitude: " + String(maxAmplitude));
    }
    return maxAmplitude > SOUND_THRESHOLD;
}

void pirTask(void *pvParameters) {
    while (1) {
        //logSerial("PIR Task running...");
        if (xSemaphoreTake(pirSemaphore, portMAX_DELAY) == pdTRUE) {
            logSerial("PIR event detected.");
            // Handle PIR event
            if (PIRDetectionToggle){
              sendTheatreChatOscMessage("PIR triggered.");
            }

        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Add delay to avoid CPU hogging
    }
}


void sendTheatreChatOscMessage(String messageToSend) {
    // Check if theatreChat is enabled
    if (!theatreChatEnabled) {
        logSerial("TheatreChat is disabled. Message not sent.");
        return; // Exit the function if theatreChat is not enabled
    }

    // Define OSC Address Pattern
    String oscAddress = "/theatrechat/message/" + String(theatreChatChannel);
    logSerial("Message to send: " + messageToSend);

    // Get Broadcast IP Address
    IPAddress broadcastIP = getBroadcastAddress();

    // Send OSC message using ArduinoOSC
    OscWiFi.send(broadcastIP.toString().c_str(), theatreChatPort, oscAddress.c_str(), theatreChatName, messageToSend.c_str());

    logSerial("OSC message '" + oscAddress + "' sent to " + broadcastIP.toString() + ":" + String(theatreChatPort));
}

// Callback function to process received OSC messages
void receiveTheatreChatOscMessage(OscMessage &msg) {
    // Ensure the OSC address starts with /theatrechat/message/ and matches the theatreChatChannel
    String address = msg.address();
    if (!address.startsWith("/theatrechat/message/") || !address.endsWith(theatreChatChannel)) {
        logSerial("Invalid OSC address or channel. Message ignored.");
        return; // Exit if the OSC message address doesn't match the expected format
    }

    // Check that there are at least two arguments: sending name and message to analyze
    if (msg.size() < 2) {
        logSerial("Invalid OSC message format. Not enough arguments.");
        return; // Exit if the message does not have enough arguments
    }

    // Extract the sending name (first argument)
    String sendingName = msg.arg<String>(0);

    // Extract the message to analyze (second argument)
    String messageToAnalyze = msg.arg<String>(1);

    // Log the received message
    logSerial("Received message from: " + sendingName + " - " + messageToAnalyze);

    // Split the messageToAnalyze into target and command
    int spaceIndex = messageToAnalyze.indexOf(' ');
    if (spaceIndex == -1) {
        logSerial("Invalid message format. No command found.");
        return; // Exit if the message is not in the correct format [target] [command]
    }

    String target = messageToAnalyze.substring(0, spaceIndex);
    String command = messageToAnalyze.substring(spaceIndex + 1);

    // Convert target and theatreChatName to lowercase for case-insensitive comparison
    String targetLower = target;
    targetLower.toLowerCase();

    String nameLower = theatreChatName;
    nameLower.toLowerCase();

    // Check if the target matches theatreChatName or "all" (case-insensitive)
    if (targetLower == nameLower || targetLower == "all") {
        logSerial("Target matches theatreChatName. Processing command: " + command); 
        // Placeholder for command processing
        processCommand(command);
        sendTheatreChatOscMessage("reply" + messageToAnalyze);
    } else {
        logSerial("Target does not match theatreChatName. Ignoring command.");
    }
}


// Placeholder function for command processing
void processCommand(String command) {
    if (command == "micOn") {
        handleMicOn();
    } else if (command == "micOff") {
        handleMicOff();
    } else if (command == "micReady") {
        handleMicReady();
    } else if (command == "clear") {
        handleClear();
    } else if (command == "ip") {
        handleDisplayIP();
    } else if (command == "name") {
        handleDisplayName();     
    } else if (command == "identify") {
        handleIdentify();                      
    } else if (command.startsWith("ledOn ")) {
        String ledColor = command.substring(6);  // Extract everything after "ledOn "
        handleLedOn(ledColor);
    } else if (command.startsWith("ledOff ")) {
        handleLedOff();
    } else if (command.startsWith("display ")) {
        // Manually handle the display command without using add
        String displayText = command.substring(8);  // Extract the display argument
        logSerial("Simulating display command: " + displayText);
        displayScreen(displayText);  // Directly pass the display text to your display function
    } else {
        logSerial("Unknown command: " + command);
    }
}


void handleGetUptime(AsyncWebServerRequest *request) {
    //logSerial("handleGetUptime function called");
    unsigned long millisTime = millis();

    // Calculate hours, minutes, and seconds
    unsigned long seconds = (millisTime / 1000) % 60;
    unsigned long minutes = (millisTime / (1000 * 60)) % 60;
    unsigned long hours = (millisTime / (1000 * 60 * 60)) % 24;
    
    // Format uptime as a string
    String uptime = String(hours) + " hrs " + String(minutes) + " min " + String(seconds) + " sec";
    
    // Send uptime as the response
    request->send(200, "text/plain", uptime);
}

void handleGetBatteryPercentage(AsyncWebServerRequest *request) {
    if (batteryConnected && batteryPercent >= 0 && batteryPercent <= 100) {
        request->send(200, "text/plain", String(batteryPercent));
    } else {
        request->send(200, "text/plain", "N/A");
    }
}

void cameraTask(void *pvParameters) {
    while (1) {
        // Handle camera streaming
        // Use task notifications or queues if needed
        vTaskDelay(pdMS_TO_TICKS(30)); // Adjust delay as necessary
    }
}

void applyCameraSettings() {
    sensor_t * s = esp_camera_sensor_get();

    s->set_brightness(s, cameraBrightness);
    s->set_contrast(s, cameraContrast);
    s->set_saturation(s, cameraSaturation);
    s->set_special_effect(s, cameraSpecialEffect);
    s->set_whitebal(s, cameraWhiteBalance);
    s->set_awb_gain(s, cameraAwbGain);
    s->set_wb_mode(s, cameraWbMode);
    // Apply other settings as needed
}

void batteryTask(void *pvParameters) {
    while (1) {
        // Your existing code...
        if (PMU.isBatteryConnect()) {
            batteryConnected = true;
            batteryPercent = PMU.getBatteryPercent();
        } else {
            batteryConnected = false;
            batteryPercent = -1;
        }
        // Sleep for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Monitor stack usage
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        //logSerial("Battery Task - Stack High Water Mark: " + String(stackHighWaterMark));
    }
}

void IRAM_ATTR pirISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(pirSemaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void sensorTask(void *pvParameters) {
    uint8_t event;  // Declare the event variable
    while (1) {
        logSerial("Sensor Task running...");
        if (xQueueReceive(sensorEventQueue, &event, portMAX_DELAY)) {
            if (event == 1) {
                logSerial("PIR triggered in Sensor Task.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Add delay to avoid CPU hogging
    }
}


void soundDetectionTask(void *pvParameters) {
    while (1) {
        //logSerial("Sound Detection Task running...");
        if (detectSound()) {
            if (xSemaphoreTake(soundSemaphore, 0) == pdTRUE) {
                logSerial("Sound event detected.");
                // Handle microphone event
                micFlag = true;
                micTimestamp = millis();
                if (soundDetectionToggle){
                  sendTheatreChatOscMessage("Microphone triggered.");
                }

            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Add delay to avoid CPU hogging
    }
}

// Implement other handlers similarly





void setup() {
    Serial.begin(115200);
    delay(3000);
    logSerial("Starting...");

    // Initialize Wire once
    Wire.begin(I2C_SDA, I2C_SCL);
    //Wire.setClock(100000); // Reduce I²C speed to 100kHz
    Wire.setClock(400000);

    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }


    Serial.printf("getID:0x%x\n", PMU.getChipID());

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);

    //Set the working voltage of the modem, please do not modify the parameters
    // PMU.setDC1Voltage(3300);         //ESP32s3 Core VDD
    // PMU.enableDC1();                 //It is enabled by default, please do not operate

    // Board 5 Pin socket 3.3V power output control
    PMU.setDC3Voltage(3100);         //Extern 3100~ 3400V
    PMU.enableDC3();

    // Camera working voltage, please do not change
    PMU.setALDO1Voltage(1800);      // CAM DVDD
    PMU.enableALDO1();

    // Camera working voltage, please do not change
    PMU.setALDO2Voltage(2800);      // CAM DVDD
    PMU.enableALDO2();

    // Camera working voltage, please do not change
    PMU.setALDO4Voltage(3000);      // CAM AVDD
    PMU.enableALDO4();

    // Pyroelectric sensor working voltage, please do not change
    PMU.setALDO3Voltage(3300);        // PIR VDD
    PMU.enableALDO3();

    // Microphone working voltage, please do not change
    PMU.setBLDO1Voltage(3300);       // MIC VDD
    PMU.enableBLDO1();

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();

    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    
    pinMode(PMU_INPUT_PIN, INPUT);
    attachInterrupt(PMU_INPUT_PIN, setFlag, FALLING);

    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       //CHARGE
    );





    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

    // Set the time of pressing the button to turn off
    PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU.getPowerKeyPressOffTime();
    Serial.print("PowerKeyPressOffTime:");
    switch (opt) {
    case XPOWERS_POWEROFF_4S: Serial.println("4 Second");
        break;
    case XPOWERS_POWEROFF_6S: Serial.println("6 Second");
        break;
    case XPOWERS_POWEROFF_8S: Serial.println("8 Second");
        break;
    case XPOWERS_POWEROFF_10S: Serial.println("10 Second");
        break;
    default:
        break;
    }
    // Set the button power-on press time
    PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    opt = PMU.getPowerKeyPressOnTime();
    Serial.print("PowerKeyPressOnTime:");
    switch (opt) {
    case XPOWERS_POWERON_128MS: Serial.println("128 Ms");
        break;
    case XPOWERS_POWERON_512MS: Serial.println("512 Ms");
        break;
    case XPOWERS_POWERON_1S: Serial.println("1 Second");
        break;
    case XPOWERS_POWERON_2S: Serial.println("2 Second");
        break;
    default:
        break;
    }

    /*
    The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    //PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);

    pinMode(USER_BUTTON_PIN, INPUT_PULLUP);  // Set button pin as input with internal pull-up resistor
    attachInterrupt(USER_BUTTON_PIN, userButtonISR, FALLING);  // Attach interrupt on falling edge

    // Initialize the display
    u8g2.begin();
    displayScreen("Power On");

// Camera configuration
    // Initialize camera_config
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    //camera_config.frame_size = FRAMESIZE_SVGA;
    camera_config.frame_size = FRAMESIZE_VGA;    
    camera_config.pixel_format = PIXFORMAT_JPEG; // for streaming
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.jpeg_quality = 6;
    camera_config.fb_count = 1;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
    if (camera_config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
            logSerial("PSRAM detected and available.");
            camera_config.jpeg_quality = 6;
            camera_config.fb_count = 2;
            camera_config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
            // Limit the frame size when PSRAM is not available
            logSerial("PSRAM not detected, using DRAM.");
            camera_config.frame_size = FRAMESIZE_VGA;
            camera_config.fb_location = CAMERA_FB_IN_DRAM;
        }

    } else {
        // Best option for face detection/recognition
        camera_config.frame_size = FRAMESIZE_240X240;
    }


#if CONFIG_IDF_TARGET_ESP32S3
        camera_config.fb_count = 2;
#endif

    // Initialize Preferences before camera setup
    if (!preferences.begin("settings", false)) {  // "settings" is the namespace
        logSerial("Failed to initialize Preferences");
        displayScreen("Failed to load Saved Data");
    } else {
        //preferences.clear();
        // Load saved settings
        loadSettings();
        //debugMode = true;
    }  

    logSerial("Loaded settings:");
    logSerial("Debug Mode: " + String(debugMode ? "Enabled" : "Disabled"));
    logSerial("Camera Flip: " + String(cameraFlip ? "Yes" : "No"));
    logSerial("Camera Mirror: " + String(cameraMirror ? "Yes" : "No"));
    logSerial("Camera Resolution: " + cameraResolution);
    logSerial("Camera Brightness: " + String(cameraBrightness));
    logSerial("Camera Contrast: " + String(cameraContrast));
    logSerial("Camera Quality: " + String(cameraQuality));
    logSerial("IR Enabled: " + String(irEnabled ? "Yes" : "No"));
    logSerial("IR Brightness: " + String(irBrightness));
    logSerial("OSC Receive: " + String(oscReceiveEnabled ? "Enabled" : "Disabled"));
    logSerial("OSC Port: " + String(oscPort));
    logSerial("Theatre Chat: " + String(theatreChatEnabled ? "Enabled" : "Disabled"));
    logSerial("Theatre Chat Port: " + String(theatreChatPort));
    logSerial("Theatre Chat Channel: " + theatreChatChannel);
    logSerial("Theatre Chat Name: " + theatreChatName);
    logSerial("Theatre Chat Message: " + theatreChatMessage);
    logSerial("Device Orientation: " + String(deviceOrientation) + " degrees");
    logSerial("Backstage Mode: " + String(backstageMode ? "Enabled" : "Disabled"));
    displayScreen("Saved Data Loaded");

    displayScreen("Wifi Starting...");
    // Initialize WiFi
WiFi.mode(WIFI_STA);
WiFi.setSleep(false); // Disable Wi-Fi sleep

WiFiManager wm;
bool res = wm.autoConnect("AutoConnectAP", "hector1995");

if (!res) {
    logSerial("Failed to connect");
    displayScreen("WiFi Failed");
    WiFi.disconnect();
    
    // Enter AP mode with a custom SSID and password
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MST_BS_CAM", "hector1995"); // Replace with desired SSID and password
    
    delay(100);  // Small delay before checking the IP
    
    // Wait for the AP to assign an IP address
    int retries = 20; // Limit the number of retries (e.g., 20 * 100ms = 2 seconds)
    while (WiFi.softAPIP().toString() == "0.0.0.0" && retries > 0) {
        delay(100); // Wait 100ms between each retry
        retries--;
    }

    String ip = WiFi.softAPIP().toString(); // Get IP for AP mode
    logSerial("AP IP Address: " + ip);
    displayScreen("IP: " + ip);
} else {
    logSerial("Connected...yeey :)");
    String ip = WiFi.localIP().toString(); // Get IP for STA mode
    logSerial("IP Address: " + ip);
    displayScreen("IP: " + ip);

    if (!preferences.getBool("didRebootOnce", false)) {
      logSerial("First successful WiFi connection -> rebooting device once...");
      preferences.putBool("didRebootOnce", true);  // Remember we have already rebooted
      delay(1000);                                 // Short delay so messages get printed
      ESP.restart();                               // Reboot once
    }


    
}

if (res) {
    if (mdnsEnabled) {
          String mdnsName = "bscam-" + String(ESP.getEfuseMac(), HEX).substring(8); 
          // or use your getDefaultHostname() function
          if (!MDNS.begin(mdnsName.c_str())) {
              logSerial("Error starting mDNS");
          } else {
              logSerial("mDNS started with hostname: " + mdnsName);
          }

          // Example: Advertise an HTTP service on port 80
          MDNS.addService("http", "tcp", 80);

          // Optionally advertise a separate streaming service on port 81
          // so your server can discover it by `_mycamera._tcp.local.` or similar
          MDNS.addService("mycamera", "tcp", 81);

          // If you want to attach extra info:
          MDNS.addServiceTxt("mycamera", "tcp", "firmware", currentVersion);
          MDNS.addServiceTxt("mycamera", "tcp", "device", theatreChatName);
    }
}

    // Subscribing to OSC endpoints and routing them to functions
    OscWiFi.subscribe(oscPort, "/micOn", [](OscMessage &msg) { captureMessageAndProcess(msg, handleMicOn); });
    OscWiFi.subscribe(oscPort, "/micOff", [](OscMessage &msg) { captureMessageAndProcess(msg, handleMicOff); });
    OscWiFi.subscribe(oscPort, "/micReady", [](OscMessage &msg) { captureMessageAndProcess(msg, handleMicReady); });
    OscWiFi.subscribe(oscPort, "/display", [](OscMessage &msg) { captureMessageAndProcess(msg, handleDisplay); });
    OscWiFi.subscribe(oscPort, "/identify", [](OscMessage &msg) { captureMessageAndProcess(msg, handleIdentify); });
    OscWiFi.subscribe(oscPort, "/clear", [](OscMessage &msg) { captureMessageAndProcess(msg, handleClear); });
    OscWiFi.subscribe(oscPort, "/ledOn", [](OscMessage &msg) { captureMessageAndProcess(msg, handleLedOnFromOSC); });
    OscWiFi.subscribe(oscPort, "/ledOff", [](OscMessage &msg) { captureMessageAndProcess(msg, handleLedOff); });
    OscWiFi.subscribe(oscPort, "/theatrechat/message/*", receiveTheatreChatOscMessage);


    logSerial("Listening for OSC on port: " + String(oscPort));

    // Update camera_config based on loaded settings
    //camera_config.frame_size  = (cameraResolution == "QVGA") ? FRAMESIZE_QVGA :
    //                            (cameraResolution == "VGA") ? FRAMESIZE_VGA :
    //                            (cameraResolution == "SVGA") ? FRAMESIZE_SVGA : FRAMESIZE_QVGA;    

    // Initialize the camera with updated config
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        logSerial("Camera init failed with error 0x" + String(err, HEX));
        displayScreen("Camera Error");
    } else {
        logSerial("Camera initialized.");

        // Apply loaded settings
        sensor_t * s = esp_camera_sensor_get();
        if (s != NULL) {
            // Apply flip and mirror settings
            s->set_vflip(s, cameraFlip);
            s->set_hmirror(s, cameraMirror);

            logSerial("Applied camera settings:");
            //logSerial("Flip: " + String(cameraFlip ? "Yes" : "No"));
            //logSerial("Camera Mirror: " + String(cameraMirror ? "Yes" : "No"));
            //logSerial("Resolution: " + cameraResolution);
            //logSerial("Camera Quality: " + String(cameraQuality));
            //logSerial("Camera Brightness: " + String(cameraBrightness));
            //logSerial("Camera Contrast: " + String(cameraContrast));

        } else {
            logSerial("Failed to get camera sensor.");
        }
    }

    camera_fb_t *fb = esp_camera_fb_get();
if (!fb) {
    logSerial("Camera capture failed.");
} else {
    logSerial("Camera capture successful. Frame size: " + String(fb->len));
    esp_camera_fb_return(fb);
    //loadSettings();
    applyCameraSettings();
}



    delay(1000);

    //server.on("/", HTTP_GET, handleRoot);
    //streamServer.on("/stream", handleStream);
    // Initialize the web server using AsyncWebServer
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        handleRoot(request);
    });
    
    //server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
    //   handleStream();
    //});

    server.on("/setCameraMirror", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraMirror(request);
    });

    server.on("/setIr", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetIr(request);
    });

    server.on("/setOscReceive", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetOscReceive(request);
    });

    server.on("/setOscPort", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetOscPort(request);
    });

    server.on("/setDeviceOrientation", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetDeviceOrientation(request);
    });

    server.on("/setSoundThreshold", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetSoundThreshold(request);
    });

    server.on("/setTheatreChatConfig", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetTheatreChatConfig(request);
    });

    server.on("/setMessageSending", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetMessageSending(request);
    });

    server.on("/setCameraQuality", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraQuality(request);
    });

    server.on("/setCameraContrast", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraContrast(request);
    });

    server.on("/setCameraBrightness", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraBrightness(request);
    });

    server.on("/setCameraFlip", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraFlip(request);
    });

    server.on("/setCameraResolution", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraResolution(request);
    });

    server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request){
        handleGetSettings(request);
    });

    server.on("/getBatteryPercentage", HTTP_GET, [](AsyncWebServerRequest *request){
        handleGetBatteryPercentage(request);
    });

    server.on("/setDeviceName", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetDeviceName(request);
    });

    server.on("/getUptime", HTTP_GET, [](AsyncWebServerRequest *request){
        handleGetUptime(request);
    });
    server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello World");
    });
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request){
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            request->send(500, "text/plain", "Camera capture failed");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);
        response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
        request->send(response);
        esp_camera_fb_return(fb);
    });
    server.on("/getUsage", HTTP_GET, handleGetUsage);
    server.on("/sendTestOscMessage", HTTP_POST, handleSendTestOscMessage);
    server.on("/setDebugMode", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetDebugMode(request);
    });
    server.on("/restartDevice", HTTP_POST, handleRestartDevice);
    server.on("/setSoundThreshold", HTTP_POST, handleSetSoundThreshold);
    server.on("/setSoundDebounceDelay", HTTP_POST, handleSetSoundDebounceDelay);
    server.on("/setPirDebounceDelay", HTTP_POST, handleSetPirDebounceDelay);
    server.on("/setFrameRate", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetFrameRate(request);
    });
    server.on("/setCameraSaturation", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraSaturation(request);
    });

    server.on("/setCameraSpecialEffect", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraSpecialEffect(request);
    });

    server.on("/setCameraWhiteBalance", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraWhiteBalance(request);
    });

    server.on("/setCameraAwbGain", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraAwbGain(request);
    });

    server.on("/setCameraWbMode", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetCameraWbMode(request);
    });


    server.on("/setMicOnColor", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetMicOnColor(request);
    });

    server.on("/setMicOffColor", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetMicOffColor(request);
    });

    server.on("/setMicReadyColor", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetMicReadyColor(request);
    });

    server.on("/setLedState", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetLedState(request);
    });

    server.on("/setLedBrightness", HTTP_POST, [](AsyncWebServerRequest *request){
        handleSetLedBrightness(request);
    });
    server.on("/soundDetectionToggle", HTTP_POST, [](AsyncWebServerRequest *request){
        handlesoundDetectionToggle(request);
    });

    server.on("/PIRDetectionToggle", HTTP_POST, [](AsyncWebServerRequest *request){
        handlePIRDetectionToggle(request);
    });

    server.on("/getFirmwareVersion", HTTP_GET, [](AsyncWebServerRequest *request){
        handleGetFirmwareVersion(request);
    });

    server.on("/firmwareUpdate", HTTP_POST, [](AsyncWebServerRequest *request){
        handleFirmwareUpdate(request);
    });



    


    delay(1000); 

    // Start the server
    server.begin();
    logSerial("Web server started.");

    delay(1000);

    streamServer.begin();
    logSerial("Stream server started on port 81");

    delay(1000);

     pinMode(IR_LED_PIN, OUTPUT); 

         // Initialize PIR sensor pin
    pinMode(PIR_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIR_INPUT_PIN), pirISR, RISING);
    logSerial("PIR sensor initialized on pin " + String(PIR_INPUT_PIN));

    #if ESP_IDF_VERSION_VAL(4,4,1) == ESP_IDF_VERSION
        vad_inst = vad_create(VAD_MODE_0, VAD_SAMPLE_RATE_HZ, VAD_FRAME_LENGTH_MS);
    #elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,4,1)
        vad_inst = vad_create(VAD_MODE_0);
    #else
    #error "No support for this version."
    #endif

    vad_buff = (int16_t *)malloc(VAD_BUFFER_LENGTH * sizeof(short));
    if (vad_buff == NULL) {
        logSerial("Memory allocation failed for VAD buffer!");
        while (1) {
            delay(1000);
        }
    }

    initI2SMicrophone(); // Initialize the microphone

    // Initialize mutexes and semaphores
    cameraMutex = xSemaphoreCreateMutex();
    if (cameraMutex == NULL) {
        logSerial("Failed to create camera mutex.");
    }

    soundSemaphore = xSemaphoreCreateBinary();
    pirSemaphore = xSemaphoreCreateBinary();
    if (pirSemaphore == NULL || soundSemaphore == NULL) {
        logSerial("Failed to create semaphores.");
    }

        // Initialize the queue with a size of 10 (or whatever size fits your needs)
    sensorEventQueue = xQueueCreate(10, sizeof(uint8_t));
    if (sensorEventQueue == NULL) {
        logSerial("Failed to create sensor event queue");
        while (1) { vTaskDelay(portMAX_DELAY); }  // Halt execution if queue creation fails
    }

    // Create tasks with increased stack size and log creation success/failure
    BaseType_t xTask;

    xTask = xTaskCreatePinnedToCore(batteryTask, "Battery Task", 8192, NULL, 1, NULL, 1);
    if (xTask != pdPASS) {
        logSerial("Battery Task creation failed.");
    } else {
        logSerial("Battery Task created.");
    }

    xTask = xTaskCreatePinnedToCore(cameraTask, "Camera Task", 8192, NULL, 1, NULL, 1);
    if (xTask != pdPASS) {
        logSerial("Camera Task creation failed.");
    } else {
        logSerial("Camera Task created.");
    }

    xTask = xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 4096, NULL, 1, NULL, 1);
    if (xTask != pdPASS) {
        logSerial("Sensor Task creation failed.");
    } else {
        logSerial("Sensor Task created.");
    }

    xTask = xTaskCreatePinnedToCore(soundDetectionTask, "Sound Detection Task", 8192, NULL, 1, NULL, 1);
    if (xTask != pdPASS) {
        logSerial("Sound Detection Task creation failed.");
    } else {
        logSerial("Sound Detection Task created.");
    }

    xTask = xTaskCreatePinnedToCore(pirTask, "PIR Task", 8192, NULL, 1, NULL, 1);
    if (xTask != pdPASS) {
        logSerial("PIR Task creation failed.");
    } else {
        logSerial("PIR Task created.");
    }

    xTaskCreatePinnedToCore(streamTask, "Stream Task", 16384, NULL, 1, &streamTaskHandle, 1);

    loadSettings();

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
    // Monitor heap memory (optional)
    static unsigned long lastHeapPrint = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastHeapPrint >= 5000) { // Every 5 seconds
        lastHeapPrint = currentTime;
        //logSerial("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    }
    yield(); // Small delay to prevent WDT resets

    // Handle incoming OSC messages
    OscWiFi.update();

    switch (currentMode) {
        case LED_OFF:
            turnOffLeds();
            break;
        
        case LED_SOLID:
            showSolidColor(currentColor);
            break;
        
        case LED_PULSATE:
            smoothPulsate(currentColor, strip.numPixels());
            break;
        
        // Add more cases for additional behaviors if needed
    }


    yield(); // Small delay to prevent WDT resets
    //WiFiClient client = streamServer.available();
    //if (client) {
    //    // handle the client connection for streaming
    //}
    // Remove server.handleClient(); as it's not needed with AsyncWebServer

    // Only execute this block if 33ms (30Hz) have passed
    //if (currentTime - lastExecutionTime >= interval) {
        lastExecutionTime = currentTime;

        // Add `yield()` or `vTaskDelay()` to prevent WDT resets
    //    yield();
    //}

    // ------------------------
    // PIR and MIC Monitoring
    // ------------------------

    // Check PIR semaphore without blocking
    if (xSemaphoreTake(pirSemaphore, 0) == pdTRUE) {
        // Handle PIR event
        pirFlag = true;
        logSerial("PIR triggered.");
        sendTheatreChatOscMessage("PIR triggered.");
    }

    // Sound detection
    if (detectSound()) {
        if (currentTime - micTimestamp > SOUND_DEBOUNCE_DELAY) {
            micFlag = true;
            micTimestamp = currentTime;
            sendTheatreChatOscMessage("Microphone triggered.");
            logSerial("Microphone triggered.");
        }
    }

    // Combined trigger logic
    if (pirFlag && micFlag) {
        sendTheatreChatOscMessage("PIR and Microphone triggered");
        logSerial("PIR and Microphone triggered.");
        pirFlag = false;
        micFlag = false;
    }

    // Reset flags after debounce delay
    if (currentTime - micTimestamp > SOUND_DEBOUNCE_DELAY) {
        micFlag = false;
    }    

    yield(); // Small delay to prevent WDT resets

    // Reset flags if they time out
    if (pirFlag && (currentTime - micTimestamp > SOUND_DEBOUNCE_DELAY)) {
        pirFlag = false;
    }
    if (micFlag && (currentTime - micTimestamp > SOUND_DEBOUNCE_DELAY)) {
        micFlag = false;
    }

    // ------------------------

    if (pmu_flag) {
        pmu_flag = false;
        uint32_t status = PMU.getIrqStatus();

        if (PMU.isVbusInsertIrq()) {
            Serial.println("isVbusInsert");
            if (!chargingActive) {
                chargingActive = true;
                chargingScreenDisplayed = false; 
            }
        }
        if (PMU.isVbusRemoveIrq()) {
            Serial.println("isVbusRemove");
            // Exit charging mode automatically
            if (chargingActive) {
                chargingActive = false;
                chargingScreenDisplayed = false;
            }
            chargingClear();
        }
        if (PMU.isBatInsertIrq()) {
            Serial.println("isBatInsert");
        }
        if (PMU.isBatRemoveIrq()) {
            Serial.println("isBatRemove");
        }
        if (PMU.isPekeyShortPressIrq()) {
            logSerial("LEFT BUTTON!!!!!");
            handlePMUButtonPress();
        }
        if (PMU.isPekeyLongPressIrq()) {
            Serial.println("isPekeyLongPress");
        }

        // Clear PMU Interrupt Status Register
        PMU.clearIrqStatus();
    }



        if (pirTriggered) {
        pirTriggered = false;
        // Handle PIR event
    }
    //Serial.println("loop");

/*
    if (userButtonFlag) {
        userButtonFlag = false;  // Reset the flag

        // Perform the action for the button press
        logSerial("RIGHT BUTTON!!!!!");
        handleUserButtonPress();
        //handleButtonPress();  // Call your button press handler function (if applicable)
    }

    if (updatePrompt && countingForUpdate) {
        handleUpdateButton();
    }
*/


      if (userButtonFlag) {
    userButtonFlag = false;  // Reset the flag
    handleRegularButtonPress();
  }

  // Handle OTA update button hold
  if (updatePrompt && countingForUpdate) {
    handleUpdateButton();
  }
    
    if (WiFi.status() != WL_CONNECTED) {
        logSerial("WiFi disconnected!");
    } else {
        //Serial.print("Free heap: ");
        //Serial.println(ESP.getFreeHeap());
        //logSerial("WiFi connected, IP: " + WiFi.localIP().toString());
    }

    if (isStreaming && !streamingClient.connected()) {
        logSerial("Stream ended.");
        streamingClient.stop();
        isStreaming = false;
    }


    
    delay(1); // Small delay to prevent WDT resets
    }
