from __future__ import annotations

from pathlib import Path
from typing import Any, Callable

from .contracts import read_json


def capture(
    name: str,
    leg: str,
    settings: dict[str, Any],
    emitted: Path,
    compile_result: Path,
    realization: dict[str, Any] | None,
    world_data_plugin: str,
    powershell: str,
    logs: Path,
    repo_root: Path,
    run: Callable[..., float],
) -> tuple[Path | None, Path | None]:
    script_root = repo_root / "scripts" / "ue" / "world" / "test" / "integration"
    prefix = [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File"]
    terrain_contract = next((item for item in settings.get("expected_layers", [])
        if item["layer_id"] == "terrain" and item["generator_id"] == "project_mesh_terrain"
        and "producer_artifact_count" in item), None)
    terrain = None
    if terrain_contract is not None:
        terrain = emitted.with_name(f"terrain_projection_{leg}.json")
        run(f"{name}_terrain_projection_{leg}", [*prefix, str(script_root / "verify_terrain_projection.ps1"),
            "-Map", settings["map_package"], "-CompileResult", str(compile_result),
            "-EvidencePath", str(terrain), "-WorldDataPlugin", world_data_plugin,
            "-ExpectedBaseCount", str(settings["expected_topology"]["canonical_cells"]),
            "-ExpectedSectionCount", str(terrain_contract["artifact_count"] -
                terrain_contract["producer_artifact_count"])], logs)
    profile = read_json(repo_root / realization["path"]) if realization else None
    buildings = next((item for item in profile.get("layers", [])
        if item.get("layer_id") == "buildings" and item.get("generator_id") == "project_building_massing"), None) \
        if profile else None
    world = None
    if buildings is not None:
        world = emitted.with_name(f"saved_world_projection_{leg}.json")
        run(f"{name}_saved_world_projection_{leg}", [*prefix, str(script_root / "verify_saved_world_projection.ps1"),
            "-Map", settings["map_package"], "-RealizationProfile", str(repo_root / realization["path"]),
            "-EvidencePath", str(world), "-WorldDataPlugin", world_data_plugin], logs)
    return terrain, world
