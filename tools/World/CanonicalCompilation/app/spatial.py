from __future__ import annotations

import math
from typing import Any

from .contracts import CompilerError, canonical_hash


def quantize(value: float, step: float) -> float:
    places = max(0, int(math.ceil(-math.log10(step)))) + 1
    return round(round(value / step) * step, places)


def _map_coordinates(value: Any, step: float) -> Any:
    if isinstance(value, list) and value and all(isinstance(item, (int, float)) for item in value):
        return [quantize(float(item), step) for item in value]
    if isinstance(value, list):
        return [_map_coordinates(item, step) for item in value]
    raise CompilerError("unsupported_geometry", "Geometry coordinates are malformed")


def quantize_geometry(geometry: dict[str, Any], step: float) -> dict[str, Any]:
    return {
        "type": geometry["type"],
        "coordinates": _map_coordinates(geometry["coordinates"], step),
    }


def transform_geometry(geometry: dict[str, Any], grid: dict[str, Any]) -> dict[str, Any]:
    if grid["coordinate_transform"] != "fixture_affine":
        raise CompilerError(
            "projection_authority_required",
            "Production geometry projection must use the pinned OGR authority",
        )
    scale = float(grid["fixture_scale_m"])

    def transform(value: Any) -> Any:
        if isinstance(value, list) and value and all(isinstance(item, (int, float)) for item in value):
            return [float(value[0]) * scale, float(value[1]) * scale]
        if isinstance(value, list):
            return [transform(item) for item in value]
        raise CompilerError("unsupported_geometry", "Geometry coordinates are malformed")

    return quantize_geometry(
        {"type": geometry["type"], "coordinates": transform(geometry["coordinates"])},
        float(grid["coordinate_quantization"]),
    )


def geometry_points(geometry: dict[str, Any]) -> list[list[float]]:
    if geometry["type"] == "Point":
        return [geometry["coordinates"]]
    if geometry["type"] == "MultiPoint":
        return geometry["coordinates"]
    if geometry["type"] == "LineString":
        return geometry["coordinates"]
    if geometry["type"] == "MultiLineString":
        return [point for line in geometry["coordinates"] for point in line]
    if geometry["type"] == "Polygon":
        return [point for ring in geometry["coordinates"] for point in ring]
    return [point for polygon in geometry["coordinates"] for ring in polygon for point in ring]


def geometry_bounds(geometry: dict[str, Any]) -> tuple[float, float, float, float]:
    points = geometry_points(geometry)
    return min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)


def grid_id(grid: dict[str, Any]) -> str:
    return f"grid_{canonical_hash(grid)[:16]}"


def cell_id(grid_identifier: str, x_value: int, y_value: int) -> str:
    return f"{grid_identifier}:x{x_value}:y{y_value}"


def cell_bounds(grid: dict[str, Any], x_value: int, y_value: int) -> tuple[float, float, float, float]:
    width = float(grid["sample_spacing"][0]) * int(grid["cell_quads"][0])
    height = float(grid["sample_spacing"][1]) * int(grid["cell_quads"][1])
    west = float(grid["origin"][0]) + x_value * width
    south = float(grid["origin"][1]) + y_value * height
    return west, south, west + width, south + height


def intersects(left: tuple[float, float, float, float], right: tuple[float, float, float, float]) -> bool:
    return left[0] <= right[2] and left[2] >= right[0] and left[1] <= right[3] and left[3] >= right[1]
