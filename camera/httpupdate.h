#ifndef HTTPUPDATE_H
#define HTTPUPDATE_H

//#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
//#include <Update.h>
#include <ArduinoJson.h>
#include <ESP32httpUpdate.h> 
#include <WiFiClientSecure.h>

// Declare external variables
extern volatile bool userButtonFlag;
extern volatile unsigned long lastDebounceTime;
extern const unsigned long debounceDelay;
extern void handleScreenSwitch();
extern String firmwareUrl; 
extern String currentVersion;

extern int screenPress;
extern void handleUserButtonPress();

// GitHub URLs
const char* versionUrl = "https://raw.githubusercontent.com/sqkysqnt/bscamOTA/refs/heads/main/version.json";



bool updatePrompt = false;  // A flag to track if we're at the update prompt
unsigned long buttonPressStartTime = 0;  // Time when button is pressed
bool countingForUpdate = false;  // Flag to start counting for the update hold
unsigned long lastDisplayUpdateTime = 0;  // For updating the countdown display

NetworkClient client;  

// Function prototypes
void performOTAUpdate(const char* url);
void handleRegularButtonPress();
void userButtonISR();

// Function to check for updates from GitHub
String checkForUpdate() {
  displayScreen("Checking...");
  HTTPClient http;
  http.begin(versionUrl);  // Access version.json from GitHub

  int httpCode = http.GET();  // Send the GET request
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    const char* latestVersion = doc["version"];
    firmwareUrl = String(doc["firmware_url"].as<const char*>());  // Corrected assignment

    if (currentVersion != String(latestVersion)) {
      Serial.println("New firmware version available: " + String(latestVersion));
      Serial.println("Firmware URL: " + firmwareUrl);
      displayScreen("New Version Available - Update?");
      updatePrompt = true;  // Set flag that we're in the update prompt
      return firmwareUrl;   // Return the new firmware URL
    } else {
      Serial.println("Already running the latest firmware version");
      displayScreen("Already on current version.");
      return "";  // No update available
    }
  } else {
    Serial.printf("Failed to fetch version file, error: %s\n", http.errorToString(httpCode).c_str());
    displayScreen("Error");
    return "";
  }

  http.end();
}

// Function to handle the button press for the OTA update when updatePrompt is active
void handleUpdateButton() {
  if (countingForUpdate) {
    unsigned long currentTime = millis();
    unsigned long timeHeld = currentTime - buttonPressStartTime;

    if (timeHeld >= 10000) {
      // If button is held for 10 seconds, perform the OTA update
      displayScreen("Updating...");
      performOTAUpdate(firmwareUrl.c_str());  // Trigger the OTA update
      countingForUpdate = false;  // Reset the counting flag
      updatePrompt = false;       // Reset the update prompt flag
    } else {
      // Display decrementing time on the screen
      unsigned long timeLeft = (10000 - timeHeld) / 1000;  // Convert to seconds
      if (currentTime - lastDisplayUpdateTime >= 1000) {
        displayScreen("Hold for " + String(timeLeft) + "s to update");
        lastDisplayUpdateTime = currentTime;  // Update the last display time
      }
    }
  }
}

// ISR to detect button press and initiate the countdown for OTA if applicable
void userButtonISR() {
  unsigned long currentTime = millis();
  if ((currentTime - lastDebounceTime) > debounceDelay) {
    if (digitalRead(USER_BUTTON_PIN) == LOW) {
      // If the update prompt is active, start counting for the update
      if (updatePrompt && !countingForUpdate) {
        buttonPressStartTime = currentTime;  // Record the time the button was pressed
        countingForUpdate = true;            // Start counting for the update hold
      } else if (!updatePrompt) {
        // Handle regular button press tasks when not in the update prompt
        userButtonFlag = true;  // Set the flag to handle in the main loop
      }
    } else {
      // Button released, cancel the update if released early
      if (countingForUpdate) {
        countingForUpdate = false;  // Reset the flag if button is released early
        displayScreen("Update canceled");
      }
    }
    lastDebounceTime = currentTime;  // Update the debounce time
  }
}

// Function to handle regular button presses
void handleRegularButtonPress() {
  // Your existing code to handle regular button presses
  Serial.println("Regular button press detected.");
  handleUserButtonPress();  // Call the function to perform action based on current screen
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}


/*
// Function to perform OTA update
void performOTAUpdate(const char* url) {

  Serial.println("Starting OTA update...");

  t_httpUpdate_return ret = ESPhttpUpdate.update(url);  // Update to ESPhttpUpdate for ESP32



  switch (ret) {

    case HTTP_UPDATE_FAILED:

      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());

      break;



    case HTTP_UPDATE_NO_UPDATES:

      Serial.println("No updates available");

      break;



    case HTTP_UPDATE_OK:

      Serial.println("Update successful, device will restart!");

      break;

  }

}
*/

void performOTAUpdate(const char* firmwareUrl) {
  HTTPClient http;
  http.setUserAgent("ESP32 OTA Client");  // Set a user agent for compatibility
  http.begin(firmwareUrl);  // Initialize HTTP request

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Content-Length: %d\n", contentLength);

    if (contentLength <= 0) {
      Serial.println("Invalid content length received. OTA update aborted.");
      http.end();
      return;
    }

    bool canBegin = Update.begin(contentLength);
    if (canBegin) {
      Serial.println("Beginning OTA update...");
      WiFiClient* client = http.getStreamPtr();
      size_t written = Update.writeStream(*client);

      if (written == contentLength) {
        Serial.println("OTA update successfully written.");
      } else {
        Serial.printf("Written only %d/%d bytes. OTA update failed.\n", written, contentLength);
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting...");
          ESP.restart();
        } else {
          Serial.println("Update not finished. Something went wrong.");
        }
      } else {
        Serial.printf("Update failed. Error #: %d\n", Update.getError());
      }
    } else {
      Serial.println("Not enough space to begin OTA update.");
    }

  } else if (httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    // Handle HTTP 302 or 301 redirect
    String newUrl = http.header("Location");
    Serial.println("Redirected to new URL: " + newUrl);

    if (!newUrl.isEmpty()) {
      // Reattempt OTA update with the new URL
      http.end();  // End previous HTTP connection
      performOTAUpdate(newUrl.c_str());  // Recursive call with the new URL
      return;
    } else {
      Serial.println("Failed to obtain redirect URL from Location header.");
    }

  } else {
    // Any other HTTP error code
    Serial.printf("HTTP request failed with code: %d\n", httpCode);
  }

  http.end();  // End HTTP connection
}



#endif
