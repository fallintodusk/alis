from __future__ import annotations

import math
from decimal import Decimal, ROUND_FLOOR
from typing import Any

from .contracts import CompilerError
from .geometry import _segments_intersect


def _inside(point: Any, bounds: tuple[float, float, float, float]) -> bool:
    return bounds[0] <= point[0] <= bounds[2] and bounds[1] <= point[1] <= bounds[3]


def _intersects(feature: dict, bounds: tuple[float, float, float, float], prepared: dict | None = None) -> bool:
    from .terrain_surface import _prepare_water_feature, _prepared_footprint_distance, _segment_distance

    prepared = prepared or _prepare_water_feature(feature)
    west, south, east, north = bounds
    fw, fs, fe, fn = prepared["bounds"]
    radius = (float(feature["attributes"].get("width_m", 0)) * 0.5
              if feature["geometry"]["type"] in {"LineString", "MultiLineString"} else 0.0)
    if fe + radius < west or fw - radius > east or fn + radius < south or fs - radius > north:
        return False
    corners = [(west, south), (east, south), (east, north), (west, north)]
    if any(_prepared_footprint_distance(prepared, point, 1e-9) <= 1e-9 for point in corners):
        return True
    edges = list(zip(corners, corners[1:] + corners[:1]))
    segments = prepared.get("coverage_segments")
    if segments is None:
        segments = ([segment for part in prepared["parts"] for ring in part["rings"] for segment in ring["segments"]]
                    if prepared["parts"] else prepared["segments"])
        prepared["coverage_segments"] = segments
    for segment in segments:
        sw, ss, se, sn = segment["bounds"]
        if se + radius < west or sw - radius > east or sn + radius < south or ss - radius > north:
            continue
        left, right = segment["left"], segment["right"]
        if _inside(left, bounds) or _inside(right, bounds):
            return True
        if any(_segments_intersect(left, right, a, b) for a, b in edges):
            return True
        # Distance between a segment and a rectangle is attained at an endpoint
        # or a rectangle corner unless the two intersect.
        if radius > 0 and (
            min(_segment_distance(point, left, right) for point in corners) <= radius
            or min(math.hypot(max(west - p[0], 0, p[0] - east),
                              max(south - p[1], 0, p[1] - north)) for p in (left, right)) <= radius
        ):
            return True
    return False


def _height_floor(feature: dict) -> float:
    function = feature["attributes"]["surface_function"]
    if "level_m" in function:
        value = float(function["level_m"])
        if not math.isfinite(value):
            raise CompilerError("water_surface_invalid", "Water level is not finite")
        return value
    knots = function.get("knots", [])
    if len(knots) < 2 or any(len(p) < 3 or not all(math.isfinite(float(v)) for v in p[:3]) for p in knots):
        raise CompilerError("water_surface_invalid", "Flowing Water needs finite surface knots")
    # Realized Water triangles are affine combinations of evaluated vertices.
    # Their interiors need not equal the nearest-segment function. The admitted
    # feature's minimum knot bounds every evaluated vertex and therefore every
    # realized triangle, including clipped triangles across height breaklines.
    return min(float(point[2]) for point in knots)



def support_bounds(cell: dict, features: list[dict], contract: dict, height_step: float, halo: int, prepared: dict | None = None) -> list[list[float]]:
    """Conservative continuous support on the existing lattice; never refits Water."""
    from .terrain_surface import _footprint_distance, _water_height, _prepare_water_feature
    from .spatial import quantize

    base = cell["water_fit"]["halo_window"]["samples"]
    if not base or not base[0] or any(len(row) != len(base[0]) for row in base):
        raise CompilerError("terrain_surface_grid_mismatch", "Water support needs a rectangular terrain grid")
    spacing_x, spacing_y = map(float, cell["sample_spacing"])
    if not all(math.isfinite(v) and v > 0 for v in (spacing_x, spacing_y, height_step)):
        raise CompilerError("terrain_surface_grid_mismatch", "Water support spacing/quantization is invalid")
    west, _, _, north = map(float, cell["bounds"])
    x0, y0 = west - halo * spacing_x, north + halo * spacing_y
    if not all(math.isfinite(float(v)) for v in cell["bounds"]) or any(
            not math.isfinite(float(v)) for row in base for v in row):
        raise CompilerError("terrain_surface_grid_mismatch", "Water support has non-finite coordinates/heights")
    prepared = prepared or {f["feature_id"]: _prepare_water_feature(f) for f in features}
    floors = {}
    for feature in features:
        cache = prepared[feature["feature_id"]]
        if "coverage_floor" not in cache:
            cache["coverage_floor"] = _height_floor(feature)
        floors[feature["feature_id"]] = cache["coverage_floor"]
    result = [[math.inf] * len(base[0]) for _ in base]
    owners: dict[tuple[int, int], dict] = {}
    clearance = float(contract["water_clearance_m"])
    for row in range(-1, len(base)):
        for column in range(-1, len(base[0])):
            bounds = (x0 + column * spacing_x, y0 - (row + 1) * spacing_y,
                      x0 + (column + 1) * spacing_x, y0 - row * spacing_y)
            for feature in features:
                if not _intersects(feature, bounds, prepared[feature["feature_id"]]):
                    continue
                minimum = floors[feature["feature_id"]] - clearance
                limit = float((Decimal(str(minimum)) / Decimal(str(height_step))).to_integral_value(
                    rounding=ROUND_FLOOR) * Decimal(str(height_step)))
                for r, c in ((row, column), (row, column + 1), (row + 1, column), (row + 1, column + 1)):
                    if 0 <= r < len(base) and 0 <= c < len(base[0]) and limit < result[r][c]:
                        result[r][c] = limit
                        owners[r, c] = feature
    limits = contract.get("water_conditioning_limits")
    if limits is not None:
        if (not isinstance(limits, dict)
                or any(not isinstance(limits.get(key), (int, float)) or isinstance(limits[key], bool)
                       or not math.isfinite(limits[key]) or limits[key] < 0
                       for key in ("maximum_cut_m", "maximum_support_distance_m"))
                or not isinstance(limits.get("accepted_accuracy"), list)
                or not limits["accepted_accuracy"]
                or any(not isinstance(v, str) or not v for v in limits["accepted_accuracy"])):
            raise CompilerError("water_conditioning_policy_invalid", "Water support needs explicit finite quality limits")
    for (row, column), feature in owners.items():
        point = (x0 + column * spacing_x, y0 - row * spacing_y)
        covered = [f for f in features if _footprint_distance(f, point) <= 1e-9]
        old_bound = min((quantize(_water_height(f, point), height_step) - clearance for f in covered), default=math.inf)
        # Existing sampled lowering does not need a new acceptance decision.
        if result[row][column] >= min(float(base[row][column]), old_bound) - height_step * 1e-6:
            continue
        if limits is None:
            raise CompilerError("water_conditioning_policy_required", "Continuous Water support would change terrain without accepted quality limits",
                                feature_id=feature["feature_id"], point=list(point))
        cut = float(base[row][column]) - result[row][column]
        distance = _footprint_distance(feature, point)
        accuracy = feature["attributes"]["surface_function"].get("accuracy")
        if (cut > limits["maximum_cut_m"] + 1e-9
                or distance > limits["maximum_support_distance_m"] + 1e-9
                or accuracy not in limits["accepted_accuracy"]):
            raise CompilerError("water_conditioning_quality_exception", "Continuous Water support exceeds accepted source-quality/distortion limits",
                                feature_id=feature["feature_id"], point=list(point), cut_m=cut,
                                support_distance_m=distance, accuracy=accuracy)
    return result
