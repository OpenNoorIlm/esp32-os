#include <SoftwareSerial.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── OLED ──────────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  32
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Pins ───────────────────────────────────────────────────────────────────────
SoftwareSerial mySerial(10, 11);  // RX=10 (from ESP32 TX=17), TX=11 (to ESP32 RX=16)

const int fanp     = 12;
const int servoPin = 13;   // distance-sweep servo

// L298N Channel A = RIGHT side motors (2 wheels, paralleled)
// L298N Channel B = LEFT side motors (2 wheels, paralleled)
// NOTE: right side motor is mirror-mounted vs left side, so its
// logical fwd/bwd pins are swapped in software to match rotation direction.
const int rightFwd =  5;   // Channel A IN2 -- swapped (was IN1/"forward")
const int rightBwd =  3;   // Channel A IN1 -- swapped (was IN2/"backward")
const int leftFwd  =  6;   // Channel B IN3 -- was "right_p"
const int leftBwd  =  9;   // Channel B IN4 -- was "left_p"

const int usd_trig =  4;
const int usd_echo =  7;

Servo myservo;

// ── State ──────────────────────────────────────────────────────────────────────
String recStr = "";
String fan    = "off";

// ── Ultrasonic ─────────────────────────────────────────────────────────────────
float getDistance() {
  digitalWrite(usd_trig, LOW);
  delayMicroseconds(2);
  digitalWrite(usd_trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(usd_trig, LOW);
  long duration = pulseIn(usd_echo, HIGH);
  return (duration * 0.0343f) / 2.0f;
}

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

// ── OLED eye drawing ───────────────────────────────────────────────────────────
void drawEyes(const String& type, int ox, int oy) {
  display.clearDisplay();
  int lx = 32 + ox, ly = 16 + oy;
  int rx = 96 + ox, ry = 16 + oy;

  if (type == F("Normal")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillCircle(lx+2, ly+2, 5, SSD1306_BLACK);
    display.fillCircle(rx+2, ry+2, 5, SSD1306_BLACK);
    display.fillCircle(lx+4, ly,   2, SSD1306_WHITE);
    display.fillCircle(rx+4, ry,   2, SSD1306_WHITE);

  } else if (type == F("Happy")) {
    // Draw full circles then mask top half to leave a bottom-arc (happy curve)
    display.fillCircle(lx, ly, 11, SSD1306_WHITE);
    display.fillCircle(rx, ry, 11, SSD1306_WHITE);
    display.fillRect(lx-12, ly-12, 24, 13, SSD1306_BLACK);
    display.fillRect(rx-12, ry-12, 24, 13, SSD1306_BLACK);

  } else if (type == F("Sad")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillTriangle(lx-12, ly-9, lx,    ly-9, lx-12, ly-3, SSD1306_BLACK);
    display.fillTriangle(rx+12, ry-9, rx,    ry-9, rx+12, ry-3, SSD1306_BLACK);
    display.fillCircle(lx-2, ly+2, 4, SSD1306_BLACK);
    display.fillCircle(rx+2, ry+2, 4, SSD1306_BLACK);

  } else if (type == F("Angry")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillTriangle(lx,    ly-9, lx+12, ly-9, lx+12, ly-3, SSD1306_BLACK);
    display.fillTriangle(rx-12, ry-9, rx,    ry-9, rx-12, ry-3, SSD1306_BLACK);
    display.fillCircle(lx, ly+1, 5, SSD1306_BLACK);
    display.fillCircle(rx, ry+1, 5, SSD1306_BLACK);

  } else if (type == F("Surprised")) {
    display.fillCircle(lx, ly, 12, SSD1306_WHITE);
    display.fillCircle(rx, ry, 12, SSD1306_WHITE);
    display.fillCircle(lx+1, ly+1, 6, SSD1306_BLACK);
    display.fillCircle(rx+1, ry+1, 6, SSD1306_BLACK);
    display.fillCircle(lx+3, ly-1, 2, SSD1306_WHITE);
    display.fillCircle(rx+3, ry-1, 2, SSD1306_WHITE);

  } else if (type == F("Cry")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillTriangle(lx-12, ly-9, lx,    ly-9, lx-12, ly-3, SSD1306_BLACK);
    display.fillTriangle(rx+12, ry-9, rx,    ry-9, rx+12, ry-3, SSD1306_BLACK);
    display.fillCircle(lx-2, ly+2, 4, SSD1306_BLACK);
    display.fillCircle(rx+2, ry+2, 4, SSD1306_BLACK);
    display.fillCircle(lx-2, ly+12, 3, SSD1306_WHITE);
    display.fillCircle(rx+2, ry+12, 3, SSD1306_WHITE);

  } else if (type == F("Love")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillCircle(lx-2, ly,   3, SSD1306_BLACK);
    display.fillCircle(lx+2, ly,   3, SSD1306_BLACK);
    display.fillTriangle(lx-5, ly+1, lx+5, ly+1, lx, ly+6, SSD1306_BLACK);
    display.fillCircle(rx-2, ry,   3, SSD1306_BLACK);
    display.fillCircle(rx+2, ry,   3, SSD1306_BLACK);
    display.fillTriangle(rx-5, ry+1, rx+5, ry+1, rx, ry+6, SSD1306_BLACK);

  } else if (type == F("Sleepy")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRect(lx-13, ly-10, 26, 11, SSD1306_BLACK);
    display.fillRect(rx-13, ry-10, 26, 11, SSD1306_BLACK);
    display.fillCircle(lx, ly+3, 4, SSD1306_BLACK);
    display.fillCircle(rx, ry+3, 4, SSD1306_BLACK);

  } else if (type == F("Confused")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillCircle(lx+2, ly+2, 5, SSD1306_BLACK);
    display.fillCircle(lx+4, ly,   2, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-3, 24, 8, 3, SSD1306_WHITE);
    display.drawPixel(rx+8, ry-7, SSD1306_WHITE);
    display.drawPixel(rx+8, ry-6, SSD1306_WHITE);

  } else if (type == F("Excited")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRect(lx-1, ly-5, 3, 10, SSD1306_BLACK);
    display.fillRect(lx-5, ly-1, 10, 3, SSD1306_BLACK);
    display.fillRect(rx-1, ry-5, 3, 10, SSD1306_BLACK);
    display.fillRect(rx-5, ry-1, 10, 3, SSD1306_BLACK);

  } else if (type == F("Dizzy")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.drawLine(lx-5, ly-5, lx+5, ly+5, SSD1306_BLACK);
    display.drawLine(lx+5, ly-5, lx-5, ly+5, SSD1306_BLACK);
    display.drawLine(rx-5, ry-5, rx+5, ry+5, SSD1306_BLACK);
    display.drawLine(rx+5, ry-5, rx-5, ry+5, SSD1306_BLACK);

  } else if (type == F("Bored")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRect(lx-13, ly-10, 26, 13, SSD1306_BLACK);
    display.fillRect(rx-13, ry-10, 26, 13, SSD1306_BLACK);
    display.fillRect(lx-5, ly+1, 10, 3, SSD1306_BLACK);
    display.fillRect(rx-5, ry+1, 10, 3, SSD1306_BLACK);

  } else if (type == F("Evil")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillTriangle(lx,    ly-9, lx+12, ly-9, lx+12, ly,   SSD1306_BLACK);
    display.fillTriangle(rx-12, ry-9, rx,    ry-9, rx-12, ry,   SSD1306_BLACK);
    display.fillCircle(lx, ly+2, 4, SSD1306_BLACK);
    display.fillCircle(rx, ry+2, 4, SSD1306_BLACK);
    display.fillCircle(lx+2, ly, 2, SSD1306_WHITE);
    display.fillCircle(rx+2, ry, 2, SSD1306_WHITE);

  } else if (type == F("Shy")) {
    display.fillRoundRect(lx-10, ly-7, 20, 14, 5, SSD1306_WHITE);
    display.fillRoundRect(rx-10, ry-7, 20, 14, 5, SSD1306_WHITE);
    display.fillCircle(lx, ly, 4, SSD1306_BLACK);
    display.fillCircle(rx, ry, 4, SSD1306_BLACK);
    display.drawPixel(lx-4, ly+9, SSD1306_WHITE);
    display.drawPixel(lx-2, ly+10, SSD1306_WHITE);
    display.drawPixel(lx,   ly+10, SSD1306_WHITE);
    display.drawPixel(lx+2, ly+10, SSD1306_WHITE);
    display.drawPixel(lx+4, ly+9, SSD1306_WHITE);
    display.drawPixel(rx-4, ry+9, SSD1306_WHITE);
    display.drawPixel(rx-2, ry+10, SSD1306_WHITE);
    display.drawPixel(rx,   ry+10, SSD1306_WHITE);
    display.drawPixel(rx+2, ry+10, SSD1306_WHITE);
    display.drawPixel(rx+4, ry+9, SSD1306_WHITE);

  } else if (type == F("Cool")) {
    display.fillRoundRect(lx-12, ly-6, 24, 14, 4, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-6, 24, 14, 4, SSD1306_WHITE);
    display.fillRect(lx+12, ly-3, rx-lx-12, 6, SSD1306_WHITE);
    display.fillRoundRect(lx-11, ly-5, 22, 12, 3, SSD1306_BLACK);
    display.fillRoundRect(rx-11, ry-5, 22, 12, 3, SSD1306_BLACK);
    display.drawLine(lx-7, ly-3, lx-3, ly-3, SSD1306_WHITE);
    display.drawLine(rx-7, ry-3, rx-3, ry-3, SSD1306_WHITE);

  } else if (type == F("Wink")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillCircle(lx+2, ly+2, 5, SSD1306_BLACK);
    display.fillCircle(lx+4, ly,   2, SSD1306_WHITE);
    display.drawLine(rx-10, ry,   rx,    ry-4, SSD1306_WHITE);
    display.drawLine(rx,    ry-4, rx+10, ry,   SSD1306_WHITE);
    display.drawLine(rx-10, ry+1, rx,    ry-3, SSD1306_WHITE);
    display.drawLine(rx,    ry-3, rx+10, ry+1, SSD1306_WHITE);

  } else if (type == F("Dead")) {
    display.drawLine(lx-8, ly-8, lx+8, ly+8, SSD1306_WHITE);
    display.drawLine(lx+8, ly-8, lx-8, ly+8, SSD1306_WHITE);
    display.drawLine(lx-9, ly-8, lx+9, ly+8, SSD1306_WHITE);
    display.drawLine(lx+9, ly-8, lx-9, ly+8, SSD1306_WHITE);
    display.drawLine(rx-8, ry-8, rx+8, ry+8, SSD1306_WHITE);
    display.drawLine(rx+8, ry-8, rx-8, ry+8, SSD1306_WHITE);
    display.drawLine(rx-9, ry-8, rx+9, ry+8, SSD1306_WHITE);
    display.drawLine(rx+9, ry-8, rx-9, ry+8, SSD1306_WHITE);

  } else if (type == F("Nervous")) {
    display.fillRoundRect(lx-12, ly-9, 24, 18, 6, SSD1306_WHITE);
    display.fillRoundRect(rx-12, ry-9, 24, 18, 6, SSD1306_WHITE);
    display.fillCircle(lx-3, ly-2, 5, SSD1306_BLACK);
    display.fillCircle(rx+3, ry-2, 5, SSD1306_BLACK);
    display.fillCircle(lx-1, ly-4, 2, SSD1306_WHITE);
    display.fillCircle(rx+5, ry-4, 2, SSD1306_WHITE);
    display.fillCircle(lx+10, ly+11, 2, SSD1306_WHITE);
  }

  display.display();
}

void clearDisplay_() {
  display.clearDisplay();
  display.display();
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
  pinMode(usd_trig, OUTPUT);
  pinMode(usd_echo, INPUT);

  myservo.attach(servoPin);
  mySerial.begin(9600);
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 not found — continuing without OLED"));
    // while (true);
  }
  display.clearDisplay();
  display.display();
  drawEyes("Normal", 0, 0);
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

  // ── Temperature ────────────────────────────────────────────────────────────
  if (in(recStr, "temperature(")) {
    mySerial.println("no_sensor");
  }

  // ── Distance ───────────────────────────────────────────────────────────────
  if (in(recStr, "distance(")) {
    int angle = extractMatch(recStr, 1).toInt();
    myservo.write(angle);
    delay(300);
    float dist = getDistance();
    mySerial.println(dist);
    delay(200);
    myservo.write(0);
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
