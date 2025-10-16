/*******************************************************
 * ESP32 OSC Button Light (Triple Short + Triple Long)
 * - WiFiManager captive portal
 * - Web configuration and JSON API
 * - 3 configurable short-press OSC messages
 * - 3 configurable long-press OSC messages
 * - OSC send/receive (ArduinoOSCWiFi)
 * - Short press -> send enabled short messages + LED ON
 * - Long press -> send enabled long messages + LED OFF
 * - /api/press endpoint for remote trigger
 *******************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ArduinoOSCWiFi.h>
#include <ArduinoJson.h>

// ===================== USER CONFIG =====================
#define LED_PIN         5
#define BUTTON_PIN      14
#define DEFAULT_BRIGHT  255
#define DEFAULT_DEBOUNCE_MS  30
#define DEFAULT_HOLD_MS      800
#define DEFAULT_OSC_IP       "255.255.255.255"
#define DEFAULT_OSC_PORT     27900
#define DEFAULT_CHANNEL      "cameras"
#define DEFAULT_DEVICE       "button_device"
#define DEFAULT_PRESS_MSG    "button pressed"
#define DEFAULT_HOLD_MSG     "button held"
#define NOT_CONNECTED_BLINK_MS 500

// ===================== GLOBALS =====================
Preferences prefs;
WebServer server(80);

struct OscMsgConfig {
  bool enabled;
  String channel;
  String message;
};

struct Settings {
  String oscHost;
  uint16_t oscPort;
  String deviceName;
  OscMsgConfig shortMsgs[3];
  OscMsgConfig longMsgs[3];
  String recvOffPath;
  uint8_t brightness;
  uint16_t debounceMs;
  uint16_t holdMs;
} cfg;

bool ledLatchedOn = false;
bool buttonPrev = true;
uint32_t lastChangeMs = 0;
uint32_t pressStartMs = 0;
bool pressedState = false;
bool holdActionFired = false;

// ===================== UTILITIES =====================
String macLast4() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

String defaultRecvOffPath(const String& ch, const String& dev) {
  return "/" + ch + "/" + dev + "/off";
}

// ===================== LED =====================
void ledOn()   { digitalWrite(LED_PIN, HIGH); }
void ledOff()  { digitalWrite(LED_PIN, LOW); }

void ledBlink() {
  static uint32_t lastToggle = 0;
  static bool state = false;
  if (millis() - lastToggle > NOT_CONNECTED_BLINK_MS) {
    lastToggle = millis();
    state = !state;
    digitalWrite(LED_PIN, state ? HIGH : LOW);
  }
}

void renderLED() {
  if (!WiFi.isConnected()) ledBlink();
  else if (ledLatchedOn) ledOn();
  else ledOff();
}

// ===================== PERSISTENCE =====================
void loadSettings() {
  prefs.begin("osclight", true);
  cfg.oscHost    = prefs.getString("oscHost", DEFAULT_OSC_IP);
  cfg.oscPort    = prefs.getUShort("oscPort", DEFAULT_OSC_PORT);
  cfg.deviceName = prefs.getString("devName", DEFAULT_DEVICE);

  for (int i = 0; i < 3; i++) {
    String idx = String(i + 1);
    cfg.shortMsgs[i].enabled = prefs.getBool(("sp" + idx + "_en").c_str(), true);
    cfg.shortMsgs[i].channel = prefs.getString(("sp" + idx + "_ch").c_str(), DEFAULT_CHANNEL);
    cfg.shortMsgs[i].message = prefs.getString(("sp" + idx + "_msg").c_str(), DEFAULT_PRESS_MSG);

    cfg.longMsgs[i].enabled  = prefs.getBool(("lp" + idx + "_en").c_str(), false);
    cfg.longMsgs[i].channel  = prefs.getString(("lp" + idx + "_ch").c_str(), DEFAULT_CHANNEL);
    cfg.longMsgs[i].message  = prefs.getString(("lp" + idx + "_msg").c_str(), DEFAULT_HOLD_MSG);
  }

  cfg.recvOffPath = prefs.getString("recvOff", defaultRecvOffPath(cfg.shortMsgs[0].channel, cfg.deviceName));
  cfg.brightness  = prefs.getUChar("bright", DEFAULT_BRIGHT);
  cfg.debounceMs  = prefs.getUShort("debounce", DEFAULT_DEBOUNCE_MS);
  cfg.holdMs      = prefs.getUShort("hold", DEFAULT_HOLD_MS);
  prefs.end();
}

void saveSettings() {
  prefs.begin("osclight", false);
  prefs.putString("oscHost", cfg.oscHost);
  prefs.putUShort("oscPort", cfg.oscPort);
  prefs.putString("devName", cfg.deviceName);

  for (int i = 0; i < 3; i++) {
    String idx = String(i + 1);
    prefs.putBool(("sp" + idx + "_en").c_str(), cfg.shortMsgs[i].enabled);
    prefs.putString(("sp" + idx + "_ch").c_str(), cfg.shortMsgs[i].channel);
    prefs.putString(("sp" + idx + "_msg").c_str(), cfg.shortMsgs[i].message);

    prefs.putBool(("lp" + idx + "_en").c_str(), cfg.longMsgs[i].enabled);
    prefs.putString(("lp" + idx + "_ch").c_str(), cfg.longMsgs[i].channel);
    prefs.putString(("lp" + idx + "_msg").c_str(), cfg.longMsgs[i].message);
  }

  prefs.putString("recvOff", cfg.recvOffPath);
  prefs.putUChar("bright", cfg.brightness);
  prefs.putUShort("debounce", cfg.debounceMs);
  prefs.putUShort("hold", cfg.holdMs);
  prefs.end();
  Serial.println("Settings saved.");
}

// ===================== WIFI =====================
void ensureWiFi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  String apname = "button_device_" + macLast4();
  wm.setConfigPortalTimeout(180);
  wm.autoConnect(apname.c_str());
  WiFi.setSleep(false);
}

// ===================== WEB UI =====================
String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + ip[1] + "." + ip[2] + "." + ip[3];
}

void handleRoot() {
  String status = WiFi.isConnected() ? "Connected" : "Not Connected";
  IPAddress ip = WiFi.localIP();

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
                "<style>body{font-family:sans-serif;max-width:720px;margin:20px auto;padding:0 12px}"
                "input,button{font-size:1rem;padding:.4rem;margin:.2rem 0;width:100%}"
                "label{font-weight:600;margin-top:.6rem;display:block}"
                "fieldset{border:1px solid #ccc;padding:12px;border-radius:8px;margin:12px 0}"
                "legend{padding:0 8px}</style></head><body>"
                "<h2>OSC Button Light</h2><p><b>Status:</b> " + status + "</p>"
                "<form method='POST' action='/save'>"
                "<fieldset><legend>OSC Target</legend>"
                "<label>Target IP</label><input name='oscHost' value='" + cfg.oscHost + "'>"
                "<label>Target Port</label><input name='oscPort' type='number' value='" + String(cfg.oscPort) + "'>"
                "<label>Device Name</label><input name='deviceName' value='" + cfg.deviceName + "'>"
                "</fieldset>";

  // --- Short press messages
  html += "<fieldset><legend>Short Press Messages</legend>";
  for (int i = 0; i < 3; i++) {
    html += "<label><input type='checkbox' name='sp" + String(i + 1) + "_en' " +
            (cfg.shortMsgs[i].enabled ? "checked" : "") + "> Enable Msg " + String(i + 1) + "</label>";
    html += "<label>Channel</label><input name='sp" + String(i + 1) + "_ch' value='" + cfg.shortMsgs[i].channel + "'>";
    html += "<label>Message</label><input name='sp" + String(i + 1) + "_msg' value='" + cfg.shortMsgs[i].message + "'>";
  }
  html += "</fieldset>";

  // --- Long press messages
  html += "<fieldset><legend>Long Press Messages</legend>";
  for (int i = 0; i < 3; i++) {
    html += "<label><input type='checkbox' name='lp" + String(i + 1) + "_en' " +
            (cfg.longMsgs[i].enabled ? "checked" : "") + "> Enable Msg " + String(i + 1) + "</label>";
    html += "<label>Channel</label><input name='lp" + String(i + 1) + "_ch' value='" + cfg.longMsgs[i].channel + "'>";
    html += "<label>Message</label><input name='lp" + String(i + 1) + "_msg' value='" + cfg.longMsgs[i].message + "'>";
  }
  html += "</fieldset>";

  html += "<fieldset><legend>Behavior</legend>"
          "<label>Debounce (ms)</label><input name='debounceMs' type='number' value='" + String(cfg.debounceMs) + "'>"
          "<label>Hold-to-off (ms)</label><input name='holdMs' type='number' value='" + String(cfg.holdMs) + "'>"
          "</fieldset><button type='submit'>Save</button></form>"
          "<p><a href='/reboot'><button type='button'>Reboot</button></a></p>"
          "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  cfg.oscHost = server.arg("oscHost");
  cfg.oscPort = server.arg("oscPort").toInt();
  cfg.deviceName = server.arg("deviceName");

  for (int i = 0; i < 3; i++) {
    String idx = String(i + 1);
    cfg.shortMsgs[i].enabled = server.hasArg("sp" + idx + "_en");
    cfg.shortMsgs[i].channel = server.arg("sp" + idx + "_ch");
    cfg.shortMsgs[i].message = server.arg("sp" + idx + "_msg");
    cfg.longMsgs[i].enabled = server.hasArg("lp" + idx + "_en");
    cfg.longMsgs[i].channel = server.arg("lp" + idx + "_ch");
    cfg.longMsgs[i].message = server.arg("lp" + idx + "_msg");
  }

  cfg.debounceMs = server.arg("debounceMs").toInt();
  cfg.holdMs = server.arg("holdMs").toInt();
  saveSettings();
  server.send(200, "text/html", "<html><body><h3>Saved</h3><a href='/'>Back</a></body></html>");
}

void handleReboot() {
  server.send(200, "text/html", "<html><body><h3>Rebooting...</h3></body></html>");
  delay(200);
  ESP.restart();
}

// ===================== JSON API =====================
void handleApiConfigGet() {
  StaticJsonDocument<1536> doc;
  doc["oscHost"] = cfg.oscHost;
  doc["oscPort"] = cfg.oscPort;
  doc["deviceName"] = cfg.deviceName;
  JsonArray sp = doc.createNestedArray("shortMsgs");
  JsonArray lp = doc.createNestedArray("longMsgs");
  for (int i = 0; i < 3; i++) {
    JsonObject s = sp.createNestedObject();
    s["enabled"] = cfg.shortMsgs[i].enabled;
    s["channel"] = cfg.shortMsgs[i].channel;
    s["message"] = cfg.shortMsgs[i].message;
    JsonObject l = lp.createNestedObject();
    l["enabled"] = cfg.longMsgs[i].enabled;
    l["channel"] = cfg.longMsgs[i].channel;
    l["message"] = cfg.longMsgs[i].message;
  }
  doc["debounceMs"] = cfg.debounceMs;
  doc["holdMs"] = cfg.holdMs;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ===================== OSC =====================
void onOscOff(const OscMessage&) {
  Serial.println("OSC: Received LED OFF message.");
  ledLatchedOn = false;
}

void setupOsc() {
  if (cfg.recvOffPath.isEmpty())
    cfg.recvOffPath = defaultRecvOffPath(cfg.shortMsgs[0].channel, cfg.deviceName);
  OscWiFi.subscribe(cfg.oscPort, cfg.recvOffPath.c_str(), onOscOff);
}

// ===================== OSC SEND =====================
void sendOscSet(OscMsgConfig* set) {
  for (int i = 0; i < 3; i++) {
    if (!set[i].enabled) continue;
    String path = "/theatrechat/message/" + set[i].channel;
    Serial.printf("Sending OSC: %s [%s -> %s]\n",
                  path.c_str(), cfg.deviceName.c_str(), set[i].message.c_str());
    OscWiFi.send(cfg.oscHost.c_str(), cfg.oscPort, path.c_str(),
                 cfg.deviceName.c_str(), set[i].message.c_str());
  }
}

// ===================== BUTTON =====================
bool rawButtonRead() { return digitalRead(BUTTON_PIN) == LOW; }

void updateButton() {
  bool rawPressed = rawButtonRead();
  uint32_t now = millis();

  if (rawPressed != buttonPrev) {
    lastChangeMs = now;
    buttonPrev = rawPressed;
  }

  if ((now - lastChangeMs) >= cfg.debounceMs) {
    if (rawPressed != pressedState) {
      pressedState = rawPressed;
      if (pressedState) {
        pressStartMs = now;
        holdActionFired = false;
      } else {
        uint32_t dur = now - pressStartMs;
        if (!holdActionFired && dur < cfg.holdMs) {
          if (WiFi.isConnected()) sendOscSet(cfg.shortMsgs);
          ledLatchedOn = true;
        }
      }
    }
  }

  if (pressedState && !holdActionFired && (now - pressStartMs) >= cfg.holdMs) {
    holdActionFired = true;
    if (WiFi.isConnected()) sendOscSet(cfg.longMsgs);
    ledLatchedOn = false;
  }
}

// ===================== MAIN =====================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ledOff();
  loadSettings();
  ensureWiFi();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.on("/api/config", HTTP_GET, handleApiConfigGet);
  server.begin();
  setupOsc();
}

void loop() {
  static uint32_t lastOscUpdate = 0;
  server.handleClient();
  if (millis() - lastOscUpdate > 50) {
    lastOscUpdate = millis();
    OscWiFi.update();
  }
  updateButton();
  renderLED();
  delay(1);
}
