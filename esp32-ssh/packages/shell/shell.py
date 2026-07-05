"""
shell -- generic passthrough to ANY NoorShell command via esp32-ssh
--command, for CLIENT GUI apps that need more than the narrow lua/device/
oled wrappers (e.g. a file browser needing ls/cd/cat, or a control panel
needing wifi/jobs/bg/close/kill/apt/app-installer).

Usage:
    import shell
    shell.run("ls /apps")
    shell.run("wifi")
    shell.run("bg run myapp")
    shell.run("jobs")
    shell.run("close run-45213")

No host/password needed here -- esp32-ssh resolves them from its own saved
config, same as lua/device/oled.
"""
import subprocess
import os
import shutil

# esp32-ssh reconnects/logs in fresh for every --command call, so its
# stdout always starts with this 2-line banner ahead of the command's
# actual output. Every caller of run() was treating those banner lines
# as real data (ls entries, app names, job output, ...) -- this strips
# them so GUI apps only ever see the command's actual result.
_BANNER_PREFIXES = ("NOOR-SHELL", "LOGGED IN SUCCESSFULLY", "LOGIN FAILED")


def _strip_banner(output: str) -> str:
    lines = output.splitlines()
    while lines and any(lines[0].strip().upper().startswith(p) for p in _BANNER_PREFIXES):
        lines.pop(0)
    return "\n".join(lines).strip()


def _find_exe():
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.normpath(os.path.join(here, "..", "..", "build"))
    candidates = [
        os.path.join(build_dir, "esp32-ssh.exe"),  # Windows
        os.path.join(build_dir, "esp32-ssh"),      # Linux
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


def run(command: str, timeout: int = 30) -> str:
    """Sends `command` verbatim to the ESP32's NoorShell (e.g. "ls /apps",
    "wifi", "jobs") and returns whatever it printed.

    Raises RuntimeError if esp32-ssh itself failed (bad/missing
    credentials, connection failure). Does NOT raise on NoorShell-level
    "error: ..." responses (e.g. "cd /nowhere") -- those are returned as
    plain text, same as a real terminal would show them, since they're not
    failures of esp32-ssh/the connection itself.
    """
    exe = _find_exe()
    proc = subprocess.run(
        [exe, "--command", command],
        capture_output=True, text=True, timeout=timeout,
    )
    output = proc.stdout.strip()
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or output or
                            f"esp32-ssh exited with code {proc.returncode}")
    return _strip_banner(output)
