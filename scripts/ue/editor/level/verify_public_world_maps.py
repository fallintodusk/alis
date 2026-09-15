#!/usr/bin/env python3
"""Load the two promised public World maps and emit a release receipt."""

from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from pathlib import Path

import unreal


MAPS = (
    "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
    "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
)


def main() -> None:
    output = os.environ.get("ALIS_PUBLIC_MAP_RECEIPT", "").strip()
    if not output:
        raise RuntimeError("ALIS_PUBLIC_MAP_RECEIPT is required")
    loaded: list[str] = []
    for package in MAPS:
        world = unreal.EditorLoadingAndSavingUtils.load_map(package)
        if world is None:
            raise RuntimeError(f"Failed to load public World map: {package}")
        loaded.append(package)
    receipt = {
        "schema": "alis-public-world-map-load-v1",
        "status": "accepted",
        "maps": loaded,
        "checked_utc": datetime.now(timezone.utc).isoformat(),
    }
    path = Path(output)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="ascii")


main()
