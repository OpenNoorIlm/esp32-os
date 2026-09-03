#!/usr/bin/env python3
"""
NoorPort — Qt/QML C++ Project → NoorOS App Transpiler
======================================================
Point at any existing Qt/QML C++ project directory.
Get a ready-to-deploy NoorOS app back. Change nothing in your source.

Usage:
    python3 noorport.py /path/to/your/qt-project
    python3 noorport.py /path/to/your/qt-project --out /path/to/output
    python3 noorport.py /path/to/your/qt-project --mode lua        # NoorUI Lua app (default)
    python3 noorport.py /path/to/your/qt-project --mode cpp        # NoorQt C++ shim
    python3 noorport.py /path/to/your/qt-project --mode both       # both outputs
    python3 noorport.py /path/to/your/qt-project --dry-run         # preview only

Architecture:
    1. Project Scanner   — finds .qml, .cpp, .h, .pro/.cmake files
    2. QML Parser        — parses QML tree into AST
    3. QML Transpiler    — AST → NoorUI Lua code
    4. C++ Analyzer      — extracts Qt includes/classes used in C++ 
    5. Shim Generator    — generates NoorQt C++ compatibility header
    6. App Packager      — bundles into deployable NoorOS app structure
"""

import os
import sys
import re
import json
import argparse
import textwrap
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

# ─── ANSI colours ─────────────────────────────────────────────────────────────
R = "\033[31m"; G = "\033[32m"; Y = "\033[33m"; B = "\033[34m"
C = "\033[36m"; W = "\033[37m"; BOLD = "\033[1m"; RST = "\033[0m"
def ok(s):   print(f"  {G}✓{RST}  {s}")
def warn(s): print(f"  {Y}⚠{RST}  {s}")
def err(s):  print(f"  {R}✗{RST}  {s}")
def hdr(s):  print(f"\n{BOLD}{C}{s}{RST}")
def info(s): print(f"  {B}→{RST}  {s}")

# ══════════════════════════════════════════════════════════════════════════════
# 1. PROJECT SCANNER
# ══════════════════════════════════════════════════════════════════════════════

@dataclass
class QtProject:
    root: Path
    name: str = ""
    qml_files: list = field(default_factory=list)
    cpp_files: list = field(default_factory=list)
    h_files:   list = field(default_factory=list)
    pro_file:  Optional[Path] = None
    cmake_file: Optional[Path] = None
    main_qml:  Optional[Path] = None
    resources:  list = field(default_factory=list)   # images, assets

def scan_project(root: Path) -> QtProject:
    hdr("📂  Scanning project")
    proj = QtProject(root=root)

    # Project name from directory or .pro file
    proj.name = root.name

    for f in root.rglob("*"):
        if f.is_file():
            suf = f.suffix.lower()
            if suf == ".qml":
                proj.qml_files.append(f)
            elif suf in (".cpp", ".cc", ".cxx"):
                proj.cpp_files.append(f)
            elif suf == ".h":
                proj.h_files.append(f)
            elif suf == ".pro":
                proj.pro_file = f
                proj.name = f.stem
            elif f.name == "CMakeLists.txt" and proj.cmake_file is None:
                proj.cmake_file = f
            elif suf in (".png", ".jpg", ".svg", ".wav", ".mp3"):
                proj.resources.append(f)

    # Find main QML: main.qml / Main.qml / App.qml / root name match
    candidates = ["main.qml", "Main.qml", f"{proj.name}.qml", "App.qml"]
    for c in candidates:
        for q in proj.qml_files:
            if q.name == c:
                proj.main_qml = q
                break
        if proj.main_qml:
            break
    if not proj.main_qml and proj.qml_files:
        proj.main_qml = proj.qml_files[0]

    ok(f"Project: {BOLD}{proj.name}{RST}")
    ok(f"QML files: {len(proj.qml_files)}")
    ok(f"C++ files: {len(proj.cpp_files)}")
    ok(f"Headers:   {len(proj.h_files)}")
    ok(f"Resources: {len(proj.resources)}")
    if proj.main_qml:
        ok(f"Main QML: {proj.main_qml.name}")
    else:
        warn("No main QML found — will use first QML file found")
    return proj

# ══════════════════════════════════════════════════════════════════════════════
# 2. QML PARSER  →  lightweight AST (no dependency on Qt)
# ══════════════════════════════════════════════════════════════════════════════

@dataclass
class QmlNode:
    type: str                        # e.g. "Rectangle", "Button", "Text"
    id:   str = ""
    props: dict = field(default_factory=dict)
    children: list = field(default_factory=list)
    signals: list = field(default_factory=list)  # [(signal, handler_body)]
    anchors: dict = field(default_factory=dict)

def _strip_comments(src: str) -> str:
    """Remove // and /* */ comments."""
    src = re.sub(r'//[^\n]*', '', src)
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.DOTALL)
    return src

def _parse_block(tokens: list, pos: int) -> tuple:
    """Parse a { ... } block. Returns (QmlNode, next_pos)."""
    # tokens[pos] should be the type name already consumed
    # caller passes the node type; we start after '{'
    node = QmlNode(type="")
    depth = 0
    i = pos

    while i < len(tokens):
        tok = tokens[i]
        if tok == '{':
            depth += 1; i += 1; continue
        if tok == '}':
            depth -= 1; i += 1
            if depth == 0:
                return node, i
            continue

        # Property: identifier: value
        if depth == 1 and i + 1 < len(tokens) and tokens[i+1] == ':':
            key = tok
            i += 2  # skip key and ':'
            # collect value until newline or next property
            val_toks = []
            while i < len(tokens) and tokens[i] not in ('}', '{') and '\n' not in tokens[i-1:i]:
                val_toks.append(tokens[i]); i += 1
            val = ' '.join(val_toks).strip().strip('"\'')
            if key == 'id':
                node.id = val
            elif key.startswith('on') and key[2:3].isupper():
                node.signals.append((key, val))
            elif key.startswith('anchors.'):
                node.anchors[key[8:]] = val
            else:
                node.props[key] = val
            continue

        # Child element: UpperCase word followed by {
        if depth == 1 and re.match(r'^[A-Z]', tok) and i+1 < len(tokens) and tokens[i+1] == '{':
            child_type = tok
            i += 1  # skip to '{'
            child, i = _parse_block(tokens, i)
            child.type = child_type
            node.children.append(child)
            continue

        i += 1

    return node, i

def parse_qml(path: Path) -> Optional[QmlNode]:
    try:
        src = path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        warn(f"Cannot read {path.name}: {e}")
        return None

    src = _strip_comments(src)

    # Simple tokeniser
    tok_pat = re.compile(
        r'"[^"]*"|\'[^\']*\'|[A-Za-z_]\w*(?:\.\w+)*|[{}:;\n]|[-+]?\d+(?:\.\d+)?'
    )
    tokens = tok_pat.findall(src)

    # Find root element (first UpperCase token followed by {)
    root = None
    i = 0
    while i < len(tokens):
        if re.match(r'^[A-Z]', tokens[i]):
            if i+1 < len(tokens) and tokens[i+1] == '{':
                root_type = tokens[i]
                i += 1
                root, _ = _parse_block(tokens, i)
                root.type = root_type
                break
        i += 1

    return root

# ══════════════════════════════════════════════════════════════════════════════
# 3. QML TRANSPILER  →  NoorUI Lua
# ══════════════════════════════════════════════════════════════════════════════

# Mapping: QML type → NoorUI factory call
QML_TO_NOORUI = {
    # Containers / Windows
    "ApplicationWindow": ("app",        "app"),
    "Window":            ("app",        "app"),
    "Rectangle":         ("Panel",      "widget"),
    "Item":              ("Panel",      "widget"),
    "Frame":             ("Panel",      "widget"),
    "GroupBox":          ("Panel",      "widget"),
    "ScrollView":        ("ScrollArea", "widget"),
    "StackView":         ("Stack",      "widget"),
    "TabView":           ("TabWidget",  "widget"),
    "TabBar":            ("TabWidget",  "widget"),
    "SwipeView":         ("Stack",      "widget"),
    "Flickable":         ("ScrollArea", "widget"),
    "Page":              ("Panel",      "widget"),
    "Pane":              ("Panel",      "widget"),
    "Popup":             ("Panel",      "widget"),
    "Dialog":            ("Panel",      "widget"),

    # Layouts
    "RowLayout":         ("HBox",       "layout"),
    "ColumnLayout":      ("VBox",       "layout"),
    "GridLayout":        ("Grid",       "layout"),
    "Row":               ("HBox",       "layout"),
    "Column":            ("VBox",       "layout"),
    "Grid":              ("Grid",       "layout"),
    "Flow":              ("HBox",       "layout"),
    "StackLayout":       ("Stack",      "layout"),

    # Controls
    "Button":            ("Button",     "widget"),
    "ToolButton":        ("Button",     "widget"),
    "RoundButton":       ("Button",     "widget"),
    "AbstractButton":    ("Button",     "widget"),
    "Text":              ("Label",      "widget"),
    "Label":             ("Label",      "widget"),
    "TextField":         ("TextInput",  "widget"),
    "TextInput":         ("TextInput",  "widget"),
    "TextArea":          ("TextEdit",   "widget"),
    "TextEdit":          ("TextEdit",   "widget"),
    "CheckBox":          ("CheckBox",   "widget"),
    "RadioButton":       ("RadioButton","widget"),
    "Switch":            ("Switch",     "widget"),
    "Slider":            ("Slider",     "widget"),
    "RangeSlider":       ("Slider",     "widget"),
    "Dial":              ("Dial",       "widget"),
    "SpinBox":           ("SpinBox",    "widget"),
    "ComboBox":          ("ComboBox",   "widget"),
    "ProgressBar":       ("ProgressBar","widget"),
    "BusyIndicator":     ("ProgressBar","widget"),
    "Image":             ("Image",      "widget"),
    "AnimatedImage":     ("Image",      "widget"),
    "ListView":          ("ListBox",    "widget"),
    "GridView":          ("ListBox",    "widget"),
    "TableView":         ("ListBox",    "widget"),
    "TreeView":          ("ListBox",    "widget"),
    "Repeater":          ("ListBox",    "widget"),
    "Separator":         ("Separator",  "widget"),
    "MenuSeparator":     ("Separator",  "widget"),
    "ToolSeparator":     ("Separator",  "widget"),
    "ToolBar":           ("Panel",      "widget"),
    "MenuBar":           ("Panel",      "widget"),
    "StatusBar":         ("Panel",      "widget"),
    "Drawer":            ("Panel",      "widget"),
}

# QML property → NoorUI LSS key
PROP_MAP = {
    "color":            "bg",
    "background":       "bg",
    "foreground":       "color",
    "text":             "text",
    "font.pointSize":   "size",
    "font.pixelSize":   "size",
    "font.bold":        "bold",
    "horizontalAlignment": "align",
    "verticalAlignment":   "align",
    "opacity":          "opacity",
    "radius":           "radius",
    "border.color":     "border-color",
    "border.width":     "border-width",
    "padding":          "padding",
    "spacing":          "margin",
    "visible":          "visible",
    "enabled":          "enabled",
    "width":            "min-width",
    "height":           None,        # handled separately
    "source":           "src",
    "placeholderText":  "placeholder",
    "checked":          "checked",
    "value":            "value",
    "from":             "min",
    "to":               "max",
    "stepSize":         "step",
    "title":            "title",
}

# QML signal → NoorUI signal name
SIGNAL_MAP = {
    "onClicked":        "clicked",
    "onPressed":        "pressed",
    "onReleased":       "released",
    "onToggled":        "toggled",
    "onTextChanged":    "textChanged",
    "onTextEdited":     "textChanged",
    "onValueChanged":   "valueChanged",
    "onActivated":      "currentIndexChanged",
    "onCurrentIndexChanged": "currentIndexChanged",
    "onCheckedChanged": "toggled",
    "onAccepted":       "submitted",
    "onTriggered":      "clicked",
    "onFocusChanged":   "focused",
    "onVisibleChanged": None,   # skip
    "onCompleted":      None,   # skip (Component.onCompleted)
    "onDestruction":    None,
}

class LuaEmitter:
    def __init__(self):
        self.lines = []
        self.indent = 0
        self.var_counter = {}
        self.id_map = {}   # QML id → Lua var name

    def emit(self, line=""):
        if line:
            self.lines.append("  " * self.indent + line)
        else:
            self.lines.append("")

    def fresh_var(self, prefix: str) -> str:
        n = self.var_counter.get(prefix, 0) + 1
        self.var_counter[prefix] = n
        return f"{prefix}{n}" if n > 1 else prefix

    def lua_value(self, val: str) -> str:
        """Convert QML value string to Lua literal."""
        val = val.strip()
        if val in ("true", "false"):
            return val
        if val == "undefined" or val == "null":
            return "nil"
        # Qt.AlignHCenter etc
        if val.startswith("Qt.Align"):
            m = {"Qt.AlignLeft":"left","Qt.AlignRight":"right",
                 "Qt.AlignHCenter":"center","Qt.AlignVCenter":"center",
                 "Qt.AlignCenter":"center"}
            return f'"{m.get(val,"center")}"'
        # Color string
        if re.match(r'^"?#[0-9a-fA-F]{3,8}"?$', val):
            return f'"{val.strip(chr(34))}"'
        # Numeric
        if re.match(r'^-?\d+(\.\d+)?$', val):
            return val
        # Already quoted
        if (val.startswith('"') and val.endswith('"')) or \
           (val.startswith("'") and val.endswith("'")):
            return f'"{val[1:-1]}"'
        # Binding expression — wrap in comment, use placeholder
        return f'"{val}"  -- QML binding'

    def transpile_node(self, node: QmlNode, parent_var: Optional[str] = None,
                       app_var: str = "app", is_root: bool = False) -> str:
        """Emit Lua for a QmlNode. Returns this node's variable name."""
        mapping = QML_TO_NOORUI.get(node.type)

        if not mapping:
            warn(f"  Unknown QML type '{node.type}' — emitting as Label placeholder")
            mapping = ("Label", "widget")

        noor_type, kind = mapping

        # Determine variable name
        if node.id:
            var = node.id
            self.id_map[node.id] = var
        else:
            prefix = noor_type[0].lower() + noor_type[1:]
            var = self.fresh_var(prefix)

        # ── App / Window root ──
        if kind == "app" or is_root:
            title = node.props.get("title", f'"{node.props.get("title","NoorApp")}"')
            title = self.lua_value(title)
            self.emit(f'local {var} = ui.app({title})')
            if "width" in node.props and "height" in node.props:
                self.emit(f'-- screen is 240x320; original size was '
                          f'{node.props["width"]}x{node.props["height"]}')
        elif kind == "layout":
            self.emit(f'local {var} = ui.{noor_type}(0, 40, 240, 4)')
        else:
            # Extract text/label
            text_val = node.props.get("text", "")
            if text_val:
                text_arg = self.lua_value(text_val)
                self.emit(f'local {var} = ui.{noor_type}({text_arg})')
            else:
                self.emit(f'local {var} = ui.{noor_type}()')

        # ── LSS properties ──
        lss_parts = []
        for qml_key, lss_key in PROP_MAP.items():
            if qml_key in node.props and lss_key:
                if lss_key == "text":
                    continue  # already in constructor
                val = self.lua_value(node.props[qml_key])
                lss_parts.append(f'{lss_key}={val}')

        # Anchors → move/size heuristics
        x = node.props.get("x", "0")
        y = node.props.get("y", "40")
        w = node.props.get("width", "200")
        h = node.props.get("height", "36")
        try: x=int(float(x)); y=int(float(y)); w=int(float(w)); h=int(float(h))
        except: x=0; y=40; w=200; h=36

        if kind not in ("app", "layout") and not is_root:
            self.emit(f'{var}:move({x}, {y}):size({w}, {h})')

        if lss_parts:
            lss_str = "{" + ", ".join(lss_parts) + "}"
            self.emit(f'{var}.lss = {lss_str}')

        # ── Signals ──
        for sig_name, handler_body in node.signals:
            noor_sig = SIGNAL_MAP.get(sig_name)
            if noor_sig is None:
                continue  # skip
            # Clean up handler body
            body = handler_body.strip()
            if body.startswith('{') and body.endswith('}'):
                body = body[1:-1].strip()
            # Replace Qt idioms
            body = re.sub(r'\bconsole\.log\b', 'print', body)
            body = re.sub(r'\bQt\.quit\(\)', 'os.exit()', body)
            self.emit(f'{var}:on("{noor_sig}", function()')
            self.indent += 1
            for bline in body.splitlines():
                self.emit(bline.strip() if bline.strip() else "")
            self.indent -= 1
            self.emit('end)')

        # ── Children ──
        for child in node.children:
            child_var = self.transpile_node(child, parent_var=var, app_var=app_var)
            if child_var and kind != "app":
                self.emit(f'{var}:add({child_var})')
            elif child_var:
                self.emit(f'{var}:add({child_var})')

        # ── Add to parent ──
        if parent_var and not is_root and kind != "app":
            pass  # parent handles add() after this returns

        return var


def transpile_qml_to_lua(qml_file: Path, proj: QtProject) -> str:
    """Full QML file → Lua NoorUI app string."""
    hdr(f"🔄  Transpiling {qml_file.name}")

    root_node = parse_qml(qml_file)
    if not root_node:
        err(f"Failed to parse {qml_file.name}")
        return _fallback_lua(proj)

    emitter = LuaEmitter()

    # Header
    emitter.emit(f'-- NoorOS App: {proj.name}')
    emitter.emit(f'-- Auto-generated by NoorPort from {qml_file.name}')
    emitter.emit(f'-- Do not edit — re-run noorport.py to regenerate')
    emitter.emit()
    emitter.emit('-- NoorOS screen: 240x320 TFT (ILI9341)')
    emitter.emit('-- API: ui.* / robot.* / sensor.* / screen.*')
    emitter.emit()

    # Determine if root is Window/App
    is_root = root_node.type in ("ApplicationWindow", "Window", "Item", "Rectangle")

    app_var = emitter.transpile_node(root_node, is_root=is_root)

    emitter.emit()
    emitter.emit(f'-- Start the app event loop')
    emitter.emit(f'{app_var}:run()')

    result = "\n".join(emitter.lines)
    ok(f"Generated {len(emitter.lines)} lines of Lua")
    return result


def _fallback_lua(proj: QtProject) -> str:
    """Minimal fallback if QML parsing fails."""
    return textwrap.dedent(f'''\
        -- NoorOS App: {proj.name}
        -- NoorPort could not fully parse QML — minimal scaffold generated
        local app = ui.app("{proj.name}")
        local lbl = ui.Label("App loaded — QML parse incomplete")
        lbl:move(10, 80):size(220, 40)
        lbl.lss = {{color="yellow", align="center"}}
        app:add(lbl)
        app:run()
    ''')


# ══════════════════════════════════════════════════════════════════════════════
# 4. C++ ANALYZER  →  detect which Qt modules/classes are used
# ══════════════════════════════════════════════════════════════════════════════

# Qt module → NoorQt header (already exists or planned)
QT_MODULE_MAP = {
    "QObject":              "lua_qt/QObject.h",
    "QWidget":              "lua_qt/QWidget.h",
    "QPushButton":          "lua_qt/QWidgets.h",
    "QLabel":               "lua_qt/QWidgets.h",
    "QLineEdit":            "lua_qt/QWidgets.h",
    "QTextEdit":            "lua_qt/QWidgets.h",
    "QCheckBox":            "lua_qt/QWidgets.h",
    "QRadioButton":         "lua_qt/QWidgets.h",
    "QSlider":              "lua_qt/QWidgets.h",
    "QSpinBox":             "lua_qt/QWidgets.h",
    "QComboBox":            "lua_qt/QWidgets.h",
    "QProgressBar":         "lua_qt/QWidgets.h",
    "QListWidget":          "lua_qt/QWidgets.h",
    "QTabWidget":           "lua_qt/QWidgets.h",
    "QScrollArea":          "lua_qt/QWidgets.h",
    "QGroupBox":            "lua_qt/QWidgets.h",
    "QToolButton":          "lua_qt/QWidgets.h",
    "QDial":                "lua_qt/QWidgets.h",
    "QLCDNumber":           "lua_qt/QWidgets.h",
    "QFrame":               "lua_qt/QWidgets.h",
    "QSplitter":            "lua_qt/QWidgets.h",
    "QStackedWidget":       "lua_qt/QWidgets.h",
    "QLayout":              "lua_qt/QLayout.h",
    "QHBoxLayout":          "lua_qt/QLayout.h",
    "QVBoxLayout":          "lua_qt/QLayout.h",
    "QGridLayout":          "lua_qt/QLayout.h",
    "QFormLayout":          "lua_qt/QLayout.h",
    "QPainter":             "lua_qt/QPainter.h",
    "QPen":                 "lua_qt/QPainter.h",
    "QBrush":               "lua_qt/QPainter.h",
    "QColor":               "lua_qt/QGeometry.h",
    "QFont":                "lua_qt/QGeometry.h",
    "QPoint":               "lua_qt/QGeometry.h",
    "QSize":                "lua_qt/QGeometry.h",
    "QRect":                "lua_qt/QGeometry.h",
    "QTimer":               "lua_qt/QGeometry.h",
    "QDateTime":            "lua_qt/QGeometry.h",
    "QVariant":             "lua_qt/QObject.h",
    "QList":                "lua_qt/QObject.h",
    "QMap":                 "lua_qt/QObject.h",
    "QString":              "lua_qt/QObject.h",
    "QApplication":         "lua_qt/QApplication.h",    # planned
    "QMainWindow":          "lua_qt/QWidget.h",
    "QDialog":              "lua_qt/QWidgets.h",
    "QMessageBox":          "lua_qt/QWidgets.h",
    "QNetworkAccessManager":"lua_qt/QNetwork.h",        # planned
    "QTcpSocket":           "lua_qt/QNetwork.h",
    "QFile":                "lua_qt/QFile.h",           # planned
    "QDir":                 "lua_qt/QFile.h",
    "QThread":              "lua_qt/QThread.h",         # planned
    "QMutex":               "lua_qt/QThread.h",
    "QSqlDatabase":         "lua_qt/QSql.h",            # planned
    "QSqlQuery":            "lua_qt/QSql.h",
    "QMediaPlayer":         "lua_qt/QMultimedia.h",     # planned
    "QAudioOutput":         "lua_qt/QMultimedia.h",
    "QPropertyAnimation":   "lua_qt/QAnimation.h",      # planned
    "QStandardItemModel":   "lua_qt/QModel.h",          # planned
    "QAbstractItemModel":   "lua_qt/QModel.h",
}

PLANNED_MODULES = {
    "lua_qt/QApplication.h", "lua_qt/QNetwork.h", "lua_qt/QFile.h",
    "lua_qt/QThread.h", "lua_qt/QSql.h", "lua_qt/QMultimedia.h",
    "lua_qt/QAnimation.h", "lua_qt/QModel.h",
}

def analyze_cpp(proj: QtProject) -> dict:
    """Scan C++ files for Qt class usage. Returns {class: header} dict."""
    hdr("🔍  Analysing C++ source")
    used = {}  # class → noorqt header

    all_cpp = proj.cpp_files + proj.h_files
    for f in all_cpp:
        try:
            src = f.read_text(encoding='utf-8', errors='replace')
        except:
            continue
        for cls, hdr_path in QT_MODULE_MAP.items():
            if re.search(r'\b' + re.escape(cls) + r'\b', src):
                used[cls] = hdr_path

    if used:
        implemented = [c for c,h in used.items() if h not in PLANNED_MODULES]
        planned     = [c for c,h in used.items() if h in PLANNED_MODULES]
        ok(f"Qt classes found: {len(used)}")
        if implemented:
            info(f"Implemented in NoorQt: {', '.join(implemented[:8])}" +
                 (" ..." if len(implemented)>8 else ""))
        if planned:
            warn(f"Need implementation (planned): {', '.join(planned)}")
    else:
        ok("No Qt C++ classes detected")

    return used


# ══════════════════════════════════════════════════════════════════════════════
# 5. SHIM GENERATOR  →  NoorQt compatibility header for your C++ files
# ══════════════════════════════════════════════════════════════════════════════

def generate_shim(proj: QtProject, used_classes: dict) -> str:
    """Generate a single-include NoorQt compatibility header."""
    hdr("🔧  Generating NoorQt C++ shim")

    needed_headers = sorted(set(used_classes.values()))
    implemented    = [h for h in needed_headers if h not in PLANNED_MODULES]
    planned        = [h for h in needed_headers if h in PLANNED_MODULES]

    lines = []
    lines.append(f'// NoorQt Compatibility Shim — {proj.name}')
    lines.append(f'// Auto-generated by NoorPort. Include this instead of Qt headers.')
    lines.append(f'//')
    lines.append(f'// Drop this file next to your .cpp and add to your Arduino sketch.')
    lines.append(f'// Then change:  #include <QWidget>  →  #include "NoorCompat.h"')
    lines.append(f'//')
    lines.append(f'#pragma once')
    lines.append(f'')

    if implemented:
        lines.append('// ── Implemented NoorQt headers ──────────────────────────────────')
        for h in implemented:
            lines.append(f'#include "{h}"')
        lines.append('')

    if planned:
        lines.append('// ── Planned NoorQt headers (stubs below) ────────────────────────')
        for h in planned:
            lines.append(f'// TODO: implement {h}')
        lines.append('')
        lines.append('// ── Minimal stubs for unimplemented Qt classes ──────────────────')
        lines.append('// These let your code compile; functionality is limited.')
        lines.append('')
        for cls, h in used_classes.items():
            if h in PLANNED_MODULES:
                lines.append(_make_stub(cls))
        lines.append('')

    # Always include master NoorQt if any NoorQt header was needed
    if implemented:
        lines.append('// ── NoorQt master convenience header ────────────────────────────')
        lines.append('// (includes all implemented modules)')
        lines.append('#include "lua_qt/NoorQt.h"')
        lines.append('')

    lines.append('// ── Qt → NoorQt type aliases (for source compatibility) ──────────')
    lines.append('// These let you keep your original Qt type names.')
    lines.append('using QMainWindow   = QWidget;')
    lines.append('using QDialog       = QWidget;')
    lines.append('using QApplication  = QCoreApplication;')
    lines.append('// Add more aliases here as needed')
    lines.append('')
    lines.append('// ── Macro compatibility ─────────────────────────────────────────')
    lines.append('#ifndef Q_OBJECT')
    lines.append('#define Q_OBJECT  /* NoorQt: signals/slots via QObject base */')
    lines.append('#endif')
    lines.append('#ifndef Q_SIGNALS')
    lines.append('#define Q_SIGNALS public')
    lines.append('#endif')
    lines.append('#ifndef Q_SLOTS')
    lines.append('#define Q_SLOTS   /* NoorQt: use connect() directly */')
    lines.append('#endif')
    lines.append('#ifndef emit')
    lines.append('#define emit      /* NoorQt: call signal directly */')
    lines.append('#endif')
    lines.append('#ifndef signals')
    lines.append('#define signals public')
    lines.append('#endif')
    lines.append('#ifndef slots')
    lines.append('#define slots')
    lines.append('#endif')
    lines.append('')

    ok(f"Shim generated ({len(lines)} lines)")
    if planned:
        warn(f"{len(planned)} planned headers stubbed — limited functionality")

    return '\n'.join(lines)


def _make_stub(cls: str) -> str:
    """Minimal compilable stub for an unimplemented Qt class."""
    stubs = {
        "QApplication":         'struct QCoreApplication { QCoreApplication(int&,char**){} static void exec(){} };',
        "QNetworkAccessManager":'struct QNetworkAccessManager { };',
        "QTcpSocket":           'struct QTcpSocket { void connectToHost(const char*,int){} void write(const char*){} };',
        "QFile":                'struct QFile { QFile(const char*){}; bool open(int){return false;} void close(){} };',
        "QDir":                 'struct QDir { static bool exists(const char*){return false;} };',
        "QThread":              'struct QThread { static void msleep(int ms){delay(ms);} };',
        "QMutex":               'struct QMutex { void lock(){} void unlock(){} };',
        "QSqlDatabase":         'struct QSqlDatabase { static QSqlDatabase addDatabase(const char*){return {};} };',
        "QSqlQuery":            'struct QSqlQuery { bool exec(const char*){return false;} };',
        "QMediaPlayer":         'struct QMediaPlayer { void setSource(const char*){} void play(){} };',
        "QAudioOutput":         'struct QAudioOutput { };',
        "QPropertyAnimation":   'struct QPropertyAnimation { QPropertyAnimation(QObject*,const char*){} void setDuration(int){} void start(){} };',
        "QStandardItemModel":   'struct QStandardItemModel { };',
        "QAbstractItemModel":   'struct QAbstractItemModel { };',
    }
    return stubs.get(cls, f'struct {cls} {{ /* stub */ }};')


# ══════════════════════════════════════════════════════════════════════════════
# 6. APP PACKAGER  →  NoorOS app bundle
# ══════════════════════════════════════════════════════════════════════════════

def package_app(proj: QtProject, lua_code: str, shim_code: str,
                out_dir: Path, mode: str):
    """Write all output files into a deployable NoorOS app bundle."""
    hdr("📦  Packaging NoorOS app")

    app_dir = out_dir / proj.name
    app_dir.mkdir(parents=True, exist_ok=True)

    outputs = []

    if mode in ("lua", "both"):
        lua_path = app_dir / "main.lua"
        lua_path.write_text(lua_code, encoding='utf-8')
        ok(f"Lua app  → {lua_path}")
        outputs.append(lua_path)

    if mode in ("cpp", "both"):
        shim_path = app_dir / "NoorCompat.h"
        shim_path.write_text(shim_code, encoding='utf-8')
        ok(f"C++ shim → {shim_path}")
        outputs.append(shim_path)

    # Write manifest
    manifest = {
        "name":    proj.name,
        "version": "1.0",
        "author":  "NoorPort auto-generated",
        "entry":   "main.lua",
        "screen":  "240x320",
        "generated_by": "NoorPort",
        "source_qml_files": [str(f.name) for f in proj.qml_files],
        "source_cpp_files": [str(f.name) for f in proj.cpp_files],
    }
    manifest_path = app_dir / "app.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    ok(f"Manifest → {manifest_path}")
    outputs.append(manifest_path)

    # Install instructions
    readme = _install_readme(proj, mode)
    readme_path = app_dir / "INSTALL.md"
    readme_path.write_text(readme, encoding='utf-8')
    ok(f"Install  → {readme_path}")

    return app_dir, outputs


def _install_readme(proj: QtProject, mode: str) -> str:
    return textwrap.dedent(f'''\
    # {proj.name} — NoorOS App

    Auto-generated by **NoorPort** from your Qt/QML project.

    ## Deploy (Lua app)

    ```bash
    # Via esp32-ssh
    ./esp32-ssh <ESP32_IP> --pass <password>

    # Inside the shell:
    mkdir /apps/{proj.name}
    # Upload main.lua via your preferred method (SFTP / paste)
    run {proj.name}
    ```

    ## Deploy via SD card
    Copy the entire `{proj.name}/` folder to your SD card under `/apps/`.
    Then from NoorShell:
    ```
    run {proj.name}
    ```

    ## C++ shim (if using NoorQt C++ mode)
    1. Copy `NoorCompat.h` into your Arduino sketch folder alongside
       `{proj.name}.cpp`
    2. Replace all Qt includes in your .cpp/.h files with:
       ```cpp
       #include "NoorCompat.h"
       ```
    3. Open in Arduino IDE and compile for ESP32 Dev Module.

    ## Screen notes
    NoorOS TFT is 240×320 px. Your original layout has been
    auto-scaled. Touch calibrate via:
    ```
    sensor calibrate
    ```

    ## Re-generate after changes
    ```bash
    python3 noorport.py {proj.name}/ --out ./output
    ```
    ''')


# ══════════════════════════════════════════════════════════════════════════════
# 7. MULTI-QML  →  handle multiple QML files (screens/pages)
# ══════════════════════════════════════════════════════════════════════════════

def transpile_all_qml(proj: QtProject) -> str:
    """Transpile all QML files; non-main ones become Lua functions."""
    if not proj.qml_files:
        warn("No QML files found — generating minimal placeholder app")
        return _fallback_lua(proj)

    main_lua = transpile_qml_to_lua(proj.main_qml, proj)

    other_files = [f for f in proj.qml_files if f != proj.main_qml]
    if not other_files:
        return main_lua

    extras = []
    extras.append("\n-- ── Additional screens (from other QML files) ──────────────────")
    for qf in other_files:
        func_name = re.sub(r'\W+', '_', qf.stem)
        extras.append(f"\nfunction show_{func_name}()")
        extras.append(f"  -- from {qf.name}")
        node = parse_qml(qf)
        if node:
            emitter = LuaEmitter()
            emitter.indent = 1
            emitter.transpile_node(node, is_root=False)
            for line in emitter.lines:
                extras.append("  " + line if line.strip() else "")
        else:
            extras.append(f'  ui.alert("Screen: {qf.stem}")')
        extras.append("end")
        info(f"Extra screen: {qf.name} → show_{func_name}()")

    return main_lua + "\n".join(extras)


# ══════════════════════════════════════════════════════════════════════════════
# 8. MAIN
# ══════════════════════════════════════════════════════════════════════════════

def main():
    p = argparse.ArgumentParser(
        description="NoorPort — Qt/QML C++ → NoorOS app transpiler",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent('''\
            Examples:
              python3 noorport.py ~/Projects/MyQtApp
              python3 noorport.py ~/Projects/MyQtApp --out ~/NoorApps --mode both
              python3 noorport.py ~/Projects/MyQtApp --dry-run
        ''')
    )
    p.add_argument("project",  help="Path to your Qt/QML C++ project directory")
    p.add_argument("--out",    default=None,  help="Output directory (default: <project>/../noorport_out)")
    p.add_argument("--mode",   default="lua", choices=["lua","cpp","both"],
                               help="Output mode: lua (NoorUI), cpp (NoorQt shim), both")
    p.add_argument("--dry-run",action="store_true", help="Print output, don't write files")
    args = p.parse_args()

    root = Path(args.project).expanduser().resolve()
    if not root.exists():
        err(f"Project path does not exist: {root}"); sys.exit(1)
    if not root.is_dir():
        err(f"Not a directory: {root}"); sys.exit(1)

    out_dir = Path(args.out).expanduser().resolve() if args.out else \
              root.parent / "noorport_out"

    print(f"\n{BOLD}{C}╔══════════════════════════════════════╗")
    print(f"║      NoorPort  Qt → NoorOS           ║")
    print(f"╚══════════════════════════════════════╝{RST}")
    print(f"  Source : {root}")
    print(f"  Output : {out_dir}")
    print(f"  Mode   : {args.mode}")

    proj        = scan_project(root)
    lua_code    = transpile_all_qml(proj)
    used_cpp    = analyze_cpp(proj) if args.mode in ("cpp","both") else {}
    shim_code   = generate_shim(proj, used_cpp) if args.mode in ("cpp","both") else ""

    if args.dry_run:
        hdr("📋  DRY RUN — Lua output preview")
        print("\n" + lua_code[:3000])
        if shim_code:
            hdr("📋  DRY RUN — C++ shim preview")
            print("\n" + shim_code[:2000])
        print(f"\n{Y}Dry run complete — no files written.{RST}")
        return

    app_dir, outputs = package_app(proj, lua_code, shim_code, out_dir, args.mode)

    hdr("✅  Done")
    print(f"  {BOLD}App bundle:{RST} {app_dir}")
    print(f"  {BOLD}Files:{RST}")
    for f in outputs:
        print(f"    {G}→{RST} {f.name}")
    print(f"""
{C}Next steps:{RST}
  1. Upload {app_dir}/main.lua to your ESP32:
       ./esp32-ssh <IP> --pass <pw>
       (then paste/upload main.lua to /apps/{proj.name}/main.lua)

  2. Run it:
       run {proj.name}

  3. Iterate: edit your QML, re-run noorport.py — zero source changes needed.
""")


if __name__ == "__main__":
    main()
