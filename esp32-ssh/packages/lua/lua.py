"""
lua -- runs Lua code on the ESP32 over NoorShell, via esp32-ssh --command.

Usage from a CLIENT GUI app:
    import lua
    result = lua.run("print(1+1)")

No host/password needed here -- esp32-ssh resolves them from its own saved
config (set the first time you log in normally, or via --app/--command with
--host/--pass the first time).
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


def _find_exe():
    """Locates the esp32-ssh binary relative to THIS file, not the caller's
    cwd -- a GUI app can be launched from anywhere. Falls back to PATH."""
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.normpath(os.path.join(here, "..", "..", "build"))
    candidates = [
        os.path.join(build_dir, "esp32-ssh.exe"),  # Windows
        os.path.join(build_dir, "esp32-ssh"),      # Linux (compiled binary)
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


def run(code: str, timeout: int = 30) -> str:
    """Runs `code` as Lua on the ESP32 and returns whatever it printed.

    Raises RuntimeError if esp32-ssh itself failed (bad/missing
    credentials, connection failure) or if the Lua code raised an error
    on the device side.
    """
    exe = _find_exe()
    # Passed as ONE argv element (a list, no shell=True) -- avoids the
    # shell splitting `code` on spaces before esp32-ssh ever sees it.
    proc = subprocess.run(
        [exe, "--command", "lua " + code],
        capture_output=True, text=True, timeout=timeout,
    )
    output = proc.stdout.strip()
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or output or
                            f"esp32-ssh exited with code {proc.returncode}")
    output = _strip_banner(output)
    if output.startswith("lua error:"):
        raise RuntimeError(output)
    return output
