from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
WORLD_ROOT = REPO_ROOT / "tools" / "World"
SOURCE_ROOT = WORLD_ROOT / "SourceIngestion"
TEST_FIXTURE_ROOT = (
    REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data" / "Fixtures"
)
sys.path.insert(0, str(TOOLS_ROOT))

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool
from World.SourceIngestion.app.adapters import _convert_osmium_geojson
from World.SourceIngestion.app.contracts import IngestionError, file_hash, read_json
from World.SourceIngestion.app.osm_selection import select_complete_geometries
from World.SourceIngestion.app.raster_mosaic import decode_raster_mosaic


class SourceIngestionIntegrationTests(unittest.TestCase):
    def setUp(self) -> None:
        try:
            require_tools(REPO_ROOT)
        except ExecutionEnvironmentError as error:
            self.skipTest(str(error))

    def _write_cog(self, root: Path, name: str, bounds: list[float], values: list[float]) -> Path:
        west, south, east, north = bounds
        xyz = root / f"{name}.xyz"
        cog = root / f"{name}.tif"
        rows = [
            f"{west + 0.5} {north - 0.5} {values[0]}",
            f"{west + 1.5} {north - 0.5} {values[1]}",
            f"{west + 0.5} {north - 1.5} {values[2]}",
            f"{west + 1.5} {north - 1.5} {values[3]}",
        ]
        xyz.write_text("\n".join(rows) + "\n", encoding="ascii")
        run_tool(REPO_ROOT, "gdal_translate", [
            "-q", "-a_srs", "EPSG:4326", "-a_ullr", str(west), str(north), str(east), str(south),
            "-mo", "AREA_OR_POINT=Point", "-ot", "Float32", "-of", "COG", str(xyz), str(cog),
        ])
        return cog

    def _mosaic_contract(self, paths: list[Path], bounds: list[list[float]]) -> tuple[dict, dict, dict[str, Path]]:
        sources = []
        snapshots = []
        source_paths = {}
        for index, (path, coverage) in enumerate(zip(paths, bounds)):
            source_id = f"component_{index}"
            digest = file_hash(path)
            sources.append({
                "source_id": source_id,
                "adapter": "copernicus_dem",
                "provider": "fixture",
                "dataset": "fixture-dem",
                "release": "1",
                "coverage_bbox": coverage,
                "crs": {"horizontal": "EPSG:4326", "axis_order": "longitude_latitude", "vertical": "EPSG:3855", "vertical_unit": "metre"},
                "accuracy": {"horizontal_accuracy_m": 0.0, "vertical_accuracy_m": 0.0, "confidence": "fixture"},
                "raster_class": "DSM",
                "license": {"id": "CC0-1.0"},
            })
            snapshots.append({
                "source_id": source_id,
                "snapshot_id": f"{source_id}:{digest[:16]}",
                "hashes": {"sha256": digest},
                "accuracy": sources[-1]["accuracy"],
            })
            source_paths[source_id] = path
        profile = {
            "sources": sources,
            "raster_mosaic": {
                "contract_version": 1,
                "component_order": ["coverage_west_south_east_north", "source_id", "snapshot_id"],
                "resolution_policy": "same",
                "overlap_policy": "require_equal_float32_samples",
                "coverage_policy": "vrt_alpha_complete",
                "normalization": "pixel_aligned_clip_no_resample",
                "artifact_format": "COG",
                "creation_options": {"compression": "DEFLATE", "predictor": "YES", "block_size": 512, "threads": 1},
            },
        }
        return profile, {"snapshots": snapshots}, source_paths

    def test_complete_geometry_selection_keeps_crossing_objects(self) -> None:
        fixture = TEST_FIXTURE_ROOT / "Provider" / "synthetic_multipolygon" / "crossing_building.osm"
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            output = Path(directory)
            _, geojson, evidence = select_complete_geometries(
                REPO_ROOT,
                fixture,
                ["w/highway", "r/building"],
                [-0.1, -0.1, 0.1, 0.1],
                output,
            )
            collection = _convert_osmium_geojson(
                geojson,
                {"snapshot_id": "synthetic_multipolygon"},
                {"provider": "ALIS", "release": "1", "license": {"id": "CC0-1.0"}},
                {"area_id": "synthetic_multipolygon", "area_fingerprint": "sha256:" + "0" * 64},
            )
        features = {item["provider_feature_id"]: item for item in collection["features"]}
        self.assertEqual("building", features["relation/300"]["provider_class"])
        self.assertEqual(2, len(features["relation/300"]["geometry"]["coordinates"][0]))
        self.assertEqual("highway", features["way/200"]["provider_class"])
        self.assertEqual("highway", features["way/201"]["provider_class"])
        self.assertEqual(64, len(evidence["membership_contract_id"]))

    def test_multi_raster_mosaic_is_coverage_complete_and_deterministic(self) -> None:
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            root = Path(directory)
            left = self._write_cog(root, "left", [0.0, 0.0, 2.0, 2.0], [1, 2, 3, 4])
            right = self._write_cog(root, "right", [2.0, 0.0, 4.0, 2.0], [5, 6, 7, 8])
            profile, ledger, paths = self._mosaic_contract(
                [right, left], [[2.0, 0.0, 4.0, 2.0], [0.0, 0.0, 2.0, 2.0]]
            )
            area = {"area_id": "fixture", "label": "fixture", "bbox": [0.0, 0.0, 4.0, 2.0], "axis_order": "longitude_latitude", "crs": "EPSG:4326", "area_fingerprint": "sha256:" + "0" * 64}
            outputs = []
            for name in ("first", "second"):
                output = root / name
                decode_raster_mosaic(profile, paths, ledger, area, output, REPO_ROOT)
                outputs.append(output)
            first = read_json(outputs[0] / "raster" / "mosaic.json")
            second = read_json(outputs[1] / "raster" / "mosaic.json")
            validation = read_json(outputs[0] / "raster" / "mosaic_validation.json")
        self.assertEqual(first, second)
        self.assertEqual(["component_1", "component_0"], [item["source_id"] for item in first["components"]])
        self.assertEqual(0, validation["uncovered_pixels"])
        self.assertEqual(8, validation["coverage_pixels_checked"])

    def test_multi_raster_mosaic_rejects_actual_alpha_gap(self) -> None:
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            root = Path(directory)
            left = self._write_cog(root, "left", [0.0, 0.0, 2.0, 2.0], [0, 0, 0, 0])
            right = self._write_cog(root, "right", [3.0, 0.0, 5.0, 2.0], [0, 0, 0, 0])
            profile, ledger, paths = self._mosaic_contract(
                [left, right], [[0.0, 0.0, 2.0, 2.0], [3.0, 0.0, 5.0, 2.0]]
            )
            area = {"area_id": "fixture", "label": "fixture", "bbox": [0.0, 0.0, 5.0, 2.0], "axis_order": "longitude_latitude", "crs": "EPSG:4326", "area_fingerprint": "sha256:" + "0" * 64}
            with self.assertRaises(IngestionError) as context:
                decode_raster_mosaic(profile, paths, ledger, area, root / "output", REPO_ROOT)
        self.assertEqual("raster_coverage_gap", context.exception.code)

    def test_multi_raster_mosaic_rejects_half_pixel_grid_shift(self) -> None:
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            root = Path(directory)
            left = self._write_cog(root, "left", [0.0, 0.0, 2.0, 2.0], [1, 2, 3, 4])
            shifted = self._write_cog(root, "shifted", [2.5, 0.0, 4.5, 2.0], [5, 6, 7, 8])
            profile, ledger, paths = self._mosaic_contract(
                [left, shifted], [[0.0, 0.0, 2.0, 2.0], [2.5, 0.0, 4.5, 2.0]]
            )
            area = {"area_id": "fixture", "label": "fixture", "bbox": [0.0, 0.0, 4.5, 2.0], "axis_order": "longitude_latitude", "crs": "EPSG:4326", "area_fingerprint": "sha256:" + "0" * 64}
            with self.assertRaises(IngestionError) as context:
                decode_raster_mosaic(profile, paths, ledger, area, root / "output", REPO_ROOT)
        self.assertEqual("raster_component_conflict", context.exception.code)

    def test_multi_raster_mosaic_rejects_conflicting_overlap(self) -> None:
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            root = Path(directory)
            left = self._write_cog(root, "left", [0.0, 0.0, 2.0, 2.0], [1, 2, 3, 4])
            right = self._write_cog(root, "right", [1.0, 0.0, 3.0, 2.0], [9, 6, 9, 8])
            profile, ledger, paths = self._mosaic_contract(
                [left, right], [[0.0, 0.0, 2.0, 2.0], [1.0, 0.0, 3.0, 2.0]]
            )
            area = {"area_id": "fixture", "label": "fixture", "bbox": [0.0, 0.0, 3.0, 2.0], "axis_order": "longitude_latitude", "crs": "EPSG:4326", "area_fingerprint": "sha256:" + "0" * 64}
            with self.assertRaises(IngestionError) as context:
                decode_raster_mosaic(profile, paths, ledger, area, root / "output", REPO_ROOT)
        self.assertEqual("raster_overlap_conflict", context.exception.code)


if __name__ == "__main__":
    unittest.main()
