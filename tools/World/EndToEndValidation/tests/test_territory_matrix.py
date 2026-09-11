from __future__ import annotations

import json
import hashlib
from copy import deepcopy
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import ValidationFailure, load_profile
from World.EndToEndValidation.app import execution
from World.EndToEndValidation.app.layered_validation import validate_layered_realization
from World.EndToEndValidation.app.profile_inputs import profile_input_files
from World.EndToEndValidation.app.validation import (
    _validate_canonical_authority_candidate,
    _validate_incremental,
)


PROFILE_PATH = (
    REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
    / "Profiles" / "EndToEndValidation" / "kazan_territory_v1.validation.json"
)
LAYERED_LEGS = ("first", "second", "road_locality", "incremental", "clean")
ROAD_DIRTY_UNIT = "gridtwin:x0:y0"


class TerritoryMatrixContractTests(unittest.TestCase):
    def _receipt(self, leg: str) -> dict:
        dirty = {
            "first": ["*"],
            "second": [],
            "road_locality": [],
            "incremental": [],
            "clean": [],
        }[leg]
        full = leg in ("first", "clean")
        return {
            "realization_profile": "synthetic_landscape_water_twin",
            "realization_profile_sha256": "a" * 64,
            "canonical_cell_count": 2,
            "sample_spacing_m": [1, 1],
            "georeferencing_placement_error_m": 0,
            "terrain_source_height_sample_count": 8192,
            "terrain_source_height_expected_sample_count": 8192,
            "terrain_source_height_mismatch_count": 0,
            "terrain_source_height_max_error_m": 0.003125,
            "terrain_source_height_min_m": 20.3984375,
            "terrain_source_height_max_m": 30.5,
            "terrain_source_relief_m": 10.1015625,
            "terrain_source_height_tolerance_m": 0.1,
            "terrain_source_height_semantic_sha256": "d" * 64,
            "terrain_final_height_sample_count": 8192,
            "terrain_final_height_expected_sample_count": 8192,
            "terrain_final_height_mismatch_count": 0,
            "terrain_final_height_max_error_m": 0.003125,
            "terrain_final_height_min_m": 20.3984375,
            "terrain_final_height_max_m": 30.5,
            "terrain_final_relief_m": 10.1015625,
            "terrain_final_height_tolerance_m": 0.1,
            "terrain_final_height_semantic_sha256": "f" * 64,
            "world_partition": True,
            "hlod_proxy_actor_count": 0,
            "hlod_layer_reference_count": 0,
            "hlod_eligible_generated_actor_count": 0,
            "changes": {
                "landscape_components": 2,
                "updated_landscape_components": 2 if full else 0,
                "landscape_proxies": 2,
                "water_cell_actors": 2,
                "water_mesh_assets": 2,
                "water_triangles": 100 if full else 0,
                "road_cell_actors": 2,
                "road_mesh_assets": 2,
                "road_triangles": 30 if full else 0,
                "vegetation_cell_actors": 0,
                "vegetation_components": 0,
                "vegetation_instances": 0,
                "vegetation_candidates": 0,
                "vegetation_road_exclusions": 0,
                "vegetation_water_exclusions": 0,
                "vegetation_authored_mask_exclusions": 0,
                "vegetation_instance_rewrites": 0,
                "building_cell_actors": 2,
                "building_mesh_assets": 2,
                "building_triangles": 36,
                "building_triangle_rewrites": 36 if full else 0,
                "building_candidate_fragments": 2,
                "building_accepted_fragments": 2,
                "building_duplicate_fragments": 0,
                "building_contained_fragments": 0,
                "building_conflict_fragments": 0,
                "building_malformed_fragments": 0,
                "building_authored_mask_exclusions": 0,
                "gameplay_placement_actors": 2,
                "gameplay_placement_rewrites": 2 if full else 0,
                "road_sections": 0,
                "building_sections": 0,
            },
            "layer_inventories": [
                {
                    "layer_id": "terrain",
                    "generator_id": "project_landscape",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Terrain/",
                    "normalized_layer_contract_sha256": "b" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "artifacts": [{"path": f"Terrain/{index}.uasset"} for index in range(2)],
                    "final_dirty_units": dirty,
                },
                {
                    "layer_id": "water",
                    "generator_id": "project_water_mesh",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Water/",
                    "normalized_layer_contract_sha256": "c" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "artifacts": [{"path": f"Water/{index}.uasset"} for index in range(5)],
                    "final_dirty_units": dirty,
                },
                {
                    "layer_id": "roads",
                    "generator_id": "project_road_mesh",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Roads/",
                    "normalized_layer_contract_sha256": "e" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "artifacts": [{"path": f"Roads/{index}.uasset"} for index in range(4)],
                    "final_dirty_units": [ROAD_DIRTY_UNIT] if leg == "road_locality" else dirty,
                },
                {
                    "layer_id": "buildings",
                    "generator_id": "project_building_massing",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Buildings/",
                    "normalized_layer_contract_sha256": "f" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "dependency_inputs": [{"unit_id": "layer:terrain:contract"}],
                    "artifacts": [{"path": f"Buildings/{index}.uasset"} for index in range(4)],
                    "final_dirty_units": dirty,
                },
                {
                    "layer_id": "gameplay",
                    "generator_id": "project_gameplay_placement",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Gameplay/",
                    "normalized_layer_contract_sha256": "1" * 64,
                    "canonical_inputs": [{"unit_id": f"object-{index}"} for index in range(2)],
                    "dependency_inputs": [{"unit_id": "layer:terrain:contract"}],
                    "artifacts": [{"path": f"Gameplay/{index}.uasset"} for index in range(2)],
                    "final_dirty_units": dirty,
                },
            ],
        }

    def test_territory_profile_authenticates_layered_realization_inputs(self) -> None:
        profile = load_profile(PROFILE_PATH)
        self.assertEqual("kazan_territory_v1", profile["profile_id"])
        self.assertEqual(210, profile["profiles"]["kazan"]["expected_topology"]["canonical_cells"])
        topology = profile["profiles"]["kazan"]["expected_topology"]
        self.assertEqual(146, topology["vegetation_cell_actors"])
        self.assertEqual(279, topology["vegetation_components"])
        self.assertEqual(7501, topology["vegetation_instances"])
        self.assertEqual(7528, topology["vegetation_candidates"])
        self.assertEqual(7, topology["vegetation_road_exclusions"])
        self.assertEqual(19, topology["vegetation_water_exclusions"])
        self.assertEqual(1, topology["vegetation_authored_mask_exclusions"])
        self.assertEqual(171, topology["building_cell_actors"])
        self.assertEqual(655330, topology["building_triangles"])
        self.assertEqual(31932, topology["building_candidate_fragments"])
        self.assertEqual(31769, topology["building_accepted_fragments"])
        self.assertEqual(3, topology["gameplay_placement_actors"])
        vegetation = next(
            layer for layer in profile["profiles"]["kazan"]["expected_layers"]
            if layer["layer_id"] == "vegetation"
        )
        self.assertEqual(146, vegetation["artifact_count"])
        inputs = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in profile_input_files(PROFILE_PATH, REPO_ROOT)
        }
        self.assertIn(
            "Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json",
            inputs,
        )
        self.assertIn(
            "Plugins/World/ProjectWorldData/Data/GameplayPlacement/kazan_territory_v1.json",
            inputs,
        )
        self.assertIn(
            "Plugins/Resources/ProjectObject/Content/HumanMade/Consumables/Vital/Nutrition/Drink/WaterBottle/WaterBottle.uasset",
            inputs,
        )
        settings = profile["profiles"]["synthetic"]
        self.assertEqual(
            {"resolved": 1, "placed": 1, "masks": 0, "minimum_package_files": 1},
            settings["expected_authored"],
        )
        realization = execution._realization_profile_contract(
            settings["realization_profile"],
            settings["world_data_plugin"],
            settings["map_package"],
        )
        authored = execution._authored_overlay_profile_contract(
            settings["authored_overlay_profile"], settings["world_data_plugin"]
        )
        self.assertEqual("synthetic_landscape_water_twin", realization["profile_id"])
        self.assertEqual(
            ["/ProjectWorldTestData/Authored/Fixtures/L_ProjectWorldMarker"],
            authored["packages"],
        )
        self.assertIn(
            "Plugins/World/ProjectWorldTestData/Content/Authored/Fixtures/L_ProjectWorldMarker.umap",
            inputs,
        )
        self.assertIn(
            "Plugins/World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json",
            inputs,
        )

    def test_profile_preflight_rejects_multi_cell_incremental_bounds(self) -> None:
        profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
        profile["profiles"]["kazan"]["incremental_bounds"] = [
            379800.0, 6184200.0, 379900.0, 6184300.0,
        ]
        with tempfile.TemporaryDirectory(dir=PROFILE_PATH.parent) as directory:
            candidate = Path(directory) / "multi_cell.validation.json"
            candidate.write_text(json.dumps(profile), encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                load_profile(candidate)
        self.assertEqual("incremental_scope_invalid", raised.exception.code)

    def test_road_locality_selects_a_cell_with_canonical_road_content(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            (root / "canonical" / "cells").mkdir(parents=True)
            (root / "canonical" / "features").mkdir(parents=True)
            def write(path: Path, payload: dict) -> None:
                path.write_text(json.dumps(payload), encoding="utf-8")

            write(root / "compile_result.json", {
                "outputs": [{"path": "canonical/coverage.json"}],
            })
            write(root / "canonical" / "coverage.json", {
                "cells": [
                    {"cell_id": "cell-empty", "path": "canonical/cells/empty.json"},
                    {"cell_id": "cell-road", "path": "canonical/cells/road.json"},
                ],
            })
            for name in ("empty", "road"):
                write(root / "canonical" / "cells" / f"{name}.json", {
                    "feature_artifact": {"path": f"canonical/features/{name}.json"},
                })
            write(root / "canonical" / "features" / "empty.json", {
                "features": [{"feature_class": "water"}],
            })
            write(root / "canonical" / "features" / "road.json", {
                "features": [{"feature_class": "road"}],
            })
            self.assertEqual(
                "cell-road",
                execution._select_road_dirty_unit(root / "compile_result.json"),
            )

    def test_realization_profile_must_match_owner_map_and_profile_id(self) -> None:
        profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
        settings = profile["profiles"]["kazan"]
        realization_root = PROFILE_PATH.parent.parent / "Realization"
        with tempfile.TemporaryDirectory(dir=realization_root) as directory:
            root = Path(directory)
            realization = root / "wrong.realization.json"
            realization.write_text(json.dumps({
                "profile_id": "wrong",
                "world_data_plugin": "ProjectWorldData",
                "map_package": settings["map_package"],
            }), encoding="utf-8")
            settings["realization_profile"] = realization.relative_to(REPO_ROOT).as_posix()
            candidate = root / "wrong.validation.json"
            candidate.write_text(json.dumps(profile), encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                load_profile(candidate)
        self.assertEqual("realization_profile_mismatch", raised.exception.code)

    def test_clean_rebuild_removes_complete_layer_artifact_roots(self) -> None:
        settings = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        contract = execution._realization_profile_contract(
            settings["realization_profile"],
            settings["world_data_plugin"],
            settings["map_package"],
        )
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            content_root = Path(directory)
            layer_paths = execution._realization_artifact_paths(
                contract, content_root, settings["world_data_plugin"]
            )
            unrelated = content_root / "Generated" / "Twin" / "Unrelated" / "keep.uasset"
            unrelated.parent.mkdir(parents=True)
            unrelated.write_bytes(b"keep")
            for path in layer_paths:
                path.mkdir(parents=True)
                (path / "owned.uasset").write_bytes(b"owned")
            execution._remove_realization_artifacts(
                contract, content_root, settings["world_data_plugin"]
            )
            self.assertTrue(unrelated.is_file())
            self.assertTrue(all(not path.exists() for path in layer_paths))

    def test_matrix_backup_restores_complete_layer_artifact_roots(self) -> None:
        settings = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        contract = execution._realization_profile_contract(
            settings["realization_profile"],
            settings["world_data_plugin"],
            settings["map_package"],
        )
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content_root = root / "Content"
            backup_root = root / "Backup"
            presentation_root = content_root / "Generated" / "Presentation"
            layer_paths = execution._realization_artifact_paths(
                contract, content_root, settings["world_data_plugin"]
            )
            for index, path in enumerate(layer_paths):
                path.mkdir(parents=True)
                (path / "owned.uasset").write_bytes(f"before-{index}".encode())
            moves = execution._backup_generated(
                [], backup_root, content_root, presentation_root,
                settings["world_data_plugin"], layer_paths,
            )
            for path in layer_paths:
                path.mkdir(parents=True)
                (path / "candidate.uasset").write_bytes(b"candidate")
            execution._restore_generated(
                moves, [], content_root, presentation_root,
                settings["world_data_plugin"], layer_paths,
            )
            self.assertTrue(all((path / "owned.uasset").is_file() for path in layer_paths))
            self.assertTrue(all(not (path / "candidate.uasset").exists() for path in layer_paths))

    def test_layered_realization_proves_topology_layers_dirty_closure_and_zero_hlod(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        result = validate_layered_realization(
            receipts,
            expected,
            {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT,
        )
        self.assertEqual(2, result["canonical_cells"])
        self.assertEqual(
            {"terrain", "water", "roads", "buildings", "gameplay"},
            set(result["layers"]),
        )
        self.assertEqual(ROAD_DIRTY_UNIT, result["road_locality_dirty_unit"])

    def test_layered_realization_rejects_sibling_work_during_road_locality(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["road_locality"]["layer_inventories"][0]["final_dirty_units"] = [ROAD_DIRTY_UNIT]
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                expected,
                {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("road_locality_sibling_dirty", raised.exception.code)

    def test_layered_realization_accepts_declared_road_dependent_work(self) -> None:
        expected = deepcopy(load_profile(PROFILE_PATH)["profiles"]["synthetic"])
        expected["expected_layers"].append({
            "layer_id": "vegetation",
            "generator_id": "project_vegetation_instances",
            "generator_version": 1,
            "artifact_root": "/ProjectWorldTestData/Generated/Twin/Vegetation/",
            "canonical_input_count": 2,
            "artifact_count": 0,
            "incremental_dirty_count": 0,
        })
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        for leg, receipt in receipts.items():
            road_dirty = next(
                layer["final_dirty_units"] for layer in receipt["layer_inventories"]
                if layer["layer_id"] == "roads"
            )
            receipt["layer_inventories"].append({
                "layer_id": "vegetation",
                "generator_id": "project_vegetation_instances",
                "generator_version": 1,
                "artifact_root": "/ProjectWorldTestData/Generated/Twin/Vegetation/",
                "normalized_layer_contract_sha256": "f" * 64,
                "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                "dependency_inputs": [{"unit_id": "layer:roads:contract"}],
                "artifacts": [],
                "final_dirty_units": road_dirty,
            })
        validate_layered_realization(
            receipts,
            expected,
            {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT,
        )

    def test_layered_realization_rejects_hlod_or_wrong_realization_contract(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["second"]["hlod_proxy_actor_count"] = 1
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                expected,
                {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("layered_hlod_output_present", raised.exception.code)

        receipts["second"]["hlod_proxy_actor_count"] = 0
        receipts["clean"]["realization_profile_sha256"] = "d" * 64
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                expected,
                {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("realization_profile_mismatch", raised.exception.code)

    def _layered(self, receipts: dict) -> dict:
        return validate_layered_realization(
            receipts,
            load_profile(PROFILE_PATH)["profiles"]["synthetic"],
            {"profile_id": "synthetic_landscape_water_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT,
        )

    def test_layered_realization_rejects_flat_or_wrong_terrain_elevation(self) -> None:
        # The historical territory defect: every structural count, proxy identity, semantic
        # hash, and GeoReferencing XY check passed while the terrain was completely flat.
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["second"]["terrain_source_height_mismatch_count"] = 4096
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_source_height_mismatch", raised.exception.code)

    def test_layered_realization_rejects_incomplete_elevation_coverage(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["incremental"]["terrain_source_height_sample_count"] = 4096
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_source_height_coverage", raised.exception.code)

    def test_layered_realization_rejects_excess_elevation_error(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["clean"]["terrain_source_height_max_error_m"] = 5.0
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_source_height_error", raised.exception.code)

    def test_layered_realization_rejects_reconstruction_height_hash_drift(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["clean"]["terrain_source_height_semantic_sha256"] = "e" * 64
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_source_height_identity_drift", raised.exception.code)

    def test_layered_realization_rejects_invalid_height_identity(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        for leg in LAYERED_LEGS:
            receipts[leg]["terrain_source_height_semantic_sha256"] = "not-a-sha"
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_source_height_identity_invalid", raised.exception.code)

    def test_layered_realization_rejects_flat_final_surface(self) -> None:
        # The exact historical Kazan defect: Generated Base source matched canonical on all
        # 215,040 samples while the final composed heightmap was flat at raw height 0.
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        for leg in LAYERED_LEGS:
            receipts[leg]["terrain_final_height_mismatch_count"] = 8192
            receipts[leg]["terrain_final_relief_m"] = 0.0
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_final_height_mismatch", raised.exception.code)

    def test_layered_realization_rejects_incomplete_final_coverage(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["incremental"]["terrain_final_height_sample_count"] = 4096
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_final_height_coverage", raised.exception.code)

    def test_layered_realization_rejects_excess_final_error(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["clean"]["terrain_final_height_max_error_m"] = 395.8
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_final_height_error", raised.exception.code)

    def test_layered_realization_rejects_invalid_final_identity(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        for leg in LAYERED_LEGS:
            receipts[leg]["terrain_final_height_semantic_sha256"] = "nope"
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_final_height_identity_invalid", raised.exception.code)

    def test_layered_realization_rejects_final_identity_drift(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["clean"]["terrain_final_height_semantic_sha256"] = "a" * 64
        with self.assertRaises(ValidationFailure) as raised:
            self._layered(receipts)
        self.assertEqual("layered_terrain_final_height_identity_drift", raised.exception.code)

    def test_incremental_reuse_scales_to_the_complete_territory(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base = root / "base"
            incremental = root / "incremental"
            cells = [
                {"cell_id": f"cell-{index}", "path": f"canonical/cells/{index}.json"}
                for index in range(210)
            ]
            rebuilt = cells[0]["cell_id"]
            reused = [cell["cell_id"] for cell in cells[1:]]
            output = {
                "path": cells[1]["path"],
                "sha256": "e" * 64,
            }
            for target in (base, incremental):
                (target / "canonical").mkdir(parents=True)
                (target / "reports").mkdir(parents=True)
                (target / "compile_result.json").write_text(
                    json.dumps({"outputs": [output]}), encoding="utf-8"
                )
            (base / "canonical" / "coverage.json").write_text(
                json.dumps({"cells": cells}), encoding="utf-8"
            )
            (incremental / "reports" / "metrics.json").write_text(
                json.dumps({"duration_ms": 1000, "feature_processed_count": 0}), encoding="utf-8"
            )
            (incremental / "reports" / "diff.json").write_text(
                json.dumps({"terrain_rebuilt_cells": [rebuilt], "reused_cells": reused}),
                encoding="utf-8",
            )
            (incremental / "reports" / "validation.json").write_text(
                json.dumps({"checks": [{"check": "boundary", "details": {"mismatches": 0}}]}),
                encoding="utf-8",
            )
            result = _validate_incremental(base, incremental, 300.0)
        self.assertEqual(rebuilt, result["rebuilt_cell"])

    def test_fresh_compile_must_semantically_equal_the_canonical_authority(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            candidate_root = root / "candidate"
            canonical_result = root / "materialized" / "compile_result.json"
            for path in (candidate_root / "canonical" / "terrain", canonical_result.parent / "canonical" / "terrain"):
                path.mkdir(parents=True, exist_ok=True)
            coverage = {
                "profile_id": "territory",
                "world_data_plugin": "ProjectWorldData",
                "grid_id": "grid",
                "grid": {"origin": [0, 0]},
                "cells": [{"cell_id": "grid:x0:y0", "path": "canonical/cells/cell_x0_y0.json"}],
                "feature_count": 1,
                "rejected_feature_count": 0,
                "semantic_hash": "a" * 64,
            }
            artifact = b"same-semantic-terrain"
            digest = hashlib.sha256(artifact).hexdigest()
            output = {"path": "canonical/terrain/cell_x0_y0.json", "sha256": digest}
            for target in (candidate_root, canonical_result.parent):
                (target / "canonical" / "coverage.json").write_text(json.dumps(coverage), encoding="utf-8")
                (target / output["path"]).write_bytes(artifact)
            candidate = {"inputs_hash": "c" * 64, "outputs": [output]}
            (candidate_root / "compile_result.json").write_text(json.dumps(candidate), encoding="utf-8")
            canonical_result.write_text(
                json.dumps({"inputs_hash": "b" * 64, "outputs": [output]}), encoding="utf-8"
            )
            authority = {
                "inputs_hash": "b" * 64,
                "compile_result_sha256": execution.file_hash(canonical_result),
            }
            result = _validate_canonical_authority_candidate(candidate_root, canonical_result, authority)
            self.assertEqual("b" * 64, result["inputs_hash"])
            self.assertTrue(result["provenance_drift"])
            (candidate_root / output["path"]).write_bytes(b"changed-terrain")
            (candidate_root / "compile_result.json").write_text(json.dumps(candidate), encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                _validate_canonical_authority_candidate(candidate_root, canonical_result, authority)
            self.assertEqual("canonical_candidate_artifact_hash_mismatch", raised.exception.code)
            (candidate_root / output["path"]).write_bytes(artifact)
            coverage["semantic_hash"] = "d" * 64
            (candidate_root / "canonical" / "coverage.json").write_text(
                json.dumps(coverage), encoding="utf-8"
            )
            with self.assertRaises(ValidationFailure) as raised:
                _validate_canonical_authority_candidate(candidate_root, canonical_result, authority)
        self.assertEqual("canonical_candidate_mismatch", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
