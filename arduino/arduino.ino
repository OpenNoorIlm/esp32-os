#include <SoftwareSerial.h>

// ── Pins ───────────────────────────────────────────────────────────────────────
SoftwareSerial mySerial(10, 11);  // RX=10 (from ESP32 TX=17), TX=11 (to ESP32 RX=16)

const int fanp     = 12;

// L298N Channel A = RIGHT side motors (2 wheels, paralleled)
// L298N Channel B = LEFT side motors (2 wheels, paralleled)
// NOTE: right side motor is mirror-mounted vs left side, so its
// logical fwd/bwd pins are swapped in software to match rotation direction.
const int rightFwd =  5;   // Channel A IN2 -- swapped (was IN1/"forward")
const int rightBwd =  3;   // Channel A IN1 -- swapped (was IN2/"backward")
const int leftFwd  =  6;   // Channel B IN3 -- was "right_p"
const int leftBwd  =  9;   // Channel B IN4 -- was "left_p"

// ── State ──────────────────────────────────────────────────────────────────────
String recStr = "";
String fan    = "off";

// ── String helpers ─────────────────────────────────────────────────────────────
String extractMatch(const String &src, int index) {
  if (index <= 0) return "";
  int openPos  = src.indexOf('(');
  int closePos = src.lastIndexOf(')');
  if (openPos == -1 || closePos == -1 || closePos <= openPos) return "";
  String inner = src.substring(openPos + 1, closePos);
  int start = 0, curIdx = 1;
  while (true) {
    int commaPos  = inner.indexOf(',', start);
    bool lastToken = (commaPos == -1);
    if (curIdx == index)
      return lastToken ? inner.substring(start) : inner.substring(start, commaPos);
    if (lastToken) break;
    start = commaPos + 1;
    ++curIdx;
  }
  return "";
}

bool in(String str, String sub) {
  return str.indexOf(sub) != -1;
}

// ── Eyes (now handled by ESP32 TFT) ───────────────────────────────────────────
// Eyes and display are now handled by ESP32 TFT — these are stubs so
// the serial protocol stays unchanged and ESP32 still gets "ok" replies.
void drawEyes(const String& type, int ox, int oy) {
  // no-op: ESP32 renders eyes on TFT
}

void clearDisplay_() {
  // no-op: ESP32 clears TFT
}

// ── Stop all motors ────────────────────────────────────────────────────────────
void stopMotors() {
  analogWrite(rightFwd, 0);
  analogWrite(rightBwd, 0);
  analogWrite(leftFwd,  0);
  analogWrite(leftBwd,  0);
}

void replyOk() {
  mySerial.println("ok");
}

// ── Setup ──────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(rightFwd, OUTPUT);
  pinMode(rightBwd, OUTPUT);
  pinMode(leftFwd,  OUTPUT);
  pinMode(leftBwd,  OUTPUT);
  pinMode(fanp,     OUTPUT);

  mySerial.begin(9600);
  Serial.begin(9600);
  Serial.println(F("Arduino motor controller ready"));
}

// ── Loop ───────────────────────────────────────────────────────────────────────
void loop() {
  String received = mySerial.readStringUntil('\n');
  if (received.length() > 0) {
    recStr = received;
    recStr.trim();
    Serial.println("Got: " + recStr);
  }

  // ── Fan ────────────────────────────────────────────────────────────────────
  if (recStr == "fan on") {
    digitalWrite(fanp, HIGH);
    fan = "on";
    replyOk();
  }
  if (recStr == "fan off") {
    digitalWrite(fanp, LOW);
    fan = "off";
    replyOk();
  }
  if (recStr == "fan status") {
    mySerial.println(fan);
  }

  // ── Movement ───────────────────────────────────────────────────────────────
  if (in(recStr, "forward(")) {
    int duration = extractMatch(recStr, 1).toInt();
    int speed    = extractMatch(recStr, 2).toInt();
    if (speed == 0) speed = 255;
    if (duration <= 0 || duration > 300) duration = 10; // sanity clamp: max 30s
    stopMotors();
    analogWrite(rightFwd, speed);
    analogWrite(leftFwd,  speed);
    delay(duration * 100);
    stopMotors();
    replyOk();
  }
  if (in(recStr, "backward(")) {
    int duration = extractMatch(recStr, 1).toInt();
    int speed    = extractMatch(recStr, 2).toInt();
    if (speed == 0) speed = 255;
    if (duration <= 0 || duration > 300) duration = 10; // sanity clamp: max 30s
    stopMotors();
    analogWrite(rightBwd, speed);
    analogWrite(leftBwd,  speed);
    delay(duration * 100);
    stopMotors();
    replyOk();
  }
  if (in(recStr, "right(")) {
    // Tank turn right: left side forward, right side backward.
    // angle (degrees) is converted to a drive duration using MS_PER_DEGREE --
    // test with e.g. "robot right 90" and measure actual rotation, then
    // adjust MS_PER_DEGREE below until 90 in = ~90 degrees actual turn.
    const float MS_PER_DEGREE = 6.0;
    const int   TURN_SPEED    = 200; // fixed -- low PWM may not overcome motor friction
    int angle = extractMatch(recStr, 1).toInt();
    int turnMs = (int)(angle * MS_PER_DEGREE);
    stopMotors();
    analogWrite(leftFwd,  TURN_SPEED);
    analogWrite(rightBwd, TURN_SPEED);
    delay(turnMs);
    stopMotors();
    replyOk();
  }
  if (in(recStr, "left(")) {
    // Tank turn left: left side backward, right side forward.
    const float MS_PER_DEGREE = 6.0;
    const int   TURN_SPEED    = 200;
    int angle = extractMatch(recStr, 1).toInt();
    int turnMs = (int)(angle * MS_PER_DEGREE);
    stopMotors();
    analogWrite(leftBwd,  TURN_SPEED);
    analogWrite(rightFwd, TURN_SPEED);
    delay(turnMs);
    stopMotors();
    replyOk();
  }
  if (in(recStr, "stop(")) {
    stopMotors();
    replyOk();
  }

  // ── Eyes ───────────────────────────────────────────────────────────────────
  if (in(recStr, "eyes(")) {
    String eyeType = extractMatch(recStr, 1);
    int ex = extractMatch(recStr, 2).toInt();
    int ey = extractMatch(recStr, 3).toInt();
    drawEyes(eyeType, ex, ey);
    replyOk();
  }
  if (in(recStr, "clear(")) {
    clearDisplay_();
    replyOk();
  }

  // ── Shutdown / power ───────────────────────────────────────────────────────
  if (in(recStr, "shutdown("))          { replyOk(); }
  if (in(recStr, "shutdownbyseconds(")) { replyOk(); }
  if (in(recStr, "shutdownbytime("))    { replyOk(); }
  if (in(recStr, "shuton("))            { replyOk(); }
  if (in(recStr, "shutonbyseconds("))   { replyOk(); }
  if (in(recStr, "shutonbytime("))      { replyOk(); }

  recStr = "";
}
