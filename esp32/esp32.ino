#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

#include "capability.h"
#include "wifi_manager.h"
#include "fs_manager.h"
#include "robot_api.h"
#include "shell_server.h"
#include "task_manager.h"

// Existing robot HTTP API -- unchanged routes/port, now backed by the
// shared RobotApi:: functions also used by the "robot" shell command.
WebServer server(8083);

void handleForward()  { server.send(200, "text/plain", RobotApi::forward(server.arg("q"), server.arg("speed"))); }
void handleBackward() { server.send(200, "text/plain", RobotApi::backward(server.arg("q"), server.arg("speed"))); }
void handleRight()    { server.send(200, "text/plain", RobotApi::right(server.arg("q"))); }
void handleLeft()     { server.send(200, "text/plain", RobotApi::left(server.arg("q"))); }
void handleStop()     { server.send(200, "text/plain", RobotApi::stop()); }
void handleDistance() { server.send(200, "text/plain", RobotApi::distance(server.arg("q"))); }
void handleTemperature()      { server.send(200, "text/plain", RobotApi::temperature(server.arg("q"))); }
void handleTemperatureEsp32() { server.send(200, "text/plain", String(temperatureRead())); }
void handleFan()   { server.send(200, "text/plain", RobotApi::fan(server.arg("q"))); }
void handleClear() { server.send(200, "text/plain", RobotApi::clearCmd()); }
void handleEyes()  { server.send(200, "text/plain", RobotApi::eyes(server.arg("q"))); }
void handleShutdown() { RobotApi::shutdown(); server.send(200, "text/plain", "ok"); }
void handleShutdownBySeconds() { RobotApi::shutdownBySeconds(server.arg("q")); server.send(200, "text/plain", "ok"); }
void handleShutdownByTime()    { RobotApi::shutdownByTime(server.arg("q"));    server.send(200, "text/plain", "ok"); }
void handleShutOn() { RobotApi::shutOn(); server.send(200, "text/plain", "ok"); }
void handleShutOnBySeconds() { RobotApi::shutOnBySeconds(server.arg("q")); server.send(200, "text/plain", "ok"); }
void handleShutOnByTime()    { RobotApi::shutOnByTime(server.arg("q"));    server.send(200, "text/plain", "ok"); }

void handleHelp() {
  String msg =
    "NoorRobot ESP32 API\n-------------------\n"
    "/forward?q=<1|0>&speed=<0-255>\n/backward?q=<1|0>&speed=<0-255>\n"
    "/right?q=<angle>\n/left?q=<angle>\n/stop\n/distance?q=<angle>\n"
    "/temperature?q=<unit>\n/temperature_esp32\n/fan?q=<on|off|status>\n"
    "/clear\n/eyes?q=<pattern>\n/shutdown\n/shutdownbyseconds?q=<s>\n"
    "/shutdownbytime?q=<t>\n/shuton\n/shutonbyseconds?q=<s>\n/shutonbytime?q=<t>\n"
    "/help\n\nAlso available: NoorShell on TCP port " + String(SHELL_PORT) + "\n";
  server.send(200, "text/plain", msg);
}

void setup() {
  Serial.begin(115200);
  RobotApi::begin();

  WifiManager::connectOrSetup();   // blocks until connected, or enters AP setup mode
  FsManager::begin();
  ShellServer::begin();
  TaskManager::begin(ShellServer::runCommand); // lets bg/close/kill re-enter the
                                                // shell command dispatcher on a
                                                // background FreeRTOS task

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
  Serial.println("Robot HTTP API on port 8083");
  Serial.println("NoorShell on port " + String(SHELL_PORT));

  // IP is printed on the ESP32's own main UART0 (TX0/RX0 -- the same
  // pins used for USB/programming), NOT on ArduinoSerial (UART2,
  // RX=16/TX=17 in robot_api.h) which is reserved for talking to the
  // companion Arduino board.
  Serial.println("========================================");
  Serial.println("ESP32 IP (main UART0 TX0/RX0): " + WiFi.localIP().toString());
  Serial.println("========================================");
}

void loop() {
  server.handleClient();
  ShellServer::loop();

  // Re-print the IP on main UART0 every 30s so it's always easy to find
  // on the ESP32's own serial line without needing to reset the board.
  static unsigned long lastIpPrint = 0;
  if (millis() - lastIpPrint >= 30000) {
    lastIpPrint = millis();
    Serial.println("IP: " + WiFi.localIP().toString());
  }
}
