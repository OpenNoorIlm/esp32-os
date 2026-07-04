#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>

// Hold this pin LOW at power-up (the BOOT button on most dev boards) to
// force re-entry into WiFi setup mode, even if credentials already exist.
#define WIFI_BOOT_BTN_PIN 0

namespace WifiManager {

inline Preferences prefs;
inline WebServer* setupServer = nullptr;

inline String loadSsid() {
  prefs.begin("wifi", true);
  String s = prefs.getString("ssid", "");
  prefs.end();
  return s;
}

inline String loadPass() {
  prefs.begin("wifi", true);
  String p = prefs.getString("pass", "");
  prefs.end();
  return p;
}

inline void saveCredentials(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

inline bool hasCredentials() { return loadSsid().length() > 0; }

} // namespace WifiManager

namespace WifiManager {

inline void handleRoot() {
  String html =
    "<html><body style='font-family:sans-serif'>"
    "<h2>NoorRobot ESP32 WiFi Setup</h2>"
    "<form method='POST' action='/save'>"
    "SSID: <input name='ssid'><br><br>"
    "Password: <input name='pass' type='password'><br><br>"
    "<input type='submit' value='Save & Reboot'>"
    "</form></body></html>";
  setupServer->send(200, "text/html", html);
}

inline void handleSave() {
  String ssid = setupServer->arg("ssid");
  String pass = setupServer->arg("pass");
  if (ssid.length() == 0) {
    setupServer->send(400, "text/plain", "SSID required");
    return;
  }
  saveCredentials(ssid, pass);
  setupServer->send(200, "text/plain", "Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

inline void startSetupAP() {
  String apName = "NoorRobot-Setup-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());
  Serial.println("Setup AP: " + apName);
  Serial.println("Visit http://" + WiFi.softAPIP().toString() + " to configure WiFi");

  setupServer = new WebServer(80);
  setupServer->on("/", handleRoot);
  setupServer->on("/save", HTTP_POST, handleSave);
  setupServer->begin();

  while (true) {
    setupServer->handleClient();
    delay(2);
  }
}

} // namespace WifiManager

namespace WifiManager {

// Blocks until connected. If the saved network exists but is unreachable
// (router off, out of range), it retries forever rather than giving up —
// per your requirement that it should wait "until it comes into existence".
// If no credentials exist yet, or the BOOT button is held at power-up
// (letting you edit an existing saved network), it drops into setup-AP mode.
inline bool connectOrSetup(unsigned long perAttemptTimeoutMs = 15000) {
  pinMode(WIFI_BOOT_BTN_PIN, INPUT_PULLUP);
  bool forceSetup = (digitalRead(WIFI_BOOT_BTN_PIN) == LOW);

  if (forceSetup || !hasCredentials()) {
    Serial.println(forceSetup
      ? "BOOT button held -- forcing WiFi setup mode."
      : "No WiFi credentials saved -- starting setup AP.");
    startSetupAP();
    return false; // unreachable; startSetupAP() blocks until reboot
  }

  String ssid = loadSsid();
  String pass = loadPass();
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to saved WiFi: " + ssid);

  while (true) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < perAttemptTimeoutMs) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected: " + WiFi.localIP().toString());
      return true;
    }
    Serial.println("\nNetwork unreachable, retrying in 5s...");
    delay(5000);
  }
}

inline void updateCredentials(const String& ssid, const String& pass) {
  saveCredentials(ssid, pass);
}

} // namespace WifiManager
