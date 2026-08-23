# NoorRobot OS — Project Bible
> Full documentation of every component, plan, decision, and detail.

---

## 🤖 What Is This?

NoorRobot is a custom-built robot powered by an **ESP32 Dev Module** running **NoorOS** — a fully custom embedded operating system written in C++ with Lua scripting, SSH shell access, a TFT touch UI, camera streaming, sensor suite, driver system, and a full Qt6-like GUI framework called **NoorQt / LuaQt**.

Built from scratch. No off-the-shelf robot OS. Everything custom.

---

## 🧠 Hardware

| Component | Model | Role |
|-----------|-------|------|
| Main CPU | ESP32 Dev Module | Brain — WiFi, SSH, sensors, TFT, OS |
| Motor CPU | Arduino Uno R3 | Pure motor controller (dumb slave) |
| Camera | ESP32-CAM (OV3660 3MP upgrade) | Video streaming |
| Motor Driver | L298N | Controls 4x BO motors |
| Motors | BO Series 3-12V 150RPM | 4 wheels |
| Screen | 2.8" ILI9341 SPI TFT + XPT2046 touch | Main UI display |
| Speaker | 3W 8Ω stereo x2 | Robot voice + audio |
| Amp | XH-A156 PAM8403 4CH | Drives speakers |
| Battery (motors) | 2x Li-Ion 3.7V in series = 7.4V | Motor power |
| Chassis | Ebony wood | Strong, absorbed crash damage |

---

## 🔌 Wiring Architecture

```
Phone (Termux / SSH) ──WiFi──► ESP32 Dev Module
                                    │
                        ┌───────────┼───────────────┐
                        │           │               │
                   UART2 TX/RX   SPI Bus        I2C/GPIO
                        │           │               │
                   Arduino Uno   TFT 2.8"      Sensors
                   (GPIO16/17)   ILI9341       (DHT11, IR,
                        │        XPT2046        flame, etc)
                        │
                      L298N
                        │
                    4x BO Motors
```

### ESP32 Pin Map
| Pin | Role |
|-----|------|
| GPIO16 (RX2) | Arduino TX |
| GPIO17 (TX2) | Arduino TX |
| GPIO15 | TFT CS |
| GPIO2  | TFT DC |
| GPIO4  | TFT RST |
| GPIO23 | SPI MOSI |
| GPIO19 | SPI MISO |
| GPIO18 | SPI CLK |
| GPIO5  | Touch CS |
| GPIO27 | Touch IRQ |
| GPIO32 | DHT11 |
| GPIO33 | Avoid (IR obstacle) |
| GPIO34 | Tracking sensor |
| GPIO35 | Flame sensor |
| GPIO36 | Photoresistor (analog) |
| GPIO39 | Sound sensor (analog) |
| GPIO25 | Hall/magnetic sensor |
| GPIO26 | Laser emitter |
| GPIO13 | Ultrasonic TRIG |
| GPIO14 | Ultrasonic ECHO |
| GPIO12 | Servo |

### Arduino Uno Pin Map
| Pin | Role |
|-----|------|
| 3  | Motor rightBwd (IN1) |
| 5  | Motor rightFwd (IN2) |
| 6  | Motor leftFwd (IN3) |
| 9  | Motor leftBwd (IN4) |
| 10 | SoftwareSerial RX (from ESP32 TX2) |
| 11 | SoftwareSerial TX (to ESP32 RX2) |
| 12 | Fan |

---

## 📦 Software Architecture

```
NoorOS (ESP32)
├── esp32.ino              Main entry point
├── wifi_manager.h         WiFi + AP setup mode
├── shell_server.h         TCP SSH-like shell (port 2222)
├── robot_api.h            Robot movement commands → Arduino
├── sensor_manager.h       All sensor reading + shell commands
├── fs_manager.h           Virtual filesystem (SPIFFS)
├── lua_engine.cpp/h       Lua 5.4 interpreter + all ESP32 bindings
├── task_manager.h         FreeRTOS background tasks
├── package_manager.h      App package install/manage
├── capability.h           Feature flags
├── driver_manager.h       DVR driver system
├── nano_editor.h          SSH nano + TFT visual editor
├── tft_manager.h          Main TFT UI (eyes, status, sensor bar, touch buttons)
├── vkeyboard.h            QWERTY virtual keyboard for TFT
├── lua_widgets.h          NoorUI — easy Lua Qt-like widget system
├── lua_auto.h             Lua automation (PyAutoGUI equivalent)
├── lua_qt/                NoorQt — full Qt6 C++ clone
│   ├── QObject.h          Base class, signals/slots, properties, events
│   ├── QGeometry.h        QColor, QFont, QPoint, QSize, QRect, QTransform, QDateTime
│   ├── QPainter.h         Full QPainter API backed by TFT_eSPI
│   ├── QWidget.h          QWidget base + QPalette + QSizePolicy
│   ├── QLayout.h          QBoxLayout, QHBoxLayout, QVBoxLayout, QGridLayout, QFormLayout, QStackedLayout
│   ├── QWidgets.h         All concrete widgets (20+ widgets)
│   ├── QNetwork.h         QNetworkAccessManager, QTcpSocket, QUdpSocket (PLANNED)
│   ├── QFile.h            QFile, QDir, QFileInfo, QTextStream (PLANNED)
│   ├── QThread.h          QThread, QMutex, QSemaphore (PLANNED)
│   ├── QSql.h             QSqlDatabase, QSqlQuery (PLANNED)
│   ├── QMultimedia.h      QAudioOutput, QMediaPlayer (PLANNED)
│   ├── QAnimation.h       QPropertyAnimation, QSequentialAnimationGroup (PLANNED)
│   ├── QModel.h           QAbstractItemModel, QStandardItemModel (PLANNED)
│   ├── QApplication.h     QApplication, QCoreApplication (PLANNED)
│   └── NoorQt.h           Master include for all NoorQt modules
└── apps/
    ├── quran.h            Quran app (alquran.cloud API, 4 editions, TFT + SSH)
    ├── browser.h          Text web browser (HTTP fetch + HTML strip, TFT + SSH)
    ├── painter.h          Full paint app (12 tools, 4 layers, undo/redo, save)
    └── taskmanager.h      Task manager (5 tabs: procs, sysinfo, drivers, logs, perf)

Arduino (arduino.ino)
└── Motors + Fan only (stripped down, slave to ESP32 via UART)
```

---

## 🧩 NoorOS Features

### Shell (SSH via esp32-ssh binary)
```bash
# Movement
robot forward 10
robot backward 5
robot left 3
robot right 3
robot stop

# Sensors
sensor all
sensor temp
sensor distance 90
sensor light
sensor sound
sensor flame
sensor obstacle
sensor tracking
sensor magnetic
sensor laser on|off
sensor servo 45

# Apps
quran 2 255              # Ayatul Kursi all editions
quran 1 1 uthmani        # Al-Fatihah Arabic only
quran surah 36           # Full Surah Ya-Sin
quran search noor        # Search Uthmani text
browse http://example.com
paint open
taskman open
taskman ps
taskman kill <id>
taskman sysinfo

# Files
ls / cd / mkdir / rm / cat / nano / tedit

# Drivers
driver install /path/file.dvr
driver install /path/file.dvr --force-reboot
driver remove mydriver
driver list
driver info mydriver
driver reload mydriver
driver backup mydriver
driver restore mydriver
driver log

# OS
os reboot
os set motor.speed.default 200
os get motor.speed.default
os version

# Lua
lua print("Hello!")
lua screen.eyes("Happy")
run myapp              # runs /apps/myapp/main.lua
bg lua print("bg")    # background task

# System
wifi
ip
df
storage
sysinfo
jobs
kill <id>
theme dark|light|neon|ocean|fire|candy|hacker|nord
```

### TFT UI Layout (2.8" 240x320)
```
┌─────────────────────────┐
│ IP: x.x.x.x   UP: 1h2m │  ← Status bar (40px)
│ WiFi: -55dBm   CLEAR    │
├─────────────────────────┤
│                         │
│    👀  ROBOT EYES  👀   │  ← Eyes panel (140px)
│    (animated, color)    │
│                         │
│         Normal          │
├─────────────────────────┤
│TEMP  DIST  LIGHT  FLAME │  ← Sensor bar (46px)
│23C   45cm   72%    NO   │
│ Path clear              │
├─────────────────────────┤
│        [▲ FWD]          │  ← Touch buttons (80px)
│[◄ LEFT] [■ STOP] [► RIGHT]│
│        [▼ BCK]          │
└─────────────────────────┘
```

### Eye Emotions (20 types)
Normal, Happy, Sad, Angry, Surprised, Love, Sleepy, Evil, Cool, Dead, Wink, Cry, Bored, Confused, Excited, Dizzy, Nervous, Shy, Guilty, Thinking

Each emotion has:
- Unique color scheme (iris, pupil, glow)
- Unique shape/expression
- Neon glow effect
- Auto-blink every 4 seconds
- Pupil drift animation

---

## 📱 LuaQt / NoorUI

### Two APIs available:

#### 1. NoorUI (Easy, Lua-first)
```lua
local app = ui.app("My App")
local btn = ui.Button("Click!")
btn.lss = "bg: blue; color: white; radius: 8"  -- CSS string
btn.lss = {bg="blue", radius=8}                 -- Table
btn:bg("blue"):color("white"):radius(8):lss()   -- Method chain
btn:on("clicked", function() ui.toast("Hi!") end)
app:add(btn)
app:run()
```

#### 2. NoorQt (Full Qt6 C++ clone)
```cpp
// C++ side
QPushButton* btn = new QPushButton("Click!", parent);
btn->setStyleSheet("background: blue; color: white; border-radius: 8px;");
connect(btn, "clicked", []{ Serial.println("clicked!"); });

// Lua side (via bindings)
local btn = Qt.QPushButton("Click!")
btn:setStyleSheet("background: blue")
btn:connect("clicked", function() print("clicked!") end)
```

### NoorUI Widget List
| Widget | Description |
|--------|-------------|
| Button | Push button with shadow, press animation |
| Label | Text label with word wrap |
| TextInput | Single line input with virtual keyboard |
| PasswordInput | Masked input |
| CheckBox | Tri-state checkbox |
| RadioButton | Auto-exclusive radio (by group) |
| Slider | Horizontal/vertical with tick marks |
| Spinner | Integer +/- with keyboard fallback |
| ProgressBar | Striped, labeled, horizontal/vertical |
| Switch | iOS-style on/off toggle |
| ComboBox | Dropdown with search |
| ListBox | Scrollable list with selection modes |
| Badge | Notification badge |
| Separator | Horizontal/vertical line |
| Spacer | Invisible space filler |
| Image | RGB565 raw image from SPIFFS |
| Panel | Container with title bar |
| ScrollArea | Scrollable container with scrollbar |
| TabWidget | Multi-tab with close buttons |
| GroupBox | Labeled container with optional checkbox |
| ToolButton | Icon/arrow button with auto-raise |
| Dial | Rotary knob with notches |
| LCDNumber | 7-segment style display |
| Frame | Styled frame with shadow modes |
| Splitter | Resizable panel divider |
| StackedWidget | Page stack |
| SpinBox | Integer spinner |
| DoubleSpinBox | Float spinner |
| TextEdit | Multi-line text editor |
| PlainTextEdit | Plain text editor |

### NoorUI Layout List
| Layout | Description |
|--------|-------------|
| VBox | Vertical stack with stretch |
| HBox | Horizontal stack with stretch |
| Grid | Column/row grid with span support |
| Stack | Page stack (QStackedWidget equivalent) |
| Form | Label+widget pairs |
| Absolute | Manual x,y positioning |

### LSS (Lua Style Sheets) — All Properties
```
bg / background-color    background color
color / foreground       text color
border-color             border color
border-width             border thickness (px)
border-radius / radius   corner radius (px)
size / font-size         font size (1-4 for TFT)
padding                  inner padding (px)
margin                   outer margin (px)
shadow                   drop shadow (true/false)
shadow-color             shadow color
bold                     bold text (true/false)
align / text-align       left / center / right
animation                fade / pulse / bounce / slide / shake / glow
opacity                  0-255
min-width                minimum width (px)
max-width                maximum width (px)
visible                  true/false
enabled                  true/false
```

### Themes (8 built-in)
| Theme | Description |
|-------|-------------|
| dark | Default dark theme |
| light | Light/white theme |
| neon | Cyberpunk magenta + cyan |
| ocean | Deep blue tones |
| fire | Orange + red |
| candy | Pink + gold pastel |
| hacker | Matrix green on black |
| nord | Nordic muted blue-grey |
| custom | Set any color via table |

### Signals (all widgets)
```
clicked, pressed, released, toggled(bool)
textChanged(str), textEdited(str)
valueChanged(int/float)
currentIndexChanged(int), currentTextChanged(str)
stateChanged(int), itemClicked(str)
currentRowChanged(int), selectionChanged
returnPressed, editingFinished
focusIn, focusOut, shown, hidden, closed
windowTitleChanged(str), resized, moved
```

### Dialogs
```lua
ui.alert("Message", "Title")         -- OK dialog
ui.confirm("Sure?")                   -- returns true/false
ui.prompt("Enter name:", "default")   -- returns string
ui.pick({"A","B","C"}, "Choose:")     -- returns selection
ui.filepick("/")                       -- file browser
ui.colorpick()                         -- HSV color wheel
ui.progress("Loading", function(update)
  update(50)  -- 50%
  update(100) -- done
end)
ui.toast("Saved!", 2000)              -- bottom toast
```

---

## 🎨 Painter App (NoorPaint)

### Tools
| Tool | Description |
|------|-------------|
| Pen | Free draw, adjustable size 1-20 |
| Eraser | Erase with background color |
| Line | Click 2 points |
| Rect | Click 2 corners |
| RoundRect | Click 2 corners |
| Circle | Click center + edge |
| Triangle | Click 3 points |
| Fill | Flood fill (BFS) |
| Text | Tap + virtual keyboard |
| Eyedrop | Pick color from screen |
| Spray | Random scatter paint |
| Gradient | Linear gradient between 2 colors |

### Features
- 4 layers (toggle visibility, reorder)
- Undo/Redo (20 steps per layer)
- HSV color wheel picker
- Grid overlay (8px grid)
- Zoom 1x/2x/4x
- Save/Load .npt files (SPIFFS/SD/default)
- Background color separate from foreground

---

## 📖 Quran App

### Editions
- `quran-uthmani` — Arabic Uthmani script
- `en.kanzuliman` — Kanzul Iman (English translation)
- `en.kanzulirfan` — Kanzul Irfan (English translation)
- `en.jalalayn` — Tafsir Jalalayn (English tafsir)

### Features
- All 114 Surahs
- Full Ayah navigation (prev/next)
- Jump to any Surah:Ayah via virtual keyboard
- Edition tab switcher on TFT
- Search in Uthmani text
- Full surah listing

---

## 🌐 Browser App (NoorBrowser)

- Fetches any HTTP/HTTPS URL
- Strips HTML tags to readable text
- Decodes HTML entities
- Scrollable text on TFT
- Address bar with virtual keyboard
- Back / Refresh buttons
- User agent: `NoorBot/1.0 (ESP32 Robot Browser)`
- SSH: `browse http://example.com`

---

## 🔧 DVR Driver System

### File Format
```lua
--[[dvr
  name    = "my_driver"
  version = "1.0"
  author  = "Noor"
  reboot  = "ask"     -- auto | ask | manual | none
  undo    = "undo.lua"
--]]

-- Driver body (Lua code)
os.hook("boot", function()
  screen.eyes("Happy")
end)

os.set("motor.speed.default", 180)

shell.add("hello", function(args)
  return "Hello, " .. args .. "!"
end)

--[[undo
  os.set("motor.speed.default", 200)
--]]
```

### editable.dvr
Lives at `/drivers/editable.dvr`. Pre-loaded with everything editable:
- Motor speeds
- Default eye expression
- Sensor poll intervals
- Custom shell commands
- Boot hooks
- Pin overrides
- WiFi credentials
- TFT theme colors

Backup auto-saved to `/drivers/editable.bak` before every edit.

---

## 📡 Sensors

| Sensor | Pin | Type | What it does |
|--------|-----|------|--------------|
| DHT11 | GPIO32 | Digital | Temperature + humidity |
| Ultrasonic HC-SR04 | GPIO13/14 | Digital | Distance measurement |
| Servo | GPIO12 | PWM | Sweeps ultrasonic for scan |
| IR Avoid | GPIO33 | Digital | Obstacle detection |
| Tracking | GPIO34 | Digital | Line following |
| Flame | GPIO35 | Digital | Fire detection |
| Photoresistor | GPIO36 | Analog | Light level |
| Sound | GPIO39 | Analog | Sound level |
| Hall/Magnetic | GPIO25 | Digital | Magnetic field |
| Laser Emit | GPIO26 | Digital out | Tripwire beam |

---

## 🧪 Lua API Reference

### esp32.robot
```lua
esp32.robot.forward(qty, speed)
esp32.robot.backward(qty, speed)
esp32.robot.left(qty, speed)
esp32.robot.right(qty, speed)
esp32.robot.stop()
esp32.robot.eyes(type, ox, oy)
esp32.robot.fan(on)
esp32.robot.distance(angle)
esp32.robot.temperature(unit)
```

### esp32.sensor
```lua
esp32.sensor.temp("c")         -- temperature in C
esp32.sensor.temp("f")         -- temperature in F
esp32.sensor.humidity()        -- humidity %
esp32.sensor.distance()        -- distance in cm
esp32.sensor.light()           -- light 0-100%
esp32.sensor.sound()           -- sound 0-100%
esp32.sensor.obstacle()        -- true/false
esp32.sensor.flame()           -- true/false
esp32.sensor.tracking()        -- true/false (on line)
esp32.sensor.magnetic()        -- true/false
esp32.sensor.laser(true/false) -- set laser on/off
esp32.sensor.servo(angle)      -- move servo 0-180
esp32.sensor.scan(angle)       -- distance at angle
```

### screen.* (easy TFT drawing)
```lua
screen.clear("black")
screen.text("Hello!", 10, 50, "white", 2)
screen.rect(0, 0, 240, 40, "blue", true)
screen.rrect(10, 10, 100, 30, 8, "cyan", false)
screen.circle(120, 160, 30, "red", true)
screen.line(0, 0, 240, 320, "green")
screen.triangle(10,10, 100,10, 55,80, "yellow", true)
screen.button(10, 200, 100, 30, "Click", "blue", "white")
screen.card(10, 50, 220, 80, "Title", "Body text", "darkgrey")
screen.bar(10, 200, 200, 20, 75, 100, "green", "darkgrey")
screen.eyes("Happy", 0, 0)
screen.width()   -- 240
screen.height()  -- 320
```

### keyboard.*
```lua
local input = keyboard.ask("Enter URL:", "http://")
```

### auto.* (PyAutoGUI equivalent)
```lua
auto.tap(120, 160)
auto.swipe(10, 160, 230, 160, 300)
auto.type("Hello World")
auto.wait(1000)
auto.waitTouch(5000)        -- returns {x, y}
auto.screenshot()           -- saves to /screenshots/
auto.pixel(120, 160)        -- returns RGB565 color
auto.findColor(0x07FF, 4)   -- returns {x, y} or nil
auto.record()
auto.stopRecord("my_macro")
auto.play("my_macro", 3)    -- play 3 times
auto.play("my_macro", -1)   -- loop until touch
auto.macros()
auto.on("touch", function(x,y) print(x,y) end)
auto.on("flame", function() robot.stop() end)
auto.on("obstacle", function() robot.backward(1) end)
auto.loop(10, function() robot.forward(1) end)
auto.if_pixel(120,160, 0x07FF, function() print("found!") end)
```

### ui.* (NoorUI — full widget system)
See LuaQt section above.

---

## 📋 Arduino Libraries Required

```
TFT_eSPI              (Bodmer) — TFT driver
XPT2046_Touchscreen   (Paul Stoffregen) — Touch
DHT sensor library    (Adafruit) — DHT11
Adafruit Unified Sensor (Adafruit) — DHT dependency
ESP32Servo            (Kevin Harrington) — Servo
ArduinoJson           (Benoit Blanchon) — JSON for Quran API
```

### TFT_eSPI User_Setup.h configuration:
```cpp
#define ILI9341_DRIVER
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
```

---

## 🏗️ Build Instructions

### ESP32
1. Install Arduino IDE + ESP32 board package
2. Install all libraries from requirements.txt
3. Configure TFT_eSPI User_Setup.h
4. Open `esp32/esp32.ino`
5. Select board: **ESP32 Dev Module**
6. Upload

### Arduino Uno
1. Open `arduino/arduino.ino`
2. Select board: **Arduino Uno**
3. Upload (no extra libraries needed)

### ESP32-CAM
1. Connect ESP32-CAM-MB programmer
2. Open visioning.py or ESP32-CAM firmware
3. Select board: **AI Thinker ESP32-CAM**
4. Upload

### esp32-ssh (Termux / Linux)
```bash
pkg install git cmake make clang
git clone https://github.com/OpenNoorIlm/esp32-os
cd esp32-os/esp32-ssh
mkdir build && cd build
cmake ..
make
./esp32-ssh <ESP32_IP> --port 2222 --pass <password>
```

---

## 🌟 Cool Factor Summary

- Custom OS on an ESP32 running Lua apps
- SSH into a robot and control it with text commands
- Qt6-level GUI framework on a tiny microcontroller
- Full paint app with layers on a 2.8" screen
- Quran reader with 4 editions in Arabic + English
- Web browser on a robot
- Task manager with live performance graphs
- DVR driver system — edit OS behavior without reflashing
- PyAutoGUI-style automation for the touch screen
- 20 animated robot eye emotions with neon glow
- Stereo audio, flame detection, laser tripwire, magnetic sensor
- Auto chip detection — scales from ESP32 classic to ESP32-P4
