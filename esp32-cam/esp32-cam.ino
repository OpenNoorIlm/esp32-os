#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

const char* ssid     = "Galaxy A32 7E93";
const char* password = "rlis4727";

// ── Port changed to 8082 to match visioning.py default ────────────────────────
// Python usage:  python visioning.py --ip <this-ip> --port 8082
WebServer server(8082);

// Camera pan/tilt servos
Servo servoPan;   // horizontal left/right
Servo servoTilt;  // vertical up/down

// GPIO 12 and 13 are free on AI-Thinker ESP32-CAM when SD card is not used.
const int PIN_SERVO_PAN  = 12;
const int PIN_SERVO_TILT = 13;

// ── Camera config (AI-Thinker ESP32-CAM) ──────────────────────────────────────
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = 5;
  config.pin_d1       = 18;
  config.pin_d2       = 19;
  config.pin_d3       = 21;
  config.pin_d4       = 36;
  config.pin_d5       = 39;
  config.pin_d6       = 34;
  config.pin_d7       = 35;
  config.pin_xclk     = 0;
  config.pin_pclk     = 22;
  config.pin_vsync    = 25;
  config.pin_href     = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn     = 32;
  config.pin_reset    = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;   // 640×480 — good balance for YOLO
  config.jpeg_quality = 10;              // slightly better quality for detection
  config.fb_count     = 2;              // double-buffer for smoother streaming
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
  }
}

// ── Take and send a single JPEG ───────────────────────────────────────────────
void sendPicture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ── MJPEG stream handler — required by visioning.py (/stream endpoint) ────────
//    visioning.py uses cv2.VideoCapture("http://<ip>:8082/stream")
//    which expects a standard multipart/x-mixed-replace MJPEG stream.
void handleStream() {
  // Tell the client we are sending a never-ending multipart stream
  WiFiClient client = server.client();

  String response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Frame capture failed, retrying…");
      delay(50);
      continue;
    }

    // Write the MJPEG part header
    client.printf(
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n"
      "\r\n",
      (unsigned)fb->len
    );

    // Write the JPEG payload
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    // Yield so the watchdog timer doesn't fire
    delay(1);
  }
}

// ── Route handlers ────────────────────────────────────────────────────────────
void handlePicture() {
  sendPicture();
}

void handlePictureByAngle() {
  String q = server.arg("q");   // "angleX,angleY"
  int comma = q.indexOf(',');
  int ax = q.substring(0, comma).toInt();
  int ay = q.substring(comma + 1).toInt();
  ax = constrain(ax, 0, 180);
  ay = constrain(ay, 0, 180);
  servoPan.write(ax);
  servoTilt.write(ay);
  delay(400);
  sendPicture();
}

void handleMoveCamera() {
  String q = server.arg("q");   // "angleX,angleY"
  int comma = q.indexOf(',');
  int ax = q.substring(0, comma).toInt();
  int ay = q.substring(comma + 1).toInt();
  ax = constrain(ax, 0, 180);
  ay = constrain(ay, 0, 180);
  servoPan.write(ax);
  servoTilt.write(ay);
  delay(400);
  server.send(200, "text/plain", "moved to " + String(ax) + "," + String(ay));
}

void handleTemperature() {
  float t = temperatureRead();
  server.send(200, "text/plain", String(t));
}

void setup() {
  Serial.begin(115200);

  servoPan.attach(PIN_SERVO_PAN);
  servoTilt.attach(PIN_SERVO_TILT);
  servoPan.write(90);
  servoTilt.write(90);

  initCamera();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  String ip = WiFi.localIP().toString();
  Serial.println("\nConnected: " + ip);
  Serial.println("─────────────────────────────────────────");
  Serial.println("  Endpoints:");
  Serial.println("  MJPEG stream : http://" + ip + ":8082/stream");
  Serial.println("  Snapshot     : http://" + ip + ":8082/picture");
  Serial.println("  Move camera  : http://" + ip + ":8082/move_camera?q=90,90");
  Serial.println("  Temperature  : http://" + ip + ":8082/temperature");
  Serial.println("─────────────────────────────────────────");
  Serial.println("  Python client:");
  Serial.println("  python visioning.py --ip " + ip + " --port 8082");
  Serial.println("─────────────────────────────────────────");

  // ── Register routes ───────────────────────────────────────────────────────
  server.on("/stream",           handleStream);          // ← NEW: MJPEG stream
  server.on("/picture",          handlePicture);
  server.on("/picture_by_angle", handlePictureByAngle);
  server.on("/move_camera",      handleMoveCamera);
  server.on("/temperature",      handleTemperature);

  server.begin();
  Serial.println("HTTP server started on port 8082");
}

void loop() {
  server.handleClient();
}
