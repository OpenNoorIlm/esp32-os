#pragma once
// ── sensor_manager.h ──────────────────────────────────────────────────────────
// Reads all sensors now on ESP32 (moved from Arduino + new ones)
// Feeds TftManager::updateSensors() and provides data to robot_api / Lua
//
// Sensors handled:
//   DHT11        — temperature + humidity (pin 32)
//   Ultrasonic   — distance (trig 13, echo 14)
//   Servo        — sweep for distance scan (pin 12)
//   Avoid (IR)   — obstacle detection (pin 33)
//   Tracking     — line sensor (pin 34)
//   Flame        — fire detection (pin 35)
//   Photoresistor— light level analog (pin 36)
//   Sound        — sound level analog (pin 39)
//   Hall         — magnetic field (pin 25)
//   Laser        — tripwire digital out (pin 26)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <DHT.h>
#include <ESP32Servo.h>

// Pin definitions (must match tft_manager.h)
#define SM_DHT_PIN      32
#define SM_AVOID_PIN    33
#define SM_TRACK_PIN    34
#define SM_FLAME_PIN    35
#define SM_LIGHT_PIN    36   // analog
#define SM_SOUND_PIN    39   // analog
#define SM_HALL_PIN     21  // GPIO21 — moved from GPIO25 (DAC conflict) and GPIO34 (TRACK conflict)
#define SM_LASER_PIN    26
#define SM_USD_TRIG     13
#define SM_USD_ECHO     14
#define SM_SERVO_PIN    12

#define SM_DHT_TYPE DHT11

namespace SensorManager {

// ── Hardware objects ──────────────────────────────────────────────────────────
#ifdef SENSOR_MANAGER_IMPLEMENTATION
DHT   _dht(SM_DHT_PIN, SM_DHT_TYPE);
Servo _servo;

// ── Cached readings ───────────────────────────────────────────────────────────
float _tempC      = 0;
float _tempF      = 0;
float _humidity   = 0;
float _distance   = 0;
int   _lightLevel = 0;   // 0-4095
int   _soundLevel = 0;   // 0-4095
bool  _obstacle   = false;
bool  _tracking   = false;  // true = on line
bool  _flame      = false;
bool  _magnetic   = false;  // hall sensor triggered
bool  _tripwire   = false;  // laser broken
bool  _laserOn    = false;  // laser emitter state

unsigned long _lastDhtRead    = 0;
unsigned long _lastSlowRead   = 0;
unsigned long _lastFastRead   = 0;
#else
extern DHT   _dht;
extern Servo _servo;
extern float _tempC, _tempF, _humidity, _distance;
extern int _lightLevel, _soundLevel;
extern bool _obstacle, _tracking, _flame, _magnetic, _tripwire, _laserOn;
extern unsigned long _lastDhtRead, _lastSlowRead, _lastFastRead;
#endif

// ── Init ──────────────────────────────────────────────────────────────────────
inline void begin() {
  _dht.begin();

  pinMode(SM_AVOID_PIN,  INPUT);
  pinMode(SM_TRACK_PIN,  INPUT);
  pinMode(SM_FLAME_PIN,  INPUT);
  pinMode(SM_HALL_PIN,   INPUT);
  pinMode(SM_LASER_PIN,  OUTPUT);
  pinMode(SM_USD_TRIG,   OUTPUT);
  pinMode(SM_USD_ECHO,   INPUT);

  _servo.attach(SM_SERVO_PIN);
  _servo.write(90); // center

  // Laser off by default
  digitalWrite(SM_LASER_PIN, LOW);
  _laserOn = false;
}

// ── Ultrasonic distance ───────────────────────────────────────────────────────
inline float measureDistance() {
  digitalWrite(SM_USD_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(SM_USD_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(SM_USD_TRIG, LOW);
  long dur = pulseIn(SM_USD_ECHO, HIGH, 30000); // 30ms timeout
  return (dur * 0.0343f) / 2.0f;
}

// ── Sweep servo and get distance at angle ─────────────────────────────────────
inline float distanceAtAngle(int angle) {
  _servo.write(constrain(angle, 0, 180));
  delay(300);
  float d = measureDistance();
  delay(100);
  _servo.write(90);
  return d;
}

// ── Laser control ─────────────────────────────────────────────────────────────
inline void setLaser(bool on) {
  _laserOn = on;
  digitalWrite(SM_LASER_PIN, on ? HIGH : LOW);
}

inline bool laserOn() { return _laserOn; }

// ── Servo control ─────────────────────────────────────────────────────────────
inline void setServo(int angle) {
  _servo.write(constrain(angle, 0, 180));
}

// ── Fast read (obstacle, flame, tracking, hall) — call every 50ms ────────────
inline void readFast() {
  unsigned long now = millis();
  if (now - _lastFastRead < 50) return;
  _lastFastRead = now;

  _obstacle = (digitalRead(SM_AVOID_PIN) == LOW);  // LOW = obstacle detected
  _flame    = (digitalRead(SM_FLAME_PIN) == LOW);   // LOW = flame detected
  _tracking = (digitalRead(SM_TRACK_PIN) == HIGH);  // HIGH = on line
  _magnetic = (digitalRead(SM_HALL_PIN) == LOW);    // LOW = magnetic field
}

// ── Slow read (analog sensors + distance) — call every 500ms ─────────────────
inline void readSlow() {
  unsigned long now = millis();
  if (now - _lastSlowRead < 500) return;
  _lastSlowRead = now;

  // Avoid mixing the legacy analog API with the current ESP32 ADC driver;
  // that combination aborts on startup with "ADC: CONFLICT".
  _lightLevel = 0;
  _soundLevel = 0;
  _distance   = measureDistance();
}

// ── DHT read — call every 2 seconds (DHT11 is slow) ─────────────────────────
inline void readDht() {
  unsigned long now = millis();
  if (now - _lastDhtRead < 2000) return;
  _lastDhtRead = now;

  float t = _dht.readTemperature();
  float h = _dht.readHumidity();
  if (!isnan(t)) { _tempC = t; _tempF = t * 9.0f / 5.0f + 32.0f; }
  if (!isnan(h))   _humidity = h;
}

// ── Main loop — call every loop() ────────────────────────────────────────────
inline void loop() {
  readFast();
  readSlow();
  readDht();
}

// ── Getters ───────────────────────────────────────────────────────────────────
inline float tempC()      { return _tempC; }
inline float tempF()      { return _tempF; }
inline float humidity()   { return _humidity; }
inline float distance()   { return _distance; }
inline int   lightLevel() { return _lightLevel; }
inline int   soundLevel() { return _soundLevel; }
inline bool  obstacle()   { return _obstacle; }
inline bool  tracking()   { return _tracking; }
inline bool  flame()      { return _flame; }
inline bool  magnetic()   { return _magnetic; }

// Percentage helpers
inline int lightPercent() { return map(_lightLevel, 0, 4095, 0, 100); }
inline int soundPercent() { return map(_soundLevel, 0, 4095, 0, 100); }

// ── Shell command handler ─────────────────────────────────────────────────────
inline String shellCmd(const String& args) {
  if (args == "temp" || args == "temperature") {
    return "Temperature: " + String(_tempC, 1) + "C / " + String(_tempF, 1) + "F\n"
           "Humidity: " + String(_humidity, 1) + "%\n";
  }
  if (args == "distance" || args.startsWith("distance ")) {
    int angle = 90;
    int sp = args.indexOf(' ');
    if (sp > 0) angle = args.substring(sp + 1).toInt();
    float d = distanceAtAngle(angle);
    return String(d, 1) + " cm\n";
  }
  if (args == "light")    return "Light: " + String(lightPercent()) + "%\n";
  if (args == "sound")    return "Sound: " + String(soundPercent()) + "%\n";
  if (args == "obstacle") return String(_obstacle ? "Obstacle detected\n" : "Clear\n");
  if (args == "flame")    return String(_flame ? "FLAME DETECTED!\n" : "No flame\n");
  if (args == "tracking") return String(_tracking ? "On line\n" : "Off line\n");
  if (args == "magnetic") return String(_magnetic ? "Magnetic field detected\n" : "No field\n");
  if (args.startsWith("laser ")) {
    String val = args.substring(6);
    val.trim();
    if (val == "on")  { setLaser(true);  return "Laser ON\n"; }
    if (val == "off") { setLaser(false); return "Laser OFF\n"; }
  }
  if (args.startsWith("servo ")) {
    int angle = args.substring(6).toInt();
    setServo(angle);
    return "Servo -> " + String(angle) + " deg\n";
  }
  if (args == "all" || args.isEmpty()) {
    return "── Sensors ──\n"
           "Temp:     " + String(_tempC, 1) + "C / " + String(_tempF, 1) + "F\n"
           "Humidity: " + String(_humidity, 1) + "%\n"
           "Distance: " + String(_distance, 1) + " cm\n"
           "Light:    " + String(lightPercent()) + "%\n"
           "Sound:    " + String(soundPercent()) + "%\n"
           "Obstacle: " + String(_obstacle ? "YES" : "no") + "\n"
           "Flame:    " + String(_flame ? "YES!" : "no") + "\n"
           "Tracking: " + String(_tracking ? "on line" : "off line") + "\n"
           "Magnetic: " + String(_magnetic ? "YES" : "no") + "\n"
           "Laser:    " + String(_laserOn ? "ON" : "off") + "\n";
  }
  return "Usage: sensor [temp|distance [angle]|light|sound|obstacle|flame|tracking|magnetic|laser on/off|servo <angle>|all]\n";
}

} // namespace SensorManager
