from __future__ import annotations

import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .contracts import CompilerError, canonical_hash
from .geometry import clip_line
from .membership import feature_cell_membership
from .spatial import quantize
from .water_geometry import TerrainSampler, production_geometry_evidence


def _finite_values(values: Iterable[float]) -> list[float]:
    result = [float(value) for value in values]
    if not result or any(not math.isfinite(value) for value in result):
        raise CompilerError("water_elevation_samples_invalid", "Water elevation samples must be finite and non-empty")
    return result


def linear_quantile(values: Iterable[float], quantile: float) -> float:
    samples = sorted(_finite_values(values))
    if not 0.0 <= quantile <= 1.0:
        raise CompilerError("water_quantile_invalid", "Water quantile must be between zero and one")
    position = (len(samples) - 1) * quantile
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    weight = position - lower
    return samples[lower] * (1.0 - weight) + samples[upper] * weight


def rolling_quantile(values: Iterable[float], quantile: float, radius: int) -> list[float]:
    samples = _finite_values(values)
    if radius < 0:
        raise CompilerError("water_window_invalid", "Water rolling-window radius cannot be negative")
    return [
        linear_quantile(samples[max(0, index - radius):min(len(samples), index + radius + 1)], quantile)
        for index in range(len(samples))
    ]


def _upper_median(values: list[float]) -> float:
    ordered = sorted(values)
    return ordered[len(ordered) // 2]


def _nondecreasing_l1(values: list[float]) -> list[float]:
    blocks: list[tuple[list[float], float]] = []
    for value in values:
        blocks.append(([value], value))
        while len(blocks) > 1 and blocks[-2][1] > blocks[-1][1]:
            right, left = blocks.pop(), blocks.pop()
            merged = sorted([*left[0], *right[0]])
            blocks.append((merged, _upper_median(merged)))
    return [level for samples, level in blocks for _ in samples]


def fit_downstream_surface(
    values: Iterable[float], radius: int = 2, quantile: float = 0.5
) -> list[float]:
    filtered = rolling_quantile(values, quantile, radius)
    return [-value for value in _nondecreasing_l1([-sample for sample in filtered])]


@dataclass(frozen=True)
class WaterPreparation:
    attributes: dict[str, dict[str, Any]]
    visible_geometries: dict[str, dict[str, Any] | None]
    membership_geometries: dict[str, dict[str, Any]]
    excluded: dict[str, str]


def validate_water_profile(profile: dict[str, Any]) -> None:
    semantics = profile.get("water_semantics")
    if semantics is None:
        return
    class_keys = [(rule["water_class"], rule["geometry"]) for rule in semantics["class_rules"]]
    modifier_keys = [(rule["tag"], rule["value"]) for rule in semantics["modifier_rules"]]
    feature_ids = [source_id for rule in semantics["feature_rules"] for source_id in rule["source_ids"]]
    group_ids = [group["surface_group_id"] for group in semantics["surface_groups"]]
    grouped_polygons = [group["polygon_source_id"] for group in semantics["surface_groups"]]
    if len(class_keys) != len(set(class_keys)) or len(modifier_keys) != len(set(modifier_keys)):
        raise CompilerError("water_profile_conflict", "Water profile repeats a class or modifier rule")
    if len(feature_ids) != len(set(feature_ids)) or len(group_ids) != len(set(group_ids)):
        raise CompilerError("water_profile_conflict", "Water profile repeats a feature or group identity")
    if len(grouped_polygons) != len(set(grouped_polygons)):
        raise CompilerError("water_surface_group_conflict", "A polygon belongs to multiple water surface groups")
    axis_ids: list[str] = []
    for group in semantics["surface_groups"]:
        if group["polygon_source_id"] in group["flow_axis_source_ids"]:
            raise CompilerError("water_surface_group_conflict", "A water polygon cannot be its own flow axis")
        axis_ids.extend(group["flow_axis_source_ids"])
    if len(axis_ids) != len(set(axis_ids)):
        raise CompilerError("water_surface_group_conflict", "A flow axis belongs to multiple water surface groups")
    clearance = float(semantics["width"]["ribbon_buffer"]["suppression_clearance_m"])
    if clearance < 2.0 * float(profile["grid"]["coordinate_quantization"]):
        raise CompilerError(
            "water_profile_conflict",
            "Ribbon suppression clearance must cover two coordinate quantization steps",
        )


def expand_water_change_ids(profile: dict[str, Any], source_ids: Iterable[str]) -> list[str]:
    expanded = set(source_ids)
    semantics = profile.get("water_semantics")
    if semantics is None:
        return sorted(expanded)
    for group in semantics["surface_groups"]:
        members = {group["polygon_source_id"], *group["flow_axis_source_ids"]}
        if expanded & members:
            expanded.update(members)
    return sorted(expanded)


def _geometry_kind(geometry_type: str) -> str:
    if geometry_type in {"Polygon", "MultiPolygon"}:
        return "polygon"
    if geometry_type in {"LineString", "MultiLineString"}:
        return "line"
    return "point"


def _water_class(properties: dict[str, Any]) -> str:
    return str(properties.get("water") or properties.get("waterway") or properties.get("natural") or "water")


def _metric_width(value: Any) -> float | None:
    if isinstance(value, (int, float)) and float(value) > 0:
        return float(value)
    if not isinstance(value, str):
        return None
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*(?:m)?\s*", value)
    return float(match.group(1)) if match and float(match.group(1)) > 0 else None


def _line_widths(
    water: dict[str, dict[str, Any]],
    decisions: dict[str, tuple[str, str | None, str]],
    projected: dict[str, dict[str, Any]],
    surface_ids: set[str],
    semantics: dict[str, Any],
) -> dict[str, tuple[float, str, str]]:
    resolved: dict[str, tuple[float, str, str]] = {}
    for source_id in sorted(surface_ids):
        if _geometry_kind(projected[source_id]["type"]) != "line":
            continue
        raw_width = water[source_id]["properties"].get("width")
        width = _metric_width(raw_width)
        if raw_width is not None and width is None:
            raise CompilerError(
                "water_width_unapproved",
                "Water source width is present but not a supported positive metric value",
                source_id=source_id,
                width=raw_width,
            )
        water_class = decisions[source_id][2]
        if width is None and water_class not in semantics["width"]["fallback_m"]:
            raise CompilerError(
                "water_width_unapproved",
                "Water line has no source width or approved class fallback",
                source_id=source_id,
                water_class=water_class,
            )
        resolved[source_id] = (
            width if width is not None else float(semantics["width"]["fallback_m"][water_class]),
            "source-derived" if width is not None else "profile_inferred_from_water_class",
            "source_unspecified" if width is not None else semantics["width"]["fallback_accuracy"],
        )
    return resolved


def _decision(feature: dict[str, Any], semantics: dict[str, Any]) -> tuple[str, str | None, str]:
    source_id = feature["provider_feature_id"]
    override = next((rule for rule in semantics["feature_rules"] if source_id in rule["source_ids"]), None)
    kind = _geometry_kind(feature["geometry"]["type"])
    water_class = _water_class(feature["properties"])
    rule = override or next((
        candidate for candidate in semantics["class_rules"]
        if candidate["water_class"] == water_class and candidate["geometry"] == kind
    ), None)
    if rule is None:
        raise CompilerError("water_class_unapproved", "Water class and geometry have no profile decision", source_id=source_id)
    result, behavior = rule["result"], rule.get("behavior")
    if result != "surface":
        return result, behavior, water_class
    modifier_results: list[str] = []
    properties = feature["properties"]
    for tag in {candidate["tag"] for candidate in semantics["modifier_rules"]}:
        if tag not in properties:
            continue
        value = str(properties[tag])
        modifier = next((
            candidate for candidate in semantics["modifier_rules"]
            if candidate["tag"] == tag and candidate["value"] in {value, "*"}
        ), None)
        if modifier is None:
            raise CompilerError("water_modifier_unapproved", "Water modifier has no profile decision", source_id=source_id, tag=tag, value=value)
        modifier_results.append(modifier["result"])
    if "hidden" in modifier_results:
        result = "hidden"
    elif "temporal" in modifier_results:
        result = "temporal"
    return result, behavior, water_class


def _line_points(
    geometry: dict[str, Any], spacing: float, bounds: tuple[float, float, float, float], coordinate_step: float
) -> list[list[float]]:
    parts = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
    clipped = [fragment for part in parts for fragment in clip_line(part, bounds, coordinate_step)]
    samples: list[list[float]] = []
    for line in clipped:
        for start, end in zip(line, line[1:], strict=False):
            count = max(1, math.ceil(math.dist(start, end) / spacing))
            samples.extend([
                start[index] + ratio / count * (end[index] - start[index]) for index in (0, 1)
            ] for ratio in range(count))
        samples.append(line[-1])
    canonical = [[quantize(value, coordinate_step) for value in point] for point in samples]
    return [point for index, point in enumerate(canonical) if index == 0 or point != canonical[index - 1]]


def _axis_chain(
    axis_ids: list[str], projected: dict[str, dict[str, Any]], step: float
) -> list[list[float]]:
    chain: list[list[float]] = []
    for source_id in axis_ids:
        geometry = projected[source_id]
        if geometry["type"] != "LineString":
            raise CompilerError("water_flow_axis_ambiguous", "Grouped flow axes must be single directed lines", source_id=source_id)
        coordinates = geometry["coordinates"]
        if chain and any(abs(left - right) > step for left, right in zip(chain[-1], coordinates[0], strict=True)):
            raise CompilerError("water_flow_axis_disconnected", "Grouped flow axes are not ordered into one chain", source_id=source_id)
        chain.extend(coordinates[1:] if chain else coordinates)
    return chain


def _flow_function(
    axis_ids: list[str], projected: dict[str, dict[str, Any]], sampler: TerrainSampler,
    semantics: dict[str, Any], bounds: tuple[float, float, float, float], height_step: float,
    coordinate_step: float, authority_id: str,
) -> dict[str, Any]:
    configuration = semantics["elevation"]["flowing"]
    chain = _axis_chain(axis_ids, projected, float(semantics["coordinate_match_tolerance_m"]))
    points = _line_points(
        {"type": "LineString", "coordinates": chain},
        float(configuration["sample_spacing_m"]), bounds, coordinate_step,
    )
    if len(points) < 2:
        raise CompilerError(
            "water_flow_axis_outside_terrain",
            "Flow axis has fewer than two territory samples",
            authority_id=authority_id,
            axis_source_ids=axis_ids,
        )
    raw = [sampler.sample(point) for point in points]
    fitted = fit_downstream_surface(raw, int(configuration["rolling_radius"]), float(configuration["quantile"]))
    knots = [[point[0], point[1], quantize(height, height_step)] for point, height in zip(points, fitted, strict=True)]
    corrections = sorted(abs(source - result) for source, result in zip(raw, fitted, strict=True))
    introduced_plateau_m = 0.0
    start = 0
    while start < len(knots):
        end = start
        while end + 1 < len(knots) and knots[end + 1][2] == knots[start][2]:
            end += 1
        if end > start and max(raw[start:end + 1]) - min(raw[start:end + 1]) > height_step:
            introduced_plateau_m = max(
                introduced_plateau_m,
                sum(math.dist(points[index], points[index + 1]) for index in range(start, end)),
            )
        start = end + 1
    return {
        "function_id": configuration["function_id"],
        "function_version": configuration["function_version"],
        "vertical_basis": "canonical_terrain_robust_fit",
        "accuracy": "derived_not_surveyed",
        "sample_count": len(points),
        "knots": knots,
        "fit_diagnostics": {
            "max_correction_m": quantize(corrections[-1], height_step),
            "median_correction_m": quantize(linear_quantile(corrections, 0.5), height_step),
            "upstream_downstream_fall_m": quantize(knots[0][2] - knots[-1][2], height_step),
            "longest_introduced_flat_plateau_m": quantize(introduced_plateau_m, coordinate_step),
        },
    }


def prepare_water_features(
    source_features: list[dict[str, Any]], profile: dict[str, Any], projected: dict[str, dict[str, Any]],
    terrain: dict[str, dict[str, Any]] | None, repo_root: Path, scratch_root: Path,
    source_relevant_ids: set[str], targets: list[tuple[int, int]],
) -> WaterPreparation:
    semantics = profile.get("water_semantics")
    if semantics is None:
        return WaterPreparation({}, {}, {}, {})
    if terrain is None:
        raise CompilerError("water_terrain_missing", "Water semantics require canonical terrain")
    validate_water_profile(profile)
    water = {feature["provider_feature_id"]: feature for feature in source_features if feature["provider_class"] == "water"}
    decisions = {source_id: _decision(feature, semantics) for source_id, feature in water.items()}
    policy_excluded = {
        source_id: f"water_{result}"
        for source_id, (result, _, _) in decisions.items()
        if result != "surface"
    }
    excluded = {
        source_id: reason for source_id, reason in policy_excluded.items()
        if source_id in source_relevant_ids
    }
    surface_ids = set(water) - set(policy_excluded)
    grouped: dict[str, tuple[str, list[str]]] = {}
    member_group: dict[str, str] = {}
    for group in semantics["surface_groups"]:
        polygon_id, axes = group["polygon_source_id"], group["flow_axis_source_ids"]
        members = [polygon_id, *axes]
        missing = [source_id for source_id in members if source_id not in water or source_id not in surface_ids]
        if missing:
            raise CompilerError("water_flow_axis_missing", "Water surface group is missing an admitted member", missing_source_ids=missing)
        if decisions[polygon_id][1] != "flowing" or _geometry_kind(projected[polygon_id]["type"]) != "polygon":
            raise CompilerError("water_surface_group_conflict", "Water group owner is not a flowing polygon")
        if any(decisions[axis][1] != "flowing" or _geometry_kind(projected[axis]["type"]) != "line" for axis in axes):
            raise CompilerError("water_surface_group_conflict", "Water group axis is not a flowing line")
        grouped[group["surface_group_id"]] = polygon_id, axes
        for source_id in members:
            if source_id in member_group:
                raise CompilerError("water_surface_group_conflict", "Water feature belongs to multiple surface groups")
            member_group[source_id] = group["surface_group_id"]
    for source_id in surface_ids:
        result, behavior, _ = decisions[source_id]
        if behavior == "flowing" and _geometry_kind(projected[source_id]["type"]) == "polygon" and source_id not in member_group:
            raise CompilerError("water_flow_axis_missing", "Flowing water polygon has no exact axis group", source_id=source_id)
    polygons = {source_id: projected[source_id] for source_id in surface_ids if _geometry_kind(projected[source_id]["type"]) == "polygon"}
    lines = {source_id: projected[source_id] for source_id in surface_ids if _geometry_kind(projected[source_id]["type"]) == "line"}
    widths = _line_widths(water, decisions, projected, surface_ids, semantics)
    sampler = TerrainSampler.from_cells(terrain)
    core_sampler = sampler.core_view()
    buffer_contract = semantics["width"]["ribbon_buffer"]
    polygon_samples, fallback_points, visible, overlap, footprints = production_geometry_evidence(
        repo_root,
        scratch_root / "geometry",
        polygons,
        lines,
        {source_id: value[0] for source_id, value in widths.items()},
        core_sampler,
        profile["grid"]["canonical_crs"],
        float(profile["grid"]["coordinate_quantization"]),
        int(buffer_contract["quadrant_segments"]),
        float(buffer_contract["suppression_clearance_m"]),
    )
    surface_geometries = {
        source_id: polygons[source_id] if source_id in polygons else footprints[source_id]
        for source_id in surface_ids
        if source_id in polygons or source_id in footprints
    }
    surface_memberships = feature_cell_membership(
        repo_root,
        surface_geometries,
        profile["grid"],
        targets,
        scratch_root / "surface_membership",
    )
    relevant_surface_ids = {
        source_id for source_id, memberships in surface_memberships.items() if memberships
    }
    for polygon_id, axes in grouped.values():
        members = {polygon_id, *axes}
        if members & relevant_surface_ids:
            relevant_surface_ids.update(members)
    surface_ids.intersection_update(relevant_surface_ids)
    core_bounds = (
        core_sampler.x_values[0], core_sampler.y_values[0],
        core_sampler.x_values[-1], core_sampler.y_values[-1],
    )
    halo_bounds = (
        sampler.x_values[0], sampler.y_values[0],
        sampler.x_values[-1], sampler.y_values[-1],
    )
    height_step = float(profile["grid"]["height_quantization"])
    coordinate_step = float(profile["grid"]["coordinate_quantization"])
    functions: dict[str, dict[str, Any]] = {}
    for group_id, (polygon_id, axes) in grouped.items():
        if polygon_id not in surface_ids:
            continue
        bounds = core_bounds if any(source_id in source_relevant_ids for source_id in [polygon_id, *axes]) else halo_bounds
        functions[group_id] = _flow_function(
            axes, projected, sampler, semantics, bounds, height_step, coordinate_step, group_id
        )
    attributes: dict[str, dict[str, Any]] = {}
    for source_id in sorted(surface_ids):
        _, behavior, water_class = decisions[source_id]
        kind = _geometry_kind(projected[source_id]["type"])
        group_id = member_group.get(source_id) or f"water_{canonical_hash({'source_id': source_id})[:16]}"
        if group_id not in functions:
            if behavior == "standing":
                values = polygon_samples[source_id]
                fallback = not values
                if fallback:
                    try:
                        values = [core_sampler.sample(fallback_points[source_id])]
                    except CompilerError as exc:
                        raise CompilerError(
                            exc.code,
                            "Standing-water fallback is outside canonical terrain",
                            source_id=source_id,
                            point=fallback_points[source_id],
                        ) from exc
                configuration = semantics["elevation"]["standing"]
                functions[group_id] = {
                    "function_id": configuration["function_id"],
                    "function_version": configuration["function_version"],
                    "vertical_basis": "canonical_terrain_polygon_quantile",
                    "accuracy": "low_confidence_point_fallback" if fallback else "derived_not_surveyed",
                    "sample_count": len(values),
                    "level_m": quantize(linear_quantile(values, float(configuration["quantile"])), height_step),
                }
            else:
                bounds = core_bounds if source_id in source_relevant_ids else halo_bounds
                functions[group_id] = _flow_function(
                    [source_id], projected, sampler, semantics, bounds, height_step, coordinate_step, group_id
                )
        members = [source_id]
        if group_id in grouped:
            members = [grouped[group_id][0], *grouped[group_id][1]]
        value: dict[str, Any] = {
            "water_class": water_class,
            "surface_group_id": group_id,
            "surface_group_members": sorted(members),
            "surface_geometry": "polygon" if kind == "polygon" else "ribbon",
            "surface_behavior": behavior,
            "surface_role": "area" if kind == "polygon" else "flow_axis",
            "surface_function": functions[group_id],
        }
        if kind == "line":
            width, basis, accuracy = widths[source_id]
            value.update({
                "width_m": width,
                "width_basis": basis,
                "width_accuracy": accuracy,
                "ribbon_buffer": buffer_contract,
                "polygon_overlap_area_m2": overlap[source_id],
            })
        attributes[source_id] = value
    return WaterPreparation(
        attributes,
        visible,
        {source_id: geometry for source_id, geometry in footprints.items() if source_id in surface_ids},
        excluded,
    )
