#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

// All the original robot-control logic, refactored into plain functions so
// they can be called from BOTH the existing HTTP API (unchanged, port 8083)
// and the new "robot" shell command -- one implementation, two front-ends.
namespace RobotApi {

inline HardwareSerial ArduinoSerial(2); // RX=16, TX=17

inline void begin() {
  ArduinoSerial.begin(9600, SERIAL_8N1, 16, 17);
}

inline void toArduino(const String& cmd) { ArduinoSerial.println(cmd); }

inline String sanitizeReply(const String& raw) {
  String clean;
  clean.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if ((c >= 32 && c <= 126) || c == '\t') clean += c;
  }
  clean.trim();
  return clean.length() ? clean : "error: garbled reply from Arduino";
}

inline String waitReply(unsigned long timeoutMs = 2000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (ArduinoSerial.available()) {
      return sanitizeReply(ArduinoSerial.readStringUntil('\n'));
    }
  }
  return "timeout";
}

} // namespace RobotApi

namespace RobotApi {

inline String forward(String q, String speed) {
  if (q.length() == 0) q = "1";
  if (speed.length() == 0) speed = "255";
  toArduino("forward(" + q + "," + speed + ")");
  long timeoutMs = q.toInt() * 100L + 1000L; // duration the Arduino will delay, plus margin
  return waitReply(timeoutMs);
}
inline String backward(String q, String speed) {
  if (q.length() == 0) q = "1";
  if (speed.length() == 0) speed = "255";
  toArduino("backward(" + q + "," + speed + ")");
  long timeoutMs = q.toInt() * 100L + 1000L;
  return waitReply(timeoutMs);
}
inline String right(String angle) { toArduino("right(" + angle + ")"); return waitReply(); }
inline String left(String angle)  { toArduino("left(" + angle + ")");  return waitReply(); }
inline String stop()              { toArduino("stop(1)");              return waitReply(); }
inline String distance(String angle)   { toArduino("distance(" + angle + ")");    return waitReply(2000); }
inline String temperature(String unit) { toArduino("temperature(" + unit + ")"); return waitReply(2000); }
inline String fan(String q) {
  if (q == "on") toArduino("fan on");
  else if (q == "off") toArduino("fan off");
  else if (q == "status") { toArduino("fan status"); return waitReply(); }
  return "ok";
}
inline String clearCmd() { toArduino("clear(1)"); return waitReply(); }
inline String eyes(String q) { toArduino("eyes(" + q + ")"); return waitReply(); }
inline void shutdown() { toArduino("shutdown(True)"); }
inline void shutdownBySeconds(String q) { toArduino("shutdownbyseconds(" + q + ")"); }
inline void shutdownByTime(String q)    { toArduino("shutdownbytime(" + q + ")"); }
inline void shutOn() { toArduino("shuton(True)"); }
inline void shutOnBySeconds(String q) { toArduino("shutonbyseconds(" + q + ")"); }
inline void shutOnByTime(String q)    { toArduino("shutonbytime(" + q + ")"); }

} // namespace RobotApi

namespace RobotApi {

// Parses a shell line like "robot forward 1 255" or "robot distance 90"
// into the same underlying calls the HTTP API uses.
inline String shellCommand(const String& rest) {
  int sp = rest.indexOf(' ');
  String sub  = sp == -1 ? rest : rest.substring(0, sp);
  String args = sp == -1 ? ""   : rest.substring(sp + 1);
  int sp2 = args.indexOf(' ');
  String a1 = sp2 == -1 ? args : args.substring(0, sp2);
  String a2 = sp2 == -1 ? ""   : args.substring(sp2 + 1);

  if (sub == "forward")     return forward(a1, a2);
  if (sub == "backward")    return backward(a1, a2);
  if (sub == "right")       return right(a1);
  if (sub == "left")        return left(a1);
  if (sub == "stop")        return stop();
  if (sub == "distance")    return distance(a1);
  if (sub == "temperature") return temperature(a1);
  if (sub == "fan")         return fan(a1);
  if (sub == "clear")       return clearCmd();
  if (sub == "eyes") {
    // eyes() on the Arduino side expects one comma-joined string,
    // "type,x,y" -- but the shell only ever split off a1/a2 above, so an
    // x/y offset typed as "robot eyes Happy 10 20" was silently dropped
    // (a2 held "10 20" as one unsplit chunk, a3 never existed). Split a2
    // once more here so all three positional args actually reach the eye
    // renderer instead of just the expression name.
    int sp3 = a2.indexOf(' ');
    String ex = sp3 == -1 ? a2 : a2.substring(0, sp3);
    String ey = sp3 == -1 ? ""  : a2.substring(sp3 + 1);
    String q = a1;
    if (ex.length()) q += "," + ex;
    if (ey.length()) q += "," + ey;
    return eyes(q);
  }
  if (sub == "shutdown")    { shutdown(); return "ok"; }
  if (sub == "shuton")      { shutOn();   return "ok"; }
  return "error: unknown robot subcommand '" + sub + "'";
}

} // namespace RobotApi
