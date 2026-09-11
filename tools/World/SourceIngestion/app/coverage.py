from __future__ import annotations

import math
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, transform_points

from .contracts import IngestionError


def _axis_samples(start: float, end: float, maximum_step: float) -> list[float]:
    segments = max(1, math.ceil((end - start) / maximum_step))
    return [start + (end - start) * index / segments for index in range(segments + 1)]


def projected_edge_points(contract: dict[str, Any]) -> list[list[float]]:
    west, south, east, north = (float(value) for value in contract["technical_bounds"])
    expansion = float(contract["terrain_resampling_halo_m"]) + float(contract["minimum_source_margin_m"])
    west, south, east, north = west - expansion, south - expansion, east + expansion, north + expansion
    step = float(contract["edge_sample_step_m"])
    xs = _axis_samples(west, east, step)
    ys = _axis_samples(south, north, step)
    return [
        *[[x, south] for x in xs],
        *[[east, y] for y in ys[1:]],
        *[[x, north] for x in reversed(xs[:-1])],
        *[[west, y] for y in reversed(ys[1:-1])],
    ]


def required_geographic_bbox(repo_root: Path, contract: dict[str, Any]) -> tuple[list[float], int]:
    points = projected_edge_points(contract)
    try:
        geographic = transform_points(repo_root, points, contract["projected_crs"], "EPSG:4326")
    except ExecutionEnvironmentError as error:
        raise IngestionError(error.code, "Pinned OGR could not project the source coverage boundary") from error
    return [
        min(point[0] for point in geographic),
        min(point[1] for point in geographic),
        max(point[0] for point in geographic),
        max(point[1] for point in geographic),
    ], len(geographic)


def raster_union_covers(required: list[float], coverages: list[list[float]]) -> bool:
    west, south, east, north = required
    clipped = [
        [max(west, item[0]), max(south, item[1]), min(east, item[2]), min(north, item[3])]
        for item in coverages
        if item[0] < east and item[2] > west and item[1] < north and item[3] > south
    ]
    x_breaks = sorted({west, east, *(value for item in clipped for value in (item[0], item[2]))})
    for left, right in zip(x_breaks, x_breaks[1:]):
        if left == right:
            continue
        midpoint = (left + right) / 2
        intervals = sorted(
            (item[1], item[3]) for item in clipped if item[0] <= midpoint <= item[2]
        )
        cursor = south
        for lower, upper in intervals:
            if lower > cursor:
                return False
            cursor = max(cursor, upper)
            if cursor >= north:
                break
        if cursor < north:
            return False
    return bool(x_breaks) and x_breaks[0] <= west and x_breaks[-1] >= east


def build_coverage_evidence(repo_root: Path, profile: dict[str, Any]) -> dict[str, Any]:
    contract = profile["projected_coverage"]
    required_bbox, checked = required_geographic_bbox(repo_root, contract)
    area_bbox = [float(value) for value in profile["area"]["bbox"]]
    if any((
        area_bbox[0] > required_bbox[0],
        area_bbox[1] > required_bbox[1],
        area_bbox[2] < required_bbox[2],
        area_bbox[3] < required_bbox[3],
    )):
        raise IngestionError(
            "coverage_margin_shortfall",
            "Source bbox does not contain the projected technical envelope and required margins",
            required_bbox=required_bbox,
            actual_bbox=area_bbox,
        )
    raster_sources = [source for source in profile["sources"] if source["adapter"] == "copernicus_dem"]
    if not raster_union_covers(area_bbox, [source["coverage_bbox"] for source in raster_sources]):
        raise IngestionError("raster_coverage_gap", "Admitted raster tiles do not cover the complete source bbox")
    expansion = float(contract["terrain_resampling_halo_m"]) + float(contract["minimum_source_margin_m"])
    technical = [float(value) for value in contract["technical_bounds"]]
    return {
        "projection_authority": "pinned_gdal_ogr",
        "projected_crs": contract["projected_crs"],
        "technical_bounds": technical,
        "required_projected_bounds": [
            technical[0] - expansion,
            technical[1] - expansion,
            technical[2] + expansion,
            technical[3] + expansion,
        ],
        "terrain_resampling_halo_m": contract["terrain_resampling_halo_m"],
        "minimum_source_margin_m": contract["minimum_source_margin_m"],
        "edge_sample_step_m": contract["edge_sample_step_m"],
        "edge_samples_checked": checked,
        "required_geographic_bbox": required_bbox,
        "accepted_geographic_bbox": area_bbox,
        "raster_source_ids": [source["source_id"] for source in raster_sources],
        "raster_union_complete": True,
    }
