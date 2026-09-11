import sys
from pathlib import Path


COMPILATION_ROOT = Path(__file__).resolve().parent
TOOLS_ROOT = COMPILATION_ROOT.parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

from World.CanonicalCompilation.app.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
