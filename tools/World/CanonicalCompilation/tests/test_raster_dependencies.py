from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_ROOT))

from World.CanonicalCompilation.app.contracts import file_hash
from World.CanonicalCompilation.app.raster_dependencies import (
    bilinear_source_weights,
    component_dirty_cells,
    derive_raster_dependencies,
)
from World.CanonicalCompilation.app.source import SourceBundle
from World.CanonicalCompilation.app.spatial import cell_id
from World.CanonicalCompilation.app.terrain import build_terrain_cells
from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool, transform_points


LEFT_ID = "1" * 64
RIGHT_ID = "2" * 64


class RasterDependencyTests(unittest.TestCase):
    def setUp(self) -> None:
        try:
            require_tools(REPO_ROOT)
        except ExecutionEnvironmentError as error:
            self.skipTest(str(error))

    def _write_component(
        self, root: Path, name: str, west: float, values: list[list[float]]
    ) -> Path:
        xyz = root / f"{name}.xyz"
        cog = root / f"{name}.tif"
        xyz.write_text("".join(
            f"{west + column + 0.5} {3.5 - row} {value}\n"
            for row, row_values in enumerate(values)
            for column, value in enumerate(row_values)
        ), encoding="ascii")
        run_tool(REPO_ROOT, "gdal_translate", [
            "-q", "-a_srs", "EPSG:4326", "-a_ullr", str(west), "4", str(west + 4), "0",
            "-mo", "AREA_OR_POINT=Point", "-ot", "Float32", "-of", "COG", str(xyz), str(cog),
        ])
        return cog

    def _mosaic(
        self, root: Path, left_values: list[list[float]], right_values: list[list[float]]
    ) -> tuple[Path, dict]:
        root.mkdir(parents=True, exist_ok=True)
        left = self._write_component(root, "left", 0.0, left_values)
        right = self._write_component(root, "right", 4.0, right_values)
        vrt = root / "mosaic.vrt"
        cog = root / "mosaic.tif"
        run_tool(REPO_ROOT, "gdalbuildvrt", [
            "-q", "-strict", "-resolution", "same", "-addalpha", "-te", "0", "0", "8", "4",
            str(vrt), str(left), str(right),
        ])
        run_tool(REPO_ROOT, "gdal_translate", [
            "-q", "-b", "1", "-of", "COG", str(vrt), str(cog),
        ])
        raster = {
            "crs": {"horizontal": "EPSG:4326"},
            "vertical_datum": {"id": "EPSG:3855", "unit": "metre"},
            "vertical_provenance": {
                "source_ref": "fixture-mosaic",
                "source_accuracy_m": 0.0,
                "confidence": "fixture",
            },
            "size": [8, 4],
            "transform": [0.0, 1.0, 0.0, 4.0, 0.0, -1.0],
            "nodata": None,
            "data_artifact": {
                "path": cog.name,
                "byte_size": cog.stat().st_size,
                "sha256": file_hash(cog),
            },
            "components": [
                {
                    "component_id": LEFT_ID,
                    "sample_semantic_sha256": "3" * 64,
                    "size": [4, 4],
                    "transform": [0.0, 1.0, 0.0, 4.0, 0.0, -1.0],
                },
                {
                    "component_id": RIGHT_ID,
                    "sample_semantic_sha256": "4" * 64,
                    "size": [4, 4],
                    "transform": [4.0, 1.0, 0.0, 4.0, 0.0, -1.0],
                },
            ],
        }
        return cog, raster

    def _bundle(self, root: Path, raster: dict) -> SourceBundle:
        raster_path = root / "mosaic.json"
        raster_path.write_text("{}\n", encoding="ascii")
        return SourceBundle(
            root, root / "result.json", {}, {}, {}, {}, "fixture", [], raster_path, raster
        )

    def _grid(self) -> dict:
        return {
            "canonical_crs": "EPSG:4326",
            "vertical_datum": "EPSG:3855",
            "coordinate_transform": "fixture_identity_gdal",
            "origin": [0.25, 0.25],
            "sample_spacing": [0.5, 0.5],
            "height_quantization": 0.001,
            "resampling_method": "bilinear",
            "resampling_scale": [1.0, 1.0],
            "cell_quads": [2, 2],
            "alignment_cell_bounds": [1, 1, 5, 1],
            "halo_samples": 1,
        }

    def _actual_sample(
        self,
        source: Path,
        root: Path,
        x_value: float,
        y_value: float,
        source_crs: str = "EPSG:4326",
        target_crs: str = "EPSG:4326",
        pixel_size: float = 0.5,
    ) -> float:
        vrt = root / f"sample_{x_value}_{y_value}.vrt"
        run_tool(REPO_ROOT, "gdalwarp", [
            "-overwrite", "-q", "-s_srs", source_crs, "-t_srs", target_crs, "-te_srs", target_crs,
            "-te", str(x_value - pixel_size / 2), str(y_value - pixel_size / 2),
            str(x_value + pixel_size / 2), str(y_value + pixel_size / 2),
            "-ts", "1", "1", "-r", "bilinear", "-ot", "Float64", "-et", "0",
            "-wo", "NUM_THREADS=1", "-wo", "XSCALE=1.0", "-wo", "YSCALE=1.0",
            "-of", "VRT", str(source), str(vrt),
        ])
        xyz = run_tool(REPO_ROOT, "gdal_translate", ["-q", "-of", "XYZ", str(vrt), "/vsistdout/"])
        return float(xyz.split()[2])

    def _production_mosaic(self, root: Path) -> tuple[Path, dict, list[list[float]]]:
        root.mkdir(parents=True, exist_ok=True)
        paths = []
        component_records = []
        matrix_rows = [[] for _ in range(8)]
        for index, (name, west, identity, semantic) in enumerate((
            ("west", 48.996, LEFT_ID, "3" * 64),
            ("east", 49.0, RIGHT_ID, "4" * 64),
        )):
            xyz = root / f"{name}.xyz"
            cog = root / f"{name}.tif"
            values = [
                [1000.0 * index + row * 100.0 + column * 10.0 for column in range(4)]
                for row in range(8)
            ]
            xyz.write_text("".join(
                f"{west + (column + 0.5) * 0.001} {55.804 - (row + 0.5) * 0.001} {value}\n"
                for row, row_values in enumerate(values)
                for column, value in enumerate(row_values)
            ), encoding="ascii")
            run_tool(REPO_ROOT, "gdal_translate", [
                "-q", "-a_srs", "EPSG:4326", "-a_ullr", str(west), "55.804",
                str(west + 0.004), "55.796", "-mo", "AREA_OR_POINT=Point",
                "-ot", "Float32", "-of", "COG", str(xyz), str(cog),
            ])
            paths.append(cog)
            component_records.append({
                "component_id": identity,
                "sample_semantic_sha256": semantic,
                "size": [4, 8],
                "transform": [west, 0.001, 0.0, 55.804, 0.0, -0.001],
            })
            for row, values_row in enumerate(values):
                matrix_rows[row].extend(values_row)
        vrt = root / "mosaic.vrt"
        cog = root / "mosaic.tif"
        run_tool(REPO_ROOT, "gdalbuildvrt", [
            "-q", "-strict", "-resolution", "same", "-addalpha", "-te",
            "48.996", "55.796", "49.004", "55.804", str(vrt), *(str(path) for path in paths),
        ])
        run_tool(REPO_ROOT, "gdal_translate", ["-q", "-b", "1", "-of", "COG", str(vrt), str(cog)])
        return cog, {
            "crs": {"horizontal": "EPSG:4326"},
            "size": [8, 8],
            "transform": [48.996, 0.001, 0.0, 55.804, 0.0, -0.001],
            "components": component_records,
        }, matrix_rows

    def test_gdal_bilinear_dependencies_and_component_dirty_cells_match(self) -> None:
        parent = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "tests"
        parent.mkdir(parents=True, exist_ok=True)
        left = [[10 + row * 40 + column * 7 for column in range(4)] for row in range(4)]
        right = [[300 + row * 50 + column * 11 for column in range(4)] for row in range(4)]
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            root = Path(directory)
            baseline_cog, baseline_raster = self._mosaic(root / "baseline", left, right)
            cases = {
                "left_only": (2.25, 2.25),
                "right_only": (5.25, 2.25),
                "seam_west": (3.75, 2.25),
                "seam_east": (4.25, 2.25),
                "edge_corner": (0.25, 0.25),
            }
            matrix = [left_row + right_row for left_row, right_row in zip(left, right)]
            for label, (x_value, y_value) in cases.items():
                weights = bilinear_source_weights(
                    baseline_raster["transform"], baseline_raster["size"], x_value, y_value
                )
                expected = sum(matrix[row][column] * weight for (column, row), weight in weights.items())
                self.assertAlmostEqual(
                    expected, self._actual_sample(baseline_cog, root, x_value, y_value), places=6, msg=label
                )
            self.assertTrue(
                {column for column, _ in bilinear_source_weights(
                    baseline_raster["transform"], baseline_raster["size"], 3.75, 2.25
                )} == {3, 4}
            )
            self.assertTrue(
                {column for column, _ in bilinear_source_weights(
                    baseline_raster["transform"], baseline_raster["size"], 4.25, 2.25
                )} == {3, 4}
            )

            targets = [(value, 1) for value in range(1, 6)]
            grid = self._grid()
            identifier = "fixture-grid"
            dependency_index = derive_raster_dependencies(
                REPO_ROOT, baseline_raster, identifier, grid, targets
            )
            seam_id = cell_id(identifier, 3, 1)
            right_id = cell_id(identifier, 5, 1)
            self.assertEqual({LEFT_ID, RIGHT_ID}, {
                item["component_id"] for item in dependency_index.cell_components[seam_id]
            })
            self.assertEqual({RIGHT_ID}, {
                item["component_id"] for item in dependency_index.cell_components[right_id]
            })

            overlay = {"terrain_patches": []}
            baseline = build_terrain_cells(
                REPO_ROOT, self._bundle(root / "baseline", baseline_raster), identifier,
                grid, targets, overlay, root / "scratch_baseline",
            )
            changed_left = [row[:] for row in left]
            changed_left[2][2] += 1000
            _, changed_raster = self._mosaic(root / "changed", changed_left, right)
            changed_raster["components"][0]["component_id"] = "5" * 64
            changed_raster["components"][0]["sample_semantic_sha256"] = "6" * 64
            changed = build_terrain_cells(
                REPO_ROOT, self._bundle(root / "changed", changed_raster), identifier,
                grid, targets, overlay, root / "scratch_changed",
            )
            changed_cells = {
                current_id for current_id in baseline
                if baseline[current_id]["halo_window"]["samples"] != changed[current_id]["halo_window"]["samples"]
            }
            expected_dirty = {
                current_id for current_id, pixels in dependency_index.cell_pixels.items()
                if (LEFT_ID, 2, 2) in pixels
            }
            self.assertEqual(expected_dirty, changed_cells)
            self.assertNotIn(right_id, changed_cells)
            changed_dependencies = derive_raster_dependencies(
                REPO_ROOT, changed_raster, identifier, grid, targets
            )
            base_documents = {
                coordinate: {
                    "terrain_component_dependencies": dependency_index.cell_components[
                        cell_id(identifier, *coordinate)
                    ]
                }
                for coordinate in targets
            }
            selected_dirty = {
                cell_id(identifier, *coordinate)
                for coordinate in component_dirty_cells(
                    base_documents, changed_dependencies.cell_components, identifier
                )
            }
            expected_component_dirty = {
                current_id for current_id, pixels in dependency_index.cell_pixels.items()
                if any(component_id == LEFT_ID for component_id, _, _ in pixels)
            }
            self.assertEqual(expected_component_dirty, selected_dirty)
            self.assertTrue(changed_cells.issubset(selected_dirty))
            self.assertNotIn(right_id, selected_dirty)

    def test_gdal_bilinear_dependencies_match_production_crs_transform(self) -> None:
        parent = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            root = Path(directory)
            mosaic, raster, matrix = self._production_mosaic(root)
            seam = transform_points(REPO_ROOT, [[49.0, 55.8]], "EPSG:4326", "EPSG:32639")[0]
            edge = transform_points(REPO_ROOT, [[48.9965, 55.8035]], "EPSG:4326", "EPSG:32639")[0]
            cases = {
                "west_only": [seam[0] - 120.0, seam[1]],
                "seam_west": [seam[0] - 20.0, seam[1]],
                "seam_east": [seam[0] + 20.0, seam[1]],
                "east_only": [seam[0] + 120.0, seam[1]],
                "edge_corner": edge,
            }
            geographic = transform_points(
                REPO_ROOT, list(cases.values()), "EPSG:32639", "EPSG:4326"
            )
            dependencies = {}
            for (label, canonical), source_point in zip(cases.items(), geographic, strict=True):
                weights = bilinear_source_weights(
                    raster["transform"], raster["size"], source_point[0], source_point[1]
                )
                dependencies[label] = {column for column, _ in weights}
                expected = sum(
                    matrix[row][column] * weight
                    for (column, row), weight in weights.items()
                )
                self.assertAlmostEqual(
                    expected,
                    self._actual_sample(
                        mosaic, root, canonical[0], canonical[1],
                        target_crs="EPSG:32639", pixel_size=25.0,
                    ),
                    places=4,
                    msg=label,
                )
            self.assertTrue(all(column < 4 for column in dependencies["west_only"]))
            self.assertTrue(all(column >= 4 for column in dependencies["east_only"]))
            self.assertEqual({3, 4}, dependencies["seam_west"])
            self.assertEqual({3, 4}, dependencies["seam_east"])

            grid = {
                "canonical_crs": "EPSG:32639",
                "origin": [seam[0] - 25.0, seam[1] - 25.0],
                "sample_spacing": [25.0, 25.0],
                "resampling_scale": [1.0, 1.0],
                "cell_quads": [2, 2],
                "halo_samples": 1,
            }
            index = derive_raster_dependencies(REPO_ROOT, raster, "production-grid", grid, [(0, 0)])
            self.assertEqual(
                {LEFT_ID, RIGHT_ID},
                {item["component_id"] for item in index.cell_components["production-grid:x0:y0"]},
            )


if __name__ == "__main__":
    unittest.main()
