"""
robot-face-demo -- example CLIENT GUI app for esp32-ssh.

Demonstrates all three bridge packages from a PyQt5 interface, styled as
the same tiny "desktop OS" shell as esp32-app: a dock of glowing app
icons, each app opening in its own custom-chrome animated window, with a
taskbar tracking what's open.

  - oled.eyes()/oled.clear()  -> runs on the ESP32 (via NoorShell)
  - lua.run()                  -> runs Lua code on the ESP32 (via NoorShell)
  - device.run()               -> runs Python on THIS machine's CPU/GPU

Run it with:
    esp32-ssh --app path\\to\\apps\\robot-face-demo

(esp32-ssh sets PYTHONPATH to each packages/ subfolder before launching this
script, which is why "import lua" / "import device" / "import oled" work
here without any relative-import gymnastics. Running this file directly
with a bare `python main.py` will fail on those imports -- that's expected,
since nothing else sets PYTHONPATH up for you.)
"""
import sys

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGridLayout, QPushButton, QLabel, QLineEdit, QTextEdit, QMessageBox,
    QToolButton, QMdiArea, QFrame, QGraphicsDropShadowEffect,
    QGraphicsOpacityEffect, QSizeGrip,
)
from PyQt5.QtCore import (
    QThread, pyqtSignal, Qt, QSize, QPropertyAnimation, QEasingCurve,
    QParallelAnimationGroup,
)
from PyQt5.QtGui import (
    QIcon, QPixmap, QPainter, QColor, QBrush, QPen, QFont, QLinearGradient,
)

import lua
import device
import oled


# ---------------------------------------------------------------------------
# Shared plumbing (mirrors esp32-app's Worker/WorkerMixin so every button
# click round-trips on a background thread instead of freezing the GUI).
# ---------------------------------------------------------------------------

class WorkerMixin:
    """Keeps every in-flight Worker referenced in a list so a second async
    call can't garbage-collect a still-running QThread out from under
    itself (undefined behavior in Qt)."""

    def _start_worker(self, fn, on_done, on_fail=None):
        if not hasattr(self, "_workers"):
            self._workers = []
        worker = Worker(fn)
        worker.succeeded.connect(on_done)
        if on_fail is not None:
            worker.failed.connect(on_fail)
        worker.finished.connect(lambda w=worker: self._workers.remove(w) if w in self._workers else None)
        self._workers.append(worker)
        worker.start()
        return worker


class Worker(QThread):
    """Runs one blocking call (lua.run / device.run / oled.eyes / etc.) on
    a background thread so the GUI never freezes while esp32-ssh's
    subprocess round-trips over the network."""
    succeeded = pyqtSignal(object)
    failed = pyqtSignal(str)

    def __init__(self, fn):
        super().__init__()
        self.fn = fn

    def run(self):
        try:
            result = self.fn()
        except Exception as e:
            self.failed.emit(str(e))
        else:
            self.succeeded.emit(result)


def _flash_in(widget, duration: int = 260):
    """Fades a widget's contents in from transparent whenever it
    repopulates after an async result, instead of snapping into place."""
    effect = QGraphicsOpacityEffect(widget)
    widget.setGraphicsEffect(effect)
    anim = QPropertyAnimation(effect, b"opacity", widget)
    anim.setDuration(duration)
    anim.setStartValue(0.0)
    anim.setEndValue(1.0)
    anim.setEasingCurve(QEasingCurve.OutCubic)
    widget._flash_anim = anim
    anim.start()


def _shadow(blur=34, dy=10, alpha=185):
    effect = QGraphicsDropShadowEffect()
    effect.setBlurRadius(blur)
    effect.setOffset(0, dy)
    effect.setColor(QColor(0, 0, 0, alpha))
    return effect


def _wallpaper_brush() -> QBrush:
    tile = QPixmap(26, 26)
    tile.fill(QColor("#0d1117"))
    p = QPainter(tile)
    p.setRenderHint(QPainter.Antialiasing)
    p.setPen(Qt.NoPen)
    p.setBrush(QColor(255, 255, 255, 16))
    p.drawEllipse(11, 11, 3, 3)
    p.end()
    return QBrush(tile)


def _icon_base(color1: str, color2: str, size: int = 72):
    pix = QPixmap(size, size)
    pix.fill(Qt.transparent)
    p = QPainter(pix)
    p.setRenderHint(QPainter.Antialiasing)
    grad = QLinearGradient(0, 0, 0, size)
    grad.setColorAt(0, QColor(color1))
    grad.setColorAt(1, QColor(color2))
    p.setBrush(QBrush(grad))
    p.setPen(Qt.NoPen)
    p.drawRoundedRect(3, 3, size - 6, size - 6, 16, 16)
    return pix, p


def _icon_face(size: int = 72) -> QIcon:
    pix, p = _icon_base("#ff6b9d", "#c23b7a", size)
    p.setBrush(QBrush(QColor("#fffaf0")))
    p.setPen(Qt.NoPen)
    r = size * 0.34
    p.drawEllipse(int(size / 2 - r), int(size * 0.44 - r), int(r * 2), int(r * 2))
    p.setBrush(QBrush(QColor("#2a2030")))
    eye_r = size * 0.045
    for dx in (-0.14, 0.14):
        cx = size / 2 + dx * size
        p.drawEllipse(int(cx - eye_r), int(size * 0.4 - eye_r), int(eye_r * 2), int(eye_r * 2))
    pen = QPen(QColor("#2a2030"))
    pen.setWidthF(max(1.6, size * 0.035))
    pen.setCapStyle(Qt.RoundCap)
    p.setPen(pen)
    p.setBrush(Qt.NoBrush)
    smile_rect = (size / 2 - size * 0.14, size * 0.42, size * 0.28, size * 0.18)
    p.drawArc(int(smile_rect[0]), int(smile_rect[1]), int(smile_rect[2]), int(smile_rect[3]), 200 * 16, 140 * 16)
    p.end()
    return QIcon(pix)


def _icon_lua(size: int = 72) -> QIcon:
    pix, p = _icon_base("#2f3542", "#12151c", size)
    pen = QPen(QColor("#33ff77"))
    pen.setWidth(max(2, size // 16))
    pen.setCapStyle(Qt.RoundCap)
    p.setPen(pen)
    x0, y0 = size * 0.24, size * 0.38
    p.drawLine(int(x0), int(y0), int(x0 + size * 0.16), int(y0 + size * 0.14))
    p.drawLine(int(x0), int(y0 + size * 0.28), int(x0 + size * 0.16), int(y0 + size * 0.14))
    p.drawLine(int(x0 + size * 0.22), int(y0 + size * 0.28),
               int(x0 + size * 0.46), int(y0 + size * 0.28))
    p.end()
    return QIcon(pix)


def _icon_calc(size: int = 72) -> QIcon:
    pix, p = _icon_base("#4facfe", "#2266d8", size)
    p.setBrush(QBrush(QColor("#ffffff")))
    p.setPen(Qt.NoPen)
    body_w, body_h = size * 0.5, size * 0.62
    x, y = (size - body_w) / 2, (size - body_h) / 2
    p.drawRoundedRect(int(x), int(y), int(body_w), int(body_h), 6, 6)
    p.setBrush(QBrush(QColor("#2266d8")))
    screen_h = body_h * 0.2
    p.drawRoundedRect(int(x + body_w * 0.1), int(y + body_h * 0.08),
                       int(body_w * 0.8), int(screen_h), 3, 3)
    pad, gap = body_w * 0.1, body_w * 0.08
    cell = (body_w - 2 * pad - 2 * gap) / 3
    top = y + body_h * 0.36
    for row in range(3):
        for col in range(3):
            cx = x + pad + col * (cell + gap)
            cy = top + row * (cell + gap)
            p.drawRoundedRect(int(cx), int(cy), int(cell), int(cell), 2, 2)
    p.end()
    return QIcon(pix)


_EXPR_PALETTE = (
    ("#4facfe", "#2266d8"), ("#a06bff", "#6f3ef0"), ("#3ddc84", "#1a9b56"),
    ("#ffd166", "#f7a531"), ("#ff6b6b", "#c23b3b"), ("#22d3ee", "#0e7f96"),
    ("#ff6b9d", "#c23b7a"), ("#8bd450", "#4f9c1e"),
)


def _expression_icon(name: str, size: int = 56) -> QIcon:
    """A colored tile with two eye-dots and an expression-shaped mouth,
    so every emotion in the grid reads as its own little face instead of
    a plain text button."""
    color1, color2 = _EXPR_PALETTE[hash(name) % len(_EXPR_PALETTE)]
    pix, p = _icon_base(color1, color2, size)
    p.setBrush(QBrush(QColor("#fffaf0")))
    p.setPen(Qt.NoPen)
    eye_r = size * 0.06
    for dx in (-0.16, 0.16):
        cx = size / 2 + dx * size
        p.drawEllipse(int(cx - eye_r), int(size * 0.4 - eye_r), int(eye_r * 2), int(eye_r * 2))
    pen = QPen(QColor("#fffaf0"))
    pen.setWidthF(max(1.8, size * 0.045))
    pen.setCapStyle(Qt.RoundCap)
    p.setPen(pen)
    p.setBrush(Qt.NoBrush)
    key = name.lower()
    mouth_rect = (size / 2 - size * 0.16, size * 0.5, size * 0.32, size * 0.2)
    if any(k in key for k in ("sad", "angry", "mad")):
        p.drawArc(int(mouth_rect[0]), int(mouth_rect[1] - size * 0.06),
                   int(mouth_rect[2]), int(mouth_rect[3]), 20 * 16, 140 * 16)
    elif any(k in key for k in ("surpris", "shock", "wow", "wide")):
        p.setBrush(QBrush(QColor("#fffaf0")))
        p.setPen(Qt.NoPen)
        p.drawEllipse(int(size / 2 - size * 0.07), int(size * 0.52),
                       int(size * 0.14), int(size * 0.14))
    elif any(k in key for k in ("sleep", "wink", "blink", "close")):
        p.drawLine(int(size * 0.3), int(size * 0.58), int(size * 0.7), int(size * 0.58))
    else:
        p.drawArc(int(mouth_rect[0]), int(mouth_rect[1]), int(mouth_rect[2]),
                   int(mouth_rect[3]), 200 * 16, 140 * 16)
    p.end()
    return QIcon(pix)



class ExpressionTile(QToolButton):
    """One clickable expression tile in the Face grid -- an icon with a
    label, that lifts and glows on hover and squashes on press, exactly
    like the dock icons, so sending an expression feels like tapping a
    real app rather than clicking a text button."""

    BASE_SIZE = 46
    HOVER_SIZE = 52
    PRESS_SIZE = 40

    def __init__(self, icon: QIcon, label: str, glow_color: str):
        super().__init__()
        self.setIcon(icon)
        self.setIconSize(QSize(self.BASE_SIZE, self.BASE_SIZE))
        self.setText(label)
        self.setToolButtonStyle(Qt.ToolButtonTextUnderIcon)
        self.setFixedSize(84, 84)
        self.setCursor(Qt.PointingHandCursor)
        self.setAutoRaise(True)
        self.setStyleSheet("""
            QToolButton {
                color: #eef2f7;
                background: rgba(255, 255, 255, 10);
                border: 1px solid #2a3040;
                border-radius: 12px;
                font-size: 10px;
                padding-top: 4px;
            }
            QToolButton:hover { background: rgba(255, 255, 255, 26); border: 1px solid #7c98ff; }
            QToolButton:pressed { background: rgba(255, 255, 255, 46); }
        """)

        self._glow = _shadow(blur=0, dy=0, alpha=0)
        self._glow.setColor(QColor(glow_color))
        self.setGraphicsEffect(self._glow)

        self._size_anim = QPropertyAnimation(self, b"iconSize")
        self._size_anim.setDuration(140)
        self._size_anim.setEasingCurve(QEasingCurve.OutCubic)

        self._glow_anim = QPropertyAnimation(self._glow, b"blurRadius")
        self._glow_anim.setDuration(140)

    def _animate(self, target: int, glow_blur: int):
        for anim, end in ((self._size_anim, QSize(target, target)), (self._glow_anim, glow_blur)):
            anim.stop()
            anim.setEndValue(end)
            anim.start()

    def enterEvent(self, event):
        self._animate(self.HOVER_SIZE, 20)
        super().enterEvent(event)

    def leaveEvent(self, event):
        self._animate(self.BASE_SIZE, 0)
        super().leaveEvent(event)

    def mousePressEvent(self, event):
        self._animate(self.PRESS_SIZE, 4)
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        target = self.HOVER_SIZE if self.underMouse() else self.BASE_SIZE
        self._animate(target, 20 if self.underMouse() else 0)
        super().mouseReleaseEvent(event)


class DesktopIcon(QToolButton):
    """One clickable app tile in the dock -- grows and glows on hover,
    squashes on press."""

    BASE_SIZE = 56
    HOVER_SIZE = 64
    PRESS_SIZE = 48

    def __init__(self, icon: QIcon, label: str, glow_color: str):
        super().__init__()
        self.setIcon(icon)
        self.setIconSize(QSize(self.BASE_SIZE, self.BASE_SIZE))
        self.setText(label)
        self.setToolButtonStyle(Qt.ToolButtonTextUnderIcon)
        self.setFixedSize(96, 98)
        self.setCursor(Qt.PointingHandCursor)
        self.setAutoRaise(True)
        self.setStyleSheet("""
            QToolButton {
                color: #eef2f7;
                background: transparent;
                border: none;
                border-radius: 12px;
                font-size: 11px;
                padding-top: 4px;
            }
            QToolButton:hover { background: rgba(255, 255, 255, 26); }
            QToolButton:pressed { background: rgba(255, 255, 255, 46); }
        """)

        self._glow = _shadow(blur=0, dy=0, alpha=0)
        self._glow.setColor(QColor(glow_color))
        self.setGraphicsEffect(self._glow)

        self._size_anim = QPropertyAnimation(self, b"iconSize")
        self._size_anim.setDuration(160)
        self._size_anim.setEasingCurve(QEasingCurve.OutCubic)

        self._glow_anim = QPropertyAnimation(self._glow, b"blurRadius")
        self._glow_anim.setDuration(160)

    def _animate_icon(self, target: int, glow_blur: int):
        for anim, end in ((self._size_anim, QSize(target, target)), (self._glow_anim, glow_blur)):
            anim.stop()
            anim.setEndValue(end)
            anim.start()

    def enterEvent(self, event):
        self._animate_icon(self.HOVER_SIZE, 24)
        super().enterEvent(event)

    def leaveEvent(self, event):
        self._animate_icon(self.BASE_SIZE, 0)
        super().leaveEvent(event)

    def mousePressEvent(self, event):
        self._animate_icon(self.PRESS_SIZE, 6)
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        target = self.HOVER_SIZE if self.underMouse() else self.BASE_SIZE
        self._animate_icon(target, 24 if self.underMouse() else 0)
        super().mouseReleaseEvent(event)



class RunningDot(QLabel):
    """A tiny colored dot under a dock icon shown while that app has a
    window open."""

    def __init__(self, color: str):
        super().__init__()
        self.setFixedSize(7, 7)
        self.setStyleSheet(f"background: {color}; border-radius: 3px;")
        self.setVisible(False)


class TaskbarButton(QToolButton):
    """A running app's entry in the bottom taskbar."""

    def __init__(self, icon: QIcon, label: str):
        super().__init__()
        self.setIcon(icon)
        self.setIconSize(QSize(18, 18))
        self.setText(" " + label)
        self.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
        self.setCursor(Qt.PointingHandCursor)
        self.set_active(False)

        self._opacity_effect = QGraphicsOpacityEffect()
        self._opacity_effect.setOpacity(0.0)
        self.setGraphicsEffect(self._opacity_effect)
        self._fade_in = QPropertyAnimation(self._opacity_effect, b"opacity")
        self._fade_in.setDuration(220)
        self._fade_in.setStartValue(0.0)
        self._fade_in.setEndValue(1.0)
        self._fade_in.setEasingCurve(QEasingCurve.OutCubic)
        self._fade_in.start()

    def set_active(self, active: bool):
        border = "#6f8cff" if active else "rgba(255, 255, 255, 40)"
        bg = "rgba(111, 140, 255, 45)" if active else "rgba(255, 255, 255, 20)"
        self.setStyleSheet(f"""
            QToolButton {{
                color: #eef2f7;
                background: {bg};
                border: 1px solid {border};
                border-bottom: 2px solid {"#6f8cff" if active else border};
                border-radius: 6px;
                padding: 4px 10px;
            }}
            QToolButton:hover {{ background: rgba(255, 255, 255, 45); }}
        """)


class TitleBar(QWidget):
    """A fully custom, app-tinted title bar replacing native OS chrome.
    Dragging moves the window; buttons minimize/maximize/close it."""

    def __init__(self, subwindow, icon: QIcon, title: str,
                 color1: str, color2: str, on_close):
        super().__init__()
        self.subwindow = subwindow
        self._on_close = on_close
        self._drag_pos = None
        self._start_pos = None
        self._maximized = False

        self.setFixedHeight(34)
        self.setStyleSheet(f"""
            QWidget {{
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 {color1}, stop:1 {color2});
                border-top-left-radius: 9px;
                border-top-right-radius: 9px;
            }}
            QLabel {{ color: #10131a; font-weight: 600; background: transparent; }}
            QToolButton {{
                color: #10131a;
                background: transparent;
                border: none;
                border-radius: 5px;
                font-weight: bold;
                font-size: 13px;
            }}
            QToolButton:hover {{ background: rgba(0, 0, 0, 55); color: #ffffff; }}
        """)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(10, 0, 6, 0)
        layout.setSpacing(4)

        icon_label = QLabel()
        icon_label.setPixmap(icon.pixmap(16, 16))
        layout.addWidget(icon_label)

        text = QLabel(title)
        layout.addWidget(text)
        layout.addStretch(1)

        min_btn = QToolButton()
        min_btn.setText("—")
        min_btn.setFixedSize(26, 24)
        min_btn.setCursor(Qt.PointingHandCursor)
        min_btn.clicked.connect(lambda: self.subwindow.hide())
        layout.addWidget(min_btn)

        self._max_btn = QToolButton()
        self._max_btn.setText("▢")
        self._max_btn.setFixedSize(26, 24)
        self._max_btn.setCursor(Qt.PointingHandCursor)
        self._max_btn.clicked.connect(self._toggle_maximize)
        layout.addWidget(self._max_btn)

        close_btn = QToolButton()
        close_btn.setText("✕")
        close_btn.setFixedSize(26, 24)
        close_btn.setCursor(Qt.PointingHandCursor)
        close_btn.setStyleSheet("QToolButton:hover { background: #e04b4b; color: white; }")
        close_btn.clicked.connect(self._on_close)
        layout.addWidget(close_btn)

    def _toggle_maximize(self):
        if self._maximized:
            self.subwindow.showNormal()
        else:
            self.subwindow.showMaximized()
        self._maximized = not self._maximized

    def mouseDoubleClickEvent(self, event):
        self._toggle_maximize()

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._drag_pos = event.globalPos()
            self._start_pos = self.subwindow.pos()

    def mouseMoveEvent(self, event):
        if self._drag_pos is not None and not self._maximized:
            delta = event.globalPos() - self._drag_pos
            self.subwindow.move(self._start_pos + delta)

    def mouseReleaseEvent(self, event):
        self._drag_pos = None



class FaceTab(QWidget, WorkerMixin):
    """A grid of expression tiles (icon + label, glow on hover) replacing
    the old plain QPushButton grid -- each tap runs oled.eyes(expression)
    on a background Worker."""

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setSpacing(10)

        header = QLabel("Send an expression to the ESP32's OLED eyes")
        header.setStyleSheet("color: #7fa8ff; font-weight: 600;")
        layout.addWidget(header)

        grid = QGridLayout()
        grid.setSpacing(10)
        cols = 4
        for i, expr in enumerate(oled.EXPRESSIONS):
            color1, _ = _EXPR_PALETTE[hash(expr) % len(_EXPR_PALETTE)]
            tile = ExpressionTile(_expression_icon(expr), expr, color1)
            tile.clicked.connect(lambda _checked=False, e=expr: self._send_eyes(e))
            grid.addWidget(tile, i // cols, i % cols)
        layout.addLayout(grid)
        layout.addStretch(1)

        clear_btn = QPushButton("Clear eyes")
        clear_btn.clicked.connect(self._send_clear)
        layout.addWidget(clear_btn)

        self.status = QLabel("Ready.")
        self.status.setStyleSheet("color: #8891a5;")
        layout.addWidget(self.status)

    def _run_async(self, fn, on_done_label):
        self.status.setText("Sending to ESP32...")
        self._start_worker(
            fn,
            lambda r: (self.status.setText(f"{on_done_label}: {r}"), _flash_in(self.status)),
            lambda err: self.status.setText(f"Error: {err}"),
        )

    def _send_eyes(self, expression):
        self._run_async(lambda: oled.eyes(expression), f"eyes({expression})")

    def _send_clear(self):
        self._run_async(lambda: oled.clear(), "clear()")


class LuaConsoleTab(QWidget, WorkerMixin):
    """A one-line Lua REPL that executes on the ESP32 itself via
    lua.run(code), styled like a real terminal (monospace, green-tinted,
    glowing focus border) instead of a plain QLineEdit + QTextEdit."""

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        self.output = QTextEdit()
        self.output.setReadOnly(True)
        self.output.setPlaceholderText("Lua output will appear here.")
        self.output.setStyleSheet("""
            QTextEdit {
                background: #0a0f0a;
                color: #7fe89a;
                font-family: Consolas, monospace;
                border: 1px solid #1f3a24;
                border-radius: 6px;
            }
        """)
        layout.addWidget(self.output, 1)

        row = QHBoxLayout()
        self.input = QLineEdit()
        self.input.setPlaceholderText("e.g. print(1+1)")
        self.input.setStyleSheet("""
            QLineEdit {
                background: #0a0f0a;
                color: #7fe89a;
                font-family: Consolas, monospace;
                border: 1px solid #1f3a24;
            }
            QLineEdit:focus { border: 1px solid #3ddc84; }
        """)
        self.input.returnPressed.connect(self._run)
        run_btn = QPushButton("Run on ESP32")
        run_btn.clicked.connect(self._run)
        row.addWidget(self.input, 1)
        row.addWidget(run_btn)
        layout.addLayout(row)

    def _run(self):
        code = self.input.text().strip()
        if not code:
            return
        self.input.clear()
        self.output.append(f'<span style="color:#3ddc84;">&gt; {code}</span>')
        self._start_worker(
            lambda: lua.run(code),
            lambda r: self.output.append(str(r)),
            lambda err: self.output.append(f'<span style="color:#e0555f;">error: {err}</span>'),
        )


class DeviceCalculatorTab(QWidget):
    """Runs a Python expression on THIS machine (client CPU/GPU) via
    device.run() -- styled as a small calculator card with an animated
    result readout instead of a bare label."""

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setSpacing(10)

        header = QLabel("Runs locally on this PC -- no network round trip")
        header.setStyleSheet("color: #7fa8ff; font-weight: 600;")
        layout.addWidget(header)

        row = QHBoxLayout()
        self.input = QLineEdit()
        self.input.setPlaceholderText("e.g. 2 ** 10, or x = 5 then x * 2")
        self.input.returnPressed.connect(self._calculate)
        calc_btn = QPushButton("Calculate")
        calc_btn.clicked.connect(self._calculate)
        row.addWidget(self.input, 1)
        row.addWidget(calc_btn)
        layout.addLayout(row)

        result_frame = QFrame()
        result_frame.setStyleSheet("""
            QFrame {
                background: #0b0e14;
                border: 1px solid #2a3040;
                border-radius: 8px;
            }
        """)
        result_layout = QVBoxLayout(result_frame)
        self.result = QLabel("Result: (nothing yet)")
        self.result.setStyleSheet("color: #dce4f2; font-family: Consolas, monospace; font-size: 14px;")
        result_layout.addWidget(self.result)
        layout.addWidget(result_frame)

        layout.addStretch(1)

        install_row = QHBoxLayout()
        self.pkg_input = QLineEdit()
        self.pkg_input.setPlaceholderText("pip package(s), comma-separated, e.g. numpy, requests")
        install_btn = QPushButton("Install into this interpreter")
        install_btn.clicked.connect(self._install)
        install_row.addWidget(self.pkg_input, 1)
        install_row.addWidget(install_btn)
        layout.addLayout(install_row)

    def _calculate(self):
        expr = self.input.text().strip()
        if not expr:
            return
        outcome = device.run(expr)
        self.result.setText(f"Result: {outcome}")
        _flash_in(self.result)

    def _install(self):
        raw = self.pkg_input.text().strip()
        if not raw:
            return
        packages = [p.strip() for p in raw.split(",") if p.strip()]
        try:
            device.install(packages)
        except Exception as e:
            QMessageBox.warning(self, "pip install failed", str(e))
        else:
            QMessageBox.information(self, "Done", f"Installed: {', '.join(packages)}")



APP_STYLESHEET = """
QWidget {
    background: #10131a;
    color: #e8edf5;
    font-family: "Segoe UI", sans-serif;
    font-size: 12px;
}
QLabel { background: transparent; color: #c9d3e0; }
QMainWindow, QMdiArea { background: #0d1117; }
QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #4c5770, stop:1 #394258);
    color: #f5f8ff;
    border: 1px solid #6a76a0;
    border-radius: 8px;
    padding: 7px 16px;
    font-weight: 500;
}
QPushButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #5c688a, stop:1 #454f6e);
    border: 1px solid #8fa6ff;
}
QPushButton:pressed { background: #2c3348; }
QLineEdit, QTextEdit {
    background: #0b0e14;
    color: #dce4f2;
    border: 1px solid #2a3040;
    border-radius: 6px;
    padding: 5px 7px;
    selection-background-color: #6f8cff;
    selection-color: #0b0e14;
}
QLineEdit:focus, QTextEdit:focus { border: 1px solid #6f8cff; }
QScrollBar:vertical { background: #0d1117; width: 11px; margin: 2px; }
QScrollBar::handle:vertical { background: #33394a; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #495066; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QMessageBox { background: #171b24; }
"""


class MainWindow(QMainWindow):
    """The same tiny-desktop-OS shell as esp32-app: a dock of glowing app
    icons on the left, a workspace where each app opens in its own
    custom-chrome animated window, and a taskbar along the bottom."""

    APPS = (
        ("face", "Face", _icon_face, FaceTab, ("#ff6b9d", "#c23b7a")),
        ("lua", "Lua Console", _icon_lua, LuaConsoleTab, ("#3ddc84", "#1a9b56")),
        ("calc", "Calculator", _icon_calc, DeviceCalculatorTab, ("#4facfe", "#2266d8")),
    )

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Robot Face Demo -- esp32-ssh CLIENT GUI")
        self.resize(1000, 660)
        self._open_windows = {}
        self._taskbar_buttons = {}
        self._dots = {}
        self._anims = []

        central = QWidget()
        outer = QVBoxLayout(central)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        body = QHBoxLayout()
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)
        outer.addLayout(body, 1)

        dock = QFrame()
        dock.setFixedWidth(128)
        dock.setStyleSheet("""
            QFrame {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #1c2431, stop:1 #12161f);
                border-right: 1px solid #05070a;
            }
        """)
        dock_layout = QVBoxLayout(dock)
        dock_layout.setContentsMargins(12, 18, 12, 12)
        dock_layout.setSpacing(6)

        title = QLabel("ROBOT FACE")
        title.setStyleSheet("color: #7fa8ff; font-weight: bold; font-size: 13px; background: transparent;")
        title.setAlignment(Qt.AlignCenter)
        dock_layout.addWidget(title)
        dock_layout.addSpacing(10)

        self._icon_factories = {}
        for key, label, icon_fn, tab_cls, colors in self.APPS:
            icon = icon_fn()
            self._icon_factories[key] = (icon, label, tab_cls, colors)

            btn = DesktopIcon(icon, label, colors[0])
            btn.clicked.connect(lambda _checked=False, k=key: self._open_app(k))
            dock_layout.addWidget(btn, 0, Qt.AlignHCenter)

            dot = RunningDot(colors[0])
            self._dots[key] = dot
            dock_layout.addWidget(dot, 0, Qt.AlignHCenter)
            dock_layout.addSpacing(4)

        dock_layout.addStretch(1)
        body.addWidget(dock)

        self.mdi = QMdiArea()
        self.mdi.setViewMode(QMdiArea.SubWindowView)
        self.mdi.setBackground(_wallpaper_brush())
        self.mdi.setStyleSheet("QMdiArea { border: none; }")
        self.mdi.subWindowActivated.connect(self._on_active_changed)
        body.addWidget(self.mdi, 1)

        self.taskbar = QFrame()
        self.taskbar.setFixedHeight(44)
        self.taskbar.setStyleSheet("QFrame { background: #171b24; border-top: 1px solid #05070a; }")
        self.taskbar_layout = QHBoxLayout(self.taskbar)
        self.taskbar_layout.setContentsMargins(10, 6, 10, 6)
        self.taskbar_layout.setSpacing(8)
        self.taskbar_layout.addStretch(1)
        outer.addWidget(self.taskbar)

        self.setCentralWidget(central)
        self._open_app("face")

    def _open_app(self, key: str):
        existing = self._open_windows.get(key)
        if existing is not None:
            existing.show()
            existing.showNormal()
            self.mdi.setActiveSubWindow(existing)
            existing.raise_()
            return

        icon, label, tab_cls, colors = self._icon_factories[key]

        container = QFrame()
        container.setObjectName("subwindowFrame")
        container.setStyleSheet("""
            #subwindowFrame {
                background: #151a23;
                border: 1px solid #2a3040;
                border-radius: 9px;
            }
        """)
        container.setGraphicsEffect(_shadow(blur=36, dy=12, alpha=190))

        frame_layout = QVBoxLayout(container)
        frame_layout.setContentsMargins(0, 0, 0, 0)
        frame_layout.setSpacing(0)

        title_bar = TitleBar(
            subwindow=None, icon=icon, title=label, color1=colors[0],
            color2=colors[1], on_close=lambda: self._close_app(key),
        )
        frame_layout.addWidget(title_bar)

        content = tab_cls()
        content.setContentsMargins(8, 8, 8, 8)
        frame_layout.addWidget(content, 1)

        grip_row = QHBoxLayout()
        grip_row.setContentsMargins(0, 0, 2, 2)
        grip_row.addStretch(1)
        grip_row.addWidget(QSizeGrip(container))
        frame_layout.addLayout(grip_row)

        sub = self.mdi.addSubWindow(container, Qt.FramelessWindowHint)
        title_bar.subwindow = sub
        sub.resize(620, 460)
        sub.setWindowOpacity(0.0)
        sub.show()

        target_rect = sub.geometry()
        shrink_w, shrink_h = int(target_rect.width() * 0.94), int(target_rect.height() * 0.94)
        start_rect = target_rect.__class__(0, 0, shrink_w, shrink_h)
        start_rect.moveCenter(target_rect.center())
        sub.setGeometry(start_rect)

        group = QParallelAnimationGroup(self)
        fade = QPropertyAnimation(sub, b"windowOpacity")
        fade.setDuration(220)
        fade.setStartValue(0.0)
        fade.setEndValue(1.0)
        fade.setEasingCurve(QEasingCurve.OutCubic)
        grow = QPropertyAnimation(sub, b"geometry")
        grow.setDuration(220)
        grow.setStartValue(start_rect)
        grow.setEndValue(target_rect)
        grow.setEasingCurve(QEasingCurve.OutCubic)
        group.addAnimation(fade)
        group.addAnimation(grow)
        self._anims.append(group)
        group.finished.connect(lambda g=group: g in self._anims and self._anims.remove(g))
        group.start()

        taskbar_btn = TaskbarButton(icon, label)
        taskbar_btn.clicked.connect(lambda _checked=False, k=key: self._open_app(k))
        self.taskbar_layout.insertWidget(self.taskbar_layout.count() - 1, taskbar_btn)

        self._open_windows[key] = sub
        self._taskbar_buttons[key] = taskbar_btn
        self._dots[key].setVisible(True)

    def _close_app(self, key: str):
        sub = self._open_windows.get(key)
        if sub is None:
            return

        start_rect = sub.geometry()
        end_rect = start_rect.__class__(0, 0, int(start_rect.width() * 0.94),
                                         int(start_rect.height() * 0.94))
        end_rect.moveCenter(start_rect.center())

        group = QParallelAnimationGroup(self)
        fade = QPropertyAnimation(sub, b"windowOpacity")
        fade.setDuration(180)
        fade.setStartValue(1.0)
        fade.setEndValue(0.0)
        fade.setEasingCurve(QEasingCurve.InCubic)
        shrink = QPropertyAnimation(sub, b"geometry")
        shrink.setDuration(180)
        shrink.setStartValue(start_rect)
        shrink.setEndValue(end_rect)
        shrink.setEasingCurve(QEasingCurve.InCubic)
        group.addAnimation(fade)
        group.addAnimation(shrink)
        self._anims.append(group)
        group.finished.connect(lambda g=group, k=key: self._finish_close(k, g))
        group.start()

        btn = self._taskbar_buttons.get(key)
        if btn is not None:
            btn.setEnabled(False)

    def _finish_close(self, key: str, group):
        if group in self._anims:
            self._anims.remove(group)

        sub = self._open_windows.pop(key, None)
        if sub is not None:
            self.mdi.removeSubWindow(sub)
            sub.deleteLater()

        btn = self._taskbar_buttons.pop(key, None)
        if btn is not None:
            btn.deleteLater()

        dot = self._dots.get(key)
        if dot is not None:
            dot.setVisible(False)

    def _on_active_changed(self, sub):
        for key, window in self._open_windows.items():
            btn = self._taskbar_buttons.get(key)
            if btn is not None:
                btn.set_active(window is sub)


def main():
    app = QApplication(sys.argv)
    app.setStyleSheet(APP_STYLESHEET)
    font = QFont("Segoe UI", 9)
    app.setFont(font)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
