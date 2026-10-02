from __future__ import annotations

import math
import sys
import unittest
from copy import deepcopy
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import CompilerError, validate_document
from World.CanonicalCompilation.app import terrain_surface
from World.CanonicalCompilation.app.terrain_surface import (
    finalize_terrain_cells,
    validate_surface_contract,
    water_surface_dirty_cells,
)


GRID_ID = "grid_0000000000000000"
CELL_ID = f"{GRID_ID}:x0:y0"


def _profile() -> dict:
    return {
        "grid": {
            "height_quantization": 0.1,
            "halo_samples": 1,
            "cell_quads": [31, 31],
            "sample_spacing": [1.0, 1.0],
        },
        "terrain_surface": {
            "contract_id": "terrain_surface_semantics",
            "contract_version": 1,
            "roles": ["ground", "hydro_transition"],
            "hydro_transition_radius_m": 60.0,
            "water_clearance_m": 0.0,
        },
    }


def _terrain() -> dict:
    base = [[12.0, 12.0, 12.0], [12.0, 12.0, 12.0], [12.0, 12.0, 12.0]]
    return {CELL_ID: {
        "bounds": [0.0, 0.0, 0.0, 0.0],
        "sample_spacing": [1.0, 1.0],
        "core_samples": [[12.0]],
        "halo_window": {"samples": deepcopy(base)},
        "water_fit": {"core_samples": [[12.0]], "halo_window": {"samples": deepcopy(base)}},
        "height_edges": {},
        "height_border_bands": {},
    }}


def _compiled() -> SimpleNamespace:
    water = {
        "feature_id": "fixture:water",
        "feature_class": "water",
        "geometry": {
            "type": "Polygon",
            "coordinates": [[[-2.0, -2.0], [2.0, -2.0], [2.0, 2.0], [-2.0, 2.0], [-2.0, -2.0]]],
        },
        "attributes": {
            "surface_function": {"level_m": 10.0},
            "surface_geometry": "polygon",
        },
        "intersecting_cell_ids": [CELL_ID],
    }
    return SimpleNamespace(by_owner={CELL_ID: [water]})


def _overlay(delta: float = 0.0, surface_weights: dict | None = None) -> dict:
    patch = {
        "patch_id": "fixture_patch",
        "provenance_ref": "fixture_author",
        "center": [0.0, 0.0],
        "radius_m": 0.0,
        "delta_m": delta,
    }
    if surface_weights is not None:
        patch["surface_weights"] = surface_weights
    return {
        "grid_id": GRID_ID,
        "provenance": [{"provenance_id": "fixture_author", "authority": "authored_fixture"}],
        "terrain_patches": [patch],
    }


class TerrainSurfaceTests(unittest.TestCase):
    def test_surface_contract_is_explicit(self) -> None:
        profile = _profile()
        del profile["terrain_surface"]

        with self.assertRaises(CompilerError) as raised:
            validate_surface_contract(profile)
        self.assertEqual("terrain_surface_contract_missing", raised.exception.code)

    def test_v1_overlay_is_rejected(self) -> None:
        legacy = {
            "$schema": "https://alis.world/schemas/world-compiler/authored-overlay-v2.json",
            "schema_version": 1,
            "overlay_id": "legacy_empty",
            "terrain_patches": [],
            "feature_overrides": [],
            "missing_target_policy": "reject_and_require_rebase",
        }
        with self.assertRaises(CompilerError) as raised:
            validate_document(legacy, Path("legacy-overlay.json"))
        self.assertEqual("contract_violation", raised.exception.code)

    def test_prepared_polygon_distance_matches_exact_with_hole(self) -> None:
        feature = deepcopy(_compiled().by_owner[CELL_ID][0])
        feature["geometry"]["coordinates"].append(
            [[-1.0, -1.0], [1.0, -1.0], [1.0, 1.0], [-1.0, 1.0], [-1.0, -1.0]]
        )
        prepared = terrain_surface._prepare_water_feature(feature)

        for point in ((0.0, 0.0), (1.5, 0.0), (2.0, 0.0), (3.0, 0.0), (-3.0, -3.0)):
            with self.subTest(point=point):
                self.assertAlmostEqual(
                    terrain_surface._footprint_distance(feature, point),
                    terrain_surface._prepared_footprint_distance(prepared, point, 60.0),
                )

    def test_prepared_distance_skips_irrelevant_multipolygon_parts(self) -> None:
        feature = deepcopy(_compiled().by_owner[CELL_ID][0])
        near = feature["geometry"]["coordinates"]
        far = [[[1000.0, 1000.0], [1002.0, 1000.0], [1002.0, 1002.0], [1000.0, 1002.0], [1000.0, 1000.0]]]
        feature["geometry"] = {"type": "MultiPolygon", "coordinates": [near, far]}
        prepared = terrain_surface._prepare_water_feature(feature)

        with patch.object(
            terrain_surface,
            "_prepared_polygon_distance",
            wraps=terrain_surface._prepared_polygon_distance,
        ) as distance:
            actual = terrain_surface._prepared_footprint_distance(prepared, (3.0, 0.0), 60.0)

        self.assertEqual(1.0, actual)
        self.assertEqual(1, distance.call_count)

    def test_prepared_distance_preserves_line_width_at_cutoff(self) -> None:
        feature = deepcopy(_compiled().by_owner[CELL_ID][0])
        feature["geometry"] = {
            "type": "LineString",
            "coordinates": [[0.0, 0.0], [0.0, 10.0]],
        }
        feature["attributes"]["width_m"] = 10.0
        prepared = terrain_surface._prepare_water_feature(feature)

        self.assertEqual(0.0, terrain_surface._prepared_footprint_distance(prepared, (4.0, 5.0), 1.0))
        self.assertEqual(math.inf, terrain_surface._prepared_footprint_distance(prepared, (7.0, 5.0), 1.0))

    def test_finalization_uses_local_water_membership_with_transition_neighbors(self) -> None:
        near_id = f"{GRID_ID}:x1:y0"
        far_id = f"{GRID_ID}:x3:y0"
        compiled = _compiled()
        near = compiled.by_owner[CELL_ID][0]
        near["intersecting_cell_ids"] = [near_id]
        far = deepcopy(near)
        far["feature_id"] = "fixture:far_water"
        far["geometry"] = {
            "type": "Polygon",
            "coordinates": [[[90.0, -2.0], [94.0, -2.0], [94.0, 2.0], [90.0, 2.0], [90.0, -2.0]]],
        }
        far["intersecting_cell_ids"] = [far_id]
        compiled.by_owner[far_id] = [far]
        observed: list[str] = []
        original = terrain_surface._prepared_footprint_distance

        def observe(prepared: dict, point: tuple[float, float], cutoff_m: float) -> float:
            observed.append(prepared["feature"]["feature_id"])
            return original(prepared, point, cutoff_m)

        terrain = _terrain()
        with patch.object(terrain_surface, "_prepared_footprint_distance", side_effect=observe):
            finalize_terrain_cells(terrain, compiled, _profile(), GRID_ID, _overlay())

        self.assertIn("fixture:water", observed)
        self.assertNotIn("fixture:far_water", observed)
        self.assertEqual([[1.0]], terrain[CELL_ID]["surface_semantics"]["core_samples"]["hydro_transition"])

    def test_water_dirtying_includes_neighbor_transition_band(self) -> None:
        neighbor_id = f"{GRID_ID}:x1:y0"
        previous = _compiled()
        current = _compiled()
        current.by_owner[CELL_ID][0]["attributes"]["surface_function"]["level_m"] = 9.0
        terrain = _terrain()
        terrain[neighbor_id] = {
            **deepcopy(terrain[CELL_ID]),
            "bounds": [30.0, 0.0, 30.0, 0.0],
        }

        dirty = water_surface_dirty_cells(current, previous, terrain, _profile())

        self.assertEqual({CELL_ID, neighbor_id}, dirty)

    def test_hand_height_runs_after_hydro_and_preserves_water_fit_base(self) -> None:
        terrain = _terrain()
        finalize_terrain_cells(terrain, _compiled(), _profile(), GRID_ID, _overlay(delta=-1.0))
        self.assertEqual([[12.0]], terrain[CELL_ID]["water_fit"]["core_samples"])
        self.assertEqual([[9.0]], terrain[CELL_ID]["core_samples"])

    def test_hand_height_that_breaks_water_clearance_fails_closed(self) -> None:
        with self.assertRaises(CompilerError) as raised:
            finalize_terrain_cells(_terrain(), _compiled(), _profile(), GRID_ID, _overlay(delta=1.0))
        self.assertEqual("authored_height_breaks_water_clearance", raised.exception.code)

    def test_surface_override_replaces_procedural_weights(self) -> None:
        terrain = _terrain()
        finalize_terrain_cells(
            terrain,
            _compiled(),
            _profile(),
            GRID_ID,
            _overlay(surface_weights={"ground": 0.75, "hydro_transition": 0.25}),
        )
        semantics = terrain[CELL_ID]["surface_semantics"]
        self.assertEqual([[0.75]], semantics["core_samples"]["ground"])
        self.assertEqual([[0.25]], semantics["core_samples"]["hydro_transition"])

    def test_surface_overlap_and_unknown_provenance_fail_closed(self) -> None:
        overlay = _overlay(surface_weights={"ground": 0.5, "hydro_transition": 0.5})
        overlay["terrain_patches"].append({
            **overlay["terrain_patches"][0],
            "patch_id": "overlap",
        })
        with self.assertRaises(CompilerError) as overlap:
            finalize_terrain_cells(_terrain(), _compiled(), _profile(), GRID_ID, overlay)
        self.assertEqual("authored_surface_overlap", overlap.exception.code)

        missing = _overlay()
        missing["terrain_patches"][0]["provenance_ref"] = "missing"
        with self.assertRaises(CompilerError) as provenance:
            finalize_terrain_cells(_terrain(), _compiled(), _profile(), GRID_ID, missing)
        self.assertEqual("authored_overlay_provenance_missing", provenance.exception.code)

    def test_slope_cannot_be_promoted_to_a_factual_rock_role(self) -> None:
        profile = _profile()
        profile["terrain_surface"]["roles"] = ["ground", "rock"]
        with self.assertRaises(CompilerError) as raised:
            validate_surface_contract(profile)
        self.assertEqual("terrain_surface_roles_invalid", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
