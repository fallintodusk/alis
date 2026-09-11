from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, transform_points

from .contracts import CompilerError
from .spatial import cell_id


PixelDependency = tuple[str, int, int]


@dataclass(frozen=True)
class RasterDependencyIndex:
    sample_pixels: dict[tuple[int, int], frozenset[PixelDependency]]
    cell_pixels: dict[str, frozenset[PixelDependency]]
    cell_components: dict[str, list[dict[str, str]]]


def component_dirty_cells(
    base_documents: dict[tuple[int, int], dict[str, Any]],
    current_components: dict[str, list[dict[str, str]]],
    grid_identifier: str,
) -> set[tuple[int, int]]:
    return {
        coordinate
        for coordinate, document in base_documents.items()
        if document["terrain_component_dependencies"]
        != current_components[cell_id(grid_identifier, *coordinate)]
    }


def bilinear_source_weights(
    transform: list[float], size: list[int], x_value: float, y_value: float
) -> dict[tuple[int, int], float]:
    if transform[2] != 0.0 or transform[4] != 0.0:
        raise CompilerError("raster_rotation_unsupported", "Bilinear dependency raster is rotated")
    pixel_x = (x_value - transform[0]) / transform[1]
    pixel_y = (y_value - transform[3]) / transform[5]
    base_x = math.floor(pixel_x - 0.5)
    base_y = math.floor(pixel_y - 0.5)
    delta_x = pixel_x - 0.5 - base_x
    delta_y = pixel_y - 0.5 - base_y
    weighted_x = ((base_x, 1.0 - delta_x), (base_x + 1, delta_x))
    weighted_y = ((base_y, 1.0 - delta_y), (base_y + 1, delta_y))
    epsilon = 1e-12
    weights = {
        (column, row): x_weight * y_weight
        for column, x_weight in weighted_x
        for row, y_weight in weighted_y
        if x_weight > epsilon and y_weight > epsilon
        and 0 <= column < int(size[0]) and 0 <= row < int(size[1])
    }
    if not weights:
        raise CompilerError("canonical_grid_outside_raster", "Canonical sample has no source pixels")
    total = sum(weights.values())
    return {pixel: weight / total for pixel, weight in weights.items()}


def _component_pixels(
    raster: dict[str, Any], mosaic_pixel: tuple[int, int]
) -> set[PixelDependency]:
    column, row = mosaic_pixel
    transform = raster["transform"]
    center_x = transform[0] + (column + 0.5) * transform[1]
    center_y = transform[3] + (row + 0.5) * transform[5]
    dependencies: set[PixelDependency] = set()
    for component in raster.get("components", []):
        component_transform = component["transform"]
        component_column = (center_x - component_transform[0]) / component_transform[1] - 0.5
        component_row = (center_y - component_transform[3]) / component_transform[5] - 0.5
        rounded_column = round(component_column)
        rounded_row = round(component_row)
        if (
            abs(component_column - rounded_column) <= 1e-7
            and abs(component_row - rounded_row) <= 1e-7
            and 0 <= rounded_column < int(component["size"][0])
            and 0 <= rounded_row < int(component["size"][1])
        ):
            dependencies.add((component["component_id"], rounded_column, rounded_row))
    if raster.get("components") and not dependencies:
        raise CompilerError("raster_component_gap", "Mosaic pixel has no component authority")
    return dependencies


def _sample_indices(
    grid: dict[str, Any], target_cells: list[tuple[int, int]]
) -> tuple[list[tuple[int, int]], dict[tuple[int, int], set[tuple[int, int]]]]:
    quads_x, quads_y = (int(value) for value in grid["cell_quads"])
    halo = int(grid["halo_samples"])
    per_cell = {}
    all_samples = set()
    for coordinate in target_cells:
        cell_x, cell_y = coordinate
        samples = {
            (x_index, y_index)
            for x_index in range(cell_x * quads_x - halo, (cell_x + 1) * quads_x + halo + 1)
            for y_index in range(cell_y * quads_y - halo, (cell_y + 1) * quads_y + halo + 1)
        }
        per_cell[coordinate] = samples
        all_samples.update(samples)
    return sorted(all_samples), per_cell


def derive_raster_dependencies(
    repo_root: Path,
    raster: dict[str, Any],
    grid_identifier: str,
    grid: dict[str, Any],
    target_cells: list[tuple[int, int]],
) -> RasterDependencyIndex:
    if not raster.get("components"):
        empty_pixels = {cell_id(grid_identifier, *item): frozenset() for item in target_cells}
        return RasterDependencyIndex({}, empty_pixels, {key: [] for key in empty_pixels})
    if grid.get("resampling_scale") != [1.0, 1.0]:
        raise CompilerError(
            "raster_dependency_scale_unfrozen",
            "Multi-raster bilinear dependencies require frozen unit GDAL warp scale",
        )
    sample_indices, per_cell_samples = _sample_indices(grid, target_cells)
    origin_x, origin_y = (float(value) for value in grid["origin"])
    spacing_x, spacing_y = (float(value) for value in grid["sample_spacing"])
    canonical_points = [
        [origin_x + x_index * spacing_x, origin_y + y_index * spacing_y]
        for x_index, y_index in sample_indices
    ]
    try:
        source_points = (
            canonical_points
            if grid["canonical_crs"] == raster["crs"]["horizontal"]
            else transform_points(repo_root, canonical_points, grid["canonical_crs"], raster["crs"]["horizontal"])
        )
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error), **error.details) from error
    sample_pixels = {}
    for sample, point in zip(sample_indices, source_points):
        mosaic_pixels = bilinear_source_weights(
            raster["transform"], raster["size"], point[0], point[1]
        )
        sample_pixels[sample] = frozenset(
            dependency
            for mosaic_pixel in mosaic_pixels
            for dependency in _component_pixels(raster, mosaic_pixel)
        )
    component_records = {
        component["component_id"]: {
            "component_id": component["component_id"],
            "sample_semantic_sha256": component["sample_semantic_sha256"],
        }
        for component in raster["components"]
    }
    cell_pixels = {}
    cell_components = {}
    for coordinate, samples in per_cell_samples.items():
        identifier = cell_id(grid_identifier, *coordinate)
        dependencies = frozenset(
            dependency for sample in samples for dependency in sample_pixels[sample]
        )
        cell_pixels[identifier] = dependencies
        cell_components[identifier] = [
            component_records[component_id]
            for component_id in sorted({dependency[0] for dependency in dependencies})
        ]
    return RasterDependencyIndex(sample_pixels, cell_pixels, cell_components)
