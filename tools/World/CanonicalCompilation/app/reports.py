from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import canonical_hash, read_json, validate_document, write_json
from .features import CompiledFeatures
from .source import SourceBundle


def write_reports(
    staging: Path,
    profile: dict[str, Any],
    bundle: SourceBundle,
    overlay: dict[str, Any],
    features: CompiledFeatures,
    validation: dict[str, Any],
    feature_ids: list[str],
    rebuilt_ids: set[str],
    reused_ids: set[str],
    base_root: Path | None,
    terrain_rebuilt_ids: set[str],
    feature_rebuilt_ids: set[str],
    cell_entries: list[dict[str, Any]],
    manifest_updated_ids: set[str],
) -> None:
    sources = [
        {
            key: snapshot[key]
            for key in ("snapshot_id", "provider", "dataset", "release", "hashes", "accuracy", "license", "policy_result")
        }
        for snapshot in bundle.ledger["snapshots"]
    ]
    if base_root:
        base_provenance_path = base_root / "reports" / "provenance.json"
        base_provenance = read_json(base_provenance_path)
        validate_document(base_provenance, base_provenance_path)
        sources = list({
            item["snapshot_id"]: item
            for item in [*base_provenance["sources"], *sources]
        }.values())
        sources.sort(key=lambda item: item["snapshot_id"])
    provenance = {
        "$schema": "https://alis.world/schemas/world-compiler/provenance-report-v1.json",
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        "policy_result": "accepted",
        "sources": sources,
        "feature_sources": [
            {"feature_id": item["feature_id"], "source_refs": item["source_refs"]}
            for values in features.by_owner.values()
            for item in values
        ],
        "raster_authority": {
            key: bundle.raster[key]
            for key in (
                "mosaic_contract_id", "mosaic_authority_id", "sample_semantic_sha256", "components"
            )
            if key in bundle.raster
        } or None,
        "cell_lineage": [
            {
                "cell_id": item["cell_id"],
                "terrain_lineage": item["terrain_lineage"],
                "feature_lineage": item["feature_lineage"],
                "terrain_component_dependencies": item["terrain_component_dependencies"],
            }
            for item in cell_entries
        ],
        "overlay": {
            "overlay_id": overlay["overlay_id"],
            "applied_feature_overrides": features.applied_overrides,
            "terrain_patch_ids": sorted(item["patch_id"] for item in overlay["terrain_patches"]),
        },
    }
    attribution_entries = sorted(
        (
            {"provider": item["provider"], "release": item["release"], "license": item["license"]}
            for item in sources
        ),
        key=lambda item: (item["provider"], item["release"]),
    )
    current_features = {
        item["feature_id"]: item
        for values in features.by_owner.values()
        for item in values
    }
    previous_features: dict[str, dict[str, Any]] = {}
    if base_root:
        for path in sorted((base_root / "canonical" / "features").glob("*.json")):
            document = read_json(path)
            validate_document(document, path)
            previous_features.update({item["feature_id"]: item for item in document["features"]})
    added = sorted(set(current_features) - set(previous_features))
    removed = sorted(set(previous_features) - set(current_features))
    changed = sorted(
        feature_id
        for feature_id in set(current_features) & set(previous_features)
        if canonical_hash(current_features[feature_id]) != canonical_hash(previous_features[feature_id])
    )
    documents = {
        "rejections.json": {
            "$schema": "https://alis.world/schemas/world-compiler/rejection-report-v1.json",
            "schema_version": 1,
            "profile_id": profile["profile_id"],
            "rejections": features.rejections,
        },
        "provenance.json": provenance,
        "attribution.json": {
            "$schema": "https://alis.world/schemas/world-compiler/attribution-report-v1.json",
            "schema_version": 1,
            "profile_id": profile["profile_id"],
            "entries": attribution_entries,
        },
        "validation.json": validation,
        "diff.json": {
            "$schema": "https://alis.world/schemas/world-compiler/diff-report-v1.json",
            "schema_version": 1,
            "profile_id": profile["profile_id"],
            "mode": "incremental" if base_root else "full",
            "rebuilt_cells": sorted(rebuilt_ids),
            "reused_cells": sorted(reused_ids),
            "terrain_rebuilt_cells": sorted(terrain_rebuilt_ids),
            "feature_rebuilt_cells": sorted(feature_rebuilt_ids),
            "manifest_updated_cells": sorted(manifest_updated_ids),
            "added_features": added if base_root else feature_ids,
            "changed_features": changed,
            "removed_features": removed,
        },
    }
    for name, document in documents.items():
        write_json(staging / "reports" / name, document)
