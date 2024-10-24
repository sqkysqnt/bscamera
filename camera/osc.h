#ifndef OSC_H
#define OSC_H

#include <ArduinoOSCWiFi.h>
#include "utilities.h"
#include <U8g2lib.h>

extern String micOnColor;
extern String micOffColor;
extern String micReadyColor;
extern bool ledState;
extern int ledBrightness;

// Declare the global variable for the current OSC message
OscMessage currentOscMessage("/dummy");  // Initialize with a dummy address

// Declare the function properly with the correct parameter type
void setCurrentOscMessage(OscMessage &msg) {
    currentOscMessage = msg;  // Set the global message to the received message
}
uint8_t screenBuffer[1024]; // Adjust size based on your screen's memory size
bool bufferSaved = false;    // Flag to check if the buffer is saved

extern void logSerial(String message);
extern void hexToRGB(const String &hex, uint8_t &r, uint8_t &g, uint8_t &b);


// Reference to the display object
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Helper function to clear and center text on the screen
void displayScreen(String text, bool saveBuffer = false, bool recallBuffer = false) {
    if (recallBuffer && bufferSaved) {
        // Load the buffer content and display it
        u8g2.clearBuffer();
        u8g2.drawXBM(0, 0, u8g2.getDisplayWidth(), u8g2.getDisplayHeight(), screenBuffer);
        u8g2.sendBuffer();
        return;
    }

    // Proceed with generating the display content if not recalling buffer
    u8g2.clearBuffer();  // Clear the internal memory

    // List of fonts from largest to smallest
    const uint8_t *fonts[] = {
        u8g2_font_fub30_tr,  // Free Universal Bold 30pt
        u8g2_font_fub25_tr,  // 25pt
        u8g2_font_fub20_tr,  // 20pt
        u8g2_font_fub17_tr,  // 17pt
        u8g2_font_fub14_tr,  // 14pt
        u8g2_font_fub11_tr   // 11pt
    };

    const int num_fonts = sizeof(fonts) / sizeof(fonts[0]);
    const uint8_t width = u8g2.getDisplayWidth();
    const uint8_t height = u8g2.getDisplayHeight();

    // Try to select the largest font that will fit
    int font_index;
    for (font_index = 0; font_index < num_fonts; font_index++) {
        u8g2.setFont(fonts[font_index]);

        uint16_t text_width = u8g2.getStrWidth(text.c_str());
        uint16_t text_height = u8g2.getMaxCharHeight();

        if (text_width <= width && text_height <= height) {
            // The text fits with this font
            break;
        }
    }

    if (font_index == num_fonts) {
        // If none of the fonts fit, use the smallest font
        font_index = num_fonts - 1;
        u8g2.setFont(fonts[font_index]);
    }

    uint16_t text_height = u8g2.getMaxCharHeight();  // Height of the current font
    int max_lines = height / text_height;  // Maximum number of lines that can fit

    // Word wrapping and line splitting
    std::vector<String> lines;
    String current_line = "";
    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        current_line += c;

        uint16_t line_width = u8g2.getStrWidth(current_line.c_str());

        if (line_width > width || c == '\n') {
            if (c == ' ' || c == '\n') {
                lines.push_back(current_line);
                current_line = "";
            } else {
                // Split the word and add the line
                int last_space_index = current_line.lastIndexOf(' ');
                if (last_space_index >= 0) {
                    String part_to_draw = current_line.substring(0, last_space_index);
                    lines.push_back(part_to_draw);
                    current_line = current_line.substring(last_space_index + 1);
                } else {
                    lines.push_back(current_line);
                    current_line = "";
                }
            }
        }
    }

    // Add the last line if there is any remaining text
    if (current_line.length() > 0) {
        lines.push_back(current_line);
    }

    // Adjust the vertical position to center the text block
    int total_text_height = lines.size() * text_height;
    int y_pos = (height - total_text_height) / 2 + text_height;  // Start vertically centered

    // Draw each line with horizontal centering
    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i];
        uint16_t line_width = u8g2.getStrWidth(line.c_str());
        int x_pos = (width - line_width) / 2;  // Horizontally center the line
        u8g2.drawStr(x_pos, y_pos, line.c_str());
        y_pos += text_height;  // Move to the next line
        if (y_pos > height) break;  // Stop if we've run out of vertical space
    }

    // Save buffer to memory if needed
    if (saveBuffer) {
        memcpy(screenBuffer, u8g2.getBufferPtr(), u8g2.getBufferTileHeight() * u8g2.getBufferTileWidth() * 8);
        bufferSaved = true;  // Mark the buffer as saved
    }

    // Send the buffer to the display
    u8g2.sendBuffer();
}




// OSC handler functions
void handleMicOn() {
    logSerial("OSC: Mic On");
    displayScreen("Mic On");

    if (ledState){
      uint8_t r, g, b;
      hexToRGB(micOnColor, r, g, b);
      uint32_t colorVariable = strip.Color(r, g, b);

      currentMode = LED_SOLID;
      currentColor = colorVariable;
    }
    else {
      currentMode = LED_OFF;
    }    
}

void handleMicOff() {
    logSerial("OSC: Mic Off");
    displayScreen("Mic Off");

    if (ledState){
      uint8_t r, g, b;
      hexToRGB(micOffColor, r, g, b);
      uint32_t colorVariable = strip.Color(r, g, b);

      currentMode = LED_SOLID;
      currentColor = colorVariable;
    }
    else {
      currentMode = LED_OFF;
    }

}

void handleMicReady() {
    logSerial("OSC: Mic Ready");
    displayScreen("Mic Ready");



    if (ledState){
      uint8_t r, g, b;
      hexToRGB(micReadyColor, r, g, b);
      uint32_t colorVariable = strip.Color(r, g, b);

      currentMode = LED_PULSATE;
      currentColor = colorVariable;
    }
    else {
      currentMode = LED_OFF;
    }    

}

void handleStandby() {
    logSerial("OSC: Standby");
    displayScreen("Standby");
}

void handleGo() {
    logSerial("OSC: Go");
    displayScreen("Go");
}

void handleWarning() {
    logSerial("OSC: Warning");
    displayScreen("Warning");
}

void handleClear() {
    logSerial("OSC: Clear");
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    if (ledState) {
      currentMode = LED_OFF;
    }

}




void handleLedOn(String ledColor) {
    logSerial("LED Color Arguments: " + ledColor);

    // Resolve the color
    String hexColor = get_gel_color(ledColor);

    // Check if the color is valid
    if (hexColor != "#FFFFFF" || ledColor.equalsIgnoreCase("white")) {
        logSerial("Resolved Hex Color: " + hexColor);

        // Convert the hex color to RGB
        uint8_t r, g, b;
        hexToRGB(hexColor, r, g, b);
        currentColor = strip.Color(r, g, b);

        currentMode = LED_SOLID;
        showSolidColor(currentColor);
    } else {
        logSerial("Invalid color: " + ledColor);
    }
}


void handleLedOnFromOSC() {
    if (currentOscMessage.size() > 0) {
        String ledColor = "";
        for (int i = 0; i < currentOscMessage.size(); i++) {
            if (i > 0) ledColor += " ";  // Add space between words
            ledColor += currentOscMessage.arg<String>(i);
        }
        handleLedOn(ledColor);  // Pass the full color name to handleLedOn
    } else {
        logSerial("Invalid OSC message: No arguments.");
    }
}





void handleLedOff() {
    logSerial("Turning off LEDs");
    currentMode = LED_OFF;  // Set the mode to off
    turnOffLeds();  // Turn off all the LEDs off
}



void handleDisplay() {
    String displayText = currentOscMessage.arg<String>(0);  // Get the first argument as a string
    logSerial("OSC: Display - " + displayText);
    displayScreen(displayText);
}

// Call this function inside your OSC callbacks to capture the message
void captureMessageAndProcess(OscMessage &msg, void (*processFunc)()) {
    setCurrentOscMessage(msg);  // Set the global message
    processFunc();              // Call the appropriate handler
}


#endif
