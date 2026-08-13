#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>

const char* ssid     = "Galaxy A32 7E93";
const char* password = "rlis4727";

// ESP32 listens on port 8083, ESP32-CAM on 8082
WebServer server(8083);

// UART2: RX=16, TX=17 → Arduino pin 10(RX)/11(TX)
HardwareSerial ArduinoSerial(2);

// ── Helper: forward command string to Arduino ─────────────────────────────────
void toArduino(const String& cmd) {
  ArduinoSerial.println(cmd);
}

// Strip anything that isn't a printable ASCII character (or tab) so a
// glitchy/garbled UART frame can never reach the browser as raw binary,
// which is what makes it show up as an undisplayable "download".
String sanitizeReply(const String& raw) {
  String clean;
  clean.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if ((c >= 32 && c <= 126) || c == '\t') {
      clean += c;
    }
  }
  clean.trim();
  if (clean.length() == 0) {
    return "error: garbled reply from Arduino";
  }
  return clean;
}

String waitArduinoReply(unsigned long timeoutMs = 2000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (ArduinoSerial.available()) {
      String raw = ArduinoSerial.readStringUntil('\n');
      return sanitizeReply(raw);
    }
  }
  return "timeout";
}

// ── Route handlers ────────────────────────────────────────────────────────────
void handleForward() {
  String q     = server.arg("q");
  String speed = server.arg("speed");
  if (q.length() == 0)     q     = "1";
  if (speed.length() == 0) speed = "255";
  toArduino("forward(" + q + "," + speed + ")");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleBackward() {
  String q     = server.arg("q");
  String speed = server.arg("speed");
  if (q.length() == 0)     q     = "1";
  if (speed.length() == 0) speed = "255";
  toArduino("backward(" + q + "," + speed + ")");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleRight() {
  String angle = server.arg("q");
  toArduino("right(" + angle + ")");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleLeft() {
  String angle = server.arg("q");
  toArduino("left(" + angle + ")");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleStop() {
  toArduino("stop(1)");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleDistance() {
  String angle = server.arg("q");
  toArduino("distance(" + angle + ")");
  String reply = waitArduinoReply(2000);
  server.send(200, "text/plain", reply);
}

void handleTemperature() {
  String unit = server.arg("q");
  toArduino("temperature(" + unit + ")");
  String reply = waitArduinoReply(2000);
  server.send(200, "text/plain", reply);
}

void handleTemperatureEsp32() {
  float t = temperatureRead();
  server.send(200, "text/plain", String(t));
}

void handleHelp() {
  String msg =
    "NoorRobot ESP32 API\n"
    "-------------------\n"
    "/forward?q=<1|0>&speed=<0-255>\n"
    "/backward?q=<1|0>&speed=<0-255>\n"
    "/right?q=<angle>\n"
    "/left?q=<angle>\n"
    "/stop\n"
    "/distance?q=<angle>\n"
    "/temperature?q=<unit>        (from Arduino sensor)\n"
    "/temperature_esp32           (ESP32 internal temp)\n"
    "/fan?q=<on|off|status>\n"
    "/clear\n"
    "/eyes?q=<pattern>\n"
    "/shutdown\n"
    "/shutdownbyseconds?q=<seconds>\n"
    "/shutdownbytime?q=<time>\n"
    "/shuton\n"
    "/shutonbyseconds?q=<seconds>\n"
    "/shutonbytime?q=<time>\n"
    "/help                        (this message)\n";
  server.send(200, "text/plain", msg);
}

void handleFan() {
  String q = server.arg("q");
  if (q == "on") {
    toArduino("fan on");
  } else if (q == "off") {
    toArduino("fan off");
  } else if (q == "status") {
    toArduino("fan status");
    String reply = waitArduinoReply();
    server.send(200, "text/plain", reply);
    return;
  }
  server.send(200, "text/plain", "ok");
}

void handleClear() {
  toArduino("clear(1)");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleEyes() {
  String q = server.arg("q");
  toArduino("eyes(" + q + ")");
  String reply = waitArduinoReply();
  server.send(200, "text/plain", reply);
}

void handleShutdown() {
  toArduino("shutdown(True)");
  server.send(200, "text/plain", "ok");
}

void handleShutdownBySeconds() {
  String q = server.arg("q");
  toArduino("shutdownbyseconds(" + q + ")");
  server.send(200, "text/plain", "ok");
}

void handleShutdownByTime() {
  String q = server.arg("q");
  toArduino("shutdownbytime(" + q + ")");
  server.send(200, "text/plain", "ok");
}

void handleShutOn() {
  toArduino("shuton(True)");
  server.send(200, "text/plain", "ok");
}

void handleShutOnBySeconds() {
  String q = server.arg("q");
  toArduino("shutonbyseconds(" + q + ")");
  server.send(200, "text/plain", "ok");
}

void handleShutOnByTime() {
  String q = server.arg("q");
  toArduino("shutonbytime(" + q + ")");
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  ArduinoSerial.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());

  server.on("/forward",           handleForward);
  server.on("/backward",          handleBackward);
  server.on("/right",             handleRight);
  server.on("/left",              handleLeft);
  server.on("/stop",              handleStop);
  server.on("/distance",          handleDistance);
  server.on("/temperature",       handleTemperature);
  server.on("/temperature_esp32", handleTemperatureEsp32);
  server.on("/fan",               handleFan);
  server.on("/clear",             handleClear);
  server.on("/eyes",              handleEyes);
  server.on("/shutdown",          handleShutdown);
  server.on("/shutdownbyseconds", handleShutdownBySeconds);
  server.on("/shutdownbytime",    handleShutdownByTime);
  server.on("/shuton",            handleShutOn);
  server.on("/shutonbyseconds",   handleShutOnBySeconds);
  server.on("/shutonbytime",      handleShutOnByTime);
  server.on("/help",              handleHelp);

  server.begin();
  Serial.println("ESP32 HTTP server started on port 8083");
}

void loop() {
  server.handleClient();
}
