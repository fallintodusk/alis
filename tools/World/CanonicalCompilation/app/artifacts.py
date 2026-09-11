from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Any

from .contracts import (
    CompilerError,
    canonical_hash,
    file_hash,
    read_json,
    resolve_owned_path,
    validate_document,
    write_json,
)
from .features import CompiledFeatures
from .spatial import cell_bounds, cell_id


def output_descriptor(kind: str, path: Path, root: Path) -> dict[str, Any]:
    return {
        "kind": kind,
        "path": path.relative_to(root).as_posix(),
        "sha256": file_hash(path),
        "byte_size": path.stat().st_size,
    }


def _artifact_descriptor(
    kind: str,
    cell_identifier: str,
    path: Path,
    root: Path,
    dependencies: list[str],
) -> dict[str, Any]:
    return {
        "artifact_id": f"{kind}:{cell_identifier}",
        "cell_id": cell_identifier,
        "representation": kind,
        "content_hash": file_hash(path),
        "byte_size": path.stat().st_size,
        "schema_version": 1,
        "dependencies": dependencies,
        "provenance_result": "accepted",
    }


def _lineage_dependencies(lineage: dict[str, Any]) -> list[str]:
    return [
        f"compiler:{lineage['compiler_contract_sha256']}",
        *[f"source-snapshot:{item['snapshot_id']}" for item in lineage["source_snapshots"]],
        f"source-result:{lineage['source_result_sha256']}",
        f"source-area:{lineage['source_area']['area_fingerprint']}",
        f"raster-source:{lineage['raster_source_sha256']}",
        f"overlay:{lineage['overlay_sha256']}",
    ]


def _neighbors(
    identifier: str,
    targets: set[tuple[int, int]],
    x_value: int,
    y_value: int,
) -> dict[str, str | None]:
    return {
        side: cell_id(identifier, *coord) if coord in targets else None
        for side, coord in {
            "west": (x_value - 1, y_value),
            "east": (x_value + 1, y_value),
            "south": (x_value, y_value - 1),
            "north": (x_value, y_value + 1),
        }.items()
    }


def write_cells(
    staging: Path,
    profile: dict[str, Any],
    identifier: str,
    terrain: dict[str, dict[str, Any]],
    features: CompiledFeatures,
    terrain_rebuilt_ids: set[str],
    feature_rebuilt_ids: set[str],
    base_root: Path | None,
    current_lineages: dict[str, dict[str, Any]],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[str], set[str]]:
    targets = {(item["x"], item["y"]) for item in profile["target_cells"]}
    artifacts: list[dict[str, Any]] = []
    cell_entries: list[dict[str, Any]] = []
    feature_ids: list[str] = []
    manifest_updated_ids: set[str] = set()
    for x_value, y_value in sorted(targets):
        current_id = cell_id(identifier, x_value, y_value)
        stem = f"cell_x{x_value}_y{y_value}"
        terrain_path = staging / "canonical" / "terrain" / f"{stem}.json"
        feature_path = staging / "canonical" / "features" / f"{stem}.json"
        manifest_path = staging / "canonical" / "cells" / f"{stem}.json"
        base_manifest_path = base_root / manifest_path.relative_to(staging) if base_root else None
        base_manifest = None
        if base_manifest_path and base_manifest_path.is_file():
            base_manifest = read_json(base_manifest_path)
            validate_document(base_manifest, base_manifest_path)
        terrain_components = terrain[current_id]["raster_component_dependencies"]
        terrain_dependencies = [
            f"raster-component:{item['component_id']}:{item['sample_semantic_sha256']}"
            for item in terrain_components
        ]
        owned = features.by_owner[current_id]
        feature_document = {
            "$schema": "https://alis.world/schemas/world-compiler/canonical-feature-v1.json",
            "schema_version": 1,
            "grid_id": identifier,
            "cell_id": current_id,
            "features": owned,
        }
        if current_id in terrain_rebuilt_ids:
            write_json(terrain_path, terrain[current_id])
            terrain_lineage = current_lineages["terrain"]
            terrain_artifact = _artifact_descriptor(
                "terrain", current_id, terrain_path, staging,
                [*_lineage_dependencies(terrain_lineage), *terrain_dependencies],
            )
        else:
            if base_root is None or base_manifest is None:
                raise CompilerError("incremental_base_missing", "A reused cell has no accepted base")
            base_terrain = base_root / terrain_path.relative_to(staging)
            terrain_path.parent.mkdir(parents=True, exist_ok=True)
            if not base_terrain.is_file():
                raise CompilerError("base_cell_missing", "Incremental base is missing reused terrain")
            shutil.copy2(base_terrain, terrain_path)
            terrain_lineage = base_manifest["terrain_lineage"]
            terrain_artifact = {
                key: value for key, value in base_manifest["terrain_artifact"].items() if key != "path"
            }
            if (
                terrain_artifact["content_hash"] != file_hash(terrain_path)
                or terrain_artifact["byte_size"] != terrain_path.stat().st_size
            ):
                raise CompilerError("base_output_changed", "Reused terrain differs from its cell lineage")
        if current_id in feature_rebuilt_ids:
            write_json(feature_path, feature_document)
            feature_lineage = current_lineages["feature"]
            feature_artifact = _artifact_descriptor(
                "features", current_id, feature_path, staging, _lineage_dependencies(feature_lineage)
            )
        else:
            if base_root is None or base_manifest is None:
                raise CompilerError("incremental_base_missing", "A reused cell has no accepted base")
            base_features = base_root / feature_path.relative_to(staging)
            if not base_features.is_file():
                raise CompilerError("base_cell_missing", "Incremental base is missing reused features")
            if canonical_hash(read_json(base_features)) != canonical_hash(feature_document):
                raise CompilerError("impact_set_incomplete", "Unselected feature cell changed", cell_id=current_id)
            feature_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(base_features, feature_path)
            feature_lineage = base_manifest["feature_lineage"]
            feature_artifact = {
                key: value for key, value in base_manifest["feature_artifact"].items() if key != "path"
            }
            if (
                feature_artifact["content_hash"] != file_hash(feature_path)
                or feature_artifact["byte_size"] != feature_path.stat().st_size
            ):
                raise CompilerError("base_output_changed", "Reused features differ from their cell lineage")
        artifacts.extend((terrain_artifact, feature_artifact))
        owned_ids = [item["feature_id"] for item in owned]
        feature_ids.extend(owned_ids)
        manifest = {
            "$schema": "https://alis.world/schemas/world-compiler/cell-manifest-v1.json",
            "schema_version": 1,
            "grid_id": identifier,
            "cell_id": current_id,
            "cell_x": x_value,
            "cell_y": y_value,
            "bounds": list(cell_bounds(profile["grid"], x_value, y_value)),
            "neighbors": _neighbors(identifier, targets, x_value, y_value),
            "owned_feature_ids": owned_ids,
            "referenced_feature_ids": features.references[current_id],
            "terrain_artifact": {**terrain_artifact, "path": terrain_path.relative_to(staging).as_posix()},
            "feature_artifact": {**feature_artifact, "path": feature_path.relative_to(staging).as_posix()},
            "terrain_component_dependencies": terrain_components,
            "terrain_lineage": terrain_lineage,
            "feature_lineage": feature_lineage,
            "provenance_result": "accepted",
        }
        if base_manifest is None or canonical_hash(base_manifest) != canonical_hash(manifest):
            write_json(manifest_path, manifest)
            manifest_updated_ids.add(current_id)
        else:
            manifest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(base_manifest_path, manifest_path)
        cell_entries.append({
            "cell_id": current_id,
            "path": manifest_path.relative_to(staging).as_posix(),
            "sha256": file_hash(manifest_path),
            "byte_size": manifest_path.stat().st_size,
            "terrain_lineage": terrain_lineage,
            "feature_lineage": feature_lineage,
            "terrain_component_dependencies": terrain_components,
        })
    return artifacts, cell_entries, sorted(feature_ids), manifest_updated_ids


def existing_result(output_root: Path, run_inputs_hash: str) -> dict[str, Any] | None:
    result_path = output_root / "compile_result.json"
    if not result_path.is_file():
        return None
    result = read_json(result_path)
    validate_document(result, result_path)
    if result.get("status") != "accepted" or result.get("inputs_hash") != run_inputs_hash:
        raise CompilerError("output_conflict", "Output root belongs to another or incomplete compiler run")
    for entry in result["outputs"]:
        path = resolve_owned_path(output_root, entry["path"])
        if path.stat().st_size != entry["byte_size"] or file_hash(path) != entry["sha256"]:
            raise CompilerError("accepted_output_changed", "Accepted compiler output changed", kind=entry["kind"])
    return result


def accepted_base(result_path: Path) -> tuple[Path, dict[str, Any]]:
    if not result_path.is_file():
        raise CompilerError("base_result_missing", "Incremental compilation base result is unavailable")
    result = read_json(result_path)
    validate_document(result, result_path)
    if result.get("status") != "accepted" or result.get("operation") != "compile":
        raise CompilerError("base_result_rejected", "Incremental compilation requires an accepted compile result")
    root = result_path.parent
    for entry in result["outputs"]:
        path = resolve_owned_path(root, entry["path"])
        if path.stat().st_size != entry["byte_size"] or file_hash(path) != entry["sha256"]:
            raise CompilerError("base_output_changed", "Incremental base output differs from its receipt", path=entry["path"])
    return root, result


def promote_staging(staging: Path, output_root: Path) -> None:
    output_root.parent.mkdir(parents=True, exist_ok=True)
    if output_root.exists():
        raise CompilerError("output_conflict", "Compiler output appeared during staged promotion")
    os.replace(staging, output_root)
