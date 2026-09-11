from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_ROOT))

from World.CanonicalCompilation.app.geometry import clip_line, geometry_rejection_reason
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash
from World.CanonicalCompilation.app.membership import feature_cell_membership
from World.CanonicalCompilation.app.pipeline import load_profile, profile_path
from World.CanonicalCompilation.app.projection import project_features, select_source_features
from World.CanonicalCompilation.app.terrain import _warped_values
from World.CanonicalCompilation.app.validation import _feature_boundary_details
from World.ExecutionEnvironment.api import require_tools, run_tool


def _normalized_lines(geometry: dict | None) -> list[list[list[float]]]:
    if geometry is None:
        return []
    coordinates = geometry["coordinates"]
    lines = coordinates if geometry["type"] == "MultiLineString" else [coordinates]
    normalized = [
        [[round(float(value), 2) for value in point] for point in line]
        for line in lines
    ]
    return sorted(min(line, list(reversed(line))) for line in normalized)


class GeometryAuthorityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        require_tools(REPO_ROOT)

    def test_custom_road_clipping_matches_pinned_ogr_at_cell_edges(self) -> None:
        cases = {
            "crossing": [[-5.0, 5.0], [15.0, 5.0]],
            "boundary": [[-5.0, 0.0], [15.0, 0.0]],
            "touch": [[-5.0, -5.0], [0.0, 0.0]],
            "bent": [[-5.0, 5.0], [5.0, 5.0], [15.0, 5.0]],
        }
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            source = root / "lines.geojson"
            output = root / "clipped.geojson"
            source.write_text(json.dumps({
                "type": "FeatureCollection",
                "features": [
                    {
                        "type": "Feature",
                        "properties": {"case_id": case_id},
                        "geometry": {"type": "LineString", "coordinates": coordinates},
                    }
                    for case_id, coordinates in cases.items()
                ],
            }), encoding="utf-8")
            run_tool(REPO_ROOT, "ogr2ogr", [
                "-overwrite", "-f", "GeoJSON", "-clipsrc", "0", "0", "10", "10",
                str(output), str(source),
            ])
            authoritative = {
                item["properties"]["case_id"]: _normalized_lines(item["geometry"])
                for item in json.loads(output.read_text(encoding="utf-8"))["features"]
            }
        for case_id, coordinates in cases.items():
            self.assertEqual(
                authoritative.get(case_id, []),
                sorted(
                    min(line, list(reversed(line)))
                    for line in clip_line(coordinates, (0.0, 0.0, 10.0, 10.0), 0.01)
                ),
                case_id,
            )

    def test_aligned_vrt_pixel_window_matches_full_warp(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            xyz = root / "terrain.xyz"
            source = root / "terrain.tif"
            xyz.write_text("\n".join(
                f"{x} {y} {x + y * 10}"
                for y in range(3, -3, -1)
                for x in range(-2, 6)
            ), encoding="ascii")
            run_tool(REPO_ROOT, "gdal_translate", [
                "-q", "-a_srs", "EPSG:4326", "-of", "GTiff", str(xyz), str(source)
            ])
            bundle = SimpleNamespace(
                raster_path=root / "raster_contract.json",
                raster={
                    "crs": {"horizontal": "EPSG:4326"},
                    "data_artifact": {
                        "path": source.name,
                        "sha256": file_hash(source),
                        "byte_size": source.stat().st_size,
                    },
                },
            )
            grid = {
                "canonical_crs": "EPSG:4326",
                "origin": [0.0, 0.0],
                "sample_spacing": [1.0, 1.0],
                "resampling_method": "bilinear",
            }
            alignment = (-1, -1, 4, 1)
            window = (2, -1, 4, 1)
            full = _warped_values(REPO_ROOT, bundle, grid, alignment, alignment, root / "full")
            partial = _warped_values(REPO_ROOT, bundle, grid, window, alignment, root / "partial")
        self.assertEqual(
            {key: value for key, value in full.items() if key in partial},
            partial,
        )

    def test_pinned_ogr_validates_multipolygon_holes_and_invalid_polygons(self) -> None:
        valid = {
            "provider_feature_id": "way/valid-hole",
            "geometry": {
                "type": "MultiPolygon",
                "coordinates": [[
                    [[48.90, 55.70], [48.92, 55.70], [48.92, 55.72], [48.90, 55.72], [48.90, 55.70]],
                    [[48.905, 55.705], [48.905, 55.715], [48.915, 55.715], [48.915, 55.705], [48.905, 55.705]],
                ]],
            },
        }
        invalid = {
            "provider_feature_id": "way/bowtie",
            "geometry": {
                "type": "Polygon",
                "coordinates": [[
                    [48.90, 55.70], [48.92, 55.72], [48.92, 55.70], [48.90, 55.72], [48.90, 55.70]
                ]],
            },
        }
        profile = load_profile(profile_path(
            "Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_p0.compile.json"
        ))
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            projected, rejected = project_features(
                REPO_ROOT, [valid, invalid], profile["grid"], Path(directory)
            )
        self.assertEqual({"way/valid-hole"}, set(projected))
        self.assertEqual({"way/bowtie"}, rejected)
        self.assertIsNone(geometry_rejection_reason(projected["way/valid-hole"], "building"))
        self.assertEqual("polygon_zero_area", geometry_rejection_reason(invalid["geometry"], "building"))

    def test_pinned_ogr_uses_exact_geometry_membership_not_bounding_boxes(self) -> None:
        geometries = {
            "concave": {
                "type": "Polygon",
                "coordinates": [[
                    [0.1, 0.1], [1.9, 0.1], [1.9, 0.9], [0.9, 0.9],
                    [0.9, 1.9], [0.1, 1.9], [0.1, 0.1],
                ]],
            },
            "multipolygon": {
                "type": "MultiPolygon",
                "coordinates": [
                    [[[0.1, 0.1], [0.4, 0.1], [0.4, 0.4], [0.1, 0.4], [0.1, 0.1]]],
                    [[[2.1, 1.1], [2.4, 1.1], [2.4, 1.4], [2.1, 1.4], [2.1, 1.1]]],
                ],
            },
            "diagonal": {"type": "LineString", "coordinates": [[0.1, 0.1], [2.9, 1.9]]},
            "boundary": {"type": "LineString", "coordinates": [[1.0, 0.1], [1.0, 0.9]]},
            "corner": {
                "type": "Polygon",
                "coordinates": [[[1.0, 1.0], [1.2, 1.1], [1.1, 1.2], [1.0, 1.0]]],
            },
        }
        grid = {
            "coordinate_transform": "industrial_test",
            "canonical_crs": "EPSG:4326",
            "origin": [0.0, 0.0],
            "sample_spacing": [1.0, 1.0],
            "cell_quads": [1, 1],
            "coordinate_quantization": 0.01,
        }
        targets = [(x_value, y_value) for x_value in range(3) for y_value in range(2)]
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            memberships = feature_cell_membership(
                REPO_ROOT, geometries, grid, targets, Path(directory)
            )
        self.assertNotIn((1, 1), memberships["concave"])
        self.assertEqual([(0, 0), (2, 1)], memberships["multipolygon"])
        self.assertNotIn((0, 1), memberships["diagonal"])
        self.assertNotIn((2, 0), memberships["diagonal"])
        self.assertEqual([(0, 0), (1, 0)], memberships["boundary"])
        self.assertEqual([(0, 0), (0, 1), (1, 0), (1, 1)], memberships["corner"])

    def test_pinned_ogr_selects_only_features_near_new_cells(self) -> None:
        features = [
            {
                "provider_feature_id": "way/inside",
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[2.1, 0.1], [2.4, 0.1], [2.4, 0.4], [2.1, 0.4], [2.1, 0.1]]],
                },
            },
            {
                "provider_feature_id": "way/outside",
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[0.1, 0.1], [0.4, 0.1], [0.4, 0.4], [0.1, 0.4], [0.1, 0.1]]],
                },
            },
        ]
        grid = {
            "coordinate_transform": "industrial_test",
            "canonical_crs": "EPSG:4326",
            "origin": [0.0, 0.0],
            "sample_spacing": [1.0, 1.0],
            "cell_quads": [1, 1],
            "coordinate_quantization": 0.01,
        }
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            selected = select_source_features(
                REPO_ROOT, features, grid, {(2, 0)}, Path(directory)
            )
        self.assertEqual(["way/inside"], [item["provider_feature_id"] for item in selected])

    def test_line_and_polygon_water_have_exact_shared_cell_seams(self) -> None:
        grid_identifier = "fixture-grid"
        grid = {
            "origin": [-1000.0, 0.0],
            "sample_spacing": [500.0, 500.0],
            "cell_quads": [2, 2],
            "coordinate_quantization": 0.01,
            "target_cells": [{"x": 0, "y": 0}, {"x": 1, "y": 0}],
        }
        west = f"{grid_identifier}:x0:y0"
        east = f"{grid_identifier}:x1:y0"
        features = [
            {
                "feature_id": "water-line",
                "feature_class": "water",
                "geometry": {"type": "LineString", "coordinates": [[-500.0, 300.0], [500.0, 300.0]]},
                "intersecting_cell_ids": [west, east],
                "representations": [
                    {"cell_id": west, "geometry": {"type": "LineString", "coordinates": [[-500.0, 300.0], [0.0, 300.0]]}},
                    {"cell_id": east, "geometry": {"type": "LineString", "coordinates": [[0.0, 300.0], [500.0, 300.0]]}},
                ],
            },
            {
                "feature_id": "water-polygon",
                "feature_class": "water",
                "geometry": {"type": "Polygon", "coordinates": [[[-200.0, 100.0], [200.0, 100.0], [200.0, 400.0], [-200.0, 400.0], [-200.0, 100.0]]]},
                "intersecting_cell_ids": [west, east],
                "representations": [
                    {"cell_id": west, "representation": "authoritative_water"},
                    {"cell_id": east, "representation": "water_reference"},
                ],
            },
        ]
        details = _feature_boundary_details(features, grid_identifier, grid)
        self.assertEqual(2, details["water_seams"])
        self.assertEqual(1, details["water_line_seams"])
        self.assertEqual(1, details["water_polygon_seams"])

    def test_polygon_suppressed_river_tails_need_no_false_shared_seam(self) -> None:
        grid_identifier = "fixture-grid"
        grid = {
            "origin": [-1000.0, 0.0],
            "sample_spacing": [500.0, 500.0],
            "cell_quads": [2, 2],
            "coordinate_quantization": 0.01,
            "target_cells": [{"x": 0, "y": 0}, {"x": 1, "y": 0}],
        }
        west = f"{grid_identifier}:x0:y0"
        east = f"{grid_identifier}:x1:y0"
        feature = {
            "feature_id": "grouped-river-axis",
            "feature_class": "water",
            "geometry": {
                "type": "MultiLineString",
                "coordinates": [[[-500.0, 300.0], [-100.0, 300.0]], [[100.0, 300.0], [500.0, 300.0]]],
            },
            "attributes": {"surface_geometry": "ribbon", "polygon_overlap_area_m2": 0.0},
            "intersecting_cell_ids": [west, east],
            "representations": [
                {"cell_id": west, "geometry": {"type": "LineString", "coordinates": [[-500.0, 300.0], [-100.0, 300.0]]}},
                {"cell_id": east, "geometry": {"type": "LineString", "coordinates": [[100.0, 300.0], [500.0, 300.0]]}},
            ],
        }
        details = _feature_boundary_details([feature], grid_identifier, grid)
        self.assertEqual(0, details["water_line_seams"])

    def test_disconnected_cross_cell_road_still_fails_closed(self) -> None:
        grid_identifier = "fixture-grid"
        grid = {
            "origin": [-1000.0, 0.0],
            "sample_spacing": [500.0, 500.0],
            "cell_quads": [2, 2],
            "coordinate_quantization": 0.01,
            "target_cells": [{"x": 0, "y": 0}, {"x": 1, "y": 0}],
        }
        west = f"{grid_identifier}:x0:y0"
        east = f"{grid_identifier}:x1:y0"
        feature = {
            "feature_id": "broken-road",
            "feature_class": "road",
            "geometry": {
                "type": "MultiLineString",
                "coordinates": [[[-500.0, 300.0], [-100.0, 300.0]], [[100.0, 300.0], [500.0, 300.0]]],
            },
            "intersecting_cell_ids": [west, east],
            "representations": [
                {"cell_id": west, "geometry": {"type": "LineString", "coordinates": [[-500.0, 300.0], [-100.0, 300.0]]}},
                {"cell_id": east, "geometry": {"type": "LineString", "coordinates": [[100.0, 300.0], [500.0, 300.0]]}},
            ],
        }
        with self.assertRaises(CompilerError) as raised:
            _feature_boundary_details([feature], grid_identifier, grid)
        self.assertEqual("road_seam_mismatch", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
