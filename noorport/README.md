# NoorPort — Qt/QML C++ → NoorOS App Transpiler

Point NoorPort at **any existing Qt/QML C++ project**.  
Get a deployable NoorOS Lua app (or NoorQt C++ shim) back.  
**Zero changes to your source.**

---

## Quick Start

```bash
# Install (Python 3.8+ — no dependencies)
cd /home/bismillah/Downloads/esp32-os/noorport

# Transpile your Qt project to a NoorUI Lua app
python3 noorport.py ~/Projects/MyQtApp

# Transpile + generate NoorQt C++ compatibility shim
python3 noorport.py ~/Projects/MyQtApp --mode both

# Preview output without writing files
python3 noorport.py ~/Projects/MyQtApp --dry-run

# Custom output directory
python3 noorport.py ~/Projects/MyQtApp --out ~/Desktop/noor_apps
```

---

## What It Does

```
Your Qt Project/              NoorPort              ESP32 Output/
├── main.qml          ──────────────────►     noorport_out/MyApp/
├── Settings.qml      QML Parser + Transpiler ├── main.lua       ← NoorUI Lua app
├── main.cpp          C++ Analyzer            ├── NoorCompat.h   ← NoorQt C++ shim
├── mywidget.h        Shim Generator          ├── app.json       ← manifest
└── MyApp.pro         App Packager            └── INSTALL.md     ← deploy guide
```

### Mode: `--mode lua` (default)
Parses all your `.qml` files and transpiles them to NoorUI Lua.  
- `main.qml` → `main.lua` (entry point, runs directly with `run MyApp`)
- Other QML files → `show_ScreenName()` Lua functions

### Mode: `--mode cpp`
Scans your C++ source for Qt class usage and generates `NoorCompat.h` —  
a single header that maps Qt includes to NoorQt equivalents.  
Add `#include "NoorCompat.h"` to your `.cpp` and it compiles for ESP32.

### Mode: `--mode both`
Does both.

---

## QML → NoorUI Mapping

| QML Type | NoorUI Widget |
|---|---|
| `ApplicationWindow` / `Window` | `ui.app()` |
| `Rectangle` / `Item` / `Frame` | `ui.Panel()` |
| `RowLayout` / `Row` | `ui.HBox()` |
| `ColumnLayout` / `Column` | `ui.VBox()` |
| `GridLayout` | `ui.Grid()` |
| `Button` / `ToolButton` | `ui.Button()` |
| `Text` / `Label` | `ui.Label()` |
| `TextField` / `TextInput` | `ui.TextInput()` |
| `TextArea` / `TextEdit` | `ui.TextEdit()` |
| `CheckBox` | `ui.CheckBox()` |
| `RadioButton` | `ui.RadioButton()` |
| `Switch` | `ui.Switch()` |
| `Slider` | `ui.Slider()` |
| `Dial` | `ui.Dial()` |
| `SpinBox` | `ui.SpinBox()` |
| `ComboBox` | `ui.ComboBox()` |
| `ProgressBar` | `ui.ProgressBar()` |
| `ListView` / `GridView` | `ui.ListBox()` |
| `Image` | `ui.Image()` |
| `TabView` / `TabBar` | `ui.TabWidget()` |
| `ScrollView` | `ui.ScrollArea()` |
| `StackView` / `StackLayout` | `ui.Stack()` |
| `Separator` | `ui.Separator()` |

### Signal Mapping

| QML Signal | NoorUI Signal |
|---|---|
| `onClicked` | `"clicked"` |
| `onPressed` | `"pressed"` |
| `onReleased` | `"released"` |
| `onToggled` | `"toggled"` |
| `onTextChanged` | `"textChanged"` |
| `onValueChanged` | `"valueChanged"` |
| `onActivated` | `"currentIndexChanged"` |
| `onAccepted` | `"submitted"` |

### Property Mapping (LSS)

| QML Property | LSS Key |
|---|---|
| `color` | `bg` |
| `font.pointSize` | `size` |
| `font.bold` | `bold` |
| `border.color` | `border-color` |
| `border.width` | `border-width` |
| `opacity` | `opacity` |
| `radius` | `radius` |
| `padding` | `padding` |
| `horizontalAlignment` | `align` |

---

## Qt C++ → NoorQt Class Map

| Qt Class | NoorQt Header | Status |
|---|---|---|
| `QObject`, `QVariant`, `QList` | `QObject.h` | ✅ Done |
| `QColor`, `QFont`, `QRect`, `QTimer` | `QGeometry.h` | ✅ Done |
| `QPainter`, `QPen`, `QBrush` | `QPainter.h` | ✅ Done |
| `QWidget`, `QPalette` | `QWidget.h` | ✅ Done |
| `QHBoxLayout`, `QVBoxLayout`, `QGridLayout` | `QLayout.h` | ✅ Done |
| `QPushButton`, `QLabel`, `QLineEdit` ... (20+ widgets) | `QWidgets.h` | ✅ Done |
| `QApplication` | `QApplication.h` | 🔴 Planned |
| `QNetworkAccessManager`, `QTcpSocket` | `QNetwork.h` | 🔴 Planned |
| `QFile`, `QDir` | `QFile.h` | 🔴 Planned |
| `QThread`, `QMutex` | `QThread.h` | 🔴 Planned |
| `QSqlDatabase`, `QSqlQuery` | `QSql.h` | 🔴 Planned |
| `QMediaPlayer`, `QAudioOutput` | `QMultimedia.h` | 🔴 Planned |
| `QPropertyAnimation` | `QAnimation.h` | 🔴 Planned |
| `QStandardItemModel` | `QModel.h` | 🔴 Planned |

Planned classes get **minimal compilable stubs** in the shim so your code still builds while those modules are being implemented.

---

## Screen Constraints

Your Qt app runs on a **2.8" ILI9341 TFT, 240×320 px**.  
NoorPort auto-scales layouts. Keep these in mind:
- Font sizes map: Qt `pt` → NoorUI size 1–4
- Colors: full RGB888 → RGB565 internally
- Animations: fade / pulse / bounce / slide / shake / glow
- Touch: XPT2046 stylus/finger touch (single point)

---

## Deploy Output App

```bash
# 1. SSH into your robot
./esp32-ssh <ESP32_IP> --pass <password>

# 2. Create app directory
mkdir /apps/MyApp

# 3. Upload main.lua (paste content or use SFTP)
# Then from the shell:
run MyApp
```

Or copy to SD card under `/apps/MyApp/main.lua` and run from there.

---

## Limitations & Roadmap

- QML property **bindings** (e.g. `width: parent.width / 2`) are not evaluated —  
  they're emitted as Lua comments with the expression preserved for manual wiring.
- JavaScript logic inside QML signal handlers is passed through as-is;  
  complex JS needs manual review.
- `Component.onCompleted` is skipped (no equivalent lifecycle hook yet).
- C++ business logic that uses Qt APIs from planned modules (Network, File, SQL)  
  gets stubs — functionality needs manual port.
- Images: `.png`/`.svg` are noted in the manifest; copy raw RGB565 to SPIFFS.

These limitations shrink as NoorQt planned modules get implemented.

---

## Files

```
noorport/
├── noorport.py     ← main tool (run this)
└── README.md       ← this file
```

NoorPort has zero Python dependencies — just Python 3.8+.
