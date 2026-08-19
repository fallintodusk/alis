# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

from __future__ import annotations

import argparse
import json
import sys
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[5]
TEST_DATA = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
SOURCE_PROFILE = TEST_DATA / "Profiles" / "SourceIngestion" / "synthetic_landscape_water_twin.source.json"
COMPILER_PROFILE = (
    TEST_DATA / "Profiles" / "CanonicalCompilation" / "synthetic_landscape_water_twin.compile.json"
)
COMPILER_SCHEMA = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
OVERLAY_SCHEMA = "https://alis.world/schemas/world-compiler/authored-overlay-v1.json"

sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import read_json as read_compiler_json
from World.CanonicalCompilation.app.pipeline import compile_world
from World.SourceIngestion.app.cli import main as source_main


def _read(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _write(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")


def _compile_variant(
    root: Path,
    source_result: Path,
    overlay_path: Path | None = None,
    river_width_m: float | None = None,
) -> Path:
    compiler_profile_path = root / "compiler_profile.json"
    compile_output = root / "compile"

    compiler_profile = _read(COMPILER_PROFILE)
    compiler_profile["$schema"] = COMPILER_SCHEMA
    if overlay_path is not None:
        compiler_profile["authored_overlay"] = overlay_path.relative_to(REPO_ROOT).as_posix()
    if river_width_m is not None:
        compiler_profile["water_semantics"]["width"]["fallback_m"]["river"] = river_width_m
    _write(compiler_profile_path, compiler_profile)

    result, _ = compile_world(
        str(compiler_profile_path),
        source_result=source_result,
        output_root_value=compile_output,
    )
    if result["status"] != "accepted":
        raise RuntimeError(f"Synthetic compiler variant failed: {root.name}")
    return compile_output / "compile_result.json"


def _artifact_hashes(result_path: Path, representation: str) -> dict[str, str]:
    coverage = read_compiler_json(result_path.parent / "canonical" / "coverage.json")
    return {
        item["cell_id"]: item["content_hash"]
        for item in coverage["artifact_descriptors"]
        if item["representation"] == representation
    }


def _changed(left: dict[str, str], right: dict[str, str]) -> list[str]:
    if left.keys() != right.keys():
        raise RuntimeError("Canonical variants changed the target cell domain")
    return sorted(cell_id for cell_id in left if left[cell_id] != right[cell_id])


def main() -> int:
    parser = argparse.ArgumentParser(description="Build genuine package-locality canonical variants")
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()
    output_root = Path(args.output_root).resolve()
    if not output_root.is_relative_to((REPO_ROOT / "tmp").resolve()):
        raise RuntimeError("Package-locality fixtures must stay under the repository tmp root")

    source_output = output_root / "source"
    with redirect_stdout(StringIO()):
        source_exit = source_main([
            "run", "--profile", str(SOURCE_PROFILE), "--output-root", str(source_output)
        ])
    if source_exit != 0:
        raise RuntimeError("Synthetic source fixture failed")
    source_result = source_output / "run_result.json"

    terrain_overlay = _read(
        TEST_DATA / "Fixtures" / "CanonicalCompilation" /
        "synthetic_landscape_water_twin" / "authored_overlay.json"
    )
    terrain_overlay["$schema"] = OVERLAY_SCHEMA
    terrain_overlay["overlay_id"] = "synthetic_package_locality_terrain_v1"
    terrain_overlay["terrain_patches"] = [{
        "patch_id": "package_locality_cell_x1_y0",
        "center": [120.0, 5.0],
        "radius_m": 1.0,
        "delta_m": 5.0,
    }]
    terrain_overlay_path = output_root / "terrain_overlay.json"
    _write(terrain_overlay_path, terrain_overlay)

    base_result = _compile_variant(output_root / "base", source_result)
    terrain_result = _compile_variant(
        output_root / "terrain", source_result, overlay_path=terrain_overlay_path
    )
    water_result = _compile_variant(
        output_root / "water", source_result, overlay_path=terrain_overlay_path, river_width_m=14.0
    )

    base_terrain = _artifact_hashes(base_result, "terrain")
    terrain_terrain = _artifact_hashes(terrain_result, "terrain")
    water_terrain = _artifact_hashes(water_result, "terrain")
    base_features = _artifact_hashes(base_result, "features")
    terrain_features = _artifact_hashes(terrain_result, "features")
    water_features = _artifact_hashes(water_result, "features")
    terrain_changed_cells = _changed(base_terrain, terrain_terrain)
    if len(terrain_changed_cells) != 1 or _changed(base_features, terrain_features):
        raise RuntimeError("Terrain variant is not isolated to one terrain cell")
    if _changed(terrain_terrain, water_terrain):
        raise RuntimeError("Water variant changed canonical terrain")
    water_changed_cells = _changed(terrain_features, water_features)
    if not water_changed_cells:
        raise RuntimeError("Water variant did not change canonical feature authority")

    print(json.dumps({
        "base_compile_result": str(base_result),
        "terrain_compile_result": str(terrain_result),
        "water_compile_result": str(water_result),
        "terrain_changed_cell_id": terrain_changed_cells[0],
        "water_changed_cell_ids": water_changed_cells,
    }, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
