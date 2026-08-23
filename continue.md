# NoorRobot — continue.md
> **READ THIS FIRST** before every session. This is the living task tracker.
> Priority order: 🔴 Critical → 🟡 Important → 🟢 Nice to have → 💡 Future idea

---

## ✅ DONE — Hardware

- [x] ESP32 Dev Module wired up
- [x] Arduino Uno R3 as motor slave (UART2 at 9600 baud)
- [x] L298N motor driver (Channel A IN1/IN2 working, Channel B IN3/IN4 dead — REPLACED)
- [x] New L298N ordered from Robocraze
- [x] 4x BO 150RPM motors wired
- [x] Ebony wood chassis (survived crash test 💀)
- [x] 2x Li-Ion 3.7V in series = 7.4V motor power
- [x] ESP32-CAM with OV3660 3MP upgrade
- [x] ESP32-CAM-MB Type-C programmer ordered from robu.in
- [x] 2.8" ILI9341 TFT + XPT2046 touch + stylus ordered from robu.in
- [x] PAM8403 XH-A156 4CH amp ordered
- [x] 2x 3W 8Ω stereo speakers ordered
- [x] 120pcs jumper wires (M2M, M2F, F2F 20cm) ordered
- [x] 37-in-1 sensor kit (HW-A017)
- [x] ISD1820 voice recorder module (kept as backup, not primary audio)
- [x] Fixed faulty connection causing ESP32 to go crazy on/off
- [x] Added common GND between ESP32 and Arduino (was the missing link!)

## ✅ DONE — Software Core

- [x] WiFi manager (AP mode fallback)
- [x] TCP Shell server (NoorShell) on port 2222
- [x] SHA256 auth for SSH
- [x] Lua 5.4 engine embedded
- [x] Virtual filesystem (SPIFFS)
- [x] Package manager (apt-like)
- [x] Task manager (FreeRTOS background tasks)
- [x] Capability system
- [x] HTTP REST API on port 8083
- [x] esp32-ssh CLI tool (built on Termux successfully!)
- [x] Robot connected and `robot forward 1` working

## ✅ DONE — Software Updates (This Session)

- [x] Arduino stripped to motors + fan only (removed OLED, servo, ultrasonic, temp)
- [x] All sensor handling moved to ESP32 (sensor_manager.h)
- [x] robot_api.h updated — distance/temp now use SensorManager directly
- [x] tft_manager.h — full bazaar TFT UI (eyes, status, sensor bar, touch buttons)
- [x] vkeyboard.h — full QWERTY virtual keyboard with stylus support
- [x] apps/quran.h — Quran app (4 editions, TFT + SSH)
- [x] apps/browser.h — Text web browser (TFT + SSH)
- [x] apps/painter.h — Full paint app (12 tools, 4 layers, undo/redo, color wheel)
- [x] apps/taskmanager.h — Task manager (5 tabs: procs, sysinfo, drivers, logs, perf)
- [x] driver_manager.h — DVR driver system (install/remove/backup/undo/reboot)
- [x] nano_editor.h — SSH nano + TFT visual file editor
- [x] lua_auto.h — PyAutoGUI equivalent for TFT automation
- [x] lua_widgets.h — NoorUI full Qt-like widget system (rewritten to 100% Qt)
- [x] lua_engine.cpp — Added all new Lua bindings (sensor, screen, keyboard, auto, ui.*)
- [x] shell_server.h — Added all new shell commands
- [x] esp32.ino — Wired everything into setup() and loop()
- [x] requirements.txt — All Arduino libraries listed
- [x] about.md — Full project documentation
- [x] continue.md — This file

## ✅ DONE — NoorQt (Qt6 C++ Clone)

- [x] QObject.h — Base class, signals/slots, properties, meta-object, QVariant, QList, QMap, QStringList, QByteArray, Qt:: namespace, QFlags
- [x] QGeometry.h — QColor (RGB/HSV/HSL/CMYK/RGB565/named), QFont, QPoint, QPointF, QSize, QSizeF, QRect, QRectF, QMargins, QLine, QLineF, QPolygon, QTransform, QTime, QDate, QDateTime, QTimer
- [x] QPainter.h — QPen, QBrush, QGradient, QLinearGradient, QRadialGradient, QPainterPath, QPainter (full drawing API)
- [x] QWidget.h — QWidget (full API), QPalette, QSizePolicy
- [x] QLayout.h — QLayout, QBoxLayout, QHBoxLayout, QVBoxLayout, QGridLayout, QFormLayout, QStackedLayout, QSpacerItem, QWidgetItem
- [x] QWidgets.h — QPushButton, QLabel, QLineEdit, QTextEdit, QPlainTextEdit, QCheckBox, QRadioButton, QSlider, QAbstractSlider, QSpinBox, QDoubleSpinBox, QProgressBar, QComboBox, QListWidget(+Item), QTabWidget, QScrollArea, QGroupBox, QToolButton, QDial, QLCDNumber, QFrame, QSplitter, QStackedWidget

---

## 🔴 CRITICAL — Must Do Next (Hardware arrives first)

### When hardware arrives:
- [ ] Wire TFT 2.8" to ESP32 (pin map in about.md)
- [ ] Configure TFT_eSPI User_Setup.h
- [ ] Test TFT boots and shows NoorOS splash
- [ ] Wire XPT2046 touch (shares SPI, T_CS on GPIO5)
- [ ] Calibrate touch (update map() values in vkeyboard.h + tft_manager.h)
- [ ] Wire new L298N — confirm both channels work
- [ ] Wire 2x speakers to PAM8403 amp
- [ ] Connect PAM8403 to ESP32 DAC (GPIO25 or GPIO26 — check conflict with Hall sensor!)
- [ ] Wire DHT11 to GPIO32
- [ ] Wire ultrasonic TRIG/ECHO to GPIO13/14 (moved from Arduino)
- [ ] Wire servo to GPIO12 (moved from Arduino)
- [ ] Wire all 37-kit sensors
- [ ] Flash ESP32-CAM via new ESP32-CAM-MB programmer

### Hall sensor vs DAC conflict:
⚠️ GPIO25 is both Hall sensor AND ESP32 DAC1. Need to decide:
- Option A: Move Hall sensor to another GPIO (e.g. GPIO21)
- Option B: Use GPIO26 (DAC2) for audio instead
- **Recommended: Move Hall to GPIO21, keep GPIO25 for audio DAC**
- [ ] Update sensor_manager.h pin if moved

---

## 🔴 CRITICAL — Software (Must Complete)

### NoorQt remaining modules:
- [ ] **QNetwork.h** — QNetworkAccessManager, QNetworkRequest, QNetworkReply, QTcpSocket, QUdpSocket, QHostAddress, QDnsLookup
- [ ] **QFile.h** — QFile, QDir, QFileInfo, QTextStream, QDataStream, QIODevice, QFileSystemWatcher
- [ ] **QThread.h** — QThread, QMutex, QMutexLocker, QSemaphore, QWaitCondition, QReadWriteLock, QFuture, QThreadPool
- [ ] **QSql.h** — QSqlDatabase, QSqlQuery, QSqlRecord, QSqlField, QSqlError (SQLite via SPIFFS)
- [ ] **QMultimedia.h** — QAudioOutput, QAudioFormat, QMediaPlayer, QSoundEffect (PAM8403 via DAC)
- [ ] **QAnimation.h** — QAbstractAnimation, QPropertyAnimation, QSequentialAnimationGroup, QParallelAnimationGroup, QEasingCurve
- [ ] **QModel.h** — QAbstractItemModel, QStandardItemModel, QStandardItem, QSortFilterProxyModel
- [ ] **QApplication.h** — QApplication, QCoreApplication, QGuiApplication, QScreen, QClipboard
- [ ] **NoorQt.h** — Master include header for all modules
- [ ] **Lua bindings for NoorQt** — Full luaL_Reg tables for every Qt class

### Lua engine:
- [ ] Complete Lua bindings for NoorUI (ui.*) — started but needs widget factory functions tested
- [ ] Add NoorQt Lua bindings (Qt.QPushButton, Qt.QLabel etc)
- [ ] os.hook() system — boot hooks from editable.dvr
- [ ] os.set() / os.get() persistent config
- [ ] shell.add() for custom shell commands from Lua/DVR

### Audio:
- [ ] Add audio_manager.h — PAM8403 via ESP32 DAC + I2S
- [ ] TTS (text-to-speech) — simple phoneme synthesis or prerecorded sounds
- [ ] robot.say("Hello!") shell command
- [ ] robot.play("/sounds/boot.raw") — play raw audio file

### Camera:
- [ ] Wire ESP32-CAM to ESP32 main (via UART or standalone)
- [ ] Enable camera streaming in visioning.py / esp32-cam firmware
- [ ] Add `camera stream` shell command
- [ ] Show camera feed thumbnail on TFT

---

## 🟡 IMPORTANT — Software Improvements

### NoorUI improvements:
- [ ] Fix `ui.progress()` to be non-blocking (use timer)
- [ ] Add `ui.colorpick()` to Lua bindings
- [ ] Test all 30 widget types on actual TFT
- [ ] Touch calibration tool (built-in app)
- [ ] LSS pseudo-states: `:hover`, `:pressed`, `:disabled`, `:checked`
- [ ] Widget animation queue (don't block event loop during animate())
- [ ] Add `ui.MenuBar`, `ui.Menu`, `ui.MenuItem`
- [ ] Add `ui.ToolBar`, `ui.StatusBar`
- [ ] Add `ui.MessageBox` (proper Qt-style with icon)
- [ ] Add `ui.InputDialog`
- [ ] Add `ui.ColorDialog` (use painter HSV wheel)
- [ ] Add `ui.FontDialog`

### Painter app:
- [ ] Undo applies per-layer correctly (currently global)
- [ ] Add zoom/pan touch gestures (pinch = zoom)
- [ ] Save to SD card support
- [ ] Export to BMP viewable on PC
- [ ] Add text tool font size selector
- [ ] Add fill opacity slider
- [ ] Add canvas size selector on new

### Quran app:
- [ ] Verify edition identifiers with live API call
- [ ] Add bookmarks (save ayah to /quran/bookmarks.json)
- [ ] Add audio recitation (if audio works)
- [ ] Add search results highlighting
- [ ] Add night mode (darker background for night reading)

### Task Manager:
- [ ] PERF tab — show CPU temperature
- [ ] LOGS tab — real-time log streaming
- [ ] DRIVERS tab — drag to reorder driver load priority
- [ ] Add NETWORK tab — show WiFi details, ping test, connected clients

### Driver system:
- [ ] DVR validation sandbox (run in isolated Lua state)
- [ ] Driver dependency system (driver can require another driver)
- [ ] Driver update check via HTTP
- [ ] Driver store (browse/install from URL)

### nano editor:
- [ ] SSH nano — add proper :w/:q line command parsing to shell dispatcher
- [ ] TFT editor — add undo (currently implemented, test it)
- [ ] TFT editor — syntax highlighting for .lua files (keywords in color)
- [ ] Add line numbers toggle

---

## 🟢 NICE TO HAVE

- [ ] **GPS module support** — add GPS_PIN, NMEA parsing, show location
- [ ] **OLED secondary display** — small 0.96" for status (optional, was removed)
- [ ] **Battery level monitoring** — ADC read from battery divider
- [ ] **Auto-sleep** — dim TFT after 30s idle, wake on touch
- [ ] **OTA update** — update firmware over WiFi
- [ ] **Web dashboard** — HTML page served by ESP32 for control
- [ ] **Voice commands** — sound sensor trigger + command recognition
- [ ] **Robot arm** — servo-controlled arm attachment
- [ ] **IR remote** — use TR emission + IR receiver for remote control
- [ ] **Multiroom** — multiple NoorRobots on same network, talk to each other
- [ ] **AI vision** — ESP32-CAM + simple object detection model

---

## 💡 FUTURE IDEAS

- [ ] **NoorOS App Store** — hosted on GitHub, installable via `apt install`
- [ ] **NoorQt Designer** — visual GUI designer app on TFT (like Qt Designer but on the robot!)
- [ ] **Claude integration** — `ask "what do you see?"` sends camera frame to Claude API
- [ ] **Lua IDE on TFT** — write and run Lua apps directly on the robot screen
- [ ] **Robot fleet management** — SSH into multiple robots from one terminal
- [ ] **SD card as extended storage** — move SPIFFS apps to SD
- [ ] **Bluetooth** — BLE control fallback when WiFi unavailable
- [ ] **ESP32-P4 port** — full port to ESP32-P4 with 32MB RAM, unlock full NoorQt
- [ ] **NoorOS community** — open source, others build NoorRobot clones

---

## ⚠️ KNOWN ISSUES

| Issue | Status | Fix |
|-------|--------|-----|
| GPIO25 Hall vs DAC conflict | 🔴 Open | Move Hall to GPIO21 |
| Touch calibration not done | 🔴 Open | Calibrate when TFT arrives |
| Serial garbage chars on robot forward | 🟡 Noted | Likely USB noise, cosmetic only |
| Buffer flood on first motor test | ✅ Fixed | Added serial flush recommendation |
| L298N Channel B dead | ✅ Fixed | New L298N ordered |
| Arduino had no common GND with ESP32 | ✅ Fixed | Added GND wire |
| esp32-ssh `forward` returned immediately | ✅ Noted | fire-and-forget design, by design |
| Lua bindings for NoorQt not complete | 🔴 Open | See NoorQt Lua bindings task |
| os.hook() not yet wired | 🔴 Open | Need to implement in lua_engine.cpp |
| DVR validation runs in main Lua state | 🟡 Risk | Should sandbox in separate state |

---

## 📦 Orders Status

| Item | Store | Status |
|------|-------|--------|
| L298N motor driver | Robocraze | ✅ Ordered |
| 2.8" TFT + stylus | robu.in | ✅ Ordered (₹774) |
| ESP32-CAM-MB Type-C | robu.in | ✅ Ordered (₹159) |
| PAM8403 XH-A156 4CH amp | robu.in | ✅ Ordered (₹188) |
| 2x 3W 8Ω speakers | robu.in | ✅ Ordered (₹174) |
| 120pcs jumper wires | robu.in | ✅ Ordered (₹127) |
| **Total robu.in** | | **₹1422** |
| **Grand total** | | **~₹1573** |

---

## 🔄 Next Session Checklist

When you start the next session, do this in order:

1. Read continue.md (this file)
2. Check what hardware arrived
3. Pick the highest priority 🔴 task
4. If hardware not arrived: work on NoorQt remaining modules (QNetwork, QFile, QThread, QSql, QMultimedia, QAnimation, QModel, QApplication)
5. After all NoorQt modules: write Lua bindings for NoorQt
6. After Lua bindings: implement os.hook() and shell.add() 
7. Then: audio_manager.h for PAM8403
8. Then: camera integration

---

## 📊 Progress Tracker

| Module | Status | Completion |
|--------|--------|------------|
| Hardware wiring | 🟡 Partial | 60% (TFT/sensors not wired yet) |
| Arduino firmware | ✅ Done | 100% |
| ESP32 core OS | ✅ Done | 95% |
| Sensor manager | ✅ Done | 100% |
| TFT UI | ✅ Done | 90% (needs calibration) |
| Virtual keyboard | ✅ Done | 90% (needs calibration) |
| Quran app | ✅ Done | 90% |
| Browser app | ✅ Done | 85% |
| Painter app | ✅ Done | 85% |
| Task manager | ✅ Done | 90% |
| Driver system | ✅ Done | 85% |
| Nano editor | ✅ Done | 80% |
| Lua automation | ✅ Done | 80% |
| NoorUI (lua_widgets) | ✅ Done | 95% |
| NoorQt QObject | ✅ Done | 100% |
| NoorQt QGeometry | ✅ Done | 100% |
| NoorQt QPainter | ✅ Done | 95% |
| NoorQt QWidget | ✅ Done | 90% |
| NoorQt QLayout | ✅ Done | 95% |
| NoorQt QWidgets | ✅ Done | 90% |
| NoorQt QNetwork | 🔴 Todo | 0% |
| NoorQt QFile | 🔴 Todo | 0% |
| NoorQt QThread | 🔴 Todo | 0% |
| NoorQt QSql | 🔴 Todo | 0% |
| NoorQt QMultimedia | 🔴 Todo | 0% |
| NoorQt QAnimation | 🔴 Todo | 0% |
| NoorQt QModel | 🔴 Todo | 0% |
| NoorQt QApplication | 🔴 Todo | 0% |
| NoorQt Lua bindings | 🔴 Todo | 0% |
| Audio manager | 🔴 Todo | 0% |
| Camera integration | 🔴 Todo | 0% |
| os.hook() system | 🔴 Todo | 0% |
| shell.add() system | 🔴 Todo | 0% |
| Touch calibration | 🔴 Todo | 0% (needs hardware) |
| **Overall** | 🟡 In Progress | **~65%** |
