from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool

from .contracts import CompilerError, canonical_hash, file_hash
from .raster_dependencies import derive_raster_dependencies
from .source import SourceBundle
from .spatial import cell_bounds, cell_id, quantize


@dataclass(frozen=True)
class SourceRaster:
    x_values: list[float]
    y_values: list[float]
    samples: list[list[float]]
    nodata: float | None

    def sample(self, x_value: float, y_value: float) -> float:
        if len(self.x_values) < 2 or len(self.y_values) < 2:
            raise CompilerError("raster_too_small", "Source raster cannot support bilinear resampling")
        dx = self.x_values[1] - self.x_values[0]
        dy = self.y_values[1] - self.y_values[0]
        column = (x_value - self.x_values[0]) / dx
        row = (y_value - self.y_values[0]) / dy
        epsilon = 1e-7
        if (
            column < -epsilon
            or row < -epsilon
            or column > len(self.x_values) - 1 + epsilon
            or row > len(self.y_values) - 1 + epsilon
        ):
            raise CompilerError(
                "canonical_grid_outside_raster",
                "Canonical terrain halo is outside admitted raster coverage",
            )
        column = min(max(column, 0.0), len(self.x_values) - 1.0)
        row = min(max(row, 0.0), len(self.y_values) - 1.0)
        left, top = int(math.floor(column)), int(math.floor(row))
        right = min(left + 1, len(self.x_values) - 1)
        bottom = min(top + 1, len(self.y_values) - 1)
        x_weight, y_weight = column - left, row - top
        values = [
            self.samples[top][left],
            self.samples[top][right],
            self.samples[bottom][left],
            self.samples[bottom][right],
        ]
        if self.nodata is not None and any(value == self.nodata for value in values):
            raise CompilerError("raster_nodata", "Canonical terrain halo intersects source nodata")
        top_value = values[0] * (1.0 - x_weight) + values[1] * x_weight
        bottom_value = values[2] * (1.0 - x_weight) + values[3] * x_weight
        return top_value * (1.0 - y_weight) + bottom_value * y_weight


def _index_bounds(grid: dict[str, Any], target_cells: list[tuple[int, int]]) -> tuple[int, int, int, int]:
    if not target_cells:
        raise CompilerError("terrain_scope_empty", "Terrain compilation requires at least one cell")
    quads_x, quads_y = (int(value) for value in grid["cell_quads"])
    halo = int(grid["halo_samples"])
    return (
        min(cell[0] * quads_x for cell in target_cells) - halo,
        min(cell[1] * quads_y for cell in target_cells) - halo,
        max((cell[0] + 1) * quads_x for cell in target_cells) + halo,
        max((cell[1] + 1) * quads_y for cell in target_cells) + halo,
    )


def _embedded_values(
    bundle: SourceBundle,
    grid: dict[str, Any],
    index_bounds: tuple[int, int, int, int],
) -> dict[tuple[int, int], float]:
    width, height = bundle.raster["size"]
    samples = bundle.raster.get("provider_payload", {}).get("samples")
    if not isinstance(samples, list) or len(samples) != height or any(len(row) != width for row in samples):
        raise CompilerError("raster_samples_invalid", "Embedded source raster dimensions are invalid")
    transform = bundle.raster["transform"]
    source = SourceRaster(
        [transform[0] + column * transform[1] for column in range(width)],
        [transform[3] + row * transform[5] for row in range(height)],
        [[float(value) for value in row] for row in samples],
        bundle.raster.get("nodata"),
    )
    scale = float(grid["fixture_scale_m"])
    spacing_x, spacing_y = (float(value) for value in grid["sample_spacing"])
    min_x, min_y, max_x, max_y = index_bounds
    return {
        (x_index, y_index): source.sample(
            (float(grid["origin"][0]) + x_index * spacing_x) / scale,
            (float(grid["origin"][1]) + y_index * spacing_y) / scale,
        )
        for y_index in range(min_y, max_y + 1)
        for x_index in range(min_x, max_x + 1)
    }


def _artifact_path(bundle: SourceBundle) -> Path:
    artifact = bundle.raster.get("data_artifact")
    if not isinstance(artifact, dict):
        raise CompilerError("raster_artifact_missing", "Raster contract does not identify its COG")
    source_path = (bundle.raster_path.parent / artifact["path"]).resolve()
    if not source_path.is_relative_to(bundle.raster_path.parent.resolve()) or not source_path.is_file():
        raise CompilerError("raster_artifact_missing", "Raster COG escaped or is unavailable")
    if source_path.stat().st_size != artifact["byte_size"] or file_hash(source_path) != artifact["sha256"]:
        raise CompilerError("raster_artifact_changed", "Raster COG differs from its accepted contract")
    return source_path


def _warped_values(
    repo_root: Path,
    bundle: SourceBundle,
    grid: dict[str, Any],
    index_bounds: tuple[int, int, int, int],
    alignment_bounds: tuple[int, int, int, int],
    scratch_root: Path,
) -> dict[tuple[int, int], float]:
    source_path = _artifact_path(bundle)
    aligned_path = scratch_root / "aligned_canonical.vrt"
    aligned_path.parent.mkdir(parents=True, exist_ok=True)
    aligned_min_x, aligned_min_y, aligned_max_x, aligned_max_y = alignment_bounds
    spacing_x, spacing_y = (float(value) for value in grid["sample_spacing"])
    origin_x, origin_y = (float(value) for value in grid["origin"])
    west = origin_x + aligned_min_x * spacing_x - spacing_x / 2.0
    south = origin_y + aligned_min_y * spacing_y - spacing_y / 2.0
    east = origin_x + aligned_max_x * spacing_x + spacing_x / 2.0
    north = origin_y + aligned_max_y * spacing_y + spacing_y / 2.0
    aligned_width = aligned_max_x - aligned_min_x + 1
    aligned_height = aligned_max_y - aligned_min_y + 1
    min_x, min_y, max_x, max_y = index_bounds
    window_width, window_height = max_x - min_x + 1, max_y - min_y + 1
    x_offset = min_x - aligned_min_x
    y_offset = aligned_max_y - max_y
    try:
        require_tools(repo_root)
        warp_options = []
        if "resampling_scale" in grid:
            warp_options = [
                "-wo", f"XSCALE={grid['resampling_scale'][0]}",
                "-wo", f"YSCALE={grid['resampling_scale'][1]}",
            ]
        run_tool(repo_root, "gdalwarp", [
            "-overwrite", "-q",
            "-s_srs", bundle.raster["crs"]["horizontal"],
            "-t_srs", grid["canonical_crs"],
            "-te_srs", grid["canonical_crs"],
            "-te", *(format(value, ".15g") for value in (west, south, east, north)),
            "-ts", str(aligned_width), str(aligned_height),
            "-r", grid["resampling_method"],
            "-ot", "Float64",
            "-dstnodata", "nan",
            "-et", "0",
            "-wo", "NUM_THREADS=1",
            *warp_options,
            "-of", "VRT",
            str(source_path),
            str(aligned_path),
        ])
        xyz = run_tool(repo_root, "gdal_translate", [
            "-q", "-srcwin", str(x_offset), str(y_offset), str(window_width), str(window_height),
            "-of", "XYZ", str(aligned_path), "/vsistdout/",
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    values: dict[tuple[int, int], float] = {}
    tolerance = min(spacing_x, spacing_y) * 1e-7
    try:
        for line in xyz.splitlines():
            x_value, y_value, sample_value = (float(value) for value in line.split())
            x_index = round((x_value - origin_x) / spacing_x)
            y_index = round((y_value - origin_y) / spacing_y)
            expected_x = origin_x + x_index * spacing_x
            expected_y = origin_y + y_index * spacing_y
            if abs(x_value - expected_x) > tolerance or abs(y_value - expected_y) > tolerance:
                raise CompilerError("raster_alignment_invalid", "GDAL output is not aligned to the canonical grid")
            if not math.isfinite(sample_value):
                raise CompilerError("raster_nodata", "Canonical terrain halo contains GDAL nodata")
            key = x_index, y_index
            if key in values:
                raise CompilerError("raster_alignment_invalid", "GDAL output repeats a canonical sample")
            values[key] = sample_value
    except ValueError as exc:
        raise CompilerError("raster_decode_failed", "Aligned GDAL raster could not be decoded") from exc
    expected = {
        (x_index, y_index)
        for y_index in range(min_y, max_y + 1)
        for x_index in range(min_x, max_x + 1)
    }
    if set(values) != expected:
        raise CompilerError("terrain_halo_incomplete", "Aligned GDAL raster does not cover the complete halo")
    return values


def _matrix(
    values: dict[tuple[int, int], float],
    x_range: range,
    y_range: range,
) -> list[list[float]]:
    try:
        return [[values[(x_value, y_value)] for x_value in x_range] for y_value in reversed(y_range)]
    except KeyError as exc:
        raise CompilerError("terrain_halo_incomplete", "Canonical terrain window contains a missing sample") from exc


def _mask(matrix: list[list[float]]) -> list[list[int]]:
    return [[1 for _ in row] for row in matrix]


def build_terrain_cells(
    repo_root: Path,
    bundle: SourceBundle,
    grid_identifier: str,
    grid: dict[str, Any],
    target_cells: list[tuple[int, int]],
    overlay: dict[str, Any],
    scratch_root: Path,
) -> dict[str, dict[str, Any]]:
    source_vertical = bundle.raster.get("vertical_datum")
    fixture = grid["coordinate_transform"] == "fixture_affine"
    if not fixture and (
        not isinstance(source_vertical, dict) or source_vertical.get("id") != grid["vertical_datum"]
    ):
        raise CompilerError("vertical_datum_conflict", "Source raster and canonical grid vertical datums differ")
    index_bounds = _index_bounds(grid, target_cells)
    alignment = grid["alignment_cell_bounds"]
    alignment_bounds = _index_bounds(grid, [(alignment[0], alignment[1]), (alignment[2], alignment[3])])
    values = (
        _embedded_values(bundle, grid, index_bounds)
        if fixture
        else _warped_values(repo_root, bundle, grid, index_bounds, alignment_bounds, scratch_root)
    )
    dependencies = derive_raster_dependencies(
        repo_root, bundle.raster, grid_identifier, grid, target_cells
    )
    spacing_x, spacing_y = (float(value) for value in grid["sample_spacing"])
    origin_x, origin_y = (float(value) for value in grid["origin"])
    height_step = float(grid["height_quantization"])
    source_provenance = bundle.raster.get("vertical_provenance")
    if not isinstance(source_provenance, dict):
        raise CompilerError("vertical_provenance_missing", "Canonical terrain requires qualified source provenance")
    for (x_index, y_index), source_height in list(values.items()):
        world_x = origin_x + x_index * spacing_x
        world_y = origin_y + y_index * spacing_y
        height = source_height
        for patch in overlay["terrain_patches"]:
            if math.hypot(world_x - patch["center"][0], world_y - patch["center"][1]) <= patch["radius_m"]:
                height += patch["delta_m"]
        values[(x_index, y_index)] = quantize(height, height_step)
    quads_x, quads_y = (int(value) for value in grid["cell_quads"])
    halo = int(grid["halo_samples"])
    result: dict[str, dict[str, Any]] = {}
    for cell_x, cell_y in sorted(target_cells):
        start_x, end_x = cell_x * quads_x, (cell_x + 1) * quads_x
        start_y, end_y = cell_y * quads_y, (cell_y + 1) * quads_y
        core = _matrix(values, range(start_x, end_x + 1), range(start_y, end_y + 1))
        halo_matrix = _matrix(
            values,
            range(start_x - halo, end_x + halo + 1),
            range(start_y - halo, end_y + halo + 1),
        )
        edges = {
            "west": [[row[0]] for row in core],
            "east": [[row[-1]] for row in core],
            "north": [core[0]],
            "south": [core[-1]],
        }
        bands = {
            "west": _matrix(values, range(start_x - halo, start_x + halo + 1), range(start_y, end_y + 1)),
            "east": _matrix(values, range(end_x - halo, end_x + halo + 1), range(start_y, end_y + 1)),
            "north": _matrix(values, range(start_x, end_x + 1), range(end_y - halo, end_y + halo + 1)),
            "south": _matrix(values, range(start_x, end_x + 1), range(start_y - halo, start_y + halo + 1)),
        }
        bounds = cell_bounds(grid, cell_x, cell_y)
        current_id = cell_id(grid_identifier, cell_x, cell_y)
        result[current_id] = {
            "$schema": "https://alis.world/schemas/world-compiler/terrain-cell-v1.json",
            "schema_version": 1,
            "grid_id": grid_identifier,
            "cell_id": current_id,
            "bounds": list(bounds),
            "sample_spacing": grid["sample_spacing"],
            "height_quantization": grid["height_quantization"],
            "vertical_provenance": {
                "source_ref": source_provenance["source_ref"],
                "vertical_datum": grid["vertical_datum"],
                "source_accuracy_m": source_provenance["source_accuracy_m"],
                "confidence": source_provenance["confidence"],
                "sampling_quantization_residual_m": height_step * 0.5,
            },
            "raster_alignment": {
                "authority": "synthetic_fixture_affine" if fixture else "pinned_gdalwarp_vrt",
                "source_crs": bundle.raster["crs"]["horizontal"],
                "canonical_crs": grid["canonical_crs"],
                "resampling_method": grid["resampling_method"],
                **({"resampling_scale": grid["resampling_scale"]} if "resampling_scale" in grid else {}),
            },
            "raster_component_dependencies": dependencies.cell_components[current_id],
            "core_samples": core,
            "halo_window": {
                "index_bounds": [start_x - halo, start_y - halo, end_x + halo, end_y + halo],
                "samples": halo_matrix,
            },
            "height_edges": {side: f"sha256:{canonical_hash(value)}" for side, value in edges.items()},
            "height_border_bands": {side: f"sha256:{canonical_hash(value)}" for side, value in bands.items()},
            "weight_mask": {
                "core_samples": _mask(core),
                "edges": {side: f"sha256:{canonical_hash(_mask(value))}" for side, value in edges.items()},
                "border_bands": {side: f"sha256:{canonical_hash(_mask(value))}" for side, value in bands.items()},
            },
            "authored_patch_ids": sorted(
                patch["patch_id"]
                for patch in overlay["terrain_patches"]
                if bounds[0] - patch["radius_m"] <= patch["center"][0] <= bounds[2] + patch["radius_m"]
                and bounds[1] - patch["radius_m"] <= patch["center"][1] <= bounds[3] + patch["radius_m"]
            ),
        }
    return result
