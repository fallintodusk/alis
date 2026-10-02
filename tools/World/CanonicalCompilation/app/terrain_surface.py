from __future__ import annotations

import math
from bisect import bisect_right
from typing import Any

from .contracts import CompilerError, canonical_hash
from .spatial import quantize


SURFACE_ROLES = ("ground", "hydro_transition")


def validate_surface_contract(profile: dict[str, Any]) -> dict[str, Any]:
    contract = profile.get("terrain_surface")
    if not isinstance(contract, dict):
        raise CompilerError(
            "terrain_surface_contract_missing",
            "Compiler profiles must declare the canonical terrain surface contract",
        )
    roles = tuple(contract["roles"])
    if roles != SURFACE_ROLES:
        raise CompilerError(
            "terrain_surface_roles_invalid",
            "Canonical terrain surface roles must be exactly ground and hydro_transition",
            roles=list(roles),
        )
    if contract["contract_id"] != "terrain_surface_semantics" or contract["contract_version"] != 1:
        raise CompilerError("terrain_surface_contract_invalid", "Terrain surface contract is unsupported")
    return contract


def _segments(geometry: dict[str, Any]) -> list[tuple[list[float], list[float]]]:
    geometry_type = geometry["type"]
    coordinates = geometry["coordinates"]
    if geometry_type == "LineString":
        lines = [coordinates]
    elif geometry_type == "MultiLineString":
        lines = coordinates
    elif geometry_type == "Polygon":
        lines = coordinates
    elif geometry_type == "MultiPolygon":
        lines = [ring for polygon in coordinates for ring in polygon]
    else:
        return []
    return [
        (left, right)
        for line in lines
        for left, right in zip(line, line[1:], strict=False)
    ]


def _segment_distance(point: tuple[float, float], left: list[float], right: list[float]) -> float:
    dx, dy = float(right[0]) - float(left[0]), float(right[1]) - float(left[1])
    denominator = dx * dx + dy * dy
    if denominator == 0.0:
        return math.hypot(point[0] - float(left[0]), point[1] - float(left[1]))
    alpha = max(0.0, min(1.0, (
        (point[0] - float(left[0])) * dx + (point[1] - float(left[1])) * dy
    ) / denominator))
    return math.hypot(
        point[0] - (float(left[0]) + alpha * dx),
        point[1] - (float(left[1]) + alpha * dy),
    )


def _ring_relation(
    ring: list[list[float]], point: tuple[float, float]
) -> tuple[bool, float]:
    inside = False
    distance = math.inf
    for left, right in zip(ring, [ring[-1], *ring[:-1]], strict=True):
        segment_distance = _segment_distance(point, left, right)
        distance = min(distance, segment_distance)
        if segment_distance <= 1e-9:
            return True, 0.0
        if (float(left[1]) > point[1]) != (float(right[1]) > point[1]):
            intersection = float(left[0]) + (
                point[1] - float(left[1])
            ) * (float(right[0]) - float(left[0])) / (float(right[1]) - float(left[1]))
            if point[0] < intersection:
                inside = not inside
    return inside, distance


def _ring_contains(ring: list[list[float]], point: tuple[float, float]) -> bool:
    return _ring_relation(ring, point)[0]


def _polygon_distance(coordinates: list[list[list[float]]], point: tuple[float, float]) -> float:
    if not coordinates:
        return math.inf
    outer_contains, outer_distance = _ring_relation(coordinates[0], point)
    hole_relations = [_ring_relation(hole, point) for hole in coordinates[1:]]
    if not outer_contains:
        return min([outer_distance, *(distance for _, distance in hole_relations)])
    if any(contains for contains, _ in hole_relations):
        return min((distance for _, distance in hole_relations), default=math.inf)
    return 0.0


def _footprint_distance(feature: dict[str, Any], point: tuple[float, float]) -> float:
    geometry = feature["geometry"]
    if geometry["type"] == "Polygon":
        return _polygon_distance(geometry["coordinates"], point)
    if geometry["type"] == "MultiPolygon":
        return min(_polygon_distance(polygon, point) for polygon in geometry["coordinates"])
    line_distance = min(
        (_segment_distance(point, left, right) for left, right in _segments(geometry)),
        default=math.inf,
    )
    return max(0.0, line_distance - float(feature["attributes"].get("width_m", 0.0)) * 0.5)


def _coordinate_bounds(points: list[list[float]]) -> tuple[float, float, float, float]:
    return (
        min(float(point[0]) for point in points),
        min(float(point[1]) for point in points),
        max(float(point[0]) for point in points),
        max(float(point[1]) for point in points),
    )


def _bounds_distance(
    bounds: tuple[float, float, float, float], point: tuple[float, float]
) -> float:
    west, south, east, north = bounds
    dx = max(west - point[0], 0.0, point[0] - east)
    dy = max(south - point[1], 0.0, point[1] - north)
    return math.hypot(dx, dy)


def _prepare_ring(ring: list[list[float]]) -> dict[str, Any]:
    segments = [
        {
            "left": left,
            "right": right,
            "bounds": _coordinate_bounds([left, right]),
        }
        for left, right in zip(ring, [ring[-1], *ring[:-1]], strict=True)
    ]
    return {
        "bounds": _coordinate_bounds(ring),
        "segments": segments,
        "crossings_by_y": {},
        "nearby_segments": {},
    }


def _prepared_ring_relation(
    prepared: dict[str, Any], point: tuple[float, float], cutoff_m: float
) -> tuple[bool, float]:
    if _bounds_distance(prepared["bounds"], point) > cutoff_m:
        return False, math.inf
    bin_size = max(cutoff_m, 1.0)
    bin_x, bin_y = math.floor(point[0] / bin_size), math.floor(point[1] / bin_size)
    nearby_key = (cutoff_m, bin_x, bin_y)
    nearby = prepared["nearby_segments"].get(nearby_key)
    if nearby is None:
        bin_west, bin_south = bin_x * bin_size, bin_y * bin_size
        bin_east, bin_north = bin_west + bin_size, bin_south + bin_size
        nearby = [
            segment
            for segment in prepared["segments"]
            if segment["bounds"][2] >= bin_west - cutoff_m
            and segment["bounds"][0] <= bin_east + cutoff_m
            and segment["bounds"][3] >= bin_south - cutoff_m
            and segment["bounds"][1] <= bin_north + cutoff_m
        ]
        prepared["nearby_segments"][nearby_key] = nearby
    distance = min(
        (
            _segment_distance(point, segment["left"], segment["right"])
            for segment in nearby
            if _bounds_distance(segment["bounds"], point) <= cutoff_m
        ),
        default=math.inf,
    )
    if distance <= 1e-9:
        return True, 0.0
    crossings = prepared["crossings_by_y"].get(point[1])
    if crossings is None:
        crossings = sorted(
            float(segment["left"][0]) + (
                point[1] - float(segment["left"][1])
            ) * (
                float(segment["right"][0]) - float(segment["left"][0])
            ) / (
                float(segment["right"][1]) - float(segment["left"][1])
            )
            for segment in prepared["segments"]
            if (float(segment["left"][1]) > point[1])
            != (float(segment["right"][1]) > point[1])
        )
        prepared["crossings_by_y"][point[1]] = crossings
    inside = (len(crossings) - bisect_right(crossings, point[0])) % 2 == 1
    return inside, distance


def _prepared_polygon_distance(
    prepared: dict[str, Any], point: tuple[float, float], cutoff_m: float
) -> float:
    rings = prepared["rings"]
    outer_contains, outer_distance = _prepared_ring_relation(rings[0], point, cutoff_m)
    hole_relations = [
        _prepared_ring_relation(hole, point, cutoff_m)
        for hole in rings[1:]
    ]
    if not outer_contains:
        return min([outer_distance, *(distance for _, distance in hole_relations)])
    if any(contains for contains, _ in hole_relations):
        return min((distance for _, distance in hole_relations), default=math.inf)
    return 0.0


def _prepare_water_feature(feature: dict[str, Any]) -> dict[str, Any]:
    geometry = feature["geometry"]
    geometry_type = geometry["type"]
    if geometry_type in {"Polygon", "MultiPolygon"}:
        polygons = [geometry["coordinates"]] if geometry_type == "Polygon" else geometry["coordinates"]
        parts = [
            {
                "coordinates": polygon,
                "bounds": _coordinate_bounds([point for ring in polygon for point in ring]),
                "rings": [_prepare_ring(ring) for ring in polygon],
            }
            for polygon in polygons
        ]
        all_bounds = [part["bounds"] for part in parts]
        bounds = (
            min(item[0] for item in all_bounds),
            min(item[1] for item in all_bounds),
            max(item[2] for item in all_bounds),
            max(item[3] for item in all_bounds),
        )
        return {"feature": feature, "bounds": bounds, "parts": parts, "segments": [], "width_half": 0.0}
    segments = [
        {
            "left": left,
            "right": right,
            "bounds": _coordinate_bounds([left, right]),
        }
        for left, right in _segments(geometry)
    ]
    if not segments:
        return {
            "feature": feature,
            "bounds": (math.inf, math.inf, -math.inf, -math.inf),
            "parts": [],
            "segments": [],
            "width_half": float(feature["attributes"].get("width_m", 0.0)) * 0.5,
        }
    all_bounds = [segment["bounds"] for segment in segments]
    bounds = (
        min(item[0] for item in all_bounds),
        min(item[1] for item in all_bounds),
        max(item[2] for item in all_bounds),
        max(item[3] for item in all_bounds),
    )
    return {
        "feature": feature,
        "bounds": bounds,
        "parts": [],
        "segments": segments,
        "width_half": float(feature["attributes"].get("width_m", 0.0)) * 0.5,
    }


def _prepared_footprint_distance(
    prepared: dict[str, Any], point: tuple[float, float], cutoff_m: float
) -> float:
    if prepared["parts"]:
        if _bounds_distance(prepared["bounds"], point) > cutoff_m:
            return math.inf
        return min(
            (
                _prepared_polygon_distance(part, point, cutoff_m)
                for part in prepared["parts"]
                if _bounds_distance(part["bounds"], point) <= cutoff_m
            ),
            default=math.inf,
        )
    centerline_cutoff = cutoff_m + prepared["width_half"]
    if _bounds_distance(prepared["bounds"], point) > centerline_cutoff:
        return math.inf
    line_distance = min(
        (
            _segment_distance(point, segment["left"], segment["right"])
            for segment in prepared["segments"]
            if _bounds_distance(segment["bounds"], point) <= centerline_cutoff
        ),
        default=math.inf,
    )
    return max(0.0, line_distance - prepared["width_half"])


def _water_height(feature: dict[str, Any], point: tuple[float, float]) -> float:
    surface = feature["attributes"]["surface_function"]
    if "level_m" in surface:
        return float(surface["level_m"])
    knots = surface.get("knots", [])
    best: tuple[float, float] | None = None
    for left, right in zip(knots, knots[1:], strict=False):
        dx, dy = float(right[0]) - float(left[0]), float(right[1]) - float(left[1])
        denominator = dx * dx + dy * dy
        alpha = 0.0 if denominator == 0.0 else max(0.0, min(1.0, (
            (point[0] - float(left[0])) * dx + (point[1] - float(left[1])) * dy
        ) / denominator))
        projected = (float(left[0]) + alpha * dx, float(left[1]) + alpha * dy)
        candidate = (
            math.hypot(point[0] - projected[0], point[1] - projected[1]),
            float(left[2]) + alpha * (float(right[2]) - float(left[2])),
        )
        if best is None or candidate[0] < best[0]:
            best = candidate
    if best is None:
        raise CompilerError("water_surface_invalid", "Flowing Water has no evaluable surface knots")
    return best[1]


def _water_features(compiled: Any) -> list[dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for features in compiled.by_owner.values():
        for feature in features:
            if feature["feature_class"] == "water" and "surface_function" in feature["attributes"]:
                result[feature["feature_id"]] = feature
    return [result[key] for key in sorted(result)]


def _cell_has_water_influence(
    cell: dict[str, Any], feature: dict[str, Any], transition_radius_m: float, halo: int
) -> bool:
    samples = cell["water_fit"]["halo_window"]["samples"]
    west, _, _, north = (float(value) for value in cell["bounds"])
    spacing_x, spacing_y = (float(value) for value in cell["sample_spacing"])
    sample_west, sample_north = west - halo * spacing_x, north + halo * spacing_y
    return any(
        _footprint_distance(
            feature,
            (sample_west + column * spacing_x, sample_north - row * spacing_y),
        ) <= transition_radius_m
        for row, values in enumerate(samples)
        for column in range(len(values))
    )


def water_surface_dirty_cells(
    current: Any,
    previous: Any,
    terrain: dict[str, dict[str, Any]],
    profile: dict[str, Any],
) -> set[str]:
    current_features = {item["feature_id"]: item for item in _water_features(current)}
    previous_features = {item["feature_id"]: item for item in _water_features(previous)}
    changed = {
        feature_id
        for feature_id in set(current_features) | set(previous_features)
        if canonical_hash(current_features.get(feature_id)) != canonical_hash(previous_features.get(feature_id))
    }
    changed_features = [
        feature
        for feature_id in changed
        for feature in (current_features.get(feature_id), previous_features.get(feature_id))
        if feature is not None
    ]
    transition_radius = float(validate_surface_contract(profile)["hydro_transition_radius_m"])
    halo = int(profile["grid"]["halo_samples"])
    result: set[str] = set()
    for cell_id, cell in terrain.items():
        if any(_cell_has_water_influence(cell, feature, transition_radius, halo) for feature in changed_features):
            result.add(cell_id)
    for feature_id in changed:
        for feature in (current_features.get(feature_id), previous_features.get(feature_id)):
            if feature is not None:
                result.update(feature["intersecting_cell_ids"])
    return result


def _matrix_edges(matrix: list[list[Any]]) -> dict[str, list[list[Any]]]:
    return {
        "west": [[row[0]] for row in matrix],
        "east": [[row[-1]] for row in matrix],
        "north": [matrix[0]],
        "south": [matrix[-1]],
    }


def _height_bands(
    halo_samples: list[list[float]], core_rows: int, core_columns: int, halo: int
) -> dict[str, list[list[float]]]:
    return {
        "west": [row[:2 * halo + 1] for row in halo_samples[halo:halo + core_rows]],
        "east": [row[core_columns - 1:core_columns + 2 * halo] for row in halo_samples[halo:halo + core_rows]],
        "north": [row[halo:halo + core_columns] for row in halo_samples[:2 * halo + 1]],
        "south": [row[halo:halo + core_columns] for row in halo_samples[core_rows - 1:core_rows + 2 * halo]],
    }


def _weights_for_point(
    point: tuple[float, float],
    water_distance: float,
    transition_radius_m: float,
    surface_patches: list[dict[str, Any]],
) -> dict[str, float]:
    hydro = max(0.0, min(1.0, 1.0 - water_distance / transition_radius_m))
    matching = [
        patch for patch in surface_patches
        if math.hypot(point[0] - patch["center"][0], point[1] - patch["center"][1]) <= patch["radius_m"]
    ]
    if len(matching) > 1:
        raise CompilerError(
            "authored_surface_overlap",
            "Authored terrain surface overrides overlap at one canonical sample",
            patch_ids=sorted(patch["patch_id"] for patch in matching),
            point=list(point),
        )
    weights = matching[0]["surface_weights"] if matching else {
        "ground": 1.0 - hydro,
        "hydro_transition": hydro,
    }
    if set(weights) != set(SURFACE_ROLES):
        raise CompilerError("authored_surface_roles_invalid", "Authored surface override has unknown or missing roles")
    values = {role: float(weights[role]) for role in SURFACE_ROLES}
    if any(not math.isfinite(value) or value < 0.0 or value > 1.0 for value in values.values()):
        raise CompilerError("authored_surface_weight_invalid", "Authored surface weights must be finite UNorm values")
    if abs(sum(values.values()) - 1.0) > 1e-6:
        raise CompilerError("authored_surface_weight_invalid", "Authored surface weights must sum to one")
    return values


def _cell_coordinates(identifier: str) -> tuple[str, int, int]:
    try:
        grid_identifier, x_value, y_value = identifier.rsplit(":", 2)
        if not x_value.startswith("x") or not y_value.startswith("y"):
            raise ValueError
        return grid_identifier, int(x_value[1:]), int(y_value[1:])
    except (ValueError, TypeError) as error:
        raise CompilerError(
            "terrain_surface_cell_id_invalid",
            "Terrain surface finalization received a malformed canonical cell identity",
            cell_id=identifier,
        ) from error


def _water_features_for_cell(
    identifier: str,
    water_features: list[dict[str, Any]],
    profile: dict[str, Any],
    transition_radius_m: float,
) -> list[dict[str, Any]]:
    grid_identifier, cell_x, cell_y = _cell_coordinates(identifier)
    grid = profile["grid"]
    spacing_x, spacing_y = (float(value) for value in grid["sample_spacing"])
    cell_width = spacing_x * int(grid["cell_quads"][0])
    cell_height = spacing_y * int(grid["cell_quads"][1])
    halo = int(grid["halo_samples"])
    influence_x = int(math.ceil((transition_radius_m + halo * spacing_x) / cell_width))
    influence_y = int(math.ceil((transition_radius_m + halo * spacing_y) / cell_height))
    selected: list[dict[str, Any]] = []
    for feature in water_features:
        for intersecting_id in feature["intersecting_cell_ids"]:
            feature_grid, feature_x, feature_y = _cell_coordinates(intersecting_id)
            if feature_grid != grid_identifier:
                raise CompilerError(
                    "terrain_surface_grid_mismatch",
                    "Water feature membership targets another canonical grid",
                    feature_id=feature["feature_id"],
                    cell_id=intersecting_id,
                )
            if abs(feature_x - cell_x) <= influence_x and abs(feature_y - cell_y) <= influence_y:
                selected.append(feature)
                break
    return selected


def finalize_terrain_cells(
    terrain: dict[str, dict[str, Any]],
    compiled: Any,
    profile: dict[str, Any],
    grid_identifier: str,
    overlay: dict[str, Any],
) -> None:
    contract = validate_surface_contract(profile)
    if overlay["grid_id"] != grid_identifier:
        raise CompilerError("authored_overlay_grid_mismatch", "Authored overlay targets another canonical grid")
    provenance_ids = {item["provenance_id"] for item in overlay["provenance"]}
    if len(provenance_ids) != len(overlay["provenance"]):
        raise CompilerError("authored_overlay_provenance_duplicate", "Authored overlay repeats provenance identity")
    for patch in overlay["terrain_patches"]:
        if patch["provenance_ref"] not in provenance_ids:
            raise CompilerError("authored_overlay_provenance_missing", "Terrain patch has no declared provenance")
    water_features = _water_features(compiled)
    prepared_water = {
        feature["feature_id"]: _prepare_water_feature(feature)
        for feature in water_features
    }
    transition_radius = float(contract["hydro_transition_radius_m"])
    clearance = float(contract["water_clearance_m"])
    height_step = float(profile["grid"]["height_quantization"])
    halo = int(profile["grid"]["halo_samples"])
    surface_patches = [patch for patch in overlay["terrain_patches"] if "surface_weights" in patch]
    contract_receipt = {
        "contract_id": contract["contract_id"],
        "contract_version": contract["contract_version"],
        "roles": list(SURFACE_ROLES),
        "hydro_transition_radius_m": transition_radius,
        "water_clearance_m": clearance,
    }
    contract_receipt["contract_sha256"] = canonical_hash(contract_receipt)

    for current_cell_id, cell in terrain.items():
        cell_water_features = _water_features_for_cell(
            current_cell_id, water_features, profile, transition_radius
        )
        cell_prepared_water = [
            prepared_water[feature["feature_id"]]
            for feature in cell_water_features
        ]
        base_halo = cell["water_fit"]["halo_window"]["samples"]
        core_rows, core_columns = len(cell["water_fit"]["core_samples"]), len(cell["water_fit"]["core_samples"][0])
        west, _, _, north = (float(value) for value in cell["bounds"])
        spacing_x, spacing_y = (float(value) for value in cell["sample_spacing"])
        sample_west, sample_north = west - halo * spacing_x, north + halo * spacing_y
        final_halo: list[list[float]] = []
        role_halo = {role: [] for role in SURFACE_ROLES}
        for row, base_row in enumerate(base_halo):
            final_row: list[float] = []
            weight_rows = {role: [] for role in SURFACE_ROLES}
            for column, base_height in enumerate(base_row):
                point = sample_west + column * spacing_x, sample_north - row * spacing_y
                water_distances = [
                    (
                        prepared["feature"],
                        _prepared_footprint_distance(prepared, point, transition_radius),
                    )
                    for prepared in cell_prepared_water
                ]
                covering = [feature for feature, distance in water_distances if distance == 0.0]
                water_distance = min((distance for _, distance in water_distances), default=math.inf)
                water_height = min((_water_height(feature, point) for feature in covering), default=math.inf)
                if math.isfinite(water_height):
                    water_height = quantize(water_height, height_step)
                height = min(float(base_height), water_height - clearance)
                for patch in overlay["terrain_patches"]:
                    if math.hypot(point[0] - patch["center"][0], point[1] - patch["center"][1]) <= patch["radius_m"]:
                        height += float(patch["delta_m"])
                height = quantize(height, height_step)
                if height > water_height - clearance + 1e-9:
                    raise CompilerError(
                        "authored_height_breaks_water_clearance",
                        "Authored terrain height breaks canonical Water clearance",
                        point=list(point),
                        terrain_height_m=height,
                        water_height_m=water_height,
                )
                final_row.append(height)
                weights = _weights_for_point(
                    point, water_distance, transition_radius, surface_patches
                )
                for role in SURFACE_ROLES:
                    weight_rows[role].append(weights[role])
            final_halo.append(final_row)
            for role in SURFACE_ROLES:
                role_halo[role].append(weight_rows[role])
        final_core = [row[halo:halo + core_columns] for row in final_halo[halo:halo + core_rows]]
        role_core = {
            role: [row[halo:halo + core_columns] for row in role_halo[role][halo:halo + core_rows]]
            for role in SURFACE_ROLES
        }
        cell["core_samples"] = final_core
        cell["halo_window"]["samples"] = final_halo
        height_edges = _matrix_edges(final_core)
        height_bands = _height_bands(final_halo, core_rows, core_columns, halo)
        cell["height_edges"] = {
            side: f"sha256:{canonical_hash(value)}" for side, value in height_edges.items()
        }
        cell["height_border_bands"] = {
            side: f"sha256:{canonical_hash(value)}" for side, value in height_bands.items()
        }
        cell["surface_semantics"] = {
            **contract_receipt,
            "core_samples": role_core,
            "halo_samples": role_halo,
            "edge_sha256": {
                role: {
                    side: f"sha256:{canonical_hash(value)}"
                    for side, value in _matrix_edges(role_core[role]).items()
                }
                for role in SURFACE_ROLES
            },
        }
