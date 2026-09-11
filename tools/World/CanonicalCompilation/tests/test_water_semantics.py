from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import CompilerError
from World.CanonicalCompilation.app.features import compile_features
from World.CanonicalCompilation.app.source import SourceBundle
from World.CanonicalCompilation.app.spatial import grid_id
from World.CanonicalCompilation.app.water import (
    expand_water_change_ids,
    fit_downstream_surface,
    linear_quantile,
    rolling_quantile,
)
from World.CanonicalCompilation.app.water_geometry import TerrainSampler, production_geometry_evidence


def _feature(source_id: str, geometry: dict, **properties: str) -> dict:
    return {
        "provider_feature_id": source_id,
        "provider_class": "water",
        "geometry": geometry,
        "properties": properties,
        "precision": {"coordinate_decimals": 7},
        "confidence": None,
    }


def _profile(grouped: bool = False) -> dict:
    return {
        "identity_namespace": "synthetic",
        "target_cells": [{"x": 0, "y": 0}, {"x": 1, "y": 0}],
        "grid": {
            "coordinate_transform": "fixture_affine",
            "canonical_crs": "ALIS:FIXTURE_METRE_V1",
            "fixture_scale_m": 1000.0,
            "origin": [0.0, 0.0],
            "sample_spacing": [1000.0, 1000.0],
            "cell_quads": [1, 1],
            "coordinate_quantization": 0.01,
            "height_quantization": 0.1,
        },
        "water_semantics": {
            "policy_id": "synthetic_water_v1",
            "policy_version": 1,
            "coordinate_match_tolerance_m": 0.01,
            "class_rules": [
                {"water_class": "lake", "geometry": "polygon", "result": "surface", "behavior": "standing"},
                {"water_class": "river", "geometry": "polygon", "result": "surface", "behavior": "flowing"},
                {"water_class": "river", "geometry": "line", "result": "surface", "behavior": "flowing"},
                {"water_class": "fairway", "geometry": "line", "result": "non_surface"},
            ],
            "modifier_rules": [
                {"tag": "tunnel", "value": "culvert", "result": "hidden"},
                {"tag": "intermittent", "value": "yes", "result": "temporal"},
                {"tag": "intermittent", "value": "no", "result": "visible"},
                {"tag": "seasonal", "value": "*", "result": "temporal"},
            ],
            "feature_rules": [],
            "surface_groups": ([
                {
                    "surface_group_id": "synthetic_wide_river",
                    "polygon_source_id": "relation/10",
                    "flow_axis_source_ids": ["way/11"],
                }
            ] if grouped else []),
            "width": {
                "fallback_accuracy": "heuristic_visual_not_surveyed",
                "fallback_m": {"river": 12.0, "canal": 6.0, "stream": 1.5, "ditch": 1.0, "drain": 1.0},
                "ribbon_buffer": {
                    "cap_style": "round",
                    "join_style": "round",
                    "quadrant_segments": 8,
                    "suppression_clearance_m": 0.02,
                },
            },
            "elevation": {
                "quantile_interpolation": "linear_r7",
                "rolling_endpoint_policy": "shrink_to_available_samples",
                "standing": {"function_id": "standing_polygon_quantile", "function_version": 1, "quantile": 0.5},
                "flowing": {"function_id": "rolling_quantile_l1_isotonic", "function_version": 1, "sample_spacing_m": 30.0, "quantile": 0.5, "rolling_radius": 2},
            },
        },
    }


def _bundle(features: list[dict]) -> SourceBundle:
    return SourceBundle(
        root=REPO_ROOT,
        result_path=REPO_ROOT / "synthetic_result.json",
        result={},
        source_profile={},
        source_run_contract={},
        ledger={
            "snapshots": [{
                "snapshot_id": "synthetic:water-v1",
                "provider": "synthetic",
                "release": "1",
                "crs": {"horizontal": "EPSG:4326"},
            }]
        },
        feature_snapshot_id="synthetic:water-v1",
        features=features,
        raster_path=REPO_ROOT / "synthetic_raster.json",
        raster={},
    )


def _compile(features: list[dict], profile: dict | None = None):
    selected_profile = profile or _profile()
    identifier = grid_id(selected_profile["grid"])
    terrain = {
        f"{identifier}:x0:y0": {
            "bounds": [0.0, 0.0, 1000.0, 1000.0],
            "sample_spacing": [1000.0, 1000.0],
            "core_samples": [[50.0, 49.8], [50.0, 49.8]],
            "halo_window": {
                "samples": [[50.2, 50.0, 49.8, 49.6]] * 4,
            },
        },
        f"{identifier}:x1:y0": {
            "bounds": [1000.0, 0.0, 2000.0, 1000.0],
            "sample_spacing": [1000.0, 1000.0],
            "core_samples": [[49.8, 49.6], [49.8, 49.6]],
            "halo_window": {
                "samples": [[50.0, 49.8, 49.6, 49.4]] * 4,
            },
        },
    }
    with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
        return compile_features(
            _bundle(features),
            selected_profile,
            identifier,
            {"overlay_id": "empty", "feature_overrides": []},
            repo_root=REPO_ROOT,
            scratch_root=Path(directory),
            terrain=terrain,
        )


class WaterSemanticsTests(unittest.TestCase):
    def test_group_member_change_expands_to_the_whole_surface_authority(self) -> None:
        profile = _profile(grouped=True)
        expected = ["relation/10", "way/11"]
        self.assertEqual(expected, expand_water_change_ids(profile, ["relation/10"]))
        self.assertEqual(expected, expand_water_change_ids(profile, ["way/11"]))
        self.assertEqual(["way/99"], expand_water_change_ids(profile, ["way/99"]))

    def test_production_fallback_is_clipped_to_canonical_terrain(self) -> None:
        sampler = TerrainSampler(
            [0.0, 1.0, 2.0],
            [0.0, 1.0],
            {(x_value, y_value): 10.0 + x_value for x_value in (0.0, 1.0, 2.0) for y_value in (0.0, 1.0)},
        )
        polygon = {
            "type": "MultiPolygon",
            "coordinates": [
                [[[-10.0, -10.0], [-5.0, -10.0], [-5.0, -5.0], [-10.0, -5.0], [-10.0, -10.0]]],
                [[[0.25, 0.2], [1.75, 0.2], [1.75, 0.8], [0.25, 0.8], [0.25, 0.2]]],
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            samples, fallbacks, _, overlap, _ = production_geometry_evidence(
                REPO_ROOT,
                Path(directory),
                {"relation/10": polygon},
                {},
                {},
                sampler,
                "EPSG:32639",
                0.02,
                8,
                0.02,
            )
        self.assertEqual([], samples["relation/10"])
        self.assertEqual({}, overlap)
        self.assertTrue(0.0 <= fallbacks["relation/10"][0] <= 2.0)
        self.assertTrue(0.0 <= fallbacks["relation/10"][1] <= 1.0)

    def test_width_aware_suppression_proves_final_ribbon_footprint_non_overlap(self) -> None:
        sampler = TerrainSampler(
            [0.0, 10.0],
            [0.0, 10.0],
            {(x_value, y_value): 10.0 for x_value in (0.0, 10.0) for y_value in (0.0, 10.0)},
        )
        polygon = {
            "type": "Polygon",
            "coordinates": [[[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0], [0.0, 0.0]]],
        }
        lines = {
            "way/inside": {"type": "LineString", "coordinates": [[-10.0, 5.0], [20.0, 5.0]]},
            "way/near": {"type": "LineString", "coordinates": [[-10.0, -5.0], [20.0, -5.0]]},
        }
        with tempfile.TemporaryDirectory() as directory:
            _, _, visible, overlap, _ = production_geometry_evidence(
                REPO_ROOT,
                Path(directory),
                {"relation/10": polygon},
                lines,
                {"way/inside": 12.0, "way/near": 12.0},
                sampler,
                "EPSG:32639",
                0.01,
                8,
                0.01,
            )
        self.assertTrue(all(geometry is not None for geometry in visible.values()))
        self.assertEqual({"way/inside": 0.0, "way/near": 0.0}, overlap)

    def test_estimators_pin_interpolation_endpoints_and_l1_ties(self) -> None:
        self.assertEqual(2.5, linear_quantile([0.0, 10.0], 0.25))
        self.assertEqual(
            [2.5, 5.0, 12.5],
            rolling_quantile([0.0, 10.0, 20.0], 0.25, radius=1),
        )
        self.assertEqual(
            [6.0, 6.0],
            fit_downstream_surface([6.0, 10.0], radius=0),
        )

    def test_estimators_are_robust_to_positive_negative_and_mixed_contamination(self) -> None:
        standing = [50.0, 50.1, 50.2, 50.3, 50.4, 50.5, 50.6, 50.7, 50.8]
        flowing = [55.0, 54.8, 54.6, 54.4, 54.2, 54.0, 53.8, 53.6, 53.4]
        base_standing = linear_quantile(standing, 0.5)
        base_flowing = fit_downstream_surface(flowing, radius=2, quantile=0.5)
        for changes in ({4: 20.0}, {4: -20.0}, {2: -20.0, 6: 20.0}):
            changed_standing = list(standing)
            changed_flowing = list(flowing)
            for index, delta in changes.items():
                changed_standing[index] += delta
                changed_flowing[index] += delta
            self.assertLessEqual(abs(linear_quantile(changed_standing, 0.5) - base_standing), 0.100001)
            self.assertLessEqual(max(
                abs(left - right)
                for left, right in zip(
                    fit_downstream_surface(changed_flowing, radius=2, quantile=0.5), base_flowing, strict=True
                )
            ), 0.21)
        q25_mixed = list(flowing)
        q25_mixed[2] -= 20.0
        q25_mixed[6] += 20.0
        self.assertGreater(max(
            abs(left - right)
            for left, right in zip(
                fit_downstream_surface(q25_mixed, radius=2, quantile=0.25),
                fit_downstream_surface(flowing, radius=2, quantile=0.25),
                strict=True,
            )
        ), 0.6)

    def test_non_surface_hidden_and_temporal_water_are_reported_but_not_realized(self) -> None:
        line = {"type": "LineString", "coordinates": [[0.1, 0.2], [0.9, 0.2]]}
        compiled = _compile([
            _feature("way/20", line, waterway="fairway"),
            _feature("way/21", line, waterway="river", tunnel="culvert"),
            _feature("way/22", line, waterway="river", intermittent="yes"),
        ])
        self.assertFalse(any(compiled.by_owner.values()))
        self.assertEqual(
            ["water_hidden", "water_non_surface", "water_temporal"],
            sorted(item["reason_code"] for item in compiled.rejections),
        )

    def test_unknown_water_class_or_modifier_fails_closed(self) -> None:
        line = {"type": "LineString", "coordinates": [[0.1, 0.2], [0.9, 0.2]]}
        with self.assertRaises(CompilerError) as unknown_class:
            _compile([_feature("way/30", line, waterway="mystery")])
        self.assertEqual("water_class_unapproved", unknown_class.exception.code)
        with self.assertRaises(CompilerError) as unknown_modifier:
            _compile([_feature("way/31", line, waterway="river", intermittent="sometimes")])
        self.assertEqual("water_modifier_unapproved", unknown_modifier.exception.code)
        with self.assertRaises(CompilerError) as invalid_width:
            _compile([_feature("way/32", line, waterway="river", width="about six")])
        self.assertEqual("water_width_unapproved", invalid_width.exception.code)

    def test_flowing_polygon_and_axis_share_one_non_overlapping_surface_group(self) -> None:
        polygon = {
            "type": "Polygon",
            "coordinates": [[[0.25, 0.2], [1.75, 0.2], [1.75, 0.8], [0.25, 0.8], [0.25, 0.2]]],
        }
        axis = {"type": "LineString", "coordinates": [[0.0, 0.5], [2.0, 0.5]]}
        compiled = _compile([
            _feature("relation/10", polygon, natural="water", water="river"),
            _feature("way/11", axis, waterway="river", width="6 m"),
        ], _profile(grouped=True))
        features = [item for values in compiled.by_owner.values() for item in values]
        self.assertEqual({"synthetic_wide_river"}, {
            item["attributes"]["surface_group_id"] for item in features
        })
        area = next(item for item in features if item["attributes"]["surface_role"] == "area")
        flow_axis = next(item for item in features if item["attributes"]["surface_role"] == "flow_axis")
        self.assertEqual(("polygon", "flowing"), (
            area["attributes"]["surface_geometry"], area["attributes"]["surface_behavior"]
        ))
        self.assertEqual("source-derived", flow_axis["attributes"]["width_basis"])
        self.assertEqual(0.0, flow_axis["attributes"]["polygon_overlap_area_m2"])
        self.assertTrue(all(
            coordinate == round(coordinate, 2)
            for knot in area["attributes"]["surface_function"]["knots"]
            for coordinate in knot[:2]
        ))
        self.assertIn("fit_diagnostics", area["attributes"]["surface_function"])
        fragments = [
            fragment
            for representation in flow_axis["representations"]
            for fragment in (
                representation["geometry"]["coordinates"]
                if representation["geometry"]["type"] == "MultiLineString"
                else [representation["geometry"]["coordinates"]]
            )
        ]
        self.assertTrue(fragments)
        self.assertTrue(all(max(point[0] for point in fragment) <= 250.0 or
                            min(point[0] for point in fragment) >= 1750.0 for fragment in fragments))

    def test_ribbon_membership_uses_its_surface_footprint(self) -> None:
        boundary_parallel = {
            "type": "LineString",
            "coordinates": [[0.995, 0.1], [0.995, 0.9]],
        }
        compiled = _compile([
            _feature("way/40", boundary_parallel, waterway="river", width="12 m"),
        ])
        feature = next(item for values in compiled.by_owner.values() for item in values)
        identifier = grid_id(_profile()["grid"])
        expected = {f"{identifier}:x0:y0", f"{identifier}:x1:y0"}
        self.assertEqual(expected, set(feature["intersecting_cell_ids"]))
        self.assertEqual(expected, {item["cell_id"] for item in feature["representations"]})
        east = next(item for item in feature["representations"] if item["cell_id"].endswith(":x1:y0"))
        self.assertEqual("water_ribbon_reference", east["representation"])
        self.assertEqual([feature["feature_id"]], compiled.references[f"{identifier}:x1:y0"])
        self.assertIn("surface_function", feature["attributes"])
        self.assertTrue(all("surface_function" not in item for item in feature["representations"]))
        self.assertEqual(0.0, feature["attributes"]["polygon_overlap_area_m2"])

    def test_ribbon_outside_grid_uses_footprint_and_terrain_halo(self) -> None:
        outside_boundary = {
            "type": "LineString",
            "coordinates": [[-0.005, 0.1], [-0.005, 0.9]],
        }
        compiled = _compile([
            _feature("way/41", outside_boundary, waterway="river", width="12 m"),
        ])
        feature = next(item for values in compiled.by_owner.values() for item in values)
        identifier = grid_id(_profile()["grid"])
        self.assertEqual([f"{identifier}:x0:y0"], feature["intersecting_cell_ids"])
        self.assertEqual("authoritative_water_ribbon", feature["representations"][0]["representation"])
        self.assertNotIn("geometry", feature["representations"][0])
        surface = feature["attributes"]["surface_function"]
        self.assertGreater(surface["sample_count"], 1)
        self.assertTrue(all(knot[0] == -5.0 for knot in surface["knots"]))

    def test_ribbon_reaching_beyond_terrain_halo_fails_closed(self) -> None:
        outside_halo = {
            "type": "LineString",
            "coordinates": [[-1.1, 0.1], [-1.1, 0.9]],
        }
        with self.assertRaises(CompilerError) as rejected:
            _compile([
                _feature("way/42", outside_halo, waterway="river", width="2300 m"),
            ])
        self.assertEqual("water_flow_axis_outside_terrain", rejected.exception.code)

    def test_complete_water_prepass_keeps_fully_suppressed_axis_hidden(self) -> None:
        polygon = {
            "type": "Polygon",
            "coordinates": [[[0.1, 0.1], [0.9, 0.1], [0.9, 0.9], [0.1, 0.9], [0.1, 0.1]]],
        }
        axis = {"type": "LineString", "coordinates": [[0.2, 0.5], [0.8, 0.5]]}
        compiled = _compile([
            _feature("relation/50", polygon, natural="water", water="lake"),
            _feature("way/51", axis, waterway="river", width="12 m"),
        ])
        sources = {
            feature["source_refs"][0]["provider_feature_id"]
            for values in compiled.by_owner.values()
            for feature in values
        }
        self.assertEqual({"relation/50"}, sources)

    def test_complete_water_prepass_does_not_promote_outside_non_surface(self) -> None:
        outside_fairway = {
            "type": "LineString",
            "coordinates": [[-0.005, 0.1], [-0.005, 0.9]],
        }
        compiled = _compile([
            _feature("way/52", outside_fairway, waterway="fairway"),
        ])
        self.assertFalse(any(compiled.by_owner.values()))

    def test_missing_or_conflicting_flow_axis_fails_closed(self) -> None:
        polygon = {
            "type": "Polygon",
            "coordinates": [[[0.25, 0.2], [1.75, 0.2], [1.75, 0.8], [0.25, 0.8], [0.25, 0.2]]],
        }
        with self.assertRaises(CompilerError) as missing:
            _compile([_feature("relation/10", polygon, natural="water", water="river")], _profile(grouped=True))
        self.assertEqual("water_flow_axis_missing", missing.exception.code)
        profile = _profile(grouped=True)
        profile["water_semantics"]["surface_groups"].append({
            "surface_group_id": "conflicting_group",
            "polygon_source_id": "relation/10",
            "flow_axis_source_ids": ["way/12"],
        })
        with self.assertRaises(CompilerError) as conflict:
            _compile([], profile)
        self.assertEqual("water_surface_group_conflict", conflict.exception.code)


if __name__ == "__main__":
    unittest.main()
