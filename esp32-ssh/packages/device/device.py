"""
device -- runs Python code on THIS machine (the client's own CPU/GPU),
not the ESP32. Two independent tools bundled together:

  device.run(code)          -- eval/exec arbitrary Python in a persistent
                                namespace, returns the result (or None for
                                statements, or "Error: ..." string on
                                exception).
  device.install(packages)  -- installs pip packages into the CURRENT
                                interpreter (the same one this script is
                                already running under -- sys.executable --
                                so no separate hardcoded python path is
                                needed).

Usage:
    import device
    device.run("x = 1 + 1")
    result = device.run("x")          # -> 2
    device.install(["requests", "numpy"])
"""
import os
import sys
from helpers import command


class _PyEnvironment:
    def __init__(self):
        self.globals = {"__builtins__": __builtins__}
        self.locals = {}

    def run(self, code: str):
        try:
            # Try eval first (for expressions), fall back to exec (for
            # statements). A statement like "x = 1" is a SyntaxError under
            # eval, so this correctly routes it to exec instead.
            try:
                return eval(code, self.globals, self.locals)
            except SyntaxError:
                exec(code, self.globals, self.locals)
                return None
        except Exception as e:
            return f"Error: {e}"


_env = _PyEnvironment()
_cmd = command._CommandEnvironment()
_HERE = os.path.dirname(os.path.abspath(__file__))
_CACHE_DIR = os.path.join(_HERE, "cache")


def run(code: str):
    """Runs `code` in THIS process (client CPU/GPU) and returns the result.
    Statements return None; expressions return their value; exceptions are
    caught and returned as an "Error: ..." string rather than raised, so a
    GUI app's calculator-style use (device.run(expr)) never crashes it."""
    return _env.run(code)


def install(packages) -> bool:
    """Installs `packages` (a list of pip requirement strings, e.g.
    ["requests", "numpy>=1.24"]) into the SAME Python interpreter this
    script is already running under -- sys.executable -- so it always
    matches whatever --python-change picked, with no separate hardcoded
    interpreter path needed.

    Returns True on success. Raises RuntimeError with pip's stderr (or the
    launch error) on failure.
    """
    os.makedirs(_CACHE_DIR, exist_ok=True)
    req_path = os.path.join(_CACHE_DIR, "requirements.txt")
    with open(req_path, "w") as f:
        f.write("\n".join(packages) + "\n")

    result = _cmd.run(
        f'"{sys.executable}" -m pip install -r "{req_path}" --upgrade'
    )
    if "error" in result:
        raise RuntimeError(result["error"])
    if result["returncode"] != 0:
        raise RuntimeError(
            result["stderr"].strip() or
            f"pip exited with code {result['returncode']}"
        )
    return True


# Backwards-compat alias for the earlier draft's name.
runD = install
