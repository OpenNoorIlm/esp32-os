<div align="center">

# 🤖 NoorOS — esp32-os

### A fully custom embedded operating system for ESP32 robots
### With SSH shell, Lua scripting, Qt6-like GUI, 20+ sensors, camera, audio, and more

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17%20%2B%20Lua%205.4-orange.svg)](https://www.lua.org/)
[![Status](https://img.shields.io/badge/Status-Active%20Development-green.svg)]()

</div>

---

## 🌟 What Is NoorOS?

NoorOS is a **complete custom operating system** running on an ESP32 microcontroller, designed for a real handbuilt robot. It is **not a library, not a framework, not a kit** — it's a ground-up OS with:

- 🐚 **SSH-like TCP shell** — control your robot from Termux, Linux, or Windows
- 🌙 **Lua 5.4 scripting engine** — write and run apps directly on the robot
- 🖥️ **TFT touch GUI** — 2.8" ILI9341 display with animated robot eyes, sensor dashboard, touch controls
- 🎨 **NoorUI / LuaQt** — Qt6-level GUI framework for Lua (yes, Qt-like widgets on an ESP32)
- 📖 **Quran app** — with Arabic Uthmani script + 3 English translations
- 🌐 **Web browser** — HTTP/HTTPS text browser on the robot screen
- 🎨 **NoorPaint** — full paint app with layers, undo, color wheel on TFT
- 📊 **Task manager** — live process list, kill tasks, performance graphs
- 🔧 **DVR driver system** — install/edit OS drivers via .dvr Lua files, no reflashing needed
- 📝 **nano editor** — SSH terminal editor + TFT visual editor
- 🤖 **20 animated eye emotions** — with neon glow and auto-blink
- 📡 **8 sensor types** — temp, distance, obstacle, flame, light, sound, magnetic, laser tripwire
- 🎵 **Stereo audio** — PAM8403 amp + 2x 3W speakers
- 📷 **3MP camera** — ESP32-CAM with OV3660

---

## 🧠 Hardware Used

| Component | Model | Role |
|-----------|-------|------|
| Main CPU | **ESP32 Dev Module** | Brain — WiFi, SSH, sensors, TFT, OS |
| Motor CPU | **Arduino Uno R3** | Pure motor controller (UART slave) |
| Camera | **ESP32-CAM + OV3660 3MP** | Video streaming |
| Motor Driver | **L298N** | Controls 4x BO motors |
| Motors | **BO Series 3-12V 150RPM** | 4 wheels |
| Screen | **2.8" ILI9341 SPI TFT + XPT2046 touch** | Main UI |
| Speaker | **3W 8Ω stereo x2** | Robot voice + audio |
| Amp | **XH-A156 PAM8403 4CH** | Drives speakers |
| Battery | **2x Li-Ion 3.7V in series = 7.4V** | Motor power |
| Chassis | **Ebony wood** | Survived a crash test 💀 |
| Sensor Kit | **37-in-1 HW-A017** | DHT11, IR, flame, sound, hall, etc. |

---

## 🗂️ Project Structure

```
esp32-os/
├── esp32/                    ← Main ESP32 firmware (upload this)
│   ├── esp32.ino             Main entry point
│   ├── wifi_manager.h        WiFi + AP setup mode
│   ├── shell_server.h        TCP NoorShell (port 2222)
│   ├── robot_api.h           Robot movement commands
│   ├── sensor_manager.h      All 8 sensor types
│   ├── tft_manager.h         TFT UI — eyes, status, touch buttons
│   ├── vkeyboard.h           QWERTY virtual keyboard
│   ├── lua_engine.cpp/h      Lua 5.4 + all bindings
│   ├── driver_manager.h      DVR driver system
│   ├── nano_editor.h         SSH nano + TFT visual editor
│   ├── lua_widgets.h         NoorUI — Qt-like Lua widget system
│   ├── lua_auto.h            PyAutoGUI for TFT
│   ├── task_manager.h        FreeRTOS task management
│   ├── fs_manager.h          SPIFFS virtual filesystem
│   ├── package_manager.h     App package system
│   ├── lua_qt/               NoorQt — full Qt6 C++ clone
│   │   ├── QObject.h         QObject, QVariant, QList, Qt:: enums
│   │   ├── QGeometry.h       QColor, QFont, QRect, QPoint, QTransform...
│   │   ├── QPainter.h        QPainter, QPen, QBrush, QGradient...
│   │   ├── QWidget.h         QWidget, QPalette, QSizePolicy
│   │   ├── QLayout.h         QVBoxLayout, QHBoxLayout, QGridLayout...
│   │   └── QWidgets.h        30+ concrete widgets
│   ├── apps/
│   │   ├── quran.h           Quran reader (4 editions, SSH + TFT)
│   │   ├── browser.h         Text web browser
│   │   ├── painter.h         Full paint app (12 tools, 4 layers)
│   │   └── taskmanager.h     Task manager (5 tabs)
│   └── lua_src/              Lua 5.4 source (pre-bundled)
├── arduino/
│   └── arduino.ino           Arduino motor controller (slave)
├── esp32-ssh/                SSH client tool (build on Linux/Termux)
│   └── src/main.cpp
├── esp32-cam/                ESP32-CAM firmware
├── about.md                  📖 Full project documentation
├── continue.md               📋 Priority task tracker (READ FIRST)
└── requirements.txt          📦 Arduino library list
```

---

## 🚀 Quick Start

### 1. Build the SSH Client (Termux / Linux)

```bash
# Termux
pkg install git cmake make clang

# Linux
sudo apt install git cmake make clang

git clone https://github.com/OpenNoorIlm/esp32-os
cd esp32-os/esp32-ssh
mkdir build && cd build
cmake ..
make
```

### 2. Flash the ESP32

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install libraries (see `requirements.txt`)
4. Configure `TFT_eSPI/User_Setup.h`:
```cpp
#define ILI9341_DRIVER
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
```
5. Open `esp32/esp32.ino`, select **ESP32 Dev Module**, upload.

### 3. Flash the Arduino

1. Open `arduino/arduino.ino`
2. Select **Arduino Uno**, upload.
3. Wire Arduino pins 10/11 to ESP32 GPIO16/17 (UART2)
4. Add common GND between Arduino and ESP32 ⚠️

### 4. Connect

```bash
# Find your ESP32's IP in your router or hotspot device list
./esp32-ssh 192.168.x.x --port 2222 --pass yourpassword
```

---

## 🐚 Shell Commands

```bash
# Robot movement
robot forward 10          # Move forward (10 = duration units)
robot backward 5
robot left 3
robot right 3
robot stop

# Sensors
sensor all                # All sensor readings
sensor temp               # Temperature C/F + humidity
sensor distance 90        # Distance at servo angle 90°
sensor light              # Light level %
sensor sound              # Sound level %
sensor flame              # Flame detected?
sensor obstacle           # Obstacle in front?
sensor tracking           # On a line?
sensor magnetic           # Magnetic field?
sensor laser on|off       # Laser tripwire emitter
sensor servo 45           # Move servo to 45°

# Apps
quran 2 255               # Ayatul Kursi — all 4 editions
quran 1 1 uthmani         # Al-Fatihah Arabic only
quran surah 36            # Full Surah Ya-Sin
quran search noor         # Search in Uthmani text
browse http://example.com # Text web browser
paint open                # Open paint app on TFT
taskman open              # Open task manager on TFT
taskman ps                # List running processes
taskman kill <id>         # Kill a process
taskman sysinfo           # System information

# File system
ls                        # List files
cd /apps                  # Change directory
cat /drivers/editable.dvr # Read a file
nano /myfile.lua          # Edit in SSH nano
tedit /myfile.lua         # Edit on TFT screen

# Drivers
driver install /path/to/driver.dvr
driver install /path/to/driver.dvr --force-reboot
driver remove mydriver
driver list
driver info mydriver
driver reload mydriver
driver backup mydriver
driver restore mydriver

# OS
os reboot                 # Reboot ESP32
os set motor.speed.default 200
os get motor.speed.default
os version

# Lua scripting
lua print("Hello!")
lua screen.eyes("Happy")
lua sensor.temp()
run myapp                 # Run /apps/myapp/main.lua
bg lua robot.forward(1)   # Run in background

# System
wifi                      # WiFi status
ip                        # Get IP address
df                        # Disk usage
jobs                      # Background tasks
theme dark|light|neon|ocean|fire|candy|hacker|nord
```

---

## 🌙 Lua Scripting API

### Robot Control
```lua
esp32.robot.forward(qty, speed)
esp32.robot.backward(qty, speed)
esp32.robot.left(qty, speed)
esp32.robot.right(qty, speed)
esp32.robot.stop()
esp32.robot.eyes("Happy")       -- 20 emotion types
esp32.robot.fan(true)
```

### Sensors
```lua
esp32.sensor.temp("c")          -- 23.5
esp32.sensor.humidity()         -- 65.2
esp32.sensor.distance()         -- 34.7 cm
esp32.sensor.light()            -- 72 (percent)
esp32.sensor.sound()            -- 45 (percent)
esp32.sensor.obstacle()         -- true/false
esp32.sensor.flame()            -- true/false
esp32.sensor.tracking()         -- true/false
esp32.sensor.magnetic()         -- true/false
esp32.sensor.laser(true)        -- laser on
esp32.sensor.servo(90)          -- aim servo
esp32.sensor.scan(45)           -- distance at 45°
```

### Screen Drawing
```lua
screen.clear("black")
screen.text("NoorOS", 10, 50, "cyan", 2)
screen.rect(0, 0, 240, 40, "blue", true)    -- filled
screen.circle(120, 160, 30, "red", true)
screen.line(0, 0, 240, 320, "green")
screen.button(10, 200, 100, 30, "Go!", "blue", "white")
screen.bar(10, 250, 200, 20, 75, 100, "green")
screen.eyes("Surprised")
screen.card(10, 50, 220, 80, "Status", "All systems OK")
```

### NoorUI — Qt-like widgets
```lua
local app = ui.app("Robot Control")
app:theme("neon")

local vbox = ui.VBox(4, 30, 232, 6)

local btn = ui.Button("▲ Forward")
btn.lss = {bg="green", color="black", radius=8, shadow=true}
btn:on("clicked", function()
  esp32.robot.forward(1)
end)

local slider = ui.Slider(0, 255, 200)
slider:on("changed", function(v)
  esp32.robot.setSpeed(tonumber(v))
end)

local tabs = ui.TabWidget({"Control", "Sensors", "Settings"})
tabs:move(0, 28):size(240, 292)
tabs:addToPage(1, btn)
tabs:addToPage(1, slider)

app:add(tabs)
app:run()
```

### TFT Automation (PyAutoGUI)
```lua
auto.tap(120, 160)
auto.swipe(10, 160, 230, 160, 300)
auto.record()
auto.stopRecord("my_macro")
auto.play("my_macro", -1)    -- loop forever
auto.on("flame", function()
  esp32.robot.stop()
  screen.eyes("Angry")
end)
auto.on("obstacle", function()
  esp32.robot.backward(1)
end)
```

---

## 🎨 NoorUI / LuaQt Widgets

| Widget | Description |
|--------|-------------|
| `ui.Button(text)` | Push button with shadow + animation |
| `ui.Label(text)` | Text label with word wrap |
| `ui.TextInput(placeholder)` | Single line input (virtual keyboard) |
| `ui.PasswordInput(ph)` | Masked password input |
| `ui.CheckBox(text)` | Tri-state checkbox |
| `ui.RadioButton(group, text)` | Auto-exclusive radio |
| `ui.Slider(min, max, val)` | Draggable slider |
| `ui.Spinner(min, max, val)` | +/- integer input |
| `ui.ProgressBar(max)` | Striped progress bar |
| `ui.Switch(on, "ON", "OFF")` | iOS-style toggle |
| `ui.ComboBox({items})` | Dropdown selector |
| `ui.ListBox({items})` | Scrollable list |
| `ui.TabWidget({tabs})` | Multi-tab container |
| `ui.ScrollArea()` | Scrollable container |
| `ui.Panel(title)` | Titled container |
| `ui.GroupBox(title)` | Labeled group with checkbox |
| `ui.ToolButton()` | Arrow/icon button |
| `ui.Dial(min, max)` | Rotary knob |
| `ui.LCDNumber(digits)` | 7-segment display |
| `ui.Badge(text, color)` | Notification badge |
| `ui.Separator()` | Divider line |
| `ui.Spacer(h)` | Invisible spacer |
| `ui.Image(path)` | Image from SPIFFS |

### Layouts
| Layout | Description |
|--------|-------------|
| `ui.VBox(x,y,w,spacing)` | Vertical stack |
| `ui.HBox(x,y,h,spacing)` | Horizontal stack |
| `ui.Grid(x,y,w,cols,gap)` | Grid layout |
| `ui.Stack(x,y,w,h)` | Page stack |
| `ui.Form(x,y,w)` | Label+field pairs |

### LSS — Lua Style Sheets (3 syntaxes)
```lua
-- CSS string
widget.lss = "bg: blue; color: white; radius: 8; shadow: true"

-- Lua table
widget.lss = {bg="blue", color="white", radius=8, shadow=true}

-- Method chain
widget:bg("blue"):color("white"):radius(8):shadow():lss()
```

### Themes
```lua
ui.theme("dark")     -- Default dark
ui.theme("light")    -- White/light
ui.theme("neon")     -- Cyberpunk magenta+cyan
ui.theme("ocean")    -- Deep blue
ui.theme("fire")     -- Orange+red
ui.theme("candy")    -- Pink+gold pastel
ui.theme("hacker")   -- Matrix green
ui.theme("nord")     -- Nordic muted blue-grey
```

---

## 🎭 Robot Eye Emotions

20 animated eye types, each with unique colors and glow:

`Normal` `Happy` `Sad` `Angry` `Surprised` `Love` `Sleepy` `Evil` `Cool` `Dead` `Wink` `Cry` `Bored` `Confused` `Excited` `Dizzy` `Nervous` `Shy` `Guilty` `Thinking`

```bash
# Via shell
robot eyes Happy
robot eyes Evil

# Via Lua
esp32.robot.eyes("Surprised")
screen.eyes("Love", 0, 5)   -- with x,y offset
```

Features:
- Unique iris/pupil/glow color per emotion
- Neon glow ring effect
- Auto-blink every 4 seconds
- Pupil drift animation
- Eye label below

---

## 🔧 DVR Driver System

Edit OS behavior without reflashing. Drivers are Lua files that run at boot.

### editable.dvr
```lua
--[[dvr
  name    = "editable"
  version = "1.0"
  author  = "NoorOS"
  reboot  = "ask"
--]]

-- Everything is editable here
os.set("motor.speed.default", 200)
os.set("eye.default", "Normal")
os.set("sensor.dht.interval", 2000)

-- Add custom shell commands
shell.add("greet", function(args)
  return "Hello, " .. args .. "! I am NoorRobot."
end)

-- Boot hook
os.hook("boot", function()
  screen.eyes("Happy")
  esp32.robot.say("Boot complete!")
end)

-- Override any pin
-- os.set("pin.laser", 26)
-- os.set("wifi.ssid", "MyNetwork")
```

### Install a driver
```bash
driver install /sd/my_driver.dvr
driver install /sd/my_driver.dvr --force-reboot
```

### Backup / restore
```bash
driver backup editable    # Auto-saves before each edit
driver restore editable   # Restore from backup if broken
```

If a driver crashes on boot — TFT shows error screen with option to restore backup automatically.

---

## 🎨 NoorPaint

Full paint application on the 2.8" TFT:

**Tools:** Pen · Eraser · Line · Rectangle · Rounded Rect · Circle · Triangle · Flood Fill · Text · Eyedropper · Spray · Gradient

**Features:**
- 4 layers with visibility toggle
- 20-step undo/redo per layer
- Full HSV color wheel picker
- 8px grid overlay
- Adjustable brush size (1–20px)
- Fill / stroke toggle
- Save/load `.npt` files to SPIFFS/SD
- Background color separate from foreground

---

## 📖 Quran App

```bash
quran 2 255               # Ayatul Kursi, all 4 editions
quran 1 1 uthmani         # Arabic only
quran 1 1 kanzuliman      # Kanzul Iman only
quran 1 1 kanzulirfan     # Kanzul Irfan only
quran 1 1 jalalayn        # Tafsir Jalalayn only
quran surah 36            # Full Surah Ya-Sin
quran search noor         # Search in Uthmani text
```

**Editions:**
- `quran-uthmani` — Arabic Uthmani script
- `en.kanzuliman` — Kanzul Iman (English)
- `en.kanzulirfan` — Kanzul Irfan (English)
- `en.jalalayn` — Tafsir Jalalayn (English)

**TFT features:** Edition tabs · Swipe prev/next ayah · Tap to jump anywhere · 114 surahs

---

## 📊 Task Manager

5-tab TFT app:

| Tab | Description |
|-----|-------------|
| PROCS | All FreeRTOS tasks, stack usage, priority, state, kill button |
| SYSINFO | Chip model, CPU freq, RAM, flash, SPIFFS, WiFi, sensors |
| DRVRS | Installed drivers, edit/reload/install buttons |
| LOGS | crash.log, driver.log, real-time scroll |
| PERF | Live RAM usage graph (60-second history) |

---

## 🏗️ NoorQt — Qt6 C++ Clone

A complete Qt6-level framework for ESP32, auto-scaling to chip RAM:

| Module | Status | Classes |
|--------|--------|---------|
| QObject.h | ✅ Complete | QObject, QVariant, QEvent, QList, QMap, QByteArray, Qt:: |
| QGeometry.h | ✅ Complete | QColor, QFont, QPoint, QSize, QRect, QMargins, QLine, QPolygon, QTransform, QDateTime, QTimer |
| QPainter.h | ✅ Complete | QPainter, QPen, QBrush, QLinearGradient, QRadialGradient, QPainterPath |
| QWidget.h | ✅ Complete | QWidget, QPalette, QSizePolicy |
| QLayout.h | ✅ Complete | QBoxLayout, QHBoxLayout, QVBoxLayout, QGridLayout, QFormLayout, QStackedLayout |
| QWidgets.h | ✅ Complete | 30+ widgets (QPushButton, QLabel, QLineEdit, QSlider, QComboBox, QTabWidget...) |
| QNetwork.h | 🔄 Planned | QNetworkAccessManager, QTcpSocket, QUdpSocket |
| QFile.h | 🔄 Planned | QFile, QDir, QTextStream |
| QThread.h | 🔄 Planned | QThread, QMutex, QSemaphore |
| QSql.h | 🔄 Planned | QSqlDatabase, QSqlQuery |
| QMultimedia.h | 🔄 Planned | QAudioOutput, QMediaPlayer |
| QAnimation.h | 🔄 Planned | QPropertyAnimation |
| QModel.h | 🔄 Planned | QAbstractItemModel |
| QApplication.h | 🔄 Planned | QApplication, QScreen |

---

## 📦 Libraries Required

Install via Arduino IDE Library Manager:

```
TFT_eSPI              by Bodmer
XPT2046_Touchscreen   by Paul Stoffregen
DHT sensor library    by Adafruit
Adafruit Unified Sensor by Adafruit
ESP32Servo            by Kevin Harrington
ArduinoJson           by Benoit Blanchon
```

Full details in `requirements.txt`.

---

## 🔌 Pin Reference

### ESP32 → TFT (ILI9341 + XPT2046)
| Signal | ESP32 GPIO |
|--------|-----------|
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RST | 4 |
| MOSI | 23 |
| MISO | 19 |
| CLK | 18 |
| Touch CS | 5 |
| Touch IRQ | 27 |

### ESP32 → Arduino (UART2)
| Signal | ESP32 | Arduino |
|--------|-------|---------|
| TX | GPIO17 | Pin 10 (RX) |
| RX | GPIO16 | Pin 11 (TX) |
| GND | GND | GND ⚠️ Required! |

### ESP32 → Sensors
| Sensor | GPIO |
|--------|------|
| DHT11 | 32 |
| Ultrasonic TRIG | 13 |
| Ultrasonic ECHO | 14 |
| Servo | 12 |
| IR Obstacle | 33 |
| Tracking | 34 |
| Flame | 35 |
| Photoresistor | 36 |
| Sound | 39 |
| Hall/Magnetic | 25 |
| Laser Emit | 26 |

---

## 🤝 Contributing

This project is open source under MIT license.

**Want to contribute?**
- Build your own NoorRobot
- Write Lua apps for NoorOS
- Complete NoorQt modules (QNetwork, QThread, QSql, QMultimedia...)
- Add new sensor drivers
- Improve the TFT UI

Issues and PRs welcome at [github.com/OpenNoorIlm/esp32-os](https://github.com/OpenNoorIlm/esp32-os)

---

## 📄 License

MIT License — © OpenNoorIlm / NoorRobot Project

See [LICENSE](LICENSE) for details.

---

<div align="center">

**Built with ❤️ on an ESP32, a soldering iron, and a lot of crashes**

*"It's not a bug, it's a feature test"* — NoorRobot, 2026

</div>
