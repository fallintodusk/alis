from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import CompilerError, canonical_hash, read_json, validate_document
from .source import SourceBundle


def _snapshot_contracts(
    bundle: SourceBundle, source_ids: set[str]
) -> list[dict[str, Any]]:
    return sorted(({
        key: snapshot[key]
        for key in ("snapshot_id", "provider", "dataset", "release", "hashes")
    } for snapshot in bundle.ledger["snapshots"] if snapshot["source_id"] in source_ids),
        key=lambda item: item["snapshot_id"])


def _raster_source_hash(bundle: SourceBundle) -> str:
    artifact = bundle.raster.get("data_artifact")
    if isinstance(artifact, dict) and isinstance(artifact.get("sha256"), str):
        return artifact["sha256"]
    return canonical_hash(bundle.raster.get("provider_payload"))


def _source_contract(source: dict[str, Any]) -> dict[str, Any]:
    return {
        key: source[key]
        for key in ("source_id", "adapter", "coverage_bbox", "crs", "decode")
        if key in source
    }


def build_lineages(
    profile: dict[str, Any],
    bundle: SourceBundle,
    run_contract: dict[str, Any],
    overlay: dict[str, Any],
) -> dict[str, dict[str, Any]]:
    compiler_contract = {
        "algorithm_version": profile.get("algorithm_version", "alis-world-compiler-1"),
        "identity_namespace": profile["identity_namespace"],
        "implementation_sha256": run_contract["implementation_sha256"],
        "execution_environment_identity": run_contract["execution_environment"]["identity_sha256"],
    }
    area = bundle.ledger["area"]
    common = {
        "grid_id": run_contract["grid_id"],
        **compiler_contract,
        "compiler_contract_sha256": canonical_hash(compiler_contract),
        "source_result_sha256": run_contract["source_result_sha256"],
        "source_area": {
            "area_fingerprint": area["area_fingerprint"],
            "bbox": area["bbox"],
            "axis_order": area["axis_order"],
            "crs": area["crs"],
        },
        "overlay_id": overlay["overlay_id"],
        "overlay_sha256": run_contract["overlay_sha256"],
    }
    sources = {item["source_id"]: item for item in bundle.source_profile["sources"]}
    terrain_source_ids = {
        item["source_id"] for item in bundle.raster.get("components", [])
    } or set(sources)
    feature_source_ids = {
        snapshot["source_id"] for snapshot in bundle.ledger["snapshots"]
        if snapshot["snapshot_id"] == bundle.feature_snapshot_id
    }
    raster_contract = {
        key: bundle.raster.get(key)
        for key in (
            "mosaic_contract_id", "raster_class", "crs", "vertical_datum",
            "vertical_provenance", "pixel_type", "size", "transform", "nodata",
        )
    }
    terrain_semantics = {
        "mosaic_contract_id": bundle.raster.get("mosaic_contract_id"),
        "raster_contract": raster_contract,
    }
    if not bundle.raster.get("components"):
        terrain_semantics["raster_source_sha256"] = _raster_source_hash(bundle)
    terrain = {
        **common,
        "profile_contract_sha256": canonical_hash({
            "grid": profile["grid"],
            "raster_sampling": profile.get("raster_sampling"),
        }),
        "source_ingestion_contract_sha256": canonical_hash({
            "kind": "terrain", "raster_contract": raster_contract,
            "sources": [_source_contract(sources[item]) for item in sorted(terrain_source_ids)],
        }),
        "source_semantic_contract_sha256": canonical_hash(terrain_semantics),
        "source_snapshots": _snapshot_contracts(bundle, terrain_source_ids),
        "raster_source_sha256": _raster_source_hash(bundle),
    }
    feature_ingestion_contract = {
        "kind": "features",
        "sources": [_source_contract(sources[item]) for item in sorted(feature_source_ids)],
    }
    feature = {
        **common,
        "profile_contract_sha256": canonical_hash({
            "identity_namespace": profile["identity_namespace"],
            "water_semantics": profile.get("water_semantics"),
        }),
        "source_ingestion_contract_sha256": canonical_hash(feature_ingestion_contract),
        "source_semantic_contract_sha256": canonical_hash({
            "feature_contract": feature_ingestion_contract,
        }),
        "source_snapshots": _snapshot_contracts(bundle, feature_source_ids),
        "raster_source_sha256": canonical_hash({"kind": "not_applicable"}),
    }
    return {"terrain": terrain, "feature": feature}


def base_cells(base_root: Path) -> dict[tuple[int, int], dict[str, Any]]:
    documents: dict[tuple[int, int], dict[str, Any]] = {}
    for path in sorted((base_root / "canonical" / "cells").glob("*.json")):
        document = read_json(path)
        validate_document(document, path)
        coordinate = int(document["cell_x"]), int(document["cell_y"])
        if coordinate in documents:
            raise CompilerError("base_cell_conflict", "Accepted base repeats a cell coordinate")
        documents[coordinate] = document
    if not documents:
        raise CompilerError("base_cell_missing", "Accepted base contains no canonical cells")
    return documents


def _area_contains(current: dict[str, Any], previous: dict[str, Any]) -> bool:
    if current["crs"] != previous["crs"] or current["axis_order"] != previous["axis_order"]:
        return False
    current_bounds, previous_bounds = current["bbox"], previous["bbox"]
    return (
        current_bounds[0] <= previous_bounds[0]
        and current_bounds[1] <= previous_bounds[1]
        and current_bounds[2] >= previous_bounds[2]
        and current_bounds[3] >= previous_bounds[3]
    )


def reusable_layers(
    base_documents: dict[tuple[int, int], dict[str, Any]],
    current_lineages: dict[str, dict[str, Any]],
    source_result_changed: bool,
) -> dict[str, tuple[bool, list[str]]]:
    common_fields = (
        "grid_id",
        "algorithm_version",
        "identity_namespace",
        "compiler_contract_sha256",
    )
    feature_fields = ["source_ingestion_contract_sha256"]
    if source_result_changed:
        feature_fields.append("source_snapshots")
    layer_fields = {
        "terrain": (
            "profile_contract_sha256",
            "source_ingestion_contract_sha256",
            "source_semantic_contract_sha256",
        ),
        "feature": ("profile_contract_sha256", *feature_fields),
    }
    decisions: dict[str, tuple[bool, list[str]]] = {}
    for layer, lineage_name in (("terrain", "terrain_lineage"), ("feature", "feature_lineage")):
        reasons: set[str] = set()
        current = current_lineages[layer]
        stable_fields = (*common_fields, *layer_fields[layer])
        for document in base_documents.values():
            previous = document[lineage_name]
            reasons.update(field for field in stable_fields if previous.get(field) != current.get(field))
            if not _area_contains(current["source_area"], previous["source_area"]):
                reasons.add("source_area_not_monotonic")
        decisions[layer] = (not reasons, sorted(reasons))
    return decisions


def derive_overlay_changes(
    previous: dict[str, Any], current: dict[str, Any]
) -> tuple[list[tuple[float, float, float, float]], set[str]]:
    terrain_bounds: list[tuple[float, float, float, float]] = []
    old_patches = {item["patch_id"]: item for item in previous["terrain_patches"]}
    new_patches = {item["patch_id"]: item for item in current["terrain_patches"]}
    for patch_id in sorted(set(old_patches) | set(new_patches)):
        old, new = old_patches.get(patch_id), new_patches.get(patch_id)
        if canonical_hash(old) == canonical_hash(new):
            continue
        for patch in (old, new):
            if patch:
                x_value, y_value = patch["center"]
                radius = patch["radius_m"]
                terrain_bounds.append((x_value - radius, y_value - radius, x_value + radius, y_value + radius))
    old_overrides = {item["feature_id"]: item for item in previous["feature_overrides"]}
    new_overrides = {item["feature_id"]: item for item in current["feature_overrides"]}
    changed_features = {
        feature_id
        for feature_id in set(old_overrides) | set(new_overrides)
        if canonical_hash(old_overrides.get(feature_id)) != canonical_hash(new_overrides.get(feature_id))
    }
    if previous["overlay_id"] != current["overlay_id"]:
        changed_features.update(set(old_overrides) | set(new_overrides))
        for patch in [*old_patches.values(), *new_patches.values()]:
            x_value, y_value = patch["center"]
            radius = patch["radius_m"]
            terrain_bounds.append((x_value - radius, y_value - radius, x_value + radius, y_value + radius))
    return sorted(set(terrain_bounds)), changed_features
