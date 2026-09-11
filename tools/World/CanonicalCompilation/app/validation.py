from __future__ import annotations

from typing import Any

from .contracts import CompilerError
from .features import CompiledFeatures
from .spatial import cell_bounds, cell_id, quantize


def _boundary_details(
    terrain_cells: dict[str, dict[str, Any]],
    grid_identifier: str,
    targets: list[tuple[int, int]],
) -> dict[str, Any]:
    mismatches: list[dict[str, str]] = []
    target_set = set(targets)
    comparisons = 0
    for x_value, y_value in targets:
        current = terrain_cells[cell_id(grid_identifier, x_value, y_value)]
        for dx, dy, side, opposite in ((1, 0, "east", "west"), (0, 1, "north", "south")):
            neighbor_coord = x_value + dx, y_value + dy
            if neighbor_coord not in target_set:
                continue
            neighbor = terrain_cells[cell_id(grid_identifier, *neighbor_coord)]
            comparisons += 1
            for key in ("height_edges", "height_border_bands"):
                if current[key][side] != neighbor[key][opposite]:
                    mismatches.append({"cell": current["cell_id"], "neighbor": neighbor["cell_id"], "contract": key})
            for key in ("edges", "border_bands"):
                if current["weight_mask"][key][side] != neighbor["weight_mask"][key][opposite]:
                    mismatches.append({"cell": current["cell_id"], "neighbor": neighbor["cell_id"], "contract": f"weight_{key}"})
    if mismatches:
        raise CompilerError("boundary_mismatch", "Canonical neighboring cells disagree", mismatches=mismatches)
    return {"comparisons": comparisons, "mismatches": 0}


def _line_endpoints(geometry: dict[str, Any]) -> list[tuple[float, float]]:
    lines = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
    return [tuple(point) for line in lines for point in (line[0], line[-1])]


def _line_boundary_endpoints(
    geometry: dict[str, Any], axis: int, boundary: float, span: tuple[float, float], step: float
) -> tuple[tuple[float, float], ...]:
    epsilon = step * 1e-7
    return tuple(sorted({
        tuple(quantize(float(value), step) for value in point)
        for point in _line_endpoints(geometry)
        if abs(float(point[axis]) - boundary) <= epsilon
        and span[0] - epsilon <= float(point[1 - axis]) <= span[1] + epsilon
    }))


def _polygon_rings(geometry: dict[str, Any]) -> list[list[list[float]]]:
    polygons = geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]
    return [ring for polygon in polygons for ring in polygon]


def _polygon_seam_segments(
    geometry: dict[str, Any], axis: int, boundary: float, span: tuple[float, float], step: float
) -> tuple[tuple[float, float], ...]:
    crossings: list[float] = []
    segments: list[tuple[float, float]] = []
    other = 1 - axis
    epsilon = step * 1e-7
    for ring in _polygon_rings(geometry):
        for start, end in zip(ring, ring[1:], strict=False):
            start_axis, end_axis = float(start[axis]), float(end[axis])
            if abs(start_axis - boundary) <= epsilon and abs(end_axis - boundary) <= epsilon:
                low = min(float(start[other]), float(end[other]))
                high = max(float(start[other]), float(end[other]))
                if high - low > epsilon:
                    segments.append((quantize(low, step), quantize(high, step)))
                continue
            crosses = (
                start_axis < boundary <= end_axis
                or end_axis < boundary <= start_axis
            )
            if not crosses:
                continue
            ratio = (boundary - start_axis) / (end_axis - start_axis)
            value = float(start[other]) + ratio * (float(end[other]) - float(start[other]))
            crossings.append(quantize(value, step))
    crossings.sort()
    if len(crossings) % 2:
        raise CompilerError("water_seam_mismatch", "Water polygon has an unpaired cell-edge crossing")
    segments.extend(
        (low, high)
        for low, high in zip(crossings[::2], crossings[1::2], strict=True)
        if high - low > epsilon
    )
    clipped = (
        (max(low, span[0]), min(high, span[1]))
        for low, high in segments
    )
    return tuple(sorted({
        (quantize(low, step), quantize(high, step))
        for low, high in clipped
        if high - low > epsilon
    }))


def _feature_boundary_details(
    features: list[dict[str, Any]], grid_identifier: str, grid: dict[str, Any]
) -> dict[str, Any]:
    multi_cell = 0
    road_seams = 0
    water_line_seams = 0
    water_polygon_seams = 0
    targets = {
        cell_id(grid_identifier, int(item["x"]), int(item["y"])): (int(item["x"]), int(item["y"]))
        for item in grid["target_cells"]
    }
    for feature in features:
        intersecting = set(feature["intersecting_cell_ids"])
        representations = feature["representations"]
        represented = {item["cell_id"] for item in representations}
        if intersecting != represented or len(represented) != len(representations):
            raise CompilerError(
                "feature_boundary_mismatch",
                "Canonical feature representations disagree with cell membership",
                feature_id=feature["feature_id"],
            )
        if len(intersecting) <= 1:
            continue
        multi_cell += 1
        feature_class = feature["feature_class"]
        geometry_type = feature["geometry"]["type"]
        linear = geometry_type in {"LineString", "MultiLineString"}
        if feature_class not in {"road", "water"}:
            continue
        if linear:
            footprint_only = [item for item in representations if "geometry" not in item]
            if footprint_only and (
                feature_class != "water"
                or feature["attributes"].get("surface_geometry") != "ribbon"
                or feature["attributes"].get("polygon_overlap_area_m2") != 0.0
                or any(item["representation"] not in {
                    "authoritative_water_ribbon", "water_ribbon_reference"
                } for item in footprint_only)
            ):
                raise CompilerError(
                    "feature_boundary_mismatch",
                    "A footprint-only cell lacks valid ribbon authority",
                    feature_id=feature["feature_id"],
                )
            geometry_by_cell = {
                item["cell_id"]: item["geometry"] for item in representations if "geometry" in item
            }
            has_shared_seam = False
            seamed_cells: set[str] = set()
            coordinate_step = float(grid["coordinate_quantization"])
            for current_id in sorted(intersecting):
                current = targets[current_id]
                current_bounds = cell_bounds(grid, *current)
                for neighbor, axis, boundary, span in (
                    ((current[0] + 1, current[1]), 0, current_bounds[2], (current_bounds[1], current_bounds[3])),
                    ((current[0], current[1] + 1), 1, current_bounds[3], (current_bounds[0], current_bounds[2])),
                ):
                    neighbor_id = cell_id(grid_identifier, *neighbor)
                    if current_id not in geometry_by_cell or neighbor_id not in geometry_by_cell:
                        continue
                    left = _line_boundary_endpoints(
                        geometry_by_cell[current_id], axis, boundary, span, coordinate_step
                    )
                    right = _line_boundary_endpoints(
                        geometry_by_cell[neighbor_id], axis, boundary, span, coordinate_step
                    )
                    if left or right:
                        if left != right:
                            raise CompilerError(
                                f"{feature_class}_seam_mismatch",
                                f"Cross-cell {feature_class} fragments disagree on their shared boundary",
                                feature_id=feature["feature_id"],
                            )
                        has_shared_seam = True
                        seamed_cells.update((current_id, neighbor_id))
            if feature_class == "road":
                if len(geometry_by_cell) > 1 and set(geometry_by_cell) != seamed_cells:
                    raise CompilerError(
                        "road_seam_mismatch",
                        "Cross-cell road fragments do not form exact shared-boundary seams",
                        feature_id=feature["feature_id"],
                    )
                road_seams += int(has_shared_seam)
            else:
                water_line_seams += int(has_shared_seam)
            continue
        if feature_class != "water":
            continue
        coordinate_step = float(grid["coordinate_quantization"])
        for current_id in sorted(intersecting):
            current = targets[current_id]
            current_bounds = cell_bounds(grid, *current)
            for neighbor, axis, boundary, span in (
                ((current[0] + 1, current[1]), 0, current_bounds[2], (current_bounds[1], current_bounds[3])),
                ((current[0], current[1] + 1), 1, current_bounds[3], (current_bounds[0], current_bounds[2])),
            ):
                neighbor_id = cell_id(grid_identifier, *neighbor)
                if neighbor_id not in intersecting:
                    continue
                neighbor_bounds = cell_bounds(grid, *neighbor)
                neighbor_boundary = neighbor_bounds[0] if axis == 0 else neighbor_bounds[1]
                neighbor_span = (neighbor_bounds[1], neighbor_bounds[3]) if axis == 0 else (neighbor_bounds[0], neighbor_bounds[2])
                left = _polygon_seam_segments(feature["geometry"], axis, boundary, span, coordinate_step)
                right = _polygon_seam_segments(
                    feature["geometry"], axis, neighbor_boundary, neighbor_span, coordinate_step
                )
                if left != right:
                    raise CompilerError(
                        "water_seam_mismatch",
                        "Cross-cell water references do not derive the same quantized boundary segment",
                        feature_id=feature["feature_id"],
                    )
                water_polygon_seams += bool(left)
    return {
        "multi_cell_features": multi_cell,
        "road_seams": road_seams,
        "water_seams": water_line_seams + water_polygon_seams,
        "water_line_seams": water_line_seams,
        "water_polygon_seams": water_polygon_seams,
        "mismatches": 0,
    }


def _quality_details(
    profile: dict[str, Any], grid_identifier: str, features: list[dict[str, Any]]
) -> dict[str, Any]:
    details: dict[str, Any] = {}
    for quality in profile.get("quality_cells", []):
        identifiers = {
            cell_id(grid_identifier, int(item["x"]), int(item["y"]))
            for item in quality["cells"]
        }
        selected = [
            feature for feature in features
            if identifiers.intersection(feature["intersecting_cell_ids"])
        ]
        if not selected:
            raise CompilerError(
                "quality_cell_empty",
                "Frozen quality cell contains no canonical features",
                role=quality["role"],
                cell_ids=sorted(identifiers),
            )
        crossing = sum(
            identifiers.issubset(set(feature["intersecting_cell_ids"]))
            for feature in selected
        )
        if quality["role"] == "cross_cell_boundary" and crossing == 0:
            raise CompilerError(
                "quality_boundary_empty",
                "Frozen boundary has no cross-cell feature",
                role=quality["role"],
                cell_ids=sorted(identifiers),
            )
        classes = {
            feature_class: sum(feature["feature_class"] == feature_class for feature in selected)
            for feature_class in sorted({feature["feature_class"] for feature in selected})
        }
        details[quality["role"]] = {
            "cell_ids": sorted(identifiers),
            "feature_count": len(selected),
            "feature_classes": classes,
            "crossing_feature_count": crossing,
        }
    return details


def build_validation_report(
    profile: dict[str, Any],
    bundle_policy: str,
    grid_identifier: str,
    terrain_cells: dict[str, dict[str, Any]],
    compiled: CompiledFeatures,
) -> dict[str, Any]:
    targets = sorted((int(cell["x"]), int(cell["y"])) for cell in profile["target_cells"])
    all_features = [feature for values in compiled.by_owner.values() for feature in values]
    feature_ids = [feature["feature_id"] for feature in all_features]
    if len(feature_ids) != len(set(feature_ids)):
        raise CompilerError("duplicate_feature_identity", "Canonical feature identities are duplicated")
    if bundle_policy != "approved" or any(feature["provenance_result"] != "accepted" for feature in all_features):
        raise CompilerError("provenance_rejected", "Canonical provenance is unresolved")
    if profile.get("fixture_features") and len(compiled.rejections) != 1:
        raise CompilerError(
            "invalid_fixture_result",
            "Synthetic invalid feature must be rejected exactly once",
            rejection_count=len(compiled.rejections),
        )
    boundary = _boundary_details(terrain_cells, grid_identifier, targets)
    feature_boundary = _feature_boundary_details(
        all_features,
        grid_identifier,
        {**profile["grid"], "target_cells": profile["target_cells"]},
    )
    quality = _quality_details(profile, grid_identifier, all_features)
    owned = {feature_id for values in compiled.by_owner.values() for feature_id in (item["feature_id"] for item in values)}
    dangling = sorted({item for values in compiled.references.values() for item in values} - owned)
    if dangling:
        raise CompilerError("dangling_feature_reference", "Cell references a feature without an authoritative owner", ids=dangling)
    checks = [
        {"check": "structural", "status": "passed", "details": {"schema_validation": "required_on_write"}},
        {"check": "provenance", "status": "passed", "details": {"features": len(all_features)}},
        {"check": "geometry", "status": "passed", "details": {"rejected": len(compiled.rejections)}},
        {"check": "topology", "status": "passed", "details": {"duplicate_ids": 0}},
        {"check": "boundary", "status": "passed", "details": boundary},
        {"check": "feature_boundary", "status": "passed", "details": feature_boundary},
        {"check": "quality_cells", "status": "passed", "details": quality},
        {"check": "ownership", "status": "passed", "details": {"dangling_references": 0}},
        {
            "check": "authored_overlay",
            "status": "passed",
            "details": {"applied_feature_overrides": compiled.applied_overrides},
        },
        {
            "check": "target_selection",
            "status": "passed",
            "details": {"excluded_counts": compiled.excluded_counts},
        },
    ]
    return {
        "$schema": "https://alis.world/schemas/world-compiler/validation-report-v1.json",
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        "status": "accepted",
        "checks": checks,
        "determinism_scope": {
            "D0": "semantic_hash",
            "D1": "canonical JSON and deterministic reports excluding metrics and compile result",
            "D2": "declared terrain and feature artifacts",
        },
    }
