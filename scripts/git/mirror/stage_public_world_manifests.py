#!/usr/bin/env python3
"""Stage the active public World manifest projection for one release."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path


PUBLIC_SCOPE_IDS = {
    *(f"layer_kazan_territory_public_v1_{name}" for name in ("terrain", "water", "roads", "buildings")),
    *(f"layer_manhattan_showcase_public_v1_{name}" for name in ("terrain", "water", "roads", "buildings")),
    "map_territory_l_projectworldkazanterritory",
    "map_showcase_manhattan_l_projectworldmanhattanshowcase",
    "presentation_kazan_representative_v1",
}


class StageError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage(source_root: Path, asset_root: Path, output_root: Path) -> None:
    if output_root.exists():
        raise StageError(f"Output must not exist: {output_root}")
    active_path = source_root / "active_set.json"
    active = json.loads(active_path.read_text(encoding="utf-8-sig"))
    selected = [scope for scope in active.get("scopes", []) if str(scope.get("scope_id", "")) in PUBLIC_SCOPE_IDS]
    selected_ids = {str(item.get("scope_id", "")) for item in selected}
    if selected_ids != PUBLIC_SCOPE_IDS or len(selected) != len(PUBLIC_SCOPE_IDS):
        missing = sorted(PUBLIC_SCOPE_IDS - selected_ids)
        raise StageError(f"Public World authority is incomplete; missing scopes: {missing}")

    output_scopes = output_root / "scopes"
    output_scopes.mkdir(parents=True)
    for scope in selected:
        relative = Path(str(scope.get("manifest_path", "")))
        if relative.is_absolute() or ".." in relative.parts or relative.parts[:1] != ("scopes",):
            raise StageError(f"Unsafe active World manifest path: {relative}")
        source = source_root / relative
        if not source.is_file():
            raise StageError(f"Active public World manifest is missing: {source}")
        expected = str(scope.get("manifest_sha256", "")).lower()
        if expected != sha256(source):
            raise StageError(f"Active public World manifest hash mismatch: {source}")
        if "hlod" in source.read_text(encoding="utf-8-sig").lower():
            raise StageError(f"HLOD entered public World authority: {source}")
        document = json.loads(source.read_text(encoding="utf-8-sig"))
        if str(document.get("scope_id", "")) != str(scope.get("scope_id", "")):
            raise StageError(f"Public World scope identity mismatch: {source}")
        for artifact in document.get("artifacts", []):
            relative_artifact = Path(str(artifact.get("path", "")))
            if relative_artifact.is_absolute() or ".." in relative_artifact.parts:
                raise StageError(f"Unsafe public World artifact path: {relative_artifact}")
            artifact_path = asset_root / relative_artifact
            if not artifact_path.is_file():
                raise StageError(f"Public World artifact is missing: {artifact_path}")
            if str(artifact.get("digest_kind", "")) != "sha256" or str(
                artifact.get("digest", "")
            ).lower() != sha256(artifact_path):
                raise StageError(f"Public World artifact hash mismatch: {artifact_path}")
        target = output_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    active["scopes"] = selected
    (output_root / "active_set.json").write_text(
        json.dumps(active, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--asset-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        stage(args.source_root.resolve(), args.asset_root.resolve(), args.output_root.resolve())
    except (OSError, ValueError, KeyError, json.JSONDecodeError, StageError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] Public World manifests: {args.output_root.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
