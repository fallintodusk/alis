from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.buildings import _production_containers, prepare_buildings
from World.CanonicalCompilation.app.features import compile_features
from World.CanonicalCompilation.app.spatial import grid_id


def polygon(minimum_x: float, minimum_y: float, maximum_x: float, maximum_y: float) -> dict:
    return {
        "type": "Polygon",
        "coordinates": [[
            [minimum_x, minimum_y], [maximum_x, minimum_y],
            [maximum_x, maximum_y], [minimum_x, maximum_y],
            [minimum_x, minimum_y],
        ]],
    }


def building(
    identity: str,
    geometry: dict,
    *,
    outline: bool = False,
    part: bool = False,
    height: str | None = None,
    levels: str | None = None,
    min_height: str | None = None,
    min_level: str | None = None,
    relation: str | None = None,
    role: str | None = None,
) -> dict:
    properties = {}
    if outline:
        properties["building"] = "yes"
    if part:
        properties["building:part"] = "yes"
    if height is not None:
        properties["height"] = height
    if levels is not None:
        properties["building:levels"] = levels
    if min_height is not None:
        properties["min_height"] = min_height
    if min_level is not None:
        properties["building:min_level"] = min_level
    result = {
        "provider_feature_id": identity,
        "provider_class": "building",
        "geometry": geometry,
        "properties": properties,
    }
    if relation is not None and role is not None:
        result["relation_memberships"] = [{
            "provider_relation_id": relation,
            "relation_type": "building",
            "role": role,
        }]
    return result


class BuildingVolumeTests(unittest.TestCase):
    def prepare(self, features: list[dict]):
        projected = {item["provider_feature_id"]: item["geometry"] for item in features}
        scratch_parent = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "tests"
        scratch_parent.mkdir(parents=True, exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=scratch_parent)
        self.addCleanup(directory.cleanup)
        return prepare_buildings(
            features,
            projected,
            "fixture",
            {"snapshot_id": "snapshot", "provider": "fixture", "release": "1"},
            {"maximum_height_m": 300.0, "coverage_tolerance_square_m": 0.001},
            {"coordinate_transform": "fixture_affine", "canonical_crs": "fixture"},
            REPO_ROOT,
            Path(directory.name),
            set(),
        )

    def test_outline_only_preserves_v1_shape_as_one_grounded_volume(self) -> None:
        result = self.prepare([building("way/1", polygon(0, 0, 10, 10), outline=True, levels="3")])
        candidate = result.source_features[0]["prepared_canonical_feature"]
        self.assertEqual("outline_no_parts", candidate["attributes"]["volume_selection"])
        self.assertEqual(1, len(candidate["attributes"]["effective_volumes"]))
        self.assertEqual(0.0, candidate["attributes"]["effective_volumes"][0]["min_height_m"])
        self.assertEqual(9.0, candidate["attributes"]["effective_volumes"][0]["height_m"])

    def test_complete_parts_replace_outline_and_preserve_raised_volume(self) -> None:
        relation = "relation/90"
        features = [
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="30", relation=relation, role="outline"),
            building("way/2", polygon(-1, 0, 5, 10), part=True, height="12", relation=relation, role="part"),
            building("way/3", polygon(5, 0, 11, 10), part=True, height="30", min_height="12", relation=relation, role="part"),
        ]
        result = self.prepare(features)
        candidate = result.source_features[0]["prepared_canonical_feature"]
        self.assertEqual("alis:fixture:relation:90", candidate["feature_id"])
        self.assertEqual("complete_parts", candidate["attributes"]["volume_selection"])
        volumes = candidate["attributes"]["effective_volumes"]
        self.assertEqual(["way/2", "way/3"], [item["source_feature_id"] for item in volumes])
        self.assertEqual(12.0, volumes[1]["min_height_m"])

    def test_building_min_level_uses_the_existing_metric_fallback(self) -> None:
        relation = "relation/91"
        result = self.prepare([
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="30", relation=relation, role="outline"),
            building("way/2", polygon(0, 0, 10, 10), part=True, levels="10", min_level="4", relation=relation, role="part"),
        ])
        volume = result.source_features[0]["prepared_canonical_feature"]["attributes"]["effective_volumes"][0]
        self.assertEqual(12.0, volume["min_height_m"])
        self.assertEqual("inferred_from_min_level", volume["min_height_basis"])

    def test_explicit_relation_without_outline_rejects_part_instead_of_guessing_containment(self) -> None:
        result = self.prepare([
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="20"),
            building("way/2", polygon(1, 1, 9, 9), part=True, height="10", relation="relation/99", role="part"),
        ])
        reasons = {item["source_identity"]: item["reason_code"] for item in result.rejections}
        self.assertEqual("building_relation_outline_missing", reasons["way/2"])
        candidate = result.source_features[0]["prepared_canonical_feature"]
        self.assertEqual("outline_no_parts", candidate["attributes"]["volume_selection"])

    def test_explicit_relation_wins_over_geometric_containment(self) -> None:
        relation = "relation/92"
        result = self.prepare([
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="20", relation=relation, role="outline"),
            building("way/2", polygon(-1, -1, 11, 11), outline=True, height="25"),
            building("way/3", polygon(1, 1, 9, 9), part=True, height="10", relation=relation, role="part"),
        ])
        self.assertNotIn("ambiguous_building_containment", {item["reason_code"] for item in result.rejections})
        candidates = {
            item["prepared_canonical_feature"]["attributes"]["logical_building_id"]: item
            for item in result.source_features
        }
        self.assertEqual({"relation/92", "way/2"}, set(candidates))

    def test_unique_containment_associates_while_ambiguous_and_orphan_parts_are_excluded(self) -> None:
        features = [
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="20"),
            building("way/2", polygon(2, 2, 8, 8), outline=True, height="15"),
            building("way/3", polygon(3, 3, 4, 4), part=True, height="10"),
            building("way/4", polygon(20, 20, 21, 21), part=True, height="10"),
        ]
        result = self.prepare(features)
        reasons = {item["source_identity"]: item["reason_code"] for item in result.rejections}
        self.assertEqual("ambiguous_building_containment", reasons["way/3"])
        self.assertEqual("orphan_building_part", reasons["way/4"])

    def test_incomplete_parts_use_valid_outline_fallback_and_reject_invalid_fallback(self) -> None:
        accepted = self.prepare([
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="20"),
            building("way/2", polygon(0, 0, 5, 10), part=True, height="10"),
        ])
        candidate = accepted.source_features[0]["prepared_canonical_feature"]
        self.assertEqual("outline_fallback_incomplete_parts", candidate["attributes"]["volume_selection"])
        self.assertEqual("way/1", candidate["attributes"]["effective_volumes"][0]["source_feature_id"])

        rejected = self.prepare([
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="400"),
            building("way/2", polygon(0, 0, 5, 10), part=True, height="10"),
        ])
        self.assertFalse(rejected.source_features)
        self.assertIn("building_vertical_range_invalid", {item["reason_code"] for item in rejected.rejections})

    def test_order_is_stable(self) -> None:
        features = [
            building("way/2", polygon(0, 0, 5, 10), part=True, height="10"),
            building("way/1", polygon(0, 0, 10, 10), outline=True, height="20"),
            {"provider_feature_id": "way/8", "provider_class": "highway", "geometry": polygon(0, 0, 1, 1), "properties": {}},
        ]
        first = self.prepare(features)
        second = self.prepare(list(reversed(features)))
        self.assertEqual(first.source_features, second.source_features)

    def test_production_containment_uses_spatial_index_then_exact_covers(self) -> None:
        outline_with_hole = polygon(0, 0, 10, 10)
        outline_with_hole["coordinates"].append(polygon(4, 4, 6, 6)["coordinates"][0])
        projected = {
            "outline/1": outline_with_hole,
            "outline/2": polygon(20, 20, 30, 30),
            "part/1": polygon(1, 1, 2, 2),
            "part/2": polygon(21, 21, 22, 22),
            "part/in_hole": polygon(4.25, 4.25, 5.75, 5.75),
        }
        scratch_parent = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "tests"
        scratch_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch_parent) as directory:
            result = _production_containers(
                REPO_ROOT,
                Path(directory),
                "EPSG:32618",
                projected,
                {"outline/1", "outline/2"},
                {"part/1", "part/2", "part/in_hole"},
            )
        self.assertEqual(["outline/1"], result["part/1"])
        self.assertEqual(["outline/2"], result["part/2"])
        self.assertEqual([], result["part/in_hole"])

    def test_compilation_emits_one_provider_neutral_logical_feature_across_cells(self) -> None:
        relation = "relation/90"
        features = [
            building("way/1", polygon(0, 0, 20, 10), outline=True, height="20", relation=relation, role="outline"),
            building("way/2", polygon(0, 0, 10, 10), part=True, height="10", relation=relation, role="part"),
            building("way/3", polygon(10, 0, 20, 10), part=True, height="20", relation=relation, role="part"),
        ]
        grid = {
            "coordinate_transform": "fixture_affine",
            "fixture_scale_m": 1.0,
            "canonical_crs": "fixture",
            "coordinate_quantization": 0.01,
            "origin": [0.0, 0.0],
            "sample_spacing": [10.0, 10.0],
            "cell_quads": [1, 1],
        }
        profile = {
            "grid": grid,
            "target_cells": [{"x": 0, "y": 0}, {"x": 1, "y": 0}],
            "identity_namespace": "fixture",
            "building_semantics": {
                "maximum_height_m": 300.0,
                "coverage_tolerance_square_m": 0.001,
            },
        }
        bundle = SimpleNamespace(
            ledger={"snapshots": [{
                "snapshot_id": "snapshot",
                "provider": "fixture",
                "release": "1",
                "crs": {"horizontal": "EPSG:4326"},
            }]},
            feature_snapshot_id="snapshot",
            features=features,
        )
        scratch_parent = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "tests"
        with tempfile.TemporaryDirectory(dir=scratch_parent) as directory:
            result = compile_features(
                bundle,
                profile,
                grid_id(grid),
                {"overlay_id": "empty", "feature_overrides": []},
                REPO_ROOT,
                Path(directory),
            )
        emitted = [feature for values in result.by_owner.values() for feature in values]
        self.assertEqual(1, len(emitted))
        self.assertEqual("alis:fixture:relation:90", emitted[0]["feature_id"])
        self.assertEqual(2, len(emitted[0]["attributes"]["effective_volumes"]))
        self.assertEqual(2, len(emitted[0]["intersecting_cell_ids"]))


if __name__ == "__main__":
    unittest.main()
