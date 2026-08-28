"""Compatibility entrypoint for the request-driven release capture.

The old City17 hardcoded render driver was unsafe to reuse for Kazan. The
stable host command is run_release_capture.ps1; this file remains only for
existing editor shortcuts that already invoke its historical path.
"""

from __future__ import annotations

import os
import runpy
from pathlib import Path


if "PROJECT_CINEMATIC_CAPTURE_REQUEST" not in os.environ:
    raise RuntimeError(
        "_drive_mrq_render.py now requires run_release_capture.ps1 ownership"
    )

runpy.run_path(
    str(Path(__file__).with_name("release_capture_editor.py")),
    run_name="__main__",
)
