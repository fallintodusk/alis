"""Fail-closed fitness of the World proof-input roots against UBT's resolved module graph.

The common-check receipt hashes explicit roots (contract_inputs.py). Every first-party
module that producing code links must lie inside them. UBT's JsonExport of AlisEditor is
the resolved graph: Build.cs rules are C# and are never parsed here.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

from .contract_inputs import RUNTIME_PROVIDER_MODULES, covers_module_directory
from .contracts import ValidationFailure
from .producer_descriptors import ProducerDescriptorError, load_producer_descriptors


TARGET_EXPORT = "Binaries/Win64/AlisEditor.json"
EXPORT_SCRIPT = "scripts/ue/build/export_target_json.ps1"
DEPENDENCY_FIELDS = (
    "PublicDependencyModules",
    "PrivateDependencyModules",
    "PublicIncludePathModules",
    "PrivateIncludePathModules",
    "DynamicallyLoadedModules",
)
GRAPH_INPUT_SUFFIXES = (".Build.cs", ".Target.cs", ".uplugin")
SKIPPED_DIRECTORIES = frozenset({"Binaries", "Content", "Data", "DerivedDataCache", "Intermediate", "Saved"})


def _graph_inputs(repo_root: Path) -> list[Path]:
    """Project files UBT resolves the module graph from."""
    found = [repo_root / "Alis.uproject"]
    for top in ("Source", "Plugins"):
        for directory, children, names in os.walk(repo_root / top):
            children[:] = [name for name in children if name not in SKIPPED_DIRECTORIES]
            found.extend(Path(directory) / name for name in names if name.endswith(GRAPH_INPUT_SUFFIXES))
    return [path for path in found if path.is_file()]


def _load_export(repo_root: Path) -> dict[str, Any]:
    path = repo_root / TARGET_EXPORT
    if not path.is_file():
        raise ValidationFailure(
            "proof_input_export_missing",
            f"The UBT target export is missing; run {EXPORT_SCRIPT}",
            path=str(path),
        )
    export_time = path.stat().st_mtime_ns
    newer = sorted(
        item.relative_to(repo_root).as_posix()
        for item in _graph_inputs(repo_root)
        if item.stat().st_mtime_ns > export_time
    )
    if newer:
        raise ValidationFailure(
            "proof_input_export_stale",
            f"The UBT target export is older than a module graph input; run {EXPORT_SCRIPT}",
            path=str(path),
            newer=newer[0],
        )
    try:
        document = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValidationFailure(
            "proof_input_export_invalid",
            "The UBT target export is unreadable",
            path=str(path),
        ) from error
    modules = document.get("Modules") if isinstance(document, dict) else None
    project = document.get("ProjectFile") if isinstance(document, dict) else None
    if (
        not isinstance(modules, dict)
        or document.get("Name") != "AlisEditor"
        or not isinstance(project, str)
        or Path(project).resolve() != (repo_root / "Alis.uproject").resolve()
    ):
        raise ValidationFailure(
            "proof_input_export_invalid",
            "The UBT target export is not this project's AlisEditor module graph",
            path=str(path),
        )
    return modules


def _first_party_closure(modules: dict[str, Any], seeds: list[str], repo_root: Path) -> dict[str, str]:
    """Repository-relative directory of every first-party module the seeds reach."""
    root = repo_root.resolve()
    reached: dict[str, str] = {}
    seen: set[str] = set()
    pending = list(seeds)
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        module = modules.get(name)
        directory = module.get("Directory") if isinstance(module, dict) else None
        if not isinstance(directory, str) or not Path(directory).is_absolute():
            raise ValidationFailure(
                "proof_input_export_invalid",
                "The UBT target export has no module directory for a reached module",
                module=name,
            )
        resolved = Path(directory).resolve()
        if resolved.is_relative_to(root):
            reached[name] = resolved.relative_to(root).as_posix()
        for field in DEPENDENCY_FIELDS:
            dependencies = module.get(field, [])
            if not isinstance(dependencies, list) or not all(isinstance(item, str) for item in dependencies):
                raise ValidationFailure(
                    "proof_input_export_invalid",
                    "The UBT target export has a malformed dependency list",
                    module=name,
                    field=field,
                )
            pending.extend(dependencies)
    return reached


def assert_proof_input_fitness(repo_root: Path) -> None:
    try:
        descriptors = load_producer_descriptors(repo_root)
    except ProducerDescriptorError as error:
        raise ValidationFailure("proof_input_descriptor_invalid", str(error)) from error
    modules = _load_export(repo_root)
    named = {name for descriptor in descriptors for name in descriptor.modules}
    seeds = sorted(named | set(RUNTIME_PROVIDER_MODULES))
    absent = [name for name in seeds if name not in modules]
    if absent:
        raise ValidationFailure(
            "proof_input_module_missing",
            "A descriptor-named or runtime-provider module is absent from the UBT target export; "
            f"run {EXPORT_SCRIPT}",
            modules=absent,
        )
    reached = _first_party_closure(modules, seeds, repo_root)
    outside_repository = [name for name in seeds if name not in reached]
    if outside_repository:
        raise ValidationFailure(
            "proof_input_module_missing",
            "A descriptor-named or runtime-provider module is not a first-party module of this project",
            modules=outside_repository,
        )
    uncovered = {
        name: directory
        for name, directory in sorted(reached.items())
        if not covers_module_directory(directory)
    }
    if uncovered:
        raise ValidationFailure(
            "proof_input_root_uncovered",
            "Producing code reaches first-party modules outside the declared proof-input roots; "
            "declare their source roots in contract_inputs.py",
            modules=uncovered,
        )
