#ifndef LITTLEFS_FUNCTIONS_H
#define LITTLEFS_FUNCTIONS_H

#include "FFat.h" 
#include <LittleFS.h>  // Include LittleFS library
#include <ArduinoJson.h>  // Include ArduinoJson library for easy handling of JSON data

extern Preferences preferences;
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
extern int SOUND_THRESHOLD;
extern unsigned long SOUND_DEBOUNCE_DELAY;
extern unsigned long PIR_DEBOUNCE_DELAY;
extern int frameRate;
extern int cameraSaturation;
extern int cameraSpecialEffect;
extern bool cameraWhiteBalance;
extern bool cameraAwbGain;
extern int cameraWbMode;

void saveSettings() {
    preferences.begin("settings", false);  // Open "settings" namespace for read/write
    
    preferences.putBool("debugMode", debugMode);
    preferences.putBool("cameraFlip", cameraFlip);
    preferences.putBool("cameraMirror", cameraMirror);
    preferences.putInt("brightness", cameraBrightness);
    preferences.putInt("contrast", cameraContrast);
    preferences.putInt("quality", cameraQuality);
    preferences.putBool("irEnabled", irEnabled);
    preferences.putInt("irBrightness", irBrightness);
    preferences.putBool("oscReceive", oscReceiveEnabled);
    preferences.putInt("oscPort", oscPort);
    preferences.putBool("theatreChatEnabled", theatreChatEnabled);
    preferences.putInt("theatreChatPort", theatreChatPort);
    preferences.putString("theatreChatChannel", theatreChatChannel);
    preferences.putString("theatreChatName", theatreChatName);
    preferences.putString("theatreChatMessage", theatreChatMessage);
    preferences.putInt("deviceOrientation", deviceOrientation);
    preferences.putInt("soundThreshold", SOUND_THRESHOLD);  // Use putInt for integer
    preferences.putULong("soundDebounce", SOUND_DEBOUNCE_DELAY);  // Use putULong for unsigned long
    preferences.putULong("pirDebounce", PIR_DEBOUNCE_DELAY);  // Use putULong for unsigned long
    preferences.putInt("frameRate", frameRate);
    preferences.putInt("saturation", cameraSaturation);
    preferences.putInt("specialEffect", cameraSpecialEffect);
    preferences.putBool("whitebal", cameraWhiteBalance);
    preferences.putBool("awbGain", cameraAwbGain);
    preferences.putInt("wbMode", cameraWbMode);

    preferences.end();  // Close the preferences to free up memory
    Serial.println("Settings saved successfully");
}

void loadSettings() {
    preferences.begin("settings", true);  // Open "settings" namespace for read-only
    
    debugMode = preferences.getBool("debugMode", false);  // Default to false if not found
    cameraFlip = preferences.getBool("cameraFlip", true);  // Default to true (normal orientation)
    cameraMirror = preferences.getBool("cameraMirror", false);
    cameraBrightness = preferences.getInt("brightness", 0);  // Default brightness to 50
    cameraContrast = preferences.getInt("contrast", 0);      // Default contrast to 50
    cameraQuality = preferences.getInt("quality", 10);        // Default quality to 10
    irEnabled = preferences.getBool("irEnabled", false);
    irBrightness = preferences.getInt("irBrightness", 100);   // Default IR brightness
    oscReceiveEnabled = preferences.getBool("oscReceive", false);
    oscPort = preferences.getInt("oscPort", 27900);            // Default OSC port
    //theatreChatEnabled = preferences.getBool("theatreChatEnabled", true);
    theatreChatEnabled = preferences.getBool("theatreChatEnabled", true);
    theatreChatPort = preferences.getInt("theatreChatPort", 27900); // Default Theatre Chat port
    theatreChatChannel = preferences.getString("theatreChatChannel", "cameras");
    theatreChatName = preferences.getString("theatreChatName", "User");
    theatreChatMessage = preferences.getString("theatreChatMessage", "Hello World!");
    deviceOrientation = preferences.getInt("deviceOrientation", 0); // Default orientation
    // Updated key names to match those in saveSettings()
    SOUND_THRESHOLD = preferences.getInt("soundThreshold", 400); // Default to 400 if not set
    SOUND_DEBOUNCE_DELAY = preferences.getULong("soundDebounce", 5000); // Default to 5000 ms
    PIR_DEBOUNCE_DELAY = preferences.getULong("pirDebounce", 5000); // Default to 5000 ms
    frameRate = preferences.getInt("frameRate", 31);
    cameraSaturation = preferences.getInt("saturation", 0);
    cameraSpecialEffect = preferences.getInt("specialEffect", 0);
    cameraWhiteBalance = preferences.getBool("whitebal", true);
    cameraAwbGain = preferences.getBool("awbGain", true);
    cameraWbMode = preferences.getInt("wbMode", 0);

    preferences.end();  // Close the preferences to free up memory

    Serial.println("Loaded settings:");
    Serial.print("Debug Mode: "); Serial.println(debugMode ? "Enabled" : "Disabled");
    Serial.print("Camera Flip: "); Serial.println(cameraFlip ? "Enabled" : "Disabled");
    Serial.print("Camera Mirror: "); Serial.println(cameraMirror ? "Enabled" : "Disabled");
    Serial.print("Camera Brightness: "); Serial.println(cameraBrightness);
    Serial.print("Camera Contrast: "); Serial.println(cameraContrast);
    Serial.print("Camera Quality: "); Serial.println(cameraQuality);
    Serial.print("IR Enabled: "); Serial.println(irEnabled ? "Enabled" : "Disabled");
    Serial.print("IR Brightness: "); Serial.println(irBrightness);
    Serial.print("OSC Receive Enabled: "); Serial.println(oscReceiveEnabled ? "Enabled" : "Disabled");
    Serial.print("OSC Port: "); Serial.println(oscPort);
    Serial.print("Theatre Chat Enabled: "); Serial.println(theatreChatEnabled ? "Enabled" : "Disabled");
    Serial.print("Theatre Chat Port: "); Serial.println(theatreChatPort);
    Serial.print("Theatre Chat Channel: "); Serial.println(theatreChatChannel);
    Serial.print("Theatre Chat Name: "); Serial.println(theatreChatName);
    Serial.print("Theatre Chat Message: "); Serial.println(theatreChatMessage);
    Serial.print("Device Orientation: "); Serial.println(deviceOrientation);
    Serial.print("Sound Threshold: "); Serial.println(SOUND_THRESHOLD);
    Serial.print("Sound Debounce Delay: "); Serial.println(SOUND_DEBOUNCE_DELAY);
    Serial.print("PIR Debounce Delay: "); Serial.println(PIR_DEBOUNCE_DELAY);
    Serial.print("Frame Rate: "); Serial.println(frameRate);
    Serial.print("Camera Saturation: "); Serial.println(cameraSaturation);
    Serial.print("Camera Special Effect: "); Serial.println(cameraSpecialEffect);
    Serial.print("Camera White Balance: "); Serial.println(cameraWhiteBalance ? "Enabled" : "Disabled");
    Serial.print("Camera AWB Gain: "); Serial.println(cameraAwbGain ? "Enabled" : "Disabled");
    Serial.print("Camera WB Mode: "); Serial.println(cameraWbMode);

    Serial.println("Settings loaded successfully");
}

#endif  // LITTLEFS_FUNCTIONS_H
