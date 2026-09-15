"""Test the production adapter; intercept only credentials and final exec."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="leonos-rcctl-") as directory:
    binary = Path(directory) / "test"
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-O1", "-g",
                    "-fsanitize=address,undefined", "tools/tests/openrc_authorization_test.c",
                    "-o", str(binary)], cwd=ROOT, check=True)
    subprocess.run([binary], check=True, timeout=20)
