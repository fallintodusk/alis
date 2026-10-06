"""Generic World code stays neutral toward producer representations.

ProjectWorld and ProjectWorldEditor host every producer through the producer
registry. Their production sources and module rules name no representation
adapter's tags, classes, modules, assets, or generator ids. Test fixtures and
verify runners name the producer they exercise. The module graph UBT resolves
for the generic modules reaches no representation module.
"""

from __future__ import annotations

import json
import os
import re
import unittest
from pathlib import Path
from typing import Any, Iterable


REPO_ROOT = Path(__file__).resolve().parents[3]
GENERIC_MODULES = ("ProjectWorld", "ProjectWorldEditor")
GENERIC_PLUGIN = REPO_ROOT / "Plugins" / "World" / "ProjectWorld"
GENERIC_SCRIPTS = REPO_ROOT / "scripts" / "ue" / "world"
ADAPTER_SOURCE_ROOT = REPO_ROOT / "Plugins" / "World" / "ProjectWorldMeshTerrain" / "Source"
SOURCE_SUFFIXES = (".h", ".hpp", ".inl", ".cpp", ".cs")
# Names private to the Mesh Terrain adapter and its MeshPartition representation.
ADAPTER_NAMES = (
    "project_mesh_terrain",  # generator id
    "ProjectWorld.MeshTerrain.",  # actor tag namespace
    "ProjectWorldMeshTerrain",  # adapter modules, classes, and headers
    "MeshPartition",  # representation modules, classes, and console variables
    "MPD_ProjectTerrain",  # partition definition asset
)
REPRESENTATION_MODULE = re.compile(r"^(MeshPartition|ProjectWorldMeshTerrain|Water|PCG)")
TARGET_EXPORT = REPO_ROOT / "Binaries" / "Win64" / "AlisEditor.json"
EXPORT_SCRIPT = "scripts/ue/build/export_target_json.ps1"
DEPENDENCY_FIELDS = (
    "PublicDependencyModules",
    "PrivateDependencyModules",
    "PublicIncludePathModules",
    "PrivateIncludePathModules",
    "DynamicallyLoadedModules",
)
GRAPH_INPUT_SUFFIXES = (".Build.cs", ".Target.cs", ".uplugin")
UNSCANNED_DIRECTORIES = frozenset({"Binaries", "Content", "Data", "DerivedDataCache", "Intermediate", "Saved"})


def _source_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*") if path.is_file() and path.name.endswith(SOURCE_SUFFIXES))


def _adapter_name_hits(files: Iterable[Path]) -> list[str]:
    hits = []
    for path in files:
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
            names = [name for name in ADAPTER_NAMES if name in line]
            if names:
                hits.append(f"{path.relative_to(REPO_ROOT).as_posix()}:{number}: {', '.join(names)}")
    return hits


def _module_graph_inputs() -> list[Path]:
    """Project files UBT resolves the module graph from."""
    found = [REPO_ROOT / "Alis.uproject"]
    for top in ("Source", "Plugins"):
        for directory, children, names in os.walk(REPO_ROOT / top):
            children[:] = [name for name in children if name not in UNSCANNED_DIRECTORIES]
            found.extend(Path(directory) / name for name in names if name.endswith(GRAPH_INPUT_SUFFIXES))
    return [path for path in found if path.is_file()]


def _closure(modules: dict[str, Any], seeds: Iterable[str]) -> set[str]:
    reached: set[str] = set()
    pending = list(seeds)
    while pending:
        name = pending.pop()
        if name in reached:
            continue
        module = modules.get(name)
        if not isinstance(module, dict):
            raise AssertionError(f"The UBT target export has no module {name}; run {EXPORT_SCRIPT}")
        reached.add(name)
        for field in DEPENDENCY_FIELDS:
            pending.extend(module.get(field, []))
    return reached


class WorldGenerationNeutralityTests(unittest.TestCase):
    def _resolved_modules(self) -> dict[str, Any]:
        # An absent or stale export is an unknown graph, never a clean one.
        if not TARGET_EXPORT.is_file():
            self.fail(f"The UBT target export is missing; run {EXPORT_SCRIPT}")
        exported = TARGET_EXPORT.stat().st_mtime_ns
        newer = sorted(
            path.relative_to(REPO_ROOT).as_posix()
            for path in _module_graph_inputs()
            if path.stat().st_mtime_ns > exported
        )
        if newer:
            self.fail(f"The UBT target export is older than {newer[0]}; run {EXPORT_SCRIPT}")
        document = json.loads(TARGET_EXPORT.read_text(encoding="utf-8-sig"))
        modules = document.get("Modules") if isinstance(document, dict) else None
        if not isinstance(modules, dict) or document.get("Name") != "AlisEditor":
            self.fail(f"{TARGET_EXPORT} is not the AlisEditor module graph; run {EXPORT_SCRIPT}")
        return modules

    def test_generic_sources_name_no_adapter_internals(self) -> None:
        files = [
            path
            for module in GENERIC_MODULES
            for path in _source_files(GENERIC_PLUGIN / "Source" / module)
            if not {"Tests", "Verify"}.intersection(path.parts)
        ]
        self.assertTrue(
            {f"{module}.Build.cs" for module in GENERIC_MODULES} <= {path.name for path in files},
            "The scan must cover both generic modules and their module rules",
        )
        # Control: the same scan finds the adapter's names in the adapter itself.
        self.assertTrue(_adapter_name_hits(_source_files(ADAPTER_SOURCE_ROOT)))

        hits = _adapter_name_hits(files)
        self.assertEqual([], hits, "Generic World code names adapter internals:\n" + "\n".join(hits))

    def test_generic_world_scripts_name_no_adapter_internals(self) -> None:
        files = sorted(GENERIC_SCRIPTS.glob("*.ps1"))
        self.assertTrue(files, "The generic World script scan must have inputs")
        hits = _adapter_name_hits(files)
        self.assertEqual([], hits, "Generic World scripts name adapter internals:\n" + "\n".join(hits))

    def test_generic_modules_reach_no_representation_module(self) -> None:
        modules = self._resolved_modules()
        # Control: the adapter's own resolved closure does reach its representation.
        self.assertTrue(
            [name for name in _closure(modules, ["ProjectWorldMeshTerrainEditor"]) if name.startswith("MeshPartition")]
        )

        reached = sorted(name for name in _closure(modules, GENERIC_MODULES) if REPRESENTATION_MODULE.match(name))
        self.assertEqual([], reached, "Generic World modules resolve a representation module")
        descriptor = json.loads((GENERIC_PLUGIN / "ProjectWorld.uplugin").read_text(encoding="utf-8"))
        enabled = sorted(
            entry["Name"] for entry in descriptor.get("Plugins", []) if REPRESENTATION_MODULE.match(entry["Name"])
        )
        self.assertEqual([], enabled, "The generic World plugin enables a representation plugin")


if __name__ == "__main__":
    unittest.main()
