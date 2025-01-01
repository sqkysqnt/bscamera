#ifndef LED_H
#define LED_H

#include <Adafruit_NeoPixel.h>
#include <map>
#include <string>
#include <algorithm>
#include "colors.h"

enum LedMode {
    LED_OFF,
    LED_SOLID,
    LED_PULSATE
};

// Global variables
LedMode currentMode = LED_OFF;   // Default mode is "off"
uint32_t currentColor = 0xFF0000;  // Default color (e.g., red)

// External variables
extern String micOnColor;
extern String micOffColor;
extern String micReadyColor;
extern bool ledState;
extern int ledBrightness;

// Declare the LED strip globally (assuming 8 LEDs and pin 15)
extern Adafruit_NeoPixel strip;


// Function to return the hex code or name
// led.h

String get_gel_color(String query) {
    query.toLowerCase(); // Convert query to lowercase

    // Iterate through the array to find the match
    for (size_t i = 0; i < sizeof(gels) / sizeof(GelColor); i++) {
        String nameStr = String(gels[i].name);
        String hexStr = String(gels[i].hex);

        nameStr.toLowerCase(); // Ensure names are in lowercase for comparison

        if (query == nameStr || query == hexStr) {
            return hexStr;  // Return hex code if query matches name or hex
        }
    }

    // Default to white if no match found
    return "#FFFFFF";
}





// Helper function to scale the brightness of the color
uint32_t scaleColorBrightness(uint32_t color, int brightness) {
    uint8_t r = (uint8_t)((color >> 16) & 0xFF);
    uint8_t g = (uint8_t)((color >> 8) & 0xFF);
    uint8_t b = (uint8_t)(color & 0xFF);

    // Scale each color component by the brightness percentage
    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;

    return strip.Color(r, g, b);
}


// Function to convert hex color (e.g., #FF0000) to RGB
void hexToRGB(const String &hex, uint8_t &r, uint8_t &g, uint8_t &b) {
    // Convert the String to a C-style string and skip the first character (the '#' symbol)
    long number = (long) strtol(hex.substring(1).c_str(), NULL, 16);
    r = (number >> 16) & 0xFF;
    g = (number >> 8) & 0xFF;
    b = number & 0xFF;
}


void turnOffLeds() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));  // Set each LED to black (off)
    }
    strip.show();  // Apply the changes to turn off the LEDs
}

void showSolidColor(uint32_t colorVariable) {
    uint32_t scaledColor = scaleColorBrightness(colorVariable, ledBrightness);  // Scale the color brightness

    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, scaledColor);  // Set each LED to the scaled color
    }
    strip.show();
}

void smoothPulsate(uint32_t colorVariable, int numLeds, int delayTime = 10) {
    static int brightness = 0;           // Current brightness level
    static bool increasing = true;       // Flag to track increasing/decreasing brightness
    static unsigned long lastUpdate = 0; // Timestamp of the last update

    // Check if it's time to update the brightness
    if (millis() - lastUpdate >= delayTime) {
        lastUpdate = millis();  // Update the timestamp

        // Update brightness value
        if (increasing) {
            brightness += 1;  // Increase brightness
            if (brightness >= ledBrightness) {  // Cap the brightness to ledBrightness
                brightness = ledBrightness;
                increasing = false;  // Switch to decreasing brightness
            }
        } else {
            brightness -= 1;  // Decrease brightness
            if (brightness <= 0) {
                brightness = 0;
                increasing = true;  // Switch to increasing brightness
            }
        }

        // Apply the brightness to all LEDs
        for (int i = 0; i < numLeds; i++) {
            uint32_t color = scaleColorBrightness(colorVariable, brightness);
            strip.setPixelColor(i, color);
        }
        strip.show();
    }
}


#endif
