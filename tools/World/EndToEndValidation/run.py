from __future__ import annotations

import sys
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS_ROOT))

from World.EndToEndValidation.app.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
