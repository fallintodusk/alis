from __future__ import annotations

import json
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, run_tool

from .contracts import CompilerError
from .spatial import quantize_geometry


def _metric_crs(crs: str) -> str:
    return crs if crs.startswith("EPSG:") else 'LOCAL_CS["ProjectWorld local metres",UNIT["metre",1]]'


@dataclass(frozen=True)
class TerrainSampler:
    x_values: list[float]
    y_values: list[float]
    values: dict[tuple[float, float], float]
    core_x_values: list[float] | None = None
    core_y_values: list[float] | None = None

    @classmethod
    def from_cells(cls, terrain: dict[str, dict[str, Any]]) -> TerrainSampler:
        values: dict[tuple[float, float], float] = {}
        core_x_values: set[float] = set()
        core_y_values: set[float] = set()
        for cell in terrain.values():
            west, _, _, north = (float(value) for value in cell["bounds"])
            spacing_x, spacing_y = (float(value) for value in cell["sample_spacing"])
            core = cell["core_samples"]
            core_x_values.update(west + column * spacing_x for column in range(len(core[0])))
            core_y_values.update(north - row * spacing_y for row in range(len(core)))
            samples = cell.get("halo_window", {}).get("samples", core)
            core_rows = len(core)
            core_columns = len(core[0])
            row_count = len(samples)
            column_count = len(samples[0])
            if (
                row_count < core_rows
                or column_count < core_columns
                or (row_count - core_rows) % 2
                or (column_count - core_columns) % 2
                or any(len(row) != column_count for row in samples)
            ):
                raise CompilerError("terrain_halo_invalid", "Water sampling found an invalid terrain halo")
            y_padding = (row_count - core_rows) // 2
            x_padding = (column_count - core_columns) // 2
            sample_west = west - x_padding * spacing_x
            sample_north = north + y_padding * spacing_y
            for row, row_samples in enumerate(samples):
                y_value = sample_north - row * spacing_y
                for column, height in enumerate(row_samples):
                    point = sample_west + column * spacing_x, y_value
                    if point in values and values[point] != float(height):
                        raise CompilerError("terrain_seam_mismatch", "Water sampling found conflicting terrain seams")
                    values[point] = float(height)
        if not values:
            raise CompilerError("water_terrain_missing", "Water semantics require canonical terrain samples")
        return cls(
            sorted({point[0] for point in values}),
            sorted({point[1] for point in values}),
            values,
            sorted(core_x_values),
            sorted(core_y_values),
        )

    def core_view(self) -> TerrainSampler:
        return TerrainSampler(
            self.core_x_values or self.x_values,
            self.core_y_values or self.y_values,
            self.values,
        )

    def sample(self, point: list[float]) -> float:
        import bisect

        x_value, y_value = (float(value) for value in point[:2])
        if not (self.x_values[0] <= x_value <= self.x_values[-1] and self.y_values[0] <= y_value <= self.y_values[-1]):
            raise CompilerError("water_axis_outside_terrain", "Water axis sample is outside canonical terrain")
        x_index = max(0, min(len(self.x_values) - 2, bisect.bisect_right(self.x_values, x_value) - 1))
        y_index = max(0, min(len(self.y_values) - 2, bisect.bisect_right(self.y_values, y_value) - 1))
        west, east = self.x_values[x_index:x_index + 2]
        south, north = self.y_values[y_index:y_index + 2]
        x_weight = (x_value - west) / (east - west)
        y_weight = (y_value - south) / (north - south)
        return (
            self.values[west, south] * (1.0 - x_weight) * (1.0 - y_weight)
            + self.values[east, south] * x_weight * (1.0 - y_weight)
            + self.values[west, north] * (1.0 - x_weight) * y_weight
            + self.values[east, north] * x_weight * y_weight
        )


def _feature_collection(geometries: dict[str, dict[str, Any]], extra: dict[str, dict[str, Any]] | None = None) -> dict:
    return {
        "type": "FeatureCollection",
        "features": [{
            "type": "Feature",
            "properties": {"source_id": source_id, **((extra or {}).get(source_id, {}))},
            "geometry": geometry,
        } for source_id, geometry in sorted(geometries.items())],
    }


def _write(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, sort_keys=True, separators=(",", ":")), encoding="utf-8")


def _run(repo_root: Path, arguments: list[str]) -> None:
    try:
        run_tool(repo_root, "ogr2ogr", arguments)
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc


def _production_database(
    repo_root: Path,
    scratch_root: Path,
    polygons: dict[str, dict[str, Any]],
    lines: dict[str, dict[str, Any]],
    line_widths: dict[str, float],
    sampler: TerrainSampler,
    crs: str,
) -> Path:
    database = scratch_root / "water_geometry.gpkg"
    if database.exists():
        database.unlink()
    polygon_path, line_path, bounds_path = (
        scratch_root / "polygons.geojson",
        scratch_root / "lines.geojson",
        scratch_root / "terrain_bounds.geojson",
    )
    polygon_ids = {source_id: {"sample_id": index} for index, source_id in enumerate(sorted(polygons), 1)}
    _write(polygon_path, _feature_collection(polygons, polygon_ids))
    width_keys = {width: index for index, width in enumerate(sorted(set(line_widths.values())), 1)}
    _write(line_path, _feature_collection(lines, {
        source_id: {"width_m": line_widths[source_id], "width_key": width_keys[line_widths[source_id]]}
        for source_id in lines
    }))
    west, east = sampler.x_values[0], sampler.x_values[-1]
    south, north = sampler.y_values[0], sampler.y_values[-1]
    _write(bounds_path, _feature_collection({"territory": {
        "type": "Polygon",
        "coordinates": [[[west, south], [east, south], [east, north], [west, north], [west, south]]],
    }}))
    assigned_crs = _metric_crs(crs)
    layers = (
        (polygon_path, "polygons"),
        (line_path, "lines"),
        (bounds_path, "terrain_bounds"),
    )
    for index, (path, layer) in enumerate(layers):
        arguments = [
            "-overwrite" if index == 0 else "-update", "-f", "GPKG", str(database),
            str(path), "-nln", layer, "-a_srs", assigned_crs, "-lco", "SPATIAL_INDEX=YES",
        ]
        _run(repo_root, arguments)
    return database


def _materialize_suppression_layers(
    repo_root: Path,
    scratch_root: Path,
    database: Path,
    crs: str,
    quadrant_segments: int,
    suppression_clearance_m: float,
) -> None:
    assigned_crs = _metric_crs(crs)
    clearance = format(float(suppression_clearance_m), ".15g")
    layers = (
        ("polygon_union", "SELECT 1 AS union_id,ST_Union(p.geom) AS geometry FROM polygons p"),
        (
            "ribbon_exclusions",
            "SELECT l.width_key,l.width_m,"
            f"ST_Buffer(u.geom,(l.width_m*0.5)+{clearance},{quadrant_segments}) AS geometry "
            "FROM (SELECT DISTINCT width_key,width_m FROM lines) l CROSS JOIN polygon_union u",
        ),
    )
    for layer, sql in layers:
        path = scratch_root / f"{layer}.geojson"
        _run(repo_root, [
            "-overwrite", "-f", "GeoJSON", str(path), str(database),
            "-dialect", "SQLite", "-sql", sql,
        ])
        _run(repo_root, [
            "-update", "-f", "GPKG", str(database), str(path), "-nln", layer,
            "-a_srs", assigned_crs, "-lco", "SPATIAL_INDEX=YES",
        ])


def _rasterized_polygon_samples(
    repo_root: Path,
    scratch_root: Path,
    database: Path,
    polygons: dict[str, dict[str, Any]],
    sampler: TerrainSampler,
) -> dict[str, list[float]]:
    samples = {source_id: [] for source_id in polygons}
    if not polygons:
        return samples
    spacing_x = sampler.x_values[1] - sampler.x_values[0]
    spacing_y = sampler.y_values[1] - sampler.y_values[0]
    if any(abs(right - left - spacing_x) > 1e-8 for left, right in zip(sampler.x_values, sampler.x_values[1:], strict=False)):
        raise CompilerError("water_grid_irregular", "Water polygon rasterization requires a regular X grid")
    if any(abs(right - left - spacing_y) > 1e-8 for left, right in zip(sampler.y_values, sampler.y_values[1:], strict=False)):
        raise CompilerError("water_grid_irregular", "Water polygon rasterization requires a regular Y grid")
    extent = (
        sampler.x_values[0] - spacing_x * 0.5,
        sampler.y_values[0] - spacing_y * 0.5,
        sampler.x_values[-1] + spacing_x * 0.5,
        sampler.y_values[-1] + spacing_y * 0.5,
    )
    common = [
        "-q", "-l", "polygons", "-init", "0", "-a_nodata", "0", "-ot", "Int32",
        "-te", *(str(value) for value in extent),
        "-ts", str(len(sampler.x_values)), str(len(sampler.y_values)),
    ]
    count_path, id_path = scratch_root / "polygon_counts.tif", scratch_root / "polygon_ids.tif"
    _run_named(repo_root, "gdal_rasterize", [*common, "-burn", "1", "-add", str(database), str(count_path)])
    _run_named(repo_root, "gdal_rasterize", [*common, "-a", "sample_id", str(database), str(id_path)])
    counts = _run_named(repo_root, "gdal_translate", ["-q", "-of", "XYZ", str(count_path), "/vsistdout/"])
    identifiers = _run_named(repo_root, "gdal_translate", ["-q", "-of", "XYZ", str(id_path), "/vsistdout/"])
    source_by_id = {index: source_id for index, source_id in enumerate(sorted(polygons), 1)}
    for count_line, id_line in zip(counts.splitlines(), identifiers.splitlines(), strict=True):
        x_value, y_value, count = (float(value) for value in count_line.split())
        sample_id = int(float(id_line.split()[2]))
        if count > 1.0:
            raise CompilerError(
                "water_polygon_sample_overlap",
                "One canonical terrain sample belongs to multiple admitted water polygons",
                point=[x_value, y_value],
            )
        if sample_id:
            x_index = round((x_value - sampler.x_values[0]) / spacing_x)
            y_index = round((y_value - sampler.y_values[0]) / spacing_y)
            point = sampler.x_values[x_index], sampler.y_values[y_index]
            samples[source_by_id[sample_id]].append(sampler.values[point])
    return samples


def _run_named(repo_root: Path, tool: str, arguments: list[str]) -> str:
    try:
        return run_tool(repo_root, tool, arguments)
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc


def production_geometry_evidence(
    repo_root: Path,
    scratch_root: Path,
    polygons: dict[str, dict[str, Any]],
    lines: dict[str, dict[str, Any]],
    line_widths: dict[str, float],
    sampler: TerrainSampler,
    crs: str,
    coordinate_step: float,
    quadrant_segments: int,
    suppression_clearance_m: float,
) -> tuple[
    dict[str, list[float]],
    dict[str, list[float]],
    dict[str, dict[str, Any] | None],
    dict[str, float],
    dict[str, dict[str, Any]],
]:
    shutil.rmtree(scratch_root, ignore_errors=True)
    scratch_root.mkdir(parents=True)
    if set(lines) != set(line_widths):
        raise CompilerError("water_width_unapproved", "Every admitted water line requires one resolved width")
    database = _production_database(repo_root, scratch_root, polygons, lines, line_widths, sampler, crs)
    points_path, difference_path = (
        scratch_root / "polygon_points.geojson",
        scratch_root / "line_difference.geojson",
    )
    samples = _rasterized_polygon_samples(repo_root, scratch_root, database, polygons, sampler)
    if polygons:
        _run(repo_root, ["-overwrite", "-f", "GeoJSON", str(points_path), str(database), "-dialect", "SQLite", "-sql",
            "SELECT p.source_id,ST_PointOnSurface(ST_Intersection(p.geom,b.geom)) AS geometry "
            "FROM polygons p,terrain_bounds b WHERE ST_Intersects(p.geom,b.geom)"])
    segments = int(quadrant_segments)
    if polygons and lines:
        _materialize_suppression_layers(
            repo_root, scratch_root, database, crs, segments, suppression_clearance_m
        )
        difference_sql = (
            "SELECT l.source_id,l.width_m,CASE WHEN ST_Intersects(l.geom,e.geom) "
            "THEN ST_Difference(l.geom,e.geom) ELSE l.geom END AS geometry "
            "FROM lines l JOIN ribbon_exclusions e ON e.width_key=l.width_key"
        )
    else:
        difference_sql = "SELECT l.source_id,l.width_m,l.geom AS geometry FROM lines l"
    if lines:
        _run(repo_root, ["-overwrite", "-f", "GeoJSON", str(difference_path), str(database), "-dialect", "SQLite", "-sql", difference_sql])
    fallback_points: dict[str, list[float]] = {}
    if polygons:
        fallback_points = {
            feature["properties"]["source_id"]: [float(value) for value in feature["geometry"]["coordinates"]]
            for feature in json.loads(points_path.read_text(encoding="utf-8"))["features"]
        }
    visible = {source_id: None for source_id in lines}
    if lines:
        for feature in json.loads(difference_path.read_text(encoding="utf-8"))["features"]:
            source_id, geometry = feature["properties"]["source_id"], feature.get("geometry")
            visible[source_id] = quantize_geometry(geometry, coordinate_step) if geometry else None
    overlap, footprints = _ribbon_footprint_evidence(
        repo_root,
        scratch_root,
        database,
        polygons,
        visible,
        line_widths,
        crs,
        coordinate_step,
        segments,
    )
    return samples, fallback_points, visible, overlap, footprints


def _ribbon_footprint_evidence(
    repo_root: Path,
    scratch_root: Path,
    database: Path,
    polygons: dict[str, dict[str, Any]],
    visible: dict[str, dict[str, Any] | None],
    line_widths: dict[str, float],
    crs: str,
    coordinate_step: float,
    quadrant_segments: int,
) -> tuple[dict[str, float], dict[str, dict[str, Any]]]:
    overlap = {source_id: 0.0 for source_id in visible}
    geometries = {source_id: geometry for source_id, geometry in visible.items() if geometry is not None}
    if not geometries:
        return overlap, {}
    visible_path = scratch_root / "visible_lines.geojson"
    footprint_path = scratch_root / "ribbon_footprints.geojson"
    overlap_path = scratch_root / "ribbon_overlap.geojson"
    _write(visible_path, _feature_collection(
        geometries,
        {source_id: {"width_m": line_widths[source_id]} for source_id in geometries},
    ))
    assigned_crs = _metric_crs(crs)
    _run(repo_root, [
        "-update", "-f", "GPKG", str(database), str(visible_path), "-nln", "visible_lines",
        "-a_srs", assigned_crs, "-lco", "SPATIAL_INDEX=YES",
    ])
    _run(repo_root, [
        "-overwrite", "-f", "GeoJSON", str(footprint_path), str(database),
        "-dialect", "SQLite", "-sql",
        f"SELECT v.source_id,ST_Buffer(v.geom,v.width_m*0.5,{quadrant_segments}) AS geometry FROM visible_lines v",
    ])
    footprints = {
        feature["properties"]["source_id"]: quantize_geometry(feature["geometry"], coordinate_step)
        for feature in json.loads(footprint_path.read_text(encoding="utf-8"))["features"]
    }
    _write(footprint_path, _feature_collection(footprints))
    _run(repo_root, [
        "-update", "-f", "GPKG", str(database), str(footprint_path), "-nln", "ribbon_footprints",
        "-a_srs", assigned_crs, "-lco", "SPATIAL_INDEX=YES",
    ])
    if not polygons:
        return overlap, footprints
    _run(repo_root, [
        "-overwrite", "-f", "GeoJSON", str(overlap_path), str(database),
        "-dialect", "SQLite", "-sql",
        "SELECT f.source_id,f.geom AS geometry,COALESCE(ST_Area(ST_Intersection(f.geom,u.geom)),0.0) "
        "AS overlap_area_m2 FROM ribbon_footprints f CROSS JOIN polygon_union u",
    ])
    for feature in json.loads(overlap_path.read_text(encoding="utf-8"))["features"]:
        source_id = feature["properties"]["source_id"]
        area = float(feature["properties"]["overlap_area_m2"] or 0.0)
        if area != 0.0:
            raise CompilerError(
                "water_surface_overlap",
                "Final round-buffered ribbon footprint overlaps polygon-owned water",
                source_id=source_id,
                overlap_area_m2=area,
            )
        overlap[source_id] = 0.0
    return overlap, footprints
