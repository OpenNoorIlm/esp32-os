"""
oled -- draws to the robot's face display (SSD1306 OLED on the companion
Arduino) via NoorShell's `oled` command.

Usage:
    import oled
    oled.eyes("Happy")
    oled.eyes("Wink", 10, -5)
    oled.clear()
    oled.run("eyes Love 0 0")   # raw form, if you want it directly
"""
import subprocess
import os
import shutil

# See shell.py's _strip_banner docstring -- esp32-ssh prints a fresh
# login banner on every --command call, and it was leaking into results.
_BANNER_PREFIXES = ("NOOR-SHELL", "LOGGED IN SUCCESSFULLY", "LOGIN FAILED")


def _strip_banner(output: str) -> str:
    lines = output.splitlines()
    while lines and any(lines[0].strip().upper().startswith(p) for p in _BANNER_PREFIXES):
        lines.pop(0)
    return "\n".join(lines).strip()

# Recognized expressions, per arduino.ino's drawEyes() -- listed here so
# GUI apps can validate/enumerate rather than guessing valid strings.
EXPRESSIONS = [
    "Normal", "Happy", "Sad", "Angry", "Surprised", "Cry", "Love",
    "Sleepy", "Confused", "Excited", "Dizzy", "Bored", "Evil", "Shy",
    "Cool", "Wink", "Dead", "Nervous",
]


def _find_exe():
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.normpath(os.path.join(here, "..", "..", "build"))
    candidates = [
        os.path.join(build_dir, "esp32-ssh.exe"),
        os.path.join(build_dir, "esp32-ssh"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    on_path = shutil.which("esp32-ssh")
    if on_path:
        return on_path
    raise FileNotFoundError(
        "Could not find the esp32-ssh binary (checked "
        + ", ".join(candidates) + " and PATH). Build it first: "
        "cmake --build esp32-ssh/build"
    )


def run(sub: str, timeout: int = 30) -> str:
    """Sends `oled <sub>` to the ESP32 shell, e.g. run("eyes Happy 0 0")
    or run("clear"), and returns whatever it printed."""
    exe = _find_exe()
    proc = subprocess.run(
        [exe, "--command", "oled " + sub],
        capture_output=True, text=True, timeout=timeout,
    )
    output = proc.stdout.strip()
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or output or
                            f"esp32-ssh exited with code {proc.returncode}")
    output = _strip_banner(output)
    if output.startswith("error:"):
        raise RuntimeError(output)
    return output


def eyes(expression: str, x: int = 0, y: int = 0) -> str:
    """Convenience wrapper: oled.eyes("Happy", 10, -5)."""
    return run(f"eyes {expression} {x} {y}")


def clear() -> str:
    return run("clear")
