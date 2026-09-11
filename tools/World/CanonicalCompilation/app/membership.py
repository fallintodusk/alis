from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool

from .contracts import CompilerError
from .geometry import clip_line
from .spatial import cell_bounds


def _point_in_bounds(point: list[float], bounds: tuple[float, float, float, float]) -> bool:
    return bounds[0] <= point[0] <= bounds[2] and bounds[1] <= point[1] <= bounds[3]


def _orientation(a: list[float], b: list[float], c: list[float]) -> float:
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def _on_segment(a: list[float], b: list[float], point: list[float]) -> bool:
    return _orientation(a, b, point) == 0 and (
        min(a[0], b[0]) <= point[0] <= max(a[0], b[0])
        and min(a[1], b[1]) <= point[1] <= max(a[1], b[1])
    )


def _segments_intersect(a: list[float], b: list[float], c: list[float], d: list[float]) -> bool:
    values = _orientation(a, b, c), _orientation(a, b, d), _orientation(c, d, a), _orientation(c, d, b)
    if (values[0] > 0 > values[1] or values[0] < 0 < values[1]) and (
        values[2] > 0 > values[3] or values[2] < 0 < values[3]
    ):
        return True
    return any(
        value == 0 and _on_segment(start, end, point)
        for value, start, end, point in (
            (values[0], a, b, c), (values[1], a, b, d),
            (values[2], c, d, a), (values[3], c, d, b),
        )
    )


def _point_in_ring(point: list[float], ring: list[list[float]]) -> bool:
    inside = False
    for start, end in zip(ring, ring[1:], strict=False):
        if _on_segment(start, end, point):
            return True
        if (start[1] > point[1]) != (end[1] > point[1]):
            crossing_x = start[0] + (point[1] - start[1]) * (end[0] - start[0]) / (end[1] - start[1])
            if crossing_x > point[0]:
                inside = not inside
    return inside


def _polygon_intersects_bounds(
    polygon: list[list[list[float]]], bounds: tuple[float, float, float, float]
) -> bool:
    west, south, east, north = bounds
    corners = [[west, south], [east, south], [east, north], [west, north]]
    edges = list(zip(corners, [corners[1], corners[2], corners[3], corners[0]], strict=True))
    for ring in polygon:
        if any(_point_in_bounds(point, bounds) for point in ring):
            return True
        if any(
            _segments_intersect(start, end, edge_start, edge_end)
            for start, end in zip(ring, ring[1:], strict=False)
            for edge_start, edge_end in edges
        ):
            return True
    exterior = polygon[0]
    holes = polygon[1:]
    return any(
        _point_in_ring(corner, exterior) and not any(_point_in_ring(corner, hole) for hole in holes)
        for corner in corners
    )


def _fixture_membership(
    projected: dict[str, dict[str, Any]],
    grid: dict[str, Any],
    targets: list[tuple[int, int]],
) -> dict[str, list[tuple[int, int]]]:
    memberships: dict[str, list[tuple[int, int]]] = {}
    step = float(grid["coordinate_quantization"])
    for source_id, geometry in projected.items():
        matches: list[tuple[int, int]] = []
        for target in targets:
            bounds = cell_bounds(grid, *target)
            if geometry["type"] in {"Point", "MultiPoint"}:
                points = geometry["coordinates"] if geometry["type"] == "MultiPoint" else [geometry["coordinates"]]
                intersects = any(_point_in_bounds(point, bounds) for point in points)
            elif geometry["type"] in {"LineString", "MultiLineString"}:
                lines = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
                intersects = any(clip_line(line, bounds, step) for line in lines)
            else:
                polygons = geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]
                intersects = any(_polygon_intersects_bounds(polygon, bounds) for polygon in polygons)
            if intersects:
                matches.append(target)
        memberships[source_id] = matches
    return memberships


def _ogr_membership(
    repo_root: Path,
    projected: dict[str, dict[str, Any]],
    grid: dict[str, Any],
    targets: list[tuple[int, int]],
    scratch_root: Path,
) -> dict[str, list[tuple[int, int]]]:
    if not projected:
        return {}
    scratch_root.mkdir(parents=True, exist_ok=True)
    features_path = scratch_root / "membership_features.geojson"
    cells_path = scratch_root / "membership_cells.geojson"
    database_path = scratch_root / "membership.gpkg"
    output_path = scratch_root / "membership.json"
    features_path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {"type": "Feature", "properties": {"source_id": source_id}, "geometry": geometry}
            for source_id, geometry in sorted(projected.items())
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    cells_path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"cell_x": x_value, "cell_y": y_value},
                "geometry": {"type": "Polygon", "coordinates": [[
                    [west, south], [east, south], [east, north], [west, north], [west, south],
                ]]},
            }
            for x_value, y_value in targets
            for west, south, east, north in [cell_bounds(grid, x_value, y_value)]
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    try:
        require_tools(repo_root)
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GPKG", str(database_path), str(features_path),
            "-nln", "features", "-a_srs", grid["canonical_crs"],
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-update", "-f", "GPKG", str(database_path), str(cells_path),
            "-nln", "cells", "-a_srs", grid["canonical_crs"],
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GeoJSON", str(output_path), str(database_path),
            "-dialect", "SQLite", "-sql",
            "SELECT f.source_id, c.cell_x, c.cell_y FROM features f JOIN cells c ON ST_Intersects(f.geom, c.geom)",
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    try:
        output = json.loads(output_path.read_text(encoding="utf-8"))
        memberships = {source_id: [] for source_id in projected}
        for item in output["features"]:
            properties = item["properties"]
            source_id = properties["source_id"]
            coordinate = int(properties["cell_x"]), int(properties["cell_y"])
            if source_id not in memberships or coordinate not in targets:
                raise KeyError(source_id)
            memberships[source_id].append(coordinate)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise CompilerError("geometry_membership_failed", "Pinned OGR membership output is invalid") from exc
    step = float(grid["coordinate_quantization"])
    for source_id, geometry in projected.items():
        if geometry["type"] not in {"LineString", "MultiLineString"}:
            continue
        lines = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
        memberships[source_id] = [
            target
            for target in memberships[source_id]
            if any(clip_line(line, cell_bounds(grid, *target), step) for line in lines)
        ]
    return {source_id: sorted(set(values)) for source_id, values in memberships.items()}


def feature_cell_membership(
    repo_root: Path,
    projected: dict[str, dict[str, Any]],
    grid: dict[str, Any],
    targets: list[tuple[int, int]],
    scratch_root: Path,
) -> dict[str, list[tuple[int, int]]]:
    if grid["coordinate_transform"] == "fixture_affine":
        return _fixture_membership(projected, grid, targets)
    return _ogr_membership(repo_root, projected, grid, targets, scratch_root)
