from __future__ import annotations

import json
import sys
import tempfile
import unittest
from copy import deepcopy
from contextlib import redirect_stdout
from dataclasses import replace
from io import StringIO
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
TEST_DATA = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
SOURCE_PROFILE = TEST_DATA / "Profiles" / "SourceIngestion" / "synthetic_two_cell.source.json"
COMPILER_PROFILE = TEST_DATA / "Profiles" / "CanonicalCompilation" / "synthetic_two_cell.compile.json"
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import read_json
from World.CanonicalCompilation.app.pipeline import compilation_root, compile_world, load_profile
from World.CanonicalCompilation.app.source import load_source_bundle
from World.CanonicalCompilation.app.spatial import grid_id
from World.SourceIngestion.app.cli import main as source_main


def _water_feature(source_id: str, geometry: dict, **properties: str) -> dict:
    return {
        "provider_feature_id": source_id,
        "provider_class": "water",
        "geometry": geometry,
        "properties": properties,
        "precision": {"coordinate_decimals": 7},
        "confidence": None,
    }


def _water_semantics() -> dict:
    return {
        "policy_id": "synthetic_incremental_water_v1",
        "policy_version": 1,
        "coordinate_match_tolerance_m": 0.01,
        "class_rules": [
            {"water_class": "lake", "geometry": "polygon", "result": "surface", "behavior": "standing"},
            {"water_class": "river", "geometry": "polygon", "result": "surface", "behavior": "flowing"},
            {"water_class": "river", "geometry": "line", "result": "surface", "behavior": "flowing"},
            {"water_class": "stream", "geometry": "line", "result": "surface", "behavior": "flowing"},
        ],
        "modifier_rules": [{"tag": "intermittent", "value": "no", "result": "visible"}],
        "feature_rules": [],
        "surface_groups": [{
            "surface_group_id": "synthetic_river",
            "polygon_source_id": "relation/10",
            "flow_axis_source_ids": ["way/11"],
        }],
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
            "standing": {
                "function_id": "standing_polygon_quantile",
                "function_version": 1,
                "quantile": 0.5,
                "empty_sample_fallback": "ogr_point_on_surface_bilinear",
            },
            "flowing": {
                "function_id": "rolling_quantile_l1_isotonic",
                "function_version": 1,
                "sample_spacing_m": 30.0,
                "quantile": 0.5,
                "rolling_radius": 2,
                "direction": "non_increasing_by_source_direction",
                "loss": "l1",
                "tie_break": "lower_elevation",
            },
        },
    }


class WaterIncrementalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        with redirect_stdout(StringIO()):
            if source_main(["run", "--profile", str(SOURCE_PROFILE)]) != 0:
                raise RuntimeError("Synthetic source ingestion failed")

    def test_complete_water_prepass_survives_the_real_incremental_chain(self) -> None:
        profile = deepcopy(load_profile(COMPILER_PROFILE))
        profile["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
        profile["profile_id"] = "synthetic_water_incremental"
        profile["water_semantics"] = _water_semantics()
        base_bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
        water = [
            _water_feature(
                "relation/10",
                {"type": "Polygon", "coordinates": [[[-0.4, 0.35], [0.4, 0.35], [0.4, 0.65], [-0.4, 0.65], [-0.4, 0.35]]]},
                natural="water", water="river",
            ),
            _water_feature("way/11", {"type": "LineString", "coordinates": [[-0.9, 0.5], [0.3, 0.5]]}, waterway="river", width="12 m"),
            _water_feature(
                "relation/20",
                {"type": "Polygon", "coordinates": [[[-0.75, 0.7], [-0.55, 0.7], [-0.55, 0.9], [-0.75, 0.9], [-0.75, 0.7]]]},
                natural="water", water="lake",
            ),
            _water_feature("way/21", {"type": "LineString", "coordinates": [[-0.95, 0.8], [-0.05, 0.8]]}, waterway="stream"),
            _water_feature("way/22", {"type": "LineString", "coordinates": [[-0.005, 0.05], [-0.005, 0.3]]}, waterway="river", width="8 m"),
        ]
        initial = replace(base_bundle, features=[*base_bundle.features, *water])

        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            profile_path = root / "synthetic_water_incremental.compile.json"
            profile_path.write_text(json.dumps(profile), encoding="utf-8")
            roots = {name: root / name for name in (
                "base", "unrelated", "grouped", "standing", "widened", "narrowed", "noop"
            )}
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=initial):
                compile_world(str(profile_path), output_root_value=roots["base"])

            unrelated_features = deepcopy(initial.features)
            next(item for item in unrelated_features if item["provider_feature_id"] == "way/204")["properties"]["building:levels"] = "3"
            unrelated = replace(initial, features=unrelated_features)
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=unrelated):
                compile_world(str(profile_path), output_root_value=roots["unrelated"], base_result=roots["base"] / "compile_result.json", feature_change_ids=["way/204"])

            grouped_features = deepcopy(unrelated.features)
            next(item for item in grouped_features if item["provider_feature_id"] == "way/11")["properties"]["width"] = "6 m"
            grouped = replace(unrelated, features=grouped_features)
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=grouped):
                compile_world(str(profile_path), output_root_value=roots["grouped"], base_result=roots["unrelated"] / "compile_result.json", feature_change_ids=["way/11"])

            standing_features = deepcopy(grouped.features)
            next(item for item in standing_features if item["provider_feature_id"] == "relation/20")["geometry"] = {
                "type": "Polygon",
                "coordinates": [[[-0.65, 0.7], [-0.35, 0.7], [-0.35, 0.9], [-0.65, 0.9], [-0.65, 0.7]]],
            }
            standing = replace(grouped, features=standing_features)
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=standing):
                compile_world(str(profile_path), output_root_value=roots["standing"], base_result=roots["grouped"] / "compile_result.json", feature_change_ids=["relation/20"])

            widened_features = deepcopy(standing.features)
            next(item for item in widened_features if item["provider_feature_id"] == "way/22")["properties"]["width"] = "12 m"
            widened = replace(standing, features=widened_features)
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=widened):
                compile_world(str(profile_path), output_root_value=roots["widened"], base_result=roots["standing"] / "compile_result.json", feature_change_ids=["way/22"])

            narrowed = replace(standing, features=deepcopy(standing.features))
            with patch("World.CanonicalCompilation.app.pipeline.load_source_bundle", return_value=narrowed):
                compile_world(str(profile_path), output_root_value=roots["narrowed"], base_result=roots["widened"] / "compile_result.json", feature_change_ids=["way/22"])
                compile_world(str(profile_path), output_root_value=roots["noop"], base_result=roots["narrowed"] / "compile_result.json")

            identifier = grid_id(profile["grid"])
            self.assertEqual([f"{identifier}:x1:y0"], read_json(roots["unrelated"] / "reports" / "diff.json")["feature_rebuilt_cells"])
            self.assertEqual([f"{identifier}:x0:y0"], read_json(roots["grouped"] / "reports" / "diff.json")["feature_rebuilt_cells"])
            self.assertEqual([f"{identifier}:x0:y0"], read_json(roots["standing"] / "reports" / "diff.json")["feature_rebuilt_cells"])
            self.assertEqual(
                [f"{identifier}:x0:y0", f"{identifier}:x1:y0"],
                read_json(roots["widened"] / "reports" / "diff.json")["feature_rebuilt_cells"],
            )
            self.assertEqual(
                [f"{identifier}:x0:y0", f"{identifier}:x1:y0"],
                read_json(roots["narrowed"] / "reports" / "diff.json")["feature_rebuilt_cells"],
            )
            self.assertEqual([], read_json(roots["noop"] / "reports" / "diff.json")["feature_rebuilt_cells"])
            widened_feature = next(
                item for item in self._features(roots["widened"]) if item["source_refs"][0]["provider_feature_id"] == "way/22"
            )
            self.assertEqual(
                {f"{identifier}:x0:y0", f"{identifier}:x1:y0"},
                set(widened_feature["intersecting_cell_ids"]),
            )
            east_manifest = read_json(roots["widened"] / "canonical" / "cells" / "cell_x1_y0.json")
            self.assertIn(widened_feature["feature_id"], east_manifest["referenced_feature_ids"])

    @staticmethod
    def _features(root: Path) -> list[dict]:
        return [
            feature
            for path in sorted((root / "canonical" / "features").glob("*.json"))
            for feature in read_json(path)["features"]
        ]


if __name__ == "__main__":
    unittest.main()
