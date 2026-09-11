from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool

from .contracts import CompilerError
from .spatial import cell_bounds, geometry_bounds, intersects, quantize_geometry, transform_geometry


def project_features(
    repo_root: Path,
    source_features: list[dict[str, Any]],
    grid: dict[str, Any],
    scratch_root: Path,
) -> tuple[dict[str, dict[str, Any]], set[str]]:
    if grid["coordinate_transform"] == "fixture_affine":
        return {
            feature["provider_feature_id"]: transform_geometry(feature["geometry"], grid)
            for feature in source_features
        }, set()
    if not source_features:
        return {}, set()
    scratch_root.mkdir(parents=True, exist_ok=True)
    source_path = scratch_root / "source_features.geojson"
    projected_path = scratch_root / "projected_features.geojson"
    source_path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"provider_feature_id": feature["provider_feature_id"]},
                "geometry": feature["geometry"],
            }
            for feature in source_features
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    try:
        require_tools(repo_root)
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite",
            "-skipinvalid",
            "-dim", "XY",
            "-s_srs", "EPSG:4326",
            "-t_srs", grid["canonical_crs"],
            "-f", "GeoJSON",
            "-lco", "COORDINATE_PRECISION=12",
            str(projected_path),
            str(source_path),
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    try:
        output = json.loads(projected_path.read_text(encoding="utf-8"))
        projected = {
            item["properties"]["provider_feature_id"]: quantize_geometry(
                item["geometry"], float(grid["coordinate_quantization"])
            )
            for item in output["features"]
        }
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise CompilerError("geometry_projection_failed", "Pinned OGR output is invalid") from exc
    expected = {feature["provider_feature_id"] for feature in source_features}
    unknown = set(projected) - expected
    if unknown or any(
        not all(math.isfinite(value) for value in point)
        for geometry in projected.values()
        for point in _geometry_points(geometry)
    ):
        raise CompilerError("geometry_projection_failed", "Pinned OGR returned unknown or non-finite geometry")
    return projected, expected - set(projected)


def _geometry_points(geometry: dict[str, Any]) -> list[list[float]]:
    coordinates = geometry["coordinates"]
    if geometry["type"] == "Point":
        return [coordinates]
    if geometry["type"] == "MultiPoint":
        return coordinates
    if geometry["type"] == "LineString":
        return coordinates
    if geometry["type"] == "MultiLineString":
        return [point for line in coordinates for point in line]
    if geometry["type"] == "Polygon":
        return [point for ring in coordinates for point in ring]
    return [point for polygon in coordinates for ring in polygon for point in ring]


def select_source_features(
    repo_root: Path,
    source_features: list[dict[str, Any]],
    grid: dict[str, Any],
    target_cells: set[tuple[int, int]],
    scratch_root: Path,
) -> list[dict[str, Any]]:
    if not target_cells:
        return []
    bounds = [cell_bounds(grid, *target) for target in target_cells]
    envelope = (
        min(item[0] for item in bounds), min(item[1] for item in bounds),
        max(item[2] for item in bounds), max(item[3] for item in bounds),
    )
    if grid["coordinate_transform"] == "fixture_affine":
        return [
            feature
            for feature in source_features
            if intersects(geometry_bounds(transform_geometry(feature["geometry"], grid)), envelope)
        ]
    scratch_root.mkdir(parents=True, exist_ok=True)
    source_path = scratch_root / "selection_source.geojson"
    selected_path = scratch_root / "selection_result.geojson"
    source_path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"provider_feature_id": feature["provider_feature_id"]},
                "geometry": feature["geometry"],
            }
            for feature in source_features
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    try:
        require_tools(repo_root)
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-dim", "XY",
            "-spat", *(format(value, ".15g") for value in envelope),
            "-spat_srs", grid["canonical_crs"],
            "-s_srs", "EPSG:4326", "-t_srs", grid["canonical_crs"],
            "-f", "GeoJSON", str(selected_path), str(source_path),
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    try:
        selected_ids = {
            item["properties"]["provider_feature_id"]
            for item in json.loads(selected_path.read_text(encoding="utf-8"))["features"]
        }
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise CompilerError("geometry_selection_failed", "Pinned OGR selection output is invalid") from exc
    return [feature for feature in source_features if feature["provider_feature_id"] in selected_ids]
