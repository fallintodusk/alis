"""Temporary repository with World producer descriptors, baselines, and a UBT export."""

from __future__ import annotations

import json
import os
from pathlib import Path


VERIFY = "Project.World.Realization.Verify."
MPD = "Plugins/World/ProjectWorldMeshTerrain/Content/Terrain/MPD_Fixture.uasset"
ADAPTER_SOURCE = "Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Producer.cpp"
WORLD_EDITOR_SOURCE = "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Realization.cpp"
MODULE_DIRECTORIES = {
    "ProjectCore": "Plugins/Foundation/ProjectCore/Source/ProjectCore",
    "ProjectWorld": "Plugins/World/ProjectWorld/Source/ProjectWorld",
    "ProjectWorldEditor": "Plugins/World/ProjectWorld/Source/ProjectWorldEditor",
    "ProjectWorldMeshTerrain": "Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrain",
    "ProjectWorldMeshTerrainEditor": "Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor",
    "ProjectObject": "Plugins/Resources/ProjectObject/Source/ProjectObject",
    "ProjectObjectCapabilities": "Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities",
}
DEPENDENCIES = {
    "ProjectCore": ["Engine"],
    "ProjectWorld": ["Engine", "ProjectCore"],
    "ProjectWorldEditor": ["Engine", "ProjectCore", "ProjectWorld"],
    "ProjectWorldMeshTerrain": ["Engine", "ProjectWorld"],
    "ProjectWorldMeshTerrainEditor": ["ProjectWorldEditor", "ProjectWorldMeshTerrain"],
    "ProjectObject": ["Engine", "ProjectCore", "ProjectObjectCapabilities", "ProjectWorld"],
    "ProjectObjectCapabilities": ["ProjectCore"],
}
# Producer descriptors in their owning plugins: (plugin root, file stem, document).
DESCRIPTORS = (
    ("Plugins/World/ProjectWorld", "realization_pipeline", {
        "kind": "pipeline", "pipeline_revision": 1, "modules": ["ProjectWorld", "ProjectWorldEditor"],
    }),
    ("Plugins/World/ProjectWorld", "map", {
        "kind": "producer", "generator_id": "map", "generator_version": 1, "output_revision": 1,
        "data_inputs": [], "modules": ["ProjectWorldEditor"],
    }),
    ("Plugins/World/ProjectWorld", "project_water_mesh", {
        "kind": "producer", "generator_id": "project_water_mesh", "generator_version": 1,
        "output_revision": 1, "data_inputs": [], "modules": ["ProjectWorldEditor"],
    }),
    ("Plugins/World/ProjectWorldMeshTerrain", "project_mesh_terrain", {
        "kind": "producer", "generator_id": "project_mesh_terrain", "generator_version": 1,
        "output_revision": 1, "data_inputs": [MPD],
        "modules": ["ProjectWorldMeshTerrain", "ProjectWorldMeshTerrainEditor"],
        "runtime_acceptance": {"terrain": {
            "collision_components": "navigation_relevant_pawn_blocking",
            "non_main_pass_helpers": "required_navigation_irrelevant",
            "route_endpoints": "navigation_relevant_pawn_blocking",
        }},
    }),
)
BASELINE_VERIFIES = {
    "realization_pipeline": "Pipeline",
    "map": "Map",
    "project_water_mesh": "Water",
    "project_mesh_terrain": "MeshTerrain",
}


def write(root: Path, relative: str, value: bytes = b"baseline") -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value)
    return path


def descriptor_path(plugin_root: str, stem: str) -> str:
    return f"{plugin_root}/Data/Producers/{stem}.json"


def baseline_path(plugin_root: str, stem: str) -> str:
    return f"{plugin_root}/Data/TestFixtures/Verify/{stem}.verify.json"


def write_producers(root: Path, baselines: bool = True) -> None:
    """Descriptors, their data input, module rules files, and optionally baselines."""
    write(root, MPD)
    for directory in MODULE_DIRECTORIES.values():
        name = directory.rsplit("/", 1)[1]
        write(root, f"{directory}/{name}.Build.cs")
    for plugin_root, stem, document in DESCRIPTORS:
        payload = {"$schema": "schema.json", "schema_version": 1, **document}
        write(root, descriptor_path(plugin_root, stem), json.dumps(payload, indent=2).encode("utf-8"))
        if baselines:
            record = {"verify": VERIFY + BASELINE_VERIFIES[stem], "records": []}
            write(root, baseline_path(plugin_root, stem), json.dumps(record).encode("utf-8"))


def write_target_export(
    root: Path,
    extra_modules: dict[str, str] | None = None,
    extra_dependencies: dict[str, list[str]] | None = None,
    omitted: tuple[str, ...] = (),
) -> Path:
    """A UBT JsonExport of AlisEditor, newer than every module graph input."""
    directories = {**MODULE_DIRECTORIES, **(extra_modules or {})}
    modules = {
        "Engine": {
            "Name": "Engine",
            "Directory": str(root.parent / "engine_outside_repository" / "Engine"),
            "PublicDependencyModules": [],
        }
    }
    for name, directory in directories.items():
        if name in omitted:
            continue
        dependencies = DEPENDENCIES.get(name, []) + (extra_dependencies or {}).get(name, [])
        modules[name] = {
            "Name": name,
            "Directory": str(root / directory),
            "PublicDependencyModules": dependencies[:1],
            "PrivateDependencyModules": dependencies[1:],
            "PublicIncludePathModules": [],
            "PrivateIncludePathModules": [],
            "DynamicallyLoadedModules": [],
        }
    document = {
        "Name": "AlisEditor",
        "Configuration": "Development",
        "Platform": "Win64",
        "ProjectFile": str(root / "Alis.uproject"),
        "Modules": modules,
    }
    path = write(root, "Binaries/Win64/AlisEditor.json", json.dumps(document).encode("utf-8"))
    newest = max(
        (
            item.stat().st_mtime_ns
            for item in root.rglob("*")
            if item.is_file() and item.name.endswith((".Build.cs", ".Target.cs", ".uplugin", ".uproject"))
        ),
        default=path.stat().st_mtime_ns,
    )
    stamp = newest + 2_000_000_000
    os.utime(path, ns=(stamp, stamp))
    return path


def make_newer_than_export(root: Path, relative: str) -> None:
    export_time = (root / "Binaries/Win64/AlisEditor.json").stat().st_mtime_ns
    stamp = export_time + 2_000_000_000
    os.utime(root / relative, ns=(stamp, stamp))
