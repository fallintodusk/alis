"""Real Win32 launch control matching UE's detached child-process envelope."""

import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[4]


class NativeProcessTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Pinned native World tools use Win64")
    def test_tools_without_inherited_stdin(self):
        script = f"""
import ctypes
import sys
from pathlib import Path
sys.path.insert(0, {str(ROOT / 'tools')!r})
assert ctypes.windll.kernel32.SetStdHandle(-10, ctypes.c_void_p(-1))
from World.ExecutionEnvironment.api import require_tools, run_tool
root = Path({str(ROOT)!r})
require_tools(root)
assert 'GDAL' in run_tool(root, 'gdalinfo', ['--version'])
"""
        result = subprocess.run([sys.executable, "-B", "-c", script], stdin=subprocess.DEVNULL,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
