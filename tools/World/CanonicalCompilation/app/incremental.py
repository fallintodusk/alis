from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import CompilerError, read_json, validate_document
from .features import CompiledFeatures
from .spatial import cell_bounds, cell_id, intersects


def terrain_impact_cells(
    profile: dict[str, Any],
    change_bounds: list[tuple[float, float, float, float]],
) -> set[tuple[int, int]]:
    if not change_bounds:
        return set()
    grid = profile["grid"]
    halo = int(grid["operation_halos"].get("terrain_resampling", 0))
    expand_x = halo * float(grid["sample_spacing"][0])
    expand_y = halo * float(grid["sample_spacing"][1])
    expanded = [
        (west - expand_x, south - expand_y, east + expand_x, north + expand_y)
        for west, south, east, north in change_bounds
    ]
    targets = {(int(item["x"]), int(item["y"])) for item in profile["target_cells"]}
    affected = {
        target
        for target in targets
        if any(intersects(cell_bounds(grid, *target), bounds) for bounds in expanded)
    }
    if not affected:
        raise CompilerError("terrain_change_outside_profile", "Terrain change does not affect a target cell")
    return affected


def load_base_terrain(
    base_root: Path,
    profile: dict[str, Any],
    identifier: str,
) -> dict[str, dict[str, Any]]:
    terrain: dict[str, dict[str, Any]] = {}
    expected_ids = {
        cell_id(identifier, int(item["x"]), int(item["y"]))
        for item in profile["target_cells"]
    }
    for path in sorted((base_root / "canonical" / "terrain").glob("*.json")):
        document = read_json(path)
        validate_document(document, path)
        if document.get("cell_id") not in expected_ids:
            raise CompilerError("base_grid_conflict", "Incremental base terrain uses another grid")
        terrain[document["cell_id"]] = document
    return terrain


def load_base_features(
    base_root: Path,
    profile: dict[str, Any],
    identifier: str,
) -> CompiledFeatures:
    by_owner = {
        cell_id(identifier, int(item["x"]), int(item["y"])): []
        for item in profile["target_cells"]
    }
    for path in sorted((base_root / "canonical" / "features").glob("*.json")):
        document = read_json(path)
        validate_document(document, path)
        owner = document["cell_id"]
        if owner not in by_owner:
            raise CompilerError("base_grid_conflict", "Incremental base features use another grid")
        by_owner[owner] = document["features"]
    rejections_path = base_root / "reports" / "rejections.json"
    validation_path = base_root / "reports" / "validation.json"
    provenance_path = base_root / "reports" / "provenance.json"
    rejections_document = read_json(rejections_path)
    validation_document = read_json(validation_path)
    provenance_document = read_json(provenance_path)
    for value, path in (
        (rejections_document, rejections_path),
        (validation_document, validation_path),
        (provenance_document, provenance_path),
    ):
        validate_document(value, path)
    target_check = next(
        (item for item in validation_document["checks"] if item["check"] == "target_selection"),
        None,
    )
    excluded = target_check["details"].get("excluded_counts", {}) if target_check else {}
    applied = provenance_document["overlay"].get("applied_feature_overrides", [])
    return CompiledFeatures(by_owner, _references(by_owner), rejections_document["rejections"], excluded, applied, [])


def merge_features(
    base: CompiledFeatures,
    changed_provider_ids: set[str],
    partial: CompiledFeatures,
    profile: dict[str, Any],
    identifier: str,
    overlay: dict[str, Any],
) -> tuple[CompiledFeatures, set[str]]:
    targets = {
        cell_id(identifier, int(item["x"]), int(item["y"]))
        for item in profile["target_cells"]
    }
    base_features = [feature for values in base.by_owner.values() for feature in values]

    def source_ids(feature: dict[str, Any]) -> set[str]:
        return {item["provider_feature_id"] for item in feature["source_refs"]}

    old_changed = [
        feature
        for feature in base_features
        if source_ids(feature) & changed_provider_ids
    ]
    new_features = [feature for values in partial.by_owner.values() for feature in values]
    known_ids = {
        provider_feature_id
        for feature in [*old_changed, *new_features]
        for provider_feature_id in source_ids(feature)
    }
    unknown = sorted(
        changed_provider_ids
        - known_ids
        - set(partial.processed_source_ids)
        - {item["source_identity"] for item in partial.rejections}
    )
    if unknown:
        raise CompilerError("feature_change_unknown", "Changed feature is absent from old and new source", ids=unknown)
    affected_ids = {
        current_id
        for feature in [*old_changed, *new_features]
        for current_id in feature["intersecting_cell_ids"]
    }
    if not affected_ids.issubset(targets):
        raise CompilerError("feature_impact_outside_profile", "Changed feature resolves outside target cells")
    kept = [
        feature
        for feature in base_features
        if not (source_ids(feature) & changed_provider_ids)
    ]
    merged_by_owner = {target: [] for target in targets}
    for feature in [*kept, *new_features]:
        merged_by_owner[feature["owner_cell_id"]].append(feature)
    for features in merged_by_owner.values():
        features.sort(key=lambda item: item["feature_id"])
    changed_rejections = [
        rejection
        for rejection in base.rejections
        if rejection["source_identity"] not in changed_provider_ids
    ]
    rejections = sorted(
        [*changed_rejections, *partial.rejections],
        key=lambda item: (item["source_identity"], item["reason_code"]),
    )
    feature_ids = {feature["feature_id"] for values in merged_by_owner.values() for feature in values}
    override_ids = {item["feature_id"] for item in overlay["feature_overrides"]}
    missing = sorted(override_ids - feature_ids)
    if missing:
        raise CompilerError("overlay_rebase_required", "Authored overlay target is missing", missing_feature_ids=missing)
    merged = CompiledFeatures(
        merged_by_owner,
        _references(merged_by_owner),
        rejections,
        base.excluded_counts,
        sorted(override_ids),
        partial.processed_source_ids,
    )
    return merged, affected_ids


def _references(by_owner: dict[str, list[dict[str, Any]]]) -> dict[str, list[str]]:
    references = {owner: [] for owner in by_owner}
    for features in by_owner.values():
        for feature in features:
            for current_id in feature["intersecting_cell_ids"]:
                if current_id != feature["owner_cell_id"]:
                    references[current_id].append(feature["feature_id"])
    return {key: sorted(set(values)) for key, values in references.items()}


def feature_rebuild_cells(
    base_root: Path,
    compiled: CompiledFeatures,
    profile: dict[str, Any],
    identifier: str,
) -> set[str]:
    rebuilt: set[str] = set()
    base = load_base_features(base_root, profile, identifier)
    old_features = {
        feature["feature_id"]: feature
        for values in base.by_owner.values()
        for feature in values
    }
    new_features = {
        feature["feature_id"]: feature
        for values in compiled.by_owner.values()
        for feature in values
    }
    for feature_id in set(old_features) | set(new_features):
        old, new = old_features.get(feature_id), new_features.get(feature_id)
        if old == new:
            continue
        rebuilt.update((old or {}).get("intersecting_cell_ids", []))
        rebuilt.update((new or {}).get("intersecting_cell_ids", []))
    for item in profile["target_cells"]:
        x_value, y_value = int(item["x"]), int(item["y"])
        current_id = cell_id(identifier, x_value, y_value)
        expected = {
            "$schema": "https://alis.world/schemas/world-compiler/canonical-feature-v1.json",
            "schema_version": 1,
            "grid_id": identifier,
            "cell_id": current_id,
            "features": compiled.by_owner[current_id],
        }
        path = base_root / "canonical" / "features" / f"cell_x{x_value}_y{y_value}.json"
        manifest_path = base_root / "canonical" / "cells" / f"cell_x{x_value}_y{y_value}.json"
        references_changed = (
            not manifest_path.is_file()
            or read_json(manifest_path).get("referenced_feature_ids") != compiled.references[current_id]
        )
        if not path.is_file() or read_json(path) != expected or references_changed:
            rebuilt.add(current_id)
    return rebuilt
