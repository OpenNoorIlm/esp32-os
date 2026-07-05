import subprocess

class _CommandEnvironment:
    def __init__(self, cwd=None):
        self.cwd = cwd  # working directory persists between commands
        self.env_vars = {}  # custom environment variables persist too

    def run(self, command: str, timeout=10):
        try:
            result = subprocess.run(
                command,
                shell=True,
                cwd=self.cwd,
                env={**self.env_vars} if self.env_vars else None,
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            return {
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode,
            }
        except subprocess.TimeoutExpired:
            return {"error": "Command timed out"}
        except Exception as e:
            return {"error": str(e)}