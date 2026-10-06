from __future__ import annotations

import json
import hashlib
import contextlib
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import ValidationFailure, load_profile
from World.EndToEndValidation.app import execution
from World.EndToEndValidation.app import validation
from World.EndToEndValidation.app.layered_validation import validate_layered_realization
from World.EndToEndValidation.app.profile_inputs import profile_input_files
from World.EndToEndValidation.app.validation import (
    _canonical_bytes_from_result,
    _validate_manifest_artifact_paths,
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
    def test_saved_world_projection_rejects_compiled_building_collision_drift(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            record = {}
            for leg, collision in (("first", "collision-a"), ("clean", "collision-b")):
                parts = {}
                for name, generator, fields in (("map", "map", {"partition_count": "1"}),
                    ("buildings", "project_building_massing", {"collision.0": collision})):
                    projection = json.dumps([{"kind": "actor", "key": name, "fields": fields}],
                        sort_keys=True, separators=(",", ":"))
                    parts[name] = {"generator_id": generator, "generator_version": 1 if name == "map" else 2,
                        "comparison_version": 1, "record_count": 1, "projection_json": projection,
                        "sha256": hashlib.sha256(projection.encode()).hexdigest()}
                path = root / f"{leg}.json"
                path.write_text(json.dumps({"schema": "project-world-saved-projection:v1",
                    "status": "accepted", "map_package": "test-map", **parts}), encoding="utf-8")
                record[f"saved_world_projection_{leg}"] = str(path)
                record[f"saved_world_projection_{leg}_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            with self.assertRaises(ValidationFailure) as raised:
                validation._validate_saved_world_projections(record, "test-map")
            self.assertEqual("saved_world_reconstruction_drift", raised.exception.code)

    def test_saved_world_projection_ignores_actor_allocation_identity(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            record = {}
            for leg in ("first", "clean"):
                parts = {}
                for name, generator in (("map", "map"), ("buildings", "project_building_massing")):
                    projection = json.dumps([{"kind": "actor", "key": name,
                        "fields": {"guid": leg, "name": f"actor-{leg}", "label": leg,
                            "tags": "role=stable", "collision.0": "same"}}],
                        sort_keys=True, separators=(",", ":"))
                    parts[name] = {"generator_id": generator, "generator_version": 1 if name == "map" else 2,
                        "comparison_version": 1, "record_count": 1, "projection_json": projection,
                        "sha256": hashlib.sha256(projection.encode()).hexdigest()}
                path = root / f"{leg}.json"
                path.write_text(json.dumps({"schema": "project-world-saved-projection:v1",
                    "status": "accepted", "map_package": "test-map", **parts}), encoding="utf-8")
                record[f"saved_world_projection_{leg}"] = str(path)
                record[f"saved_world_projection_{leg}_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            result = validation._validate_saved_world_projections(record, "test-map")
            self.assertEqual(result["map"]["first_sha256"], result["map"]["clean_sha256"])
            self.assertEqual(result["buildings"]["first_sha256"], result["buildings"]["clean_sha256"])

    def test_saved_terrain_projection_rejects_compiled_collision_drift(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            record = {"realization_compile_result_sha256": "c" * 64}
            for leg, collision in (("first", "collision-a"), ("clean", "collision-b")):
                projection = json.dumps([{"kind": "section", "key": "cell:0", "collision": collision}],
                    sort_keys=True, separators=(",", ":"))
                receipt = {"schema": "project-world-mesh-terrain-audit:v2", "status": "accepted",
                    "map": "test-map", "terrain_projection_json": projection,
                    "terrain_projection_sha256": hashlib.sha256(projection.encode()).hexdigest(),
                    "terrain_projection_record_count": 1, "compile_result_sha256": "c" * 64}
                path = root / f"{leg}.json"
                path.write_text(json.dumps(receipt), encoding="utf-8")
                record[f"terrain_projection_{leg}"] = str(path)
                record[f"terrain_projection_{leg}_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            with self.assertRaises(ValidationFailure) as raised:
                validation._validate_saved_terrain_projections(record, "test-map")
            self.assertEqual("saved_terrain_reconstruction_drift", raised.exception.code)

    def test_canonical_execution_never_observes_raw_cache(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            preflight = root / "preflight.json"
            preflight.write_text("{}", encoding="utf-8")
            common = {"test_suites": [], "receipt": "fixture", "receipt_sha256": "a" * 64,
                "operation_id": "fixture", "common_contract_sha256": "b" * 64,
                "environment": {"immutable_cache_bytes": 10**12, "network_transfer_bytes": 0}}
            with mock.patch.object(execution, "REPO_ROOT", root), mock.patch.object(
                execution, "_content_mutation_lock", return_value=contextlib.nullcontext()
            ), mock.patch.object(execution, "validate_against"
            ), mock.patch.object(execution, "tree_size", side_effect=AssertionError("raw cache scanned")):
                result = execution.execute({"profiles": {}}, "cache-independent", root, preflight,
                    common, "canonical_authority")
            self.assertNotIn("immutable_cache_bytes", result["environment"])
            self.assertNotIn("network_transfer_bytes", result["environment"])

    def test_clean_mesh_partition_manifest_paths_are_the_only_reallocation_exception(self) -> None:
        first = {"map_territory": {"map/a": "external_actor", "map/world.umap": "map"},
            "layer_kazan_terrain": {"terrain/a": "external_actor", "terrain/b": "external_actor"},
            "layer_kazan_roads": {"roads/a": "external_actor"}}
        clean = {"map_territory": {"map/b": "external_actor", "map/world.umap": "map"},
            "layer_kazan_terrain": {"terrain/c": "external_actor", "terrain/d": "external_actor"},
            "layer_kazan_roads": {"roads/a": "external_actor"}}
        legs = ["first", "second", "road_locality", "clean"]
        _validate_manifest_artifact_paths(legs, [first, first, first, clean], True)
        for rejected in (
            {**clean, "layer_kazan_roads": {"roads/b": "external_actor"}},
            {**clean, "layer_kazan_terrain": {"terrain/c": "external_actor"}},
            {**clean, "map_territory": {"map/b": "external_actor", "map/other.umap": "map"}},
        ):
            with self.assertRaises(ValidationFailure) as raised:
                _validate_manifest_artifact_paths(legs, [first, first, first, rejected], True)
            self.assertEqual("generated_package_path_churn", raised.exception.code)
        with self.assertRaises(ValidationFailure):
            _validate_manifest_artifact_paths(legs, [first, first, first, clean], False)

    def test_promoted_bundle_budget_uses_output_inventory_without_metrics_file(self) -> None:
        receipt = {"outputs": [
            {"path": "canonical/coverage.json", "byte_size": 17},
            {"path": "reports/validation.json", "byte_size": 23},
        ]}
        with mock.patch("World.EndToEndValidation.app.validation._accepted", return_value=receipt):
            self.assertEqual(40, _canonical_bytes_from_result(Path("compile_result.json")))

    def test_canonical_matrix_reaches_authority_without_source_or_compile(self) -> None:
        settings = load_profile(PROFILE_PATH)["profiles"]["kazan"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(execution, "_run", side_effect=AssertionError("upstream command ran")) as upstream:
                with mock.patch.object(execution, "validate_canonical_authority", side_effect=RuntimeError("authority boundary")):
                    with self.assertRaisesRegex(RuntimeError, "authority boundary"):
                        execution._run_profile("kazan", settings, root, root, "powershell", "canonical_authority")
            upstream.assert_not_called()
            self.assertTrue((root / "kazan").is_dir())

    def test_full_matrix_starts_with_source(self) -> None:
        settings = load_profile(PROFILE_PATH)["profiles"]["kazan"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(execution, "_run", side_effect=RuntimeError("source boundary")) as runner:
                with self.assertRaisesRegex(RuntimeError, "source boundary"):
                    execution._run_profile("kazan", settings, root, root, "powershell", "full")
            self.assertEqual("kazan_source", runner.call_args.args[0])

    def _receipt(self, leg: str) -> dict:
        dirty = {
            "first": ["*"],
            "second": [],
            "road_locality": [],
            "incremental": [],
            "clean": [],
        }[leg]
        full = leg in ("first", "clean")
        receipt = {
            "realization_profile": "synthetic_territory_twin",
            "realization_profile_sha256": "a" * 64,
            "canonical_cell_count": 2,
            "sample_spacing_m": [1, 1],
            "georeferencing_placement_error_m": 0,
            "world_partition": True,
            "hlod_proxy_actor_count": 0,
            "hlod_layer_reference_count": 0,
            "hlod_eligible_generated_actor_count": 0,
            "changes": {
                "road_sections": 0,
                "building_sections": 0,
            },
            "layer_inventories": [
                {
                    "layer_id": "terrain",
                    "generator_id": "project_mesh_terrain",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Terrain/",
                    "normalized_layer_contract_sha256": "b" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "artifacts": [
                        {"path": f"Terrain/{index}.uasset", "semantic_sha256": f"{index + 1:064x}"}
                        for index in range(3)
                    ],
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
                    "layer_id": "vegetation",
                    "generator_id": "project_vegetation_instances",
                    "generator_version": 1,
                    "artifact_root": "/ProjectWorldTestData/Generated/Twin/Vegetation/",
                    "normalized_layer_contract_sha256": "d" * 64,
                    "canonical_inputs": [{"unit_id": f"cell-{index}"} for index in range(2)],
                    "artifacts": [{"path": f"Vegetation/{index}.uasset"} for index in range(2)],
                    "final_dirty_units": dirty,
                },
                {
                    "layer_id": "buildings",
                    "generator_id": "project_building_massing",
                    "generator_version": 2,
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
        contracts = load_profile(PROFILE_PATH)["profiles"]["synthetic"]["expected_layers"]
        expected_metrics = {item["layer_id"]: item["metrics"] for item in contracts}
        for inventory in receipt["layer_inventories"]:
            inventory["metrics"] = {
                key: value if full or not key.endswith("_written") else 0
                for key, value in expected_metrics[inventory["layer_id"]].items()
            }
        return receipt

    def test_territory_profile_authenticates_layered_realization_inputs(self) -> None:
        profile = load_profile(PROFILE_PATH)
        self.assertEqual("kazan_territory_v1", profile["profile_id"])
        self.assertEqual(210, profile["profiles"]["kazan"]["expected_topology"]["canonical_cells"])
        layers = {item["layer_id"]: item for item in profile["profiles"]["kazan"]["expected_layers"]}
        vegetation_metrics = layers["vegetation"]["metrics"]
        self.assertEqual(146, vegetation_metrics["cell_actors"])
        self.assertEqual(279, vegetation_metrics["components"])
        self.assertEqual(7501, vegetation_metrics["instances"])
        self.assertEqual(7528, vegetation_metrics["candidates"])
        self.assertEqual(7, vegetation_metrics["road_exclusions"])
        self.assertEqual(19, vegetation_metrics["water_exclusions"])
        self.assertEqual(1, vegetation_metrics["authored_mask_exclusions"])
        building_metrics = layers["buildings"]["metrics"]
        self.assertEqual(171, building_metrics["cell_actors"])
        self.assertEqual(665006, building_metrics["triangles"])
        self.assertEqual(31927, building_metrics["candidate_fragments"])
        self.assertEqual(31764, building_metrics["accepted_fragments"])
        self.assertEqual(3, layers["gameplay"]["metrics"]["placement_actors"])
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
        self.assertEqual("synthetic_territory_twin", realization["profile_id"])
        self.assertEqual(
            ["/ProjectWorldTestData/Authored/Fixtures/L_ProjectWorldMarker"],
            authored["packages"],
        )
        self.assertIn(
            "Plugins/World/ProjectWorldTestData/Content/Authored/Fixtures/L_ProjectWorldMarker.umap",
            inputs,
        )
        self.assertIn(
            "Plugins/World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_territory_twin.realization.json",
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
            layer_paths = execution._realization_artifact_paths(
                contract, content_root, settings["world_data_plugin"]
            )
            for index, path in enumerate(layer_paths):
                path.mkdir(parents=True)
                (path / "owned.uasset").write_bytes(f"before-{index}".encode())
            moves = execution._backup_generated(
                [], backup_root, content_root,
                settings["world_data_plugin"], layer_paths,
            )
            for path in layer_paths:
                path.mkdir(parents=True)
                (path / "candidate.uasset").write_bytes(b"candidate")
            execution._restore_generated(
                moves, [], content_root,
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
            {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT,
        )
        self.assertEqual(2, result["canonical_cells"])
        self.assertEqual(
            {"terrain", "water", "roads", "vegetation", "buildings", "gameplay"},
            set(result["layers"]),
        )
        self.assertEqual(ROAD_DIRTY_UNIT, result["road_locality_dirty_unit"])

    def test_canonical_matrix_proves_world_legs_without_compiler_incremental(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS if leg != "incremental"}
        result = validate_layered_realization(
            receipts,
            expected,
            {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT,
            require_incremental=False,
        )
        self.assertEqual(ROAD_DIRTY_UNIT, result["road_locality_dirty_unit"])

    def test_clean_mesh_terrain_package_reallocation_keeps_semantics(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS if leg != "incremental"}
        clean_terrain = next(item for item in receipts["clean"]["layer_inventories"] if item["layer_id"] == "terrain")
        for index, artifact in enumerate(clean_terrain["artifacts"]):
            artifact["path"] = f"Terrain/rebuilt-{index}.uasset"
        validate_layered_realization(
            receipts, expected, {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT, require_incremental=False,
        )
        clean_terrain["artifacts"][0]["semantic_sha256"] = "f" * 64
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts, expected, {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT, require_incremental=False,
            )
        self.assertEqual("layer_reconstruction_semantic_drift", raised.exception.code)

    def test_road_locality_rejects_mesh_terrain_path_churn(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS if leg != "incremental"}
        road_terrain = next(item for item in receipts["road_locality"]["layer_inventories"] if item["layer_id"] == "terrain")
        road_terrain["artifacts"][0]["path"] = "Terrain/changed.uasset"
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts, expected, {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT, require_incremental=False,
            )
        self.assertEqual("layer_artifact_path_churn", raised.exception.code)

    def test_mesh_terrain_receipt_separates_producer_and_builder_artifacts(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        terrain_contract = next(item for item in expected["expected_layers"] if item["layer_id"] == "terrain")
        terrain_contract["producer_artifact_count"] = 3
        terrain_contract["artifact_count"] = 4
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS if leg != "incremental"}
        for leg in ("second", "road_locality"):
            terrain = next(item for item in receipts[leg]["layer_inventories"] if item["layer_id"] == "terrain")
            terrain["artifacts"].append({"path": "Terrain/compiled.uasset", "semantic_sha256": "9" * 64})
        validate_layered_realization(
            receipts, expected, {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
            ROAD_DIRTY_UNIT, require_incremental=False,
        )

    def test_layered_realization_rejects_sibling_work_during_road_locality(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["road_locality"]["layer_inventories"][0]["final_dirty_units"] = [ROAD_DIRTY_UNIT]
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                expected,
                {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("road_locality_sibling_dirty", raised.exception.code)

    def test_layered_realization_accepts_declared_road_dependent_work(self) -> None:
        expected = load_profile(PROFILE_PATH)["profiles"]["synthetic"]
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        for leg, receipt in receipts.items():
            road_dirty = next(
                layer["final_dirty_units"] for layer in receipt["layer_inventories"]
                if layer["layer_id"] == "roads"
            )
            vegetation = next(
                layer for layer in receipt["layer_inventories"]
                if layer["layer_id"] == "vegetation"
            )
            vegetation["dependency_inputs"] = [{"unit_id": "layer:roads:contract"}]
            vegetation["final_dirty_units"] = road_dirty
        validate_layered_realization(
            receipts,
            expected,
            {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
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
                {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("layered_hlod_output_present", raised.exception.code)

        receipts["second"]["hlod_proxy_actor_count"] = 0
        receipts["clean"]["realization_profile_sha256"] = "d" * 64
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                expected,
                {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("realization_profile_mismatch", raised.exception.code)

    def test_layered_realization_rejects_missing_mesh_terrain_section(self) -> None:
        receipts = {leg: self._receipt(leg) for leg in LAYERED_LEGS}
        receipts["clean"]["layer_inventories"][0]["metrics"]["sections"] = 1
        with self.assertRaises(ValidationFailure) as raised:
            validate_layered_realization(
                receipts,
                load_profile(PROFILE_PATH)["profiles"]["synthetic"],
                {"profile_id": "synthetic_territory_twin", "sha256": "a" * 64},
                ROAD_DIRTY_UNIT,
            )
        self.assertEqual("layer_metrics_mismatch", raised.exception.code)

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
