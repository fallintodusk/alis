from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .adapters import adapt_feature
from .buildings import prepare_buildings
from .contracts import CompilerError
from .geometry import clip_line, geometry_rejection_reason
from .membership import feature_cell_membership
from .projection import project_features
from .source import SourceBundle
from .spatial import cell_bounds, cell_id
from .water import prepare_water_features


@dataclass(frozen=True)
class CompiledFeatures:
    by_owner: dict[str, list[dict[str, Any]]]
    references: dict[str, list[str]]
    rejections: list[dict[str, str]]
    excluded_counts: dict[str, int]
    applied_overrides: list[str]
    processed_source_ids: list[str]


def _representations(
    feature: dict[str, Any],
    intersecting_cells: list[tuple[int, int]],
    grid_identifier: str,
    grid: dict[str, Any],
    owner_id: str,
    membership_geometry: dict[str, Any] | None = None,
) -> list[dict[str, Any]]:
    representations: list[dict[str, Any]] = []
    for cell_x, cell_y in intersecting_cells:
        current_cell = cell_id(grid_identifier, cell_x, cell_y)
        geometry_type = feature["geometry"]["type"]
        if geometry_type in {"Polygon", "MultiPolygon"}:
            representations.append({
                "cell_id": current_cell,
                "representation": (
                    f"authoritative_{feature['feature_class']}"
                    if current_cell == owner_id
                    else f"{feature['feature_class']}_reference"
                ),
            })
            continue
        if geometry_type in {"Point", "MultiPoint"}:
            representations.append({
                "cell_id": current_cell,
                "representation": "foliage_point",
            })
            continue
        lines = (
            feature["geometry"]["coordinates"]
            if geometry_type == "MultiLineString"
            else [feature["geometry"]["coordinates"]]
        )
        fragments = [
            fragment
            for line in lines
            for fragment in clip_line(line, cell_bounds(grid, cell_x, cell_y), float(grid["coordinate_quantization"]))
        ]
        if not fragments:
            if (
                feature["feature_class"] == "water"
                and feature["attributes"].get("surface_geometry") == "ribbon"
                and membership_geometry is not None
            ):
                representations.append({
                    "cell_id": current_cell,
                    "representation": (
                        "authoritative_water_ribbon"
                        if current_cell == owner_id
                        else "water_ribbon_reference"
                    ),
                })
            continue
        representations.append({
            "cell_id": current_cell,
            "representation": f"{feature['feature_class']}_fragment",
            "geometry": {
                "type": "LineString" if len(fragments) == 1 else "MultiLineString",
                "coordinates": fragments[0] if len(fragments) == 1 else fragments,
            },
        })
    return representations


def compile_features(
    bundle: SourceBundle,
    profile: dict[str, Any],
    grid_identifier: str,
    overlay: dict[str, Any],
    repo_root: Path | None = None,
    scratch_root: Path | None = None,
    source_features: list[dict[str, Any]] | None = None,
    require_all_overrides: bool = True,
    terrain: dict[str, dict[str, Any]] | None = None,
) -> CompiledFeatures:
    grid = profile["grid"]
    targets = sorted((int(cell["x"]), int(cell["y"])) for cell in profile["target_cells"])
    snapshot = next(
        (item for item in bundle.ledger["snapshots"] if item["snapshot_id"] == bundle.feature_snapshot_id),
        None,
    )
    if snapshot is None:
        raise CompilerError("source_snapshot_missing", "Feature snapshot is absent from the source ledger")
    if snapshot.get("crs", {}).get("horizontal") != "EPSG:4326":
        raise CompilerError("feature_crs_unsupported", "Canonical compiler currently requires WGS84 source features")
    overrides = {item["feature_id"]: item for item in overlay["feature_overrides"]}
    applied_overrides: set[str] = set()
    by_owner = {cell_id(grid_identifier, *cell): [] for cell in targets}
    references: dict[str, list[str]] = {cell_id(grid_identifier, *cell): [] for cell in targets}
    rejections: list[dict[str, str]] = []
    excluded_counts: dict[str, int] = {}
    seen_ids: set[str] = set()
    requested_features = source_features if source_features is not None else bundle.features
    requested_source_ids = (
        None if source_features is None
        else {feature["provider_feature_id"] for feature in source_features}
    )
    projection_features = list(requested_features)
    if source_features is not None and profile.get("building_semantics"):
        by_source_id = {feature["provider_feature_id"]: feature for feature in projection_features}
        for feature in bundle.features:
            if feature["provider_class"] == "building":
                by_source_id[feature["provider_feature_id"]] = feature
        projection_features = list(by_source_id.values())
    if source_features is not None and profile.get("water_semantics"):
        by_source_id = {feature["provider_feature_id"]: feature for feature in projection_features}
        for feature in bundle.features:
            if feature["provider_class"] == "water":
                by_source_id[feature["provider_feature_id"]] = feature
        projection_features = list(by_source_id.values())
    production = grid["coordinate_transform"] != "fixture_affine"
    if production and (repo_root is None or scratch_root is None):
        raise CompilerError("projection_authority_required", "Production features require the pinned OGR authority")
    projected, industrial_rejections = project_features(
        repo_root or Path(), projection_features, grid, scratch_root or Path()
    )
    building_preparation = None
    if profile.get("building_semantics"):
        building_preparation = prepare_buildings(
            projection_features,
            projected,
            profile["identity_namespace"],
            snapshot,
            profile["building_semantics"],
            grid,
            repo_root or Path(),
            (scratch_root or Path()) / "buildings",
            industrial_rejections,
            requested_source_ids,
        )
        selected_features = [
            feature for feature in projection_features if feature["provider_class"] != "building"
        ] + building_preparation.source_features
        rejections.extend(building_preparation.rejections)
        excluded_counts.update(building_preparation.excluded_counts)
    else:
        selected_features = projection_features
    source_memberships = feature_cell_membership(
        repo_root or Path(), projected, grid, targets, (scratch_root or Path()) / "source_membership"
    )
    water = prepare_water_features(
        [
            feature for feature in selected_features
            if feature["provider_feature_id"] not in industrial_rejections
        ],
        profile,
        projected,
        terrain,
        repo_root or Path(),
        (scratch_root or Path()) / "water",
        {
            source_id for source_id, memberships in source_memberships.items()
            if memberships
        },
        targets,
    )
    membership_geometries = {
        source_id: geometry for source_id, geometry in projected.items()
        if source_memberships.get(source_id)
    }
    if building_preparation:
        for feature in projection_features:
            if feature["provider_class"] == "building":
                membership_geometries.pop(feature["provider_feature_id"], None)
        membership_geometries.update({
            feature["provider_feature_id"]: feature["geometry"]
            for feature in building_preparation.source_features
        })
    for source_identity, visible_geometry in water.visible_geometries.items():
        if visible_geometry is None:
            membership_geometries.pop(source_identity, None)
        else:
            membership_geometries[source_identity] = visible_geometry
    membership_geometries.update(water.membership_geometries)
    memberships = feature_cell_membership(
        repo_root or Path(), membership_geometries, grid, targets, (scratch_root or Path()) / "membership"
    )
    for source_feature in sorted(selected_features, key=lambda item: item["provider_feature_id"]):
        source_identity = source_feature["provider_feature_id"]
        if source_identity in industrial_rejections:
            rejections.append({
                "source_identity": source_identity,
                "reason_code": "industrial_geometry_invalid",
                "message": "Pinned OGR/GEOS rejected the feature geometry",
            })
            continue
        if source_identity in water.excluded:
            rejections.append({
                "source_identity": source_identity,
                "reason_code": water.excluded[source_identity],
                "message": "Water policy excludes this record from the permanent visible surface",
            })
            continue
        prepared_candidate = source_feature.get("prepared_canonical_feature")
        if prepared_candidate is not None:
            candidate, excluded_reason = deepcopy(prepared_candidate), None
        else:
            candidate, excluded_reason = adapt_feature(
                source_feature,
                profile["identity_namespace"],
                grid,
                snapshot,
                projected.get(source_identity),
            )
        if candidate is None:
            excluded_counts[excluded_reason or "unsupported"] = excluded_counts.get(excluded_reason or "unsupported", 0) + 1
            continue
        if source_identity in water.attributes:
            candidate["attributes"].update(water.attributes[source_identity])
        if source_identity in water.visible_geometries:
            visible_geometry = water.visible_geometries[source_identity]
            if visible_geometry is None:
                excluded_counts["water_axis_owned_by_polygon"] = (
                    excluded_counts.get("water_axis_owned_by_polygon", 0) + 1
                )
                continue
            candidate["geometry"] = visible_geometry
        if candidate["feature_id"] in seen_ids:
            raise CompilerError("duplicate_feature_identity", "Two source features resolve to one ALIS identity", feature_id=candidate["feature_id"])
        seen_ids.add(candidate["feature_id"])
        reason = geometry_rejection_reason(
            candidate["geometry"],
            candidate["feature_class"],
            check_self_intersections=not production,
        )
        if reason:
            rejections.append({
                "source_identity": source_feature["provider_feature_id"],
                "reason_code": reason,
                "message": "Feature geometry is invalid for canonical compilation",
            })
            continue
        intersecting_cells = memberships.get(source_identity, [])
        if not intersecting_cells:
            excluded_counts["outside_target_cells"] = excluded_counts.get("outside_target_cells", 0) + 1
            continue
        owner_cell = intersecting_cells[0]
        owner_id = cell_id(grid_identifier, *owner_cell)
        candidate["owner_cell_id"] = owner_id
        candidate["intersecting_cell_ids"] = [cell_id(grid_identifier, *cell) for cell in intersecting_cells]
        candidate["representations"] = _representations(
            candidate,
            intersecting_cells,
            grid_identifier,
            grid,
            owner_id,
            water.membership_geometries.get(source_identity),
        )
        override = overrides.get(candidate["feature_id"])
        if override:
            candidate["attributes"].update(override["set"])
            candidate["authored_overlay_ids"] = [overlay["overlay_id"]]
            applied_overrides.add(candidate["feature_id"])
        by_owner[owner_id].append(candidate)
        for current_cell in intersecting_cells:
            current_id = cell_id(grid_identifier, *current_cell)
            if current_id != owner_id:
                references[current_id].append(candidate["feature_id"])
    missing_overrides = sorted(set(overrides) - applied_overrides)
    if require_all_overrides and missing_overrides:
        raise CompilerError(
            "overlay_rebase_required",
            "Authored overlay target is missing after source compilation",
            missing_feature_ids=missing_overrides,
        )
    for value in by_owner.values():
        value.sort(key=lambda item: item["feature_id"])
    for key in references:
        references[key] = sorted(set(references[key]))
    return CompiledFeatures(
        by_owner,
        references,
        rejections,
        excluded_counts,
        sorted(applied_overrides),
        sorted(feature["provider_feature_id"] for feature in selected_features),
    )
