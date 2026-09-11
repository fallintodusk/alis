import sys
from pathlib import Path


SOURCE_ROOT = Path(__file__).resolve().parent
TOOLS_ROOT = SOURCE_ROOT.parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

from World.SourceIngestion.app.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
