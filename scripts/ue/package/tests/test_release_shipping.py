from __future__ import annotations

import copy
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "scripts/ue/package"))
SPEC = importlib.util.spec_from_file_location("release_shipping", ROOT / "scripts/ue/package/release_shipping.py")
import release_shipping


class ShippingProofFixture:
    def __init__(self, root: Path, machine_identity: dict | None = None,
                 material_records: list[dict] | None = None, manhattan_runtime_hash: str = "f" * 64) -> None:
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)
        self.module = release_shipping
        self.machine = {"status": "accepted", "revision": "a" * 40,
                        "source_state_sha256": "b" * 64, "shipping_executable_sha256": "c" * 64,
                        "shipping_package_sha256": "d" * 64, "operation_id": "kazan-operation",
                        "development_package_sha256": "1" * 64, "development_executable_sha256": "2" * 64,
                        "runtime_profile_sha256": "e" * 64, "shipping_exit_code": 0,
                        "shipping_water_exit_code": 0, "artifacts": {}, "artifact_sha256": {}}
        if machine_identity:
            self.machine.update(machine_identity)
        self.material_manifest = {"records": []}
        self.bindings = []
        self.logs = []
        for role, binding, parameter in (("terrain", "terrain.default", "SurfaceStructure"),
                                         ("roads", "road.default_asphalt", "SurfaceSignals"),
                                         ("buildings", "building.default_concrete", "SurfaceSignals")):
            pattern = f"/ProjectTexture/Patterns/{role}/DA_Pattern.RT_Primary"
            material = f"/ProjectMaterial/Surfaces/{role}/MI_Default.MI_Default"
            identity = (role[0].encode().hex() * 64)[:64]
            if material_records is not None:
                record = next(row for row in material_records if row.get("binding_id") == binding)
                pattern, material, identity = (record[key] for key in
                    ("pattern_object_path", "output_object_path", "pattern_semantic_identity"))
            self.material_manifest["records"].append({"binding_id": binding, "output_object_path": material,
                "pattern_object_path": pattern, "pattern_semantic_identity": identity})
            self.bindings.append({"role": role, "material_chain": [material], "parameter": parameter,
                                  "texture": pattern, "resource_ready": True, "width": 512, "height": 512})
            self.logs.append(f"[RuntimeCache::ColdWarmProof] identity={identity} node=Primary "
                f"cold_request_to_ready_ms=1.25 warm_request_to_ready_ms=0.01 cold_node_generations=2 "
                f"warm_node_generations=0 warm_cache_hits=1 same_resource=1 warm_ready=1 "
                f"resident_bytes=100 peak_bytes=200 target={pattern}")
        self.log = "[RuntimeCache::Initialize] Fixed global_budget_bytes=1000\n" + "\n".join(self.logs)
        manifest = self.root / "Plugins/Resources/ProjectMaterial/Data/Manifests/Surfaces/accepted.surface-manifest.json"
        manifest.parent.mkdir(parents=True)
        manifest.write_text(json.dumps(self.material_manifest))
        for city in ("kazan", "manhattan"):
            city_doc = self.machine if city == "kazan" else {
                **{key: self.machine[key] for key in ("status", "revision", "source_state_sha256",
                   "shipping_executable_sha256", "shipping_package_sha256", "shipping_exit_code", "shipping_water_exit_code")},
                "operation_id": "manhattan-operation", "runtime_profile_sha256": manhattan_runtime_hash,
                "artifacts": {}, "artifact_sha256": {}}
            map_package, runtime = self.module.CITIES[city]
            correctness = {"status": "accepted", "operation_id": city_doc["operation_id"],
                "map_package": map_package, "runtime_profile": runtime,
                "runtime_profile_sha256": city_doc["runtime_profile_sha256"], "build_configuration": "Shipping",
                "project_loading_provenance": True, "possessed_player": True, "normal_movement": True,
                "terrain_collision": True, "road_collision": True, "building_collision": True,
                "center_unloaded_at_edge": True, "edge_loaded": True, "center_reloaded": True,
                "preview_flight_restored": True, "material_texture_binding_scope":
                    "engine_parameter_readback_not_rendered_consumption", "material_texture_bindings": self.bindings,
                "gameplay_interaction_required": city == "kazan", "gameplay_interaction": city == "kazan"}
            reference = {"schema": "project-world-building-surface-reference:v1", "status": "accepted",
                "map_package": map_package, "runtime_profile_sha256": city_doc["runtime_profile_sha256"], "samples": []}
            for index in range(3):
                for kind in ("wall", "roof"):
                    reference["samples"].append({"kind": kind, "mesh": f"{map_package.rsplit('/', 1)[0]}/Buildings/SM_{index}.SM_{index}",
                        "source_triangle": index, "normal": [0, 0, 1], "reference_hit_cm": [0, 0, 5.228],
                        "source_target_cm": [0, 0, 0], "source_error_cm": 5.228})
            self._artifact(city_doc, "shipping_building_reference", reference, city)
            correctness["building_surface_evidence"] = {"schema": "project-world-building-surface-evidence:v2",
                "status": "accepted", "reference_sha256": city_doc["artifact_sha256"]["shipping_building_reference"],
                "reference_tolerance_cm": 0.05, "maximum_reference_error_cm": 0.0, "maximum_source_error_cm": 5.228,
                "capsule_query": "UWorld::ComponentSweepMulti", "capsule_radius_cm": 42.0, "capsule_half_height_cm": 96.0,
                "capsule_pawn_class": "/Script/ProjectCharacter.DefinitionCharacter", "capsule_query_channel": 2,
                "samples": [{**row, "cooked_hit_cm": [0, 0, 5.228], "reference_error_cm": 0.0,
                             "ray_query_status": "hit_named_component", "ray_query_returned_hit": True,
                             "cooked_hit_normal": [0, 0, 1], "capsule_query_status": "blocked_named_component",
                             "capsule_hit_mesh": row["mesh"], "capsule_blocking_hit": True, "capsule_start_penetrating": False,
                             "capsule_hit_count": 1, "capsule_other_blocking_hit_count": 0, "capsule_initial_penetration_count": 0,
                             "capsule_hit_cm": [0, 0, 5.228], "capsule_hit_normal": [0, 0, 1], "capsule_hit_time": 0.5,
                             "capsule_source_plane_error_cm": 5.228, "capsule_reference_plane_error_cm": 0.0,
                             "cooked_source_error_cm": 5.228} for row in reference["samples"]]}
            self._artifact(city_doc, "shipping_correctness", correctness, city)
            self._artifact(city_doc, "shipping_product_screenshot", b"viewport pixels", city)
            correctness["screenshot"] = city_doc["artifacts"]["shipping_product_screenshot"]
            self._artifact(city_doc, "shipping_correctness", correctness, city)
            water = {"status": "accepted", "operation_id": city_doc["operation_id"], "map_package": map_package,
                "runtime_profile": runtime, "runtime_profile_sha256": city_doc["runtime_profile_sha256"],
                "build_configuration": "Shipping", "capture_encoding": "visible_water_depth_mask_v1",
                "capture_source": "SCS_SceneColorSceneDepth", "final_color_capture_source": "SCS_FinalColorLDR",
                "reference_captured": True, "repeat_captured": True, "final_color_captured": True,
                "same_capture_session": True, "target_xy_error_cm": 0, "allowed_target_xy_error_cm": 25000,
                "requested_water_target_cm": [0, -80000, 0]}
            self._artifact(city_doc, "shipping_water_proof", water, city)
            self._artifact(city_doc, "shipping_water_viewpoint_authority", {"status": "accepted", "feature_class": "water",
                "target_unreal_xy_cm": [0, -80000], "feature_id": "alis:osm:relation:" +
                    ("7493502" if city == "kazan" else "2389611")}, city)
            for key in ("shipping_water_reference", "shipping_water_repeat", "shipping_water_product"):
                self._artifact(city_doc, key, b"pixels", city)
            water.update({"reference_screenshot": city_doc["artifacts"]["shipping_water_reference"],
                          "repeat_screenshot": city_doc["artifacts"]["shipping_water_repeat"],
                          "final_color_screenshot": city_doc["artifacts"]["shipping_water_product"]})
            self._artifact(city_doc, "shipping_water_proof", water, city)
            self._artifact(city_doc, "shipping_water_temporal", {"status": "accepted",
                "capture_encoding": "visible_water_depth_mask_v1", "failures": [],
                "gates": {"minimum_blue_pixels": 4096, "maximum_blue_classification_flip_ratio": 0.001,
                          "maximum_mean_blue_channel_delta": 1.5},
                "metrics": {"invalid_mask_pixels": 0, "blue_pixel_union": 4096,
                            "blue_classification_flip_ratio": 0.0, "mean_blue_channel_delta": 0.0},
                **{arm: {"sha256": city_doc["artifact_sha256"][f"shipping_water_{arm}"],
                         "path": city_doc["artifacts"][f"shipping_water_{arm}"]}
                   for arm in ("reference", "repeat")}}, city)
            development = copy.deepcopy(correctness)
            development["build_configuration"] = "Development"
            aggregate = {"schema_version": 1, "status": "accepted", "execution_count": 3,
                "children": [{}, {}, {}], "total_sample_count": 900, "streaming_failures": 0,
                "base_frame_p95_budget_ms": 16.67, "frame_p95_budget_ms": 16.67, "frame_p95_ms": 10.0,
                "source_revision": self.machine["revision"], "source_state_sha256": self.machine["source_state_sha256"],
                "runtime_profile_sha256": city_doc["runtime_profile_sha256"],
                "development_package_sha256": self.machine["development_package_sha256"],
                "development_executable_sha256": self.machine["development_executable_sha256"]}
            if city == "kazan":
                self._artifact(self.machine, "development_performance_aggregate", aggregate, city)
                self._artifact(self.machine, "development_run_01_correctness", development, city)
                self._artifact(self.machine, "development_run_01_log", self.log.encode(), city)
            else:
                development_doc = {"runtime_profile_sha256": manhattan_runtime_hash, "performance": aggregate,
                                   "artifacts": {}, "artifact_sha256": {}}
                self._artifact(development_doc, "run-01_correctness", development, city)
                self._artifact(development_doc, "run-01_log", self.log.encode(), city)
                self._artifact(self.machine, "manhattan_development_performance", development_doc, city)
                self._artifact(self.machine, "manhattan_shipping_proof", city_doc, city)

    def _artifact(self, document: dict, key: str, value: dict | bytes, city: str) -> None:
        path = self.root / city / f"{key}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode())
        document["artifacts"][key] = str(path)
        document["artifact_sha256"][key] = self.module.file_hash(path)


def add_shipping_fixture(machine: dict, root: Path, source_root: Path) -> dict:
    records = json.loads((source_root / release_shipping.MATERIAL_MANIFEST).read_text())["records"]
    old_artifacts = machine.get("artifacts", {}).copy()
    old_manhattan = (json.loads(Path(old_artifacts["manhattan_development_performance"]).read_text())
                     if "manhattan_development_performance" in old_artifacts else None)
    identity = {key: value for key, value in machine.items()
                if key not in ("artifacts", "artifact_sha256", "performance_review")}
    fixture = ShippingProofFixture(root, identity, records,
        old_manhattan["runtime_profile_sha256"] if old_manhattan else "f" * 64)
    if not old_manhattan:
        return {**machine, **fixture.machine}
    for key, value in fixture.machine.items():
        if key not in ("artifacts", "artifact_sha256"):
            machine.setdefault(key, value)
    for key, path in fixture.machine["artifacts"].items():
        if key not in ("development_performance_aggregate", "manhattan_development_performance"):
            machine["artifacts"][key] = path
            machine["artifact_sha256"][key] = fixture.machine["artifact_sha256"][key]
    fresh_manhattan = json.loads(Path(fixture.machine["artifacts"]["manhattan_development_performance"]).read_text())
    old_manhattan["artifacts"].update(fresh_manhattan["artifacts"])
    old_manhattan["artifact_sha256"].update(fresh_manhattan["artifact_sha256"])
    path = Path(old_artifacts["manhattan_development_performance"])
    path.write_text(json.dumps(old_manhattan))
    machine["artifact_sha256"]["manhattan_development_performance"] = release_shipping.file_hash(path)
    from release_performance import validate_machine_performance
    machine["performance_review"] = validate_machine_performance(machine, True, source_root, require_recorded=False)
    return machine


class ReleaseShippingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.module = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(cls.module)

    def setUp(self) -> None:
        scratch = ROOT / "tmp/world/release_completion_d54/tests/shipping"
        scratch.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        fixture = ShippingProofFixture(self.root)
        self.machine = fixture.machine
        self.log = fixture.log
        self._artifact = fixture._artifact

    def test_both_cities_pass_without_performance_review_and_preserve_source_plane_error(self) -> None:
        result = self.module.validate_shipping_cities(self.machine, self.root)
        self.assertEqual({"kazan", "manhattan"}, set(result))
        self.assertEqual(5.228, result["manhattan"]["maximum_source_error_cm"])

    def test_accepted_machine_cannot_omit_manhattan_shipping(self) -> None:
        del self.machine["artifacts"]["manhattan_shipping_proof"]
        del self.machine["artifact_sha256"]["manhattan_shipping_proof"]
        with self.assertRaisesRegex(ValueError, "Manhattan Shipping"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_nested_artifact_mutation_refuses(self) -> None:
        (self.root / "manhattan/shipping_water_product.json").write_bytes(b"changed pixels")
        with self.assertRaisesRegex(ValueError, "artifact changed"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_other_shipping_package_identity_refuses_even_with_rehashed_child(self) -> None:
        path = Path(self.machine["artifacts"]["manhattan_shipping_proof"])
        child = json.loads(path.read_text())
        child["shipping_package_sha256"] = "0" * 64
        self._artifact(self.machine, "manhattan_shipping_proof", child, "manhattan")
        with self.assertRaisesRegex(ValueError, "another candidate"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_api_readback_without_cache_log_refuses(self) -> None:
        self._artifact(self.machine, "development_run_01_log", b"API resource_ready=true", "kazan")
        with self.assertRaisesRegex(ValueError, "cache"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def _secondary_cache_inputs(self) -> tuple[dict, list[dict], str]:
        correctness = json.loads(Path(self.machine["artifacts"]["development_run_01_correctness"]).read_text())
        records = json.loads((self.root / self.module.MATERIAL_MANIFEST).read_text())["records"]
        log = self.log
        for primary in list(correctness["material_texture_bindings"]):
            secondary = primary["texture"].rsplit(".", 1)[0] + ".RT_Normal"
            correctness["material_texture_bindings"].append({**primary, "parameter": "SurfaceNormal", "texture": secondary})
            log = log.replace("target=" + primary["texture"], "target=" + secondary)
        return correctness, records, log

    def test_cache_accepts_requested_bound_secondary_bundle_outputs(self) -> None:
        correctness, records, log = self._secondary_cache_inputs()
        result = self.module._cache(correctness, log, records)
        self.assertEqual({"terrain", "roads", "buildings"}, {row["role"] for row in result})
        self.assertTrue(all(row["target"].endswith(".RT_Normal") for row in result))

    def test_cache_unbound_same_package_output_refuses(self) -> None:
        correctness, records, log = self._secondary_cache_inputs()
        with self.assertRaisesRegex(ValueError, "cache"):
            self.module._cache(correctness, log.replace(".RT_Normal", ".RT_Unbound"), records)

    def test_cache_bound_foreign_package_output_refuses(self) -> None:
        correctness, records, log = self._secondary_cache_inputs()
        for item in correctness["material_texture_bindings"]:
            if item["parameter"] == "SurfaceNormal":
                foreign = item["texture"].replace("/DA_Pattern.", "/DA_Foreign.")
                log = log.replace("target=" + item["texture"], "target=" + foreign)
                item["texture"] = foreign
        with self.assertRaisesRegex(ValueError, "cache"):
            self.module._cache(correctness, log, records)

    def test_cache_secondary_output_still_requires_accepted_identity(self) -> None:
        correctness, records, log = self._secondary_cache_inputs()
        log = log.replace("identity=" + records[0]["pattern_semantic_identity"], "identity=" + "0" * 64)
        with self.assertRaisesRegex(ValueError, "cache"):
            self.module._cache(correctness, log, records)

    def test_cache_unready_secondary_output_refuses(self) -> None:
        correctness, records, log = self._secondary_cache_inputs()
        correctness["material_texture_bindings"][-1]["resource_ready"] = False
        with self.assertRaisesRegex(ValueError, "cache"):
            self.module._cache(correctness, log, records)

    def test_warm_regeneration_or_unknown_measurement_cannot_pass(self) -> None:
        for before, after in (("warm_node_generations=0", "warm_node_generations=1"),
                              ("warm_ready=1", "warm_ready=0"),
                              ("cold_request_to_ready_ms=1.25", "cold_request_to_ready_ms=nan")):
            with self.subTest(after=after):
                self._artifact(self.machine, "development_run_01_log", self.log.replace(before, after).encode(), "kazan")
                with self.assertRaisesRegex(ValueError, "cache"):
                    self.module.validate_shipping_cities(self.machine, self.root)

    def test_changed_named_roof_hit_cannot_hide_behind_collision_boolean(self) -> None:
        path = Path(self.machine["artifacts"]["shipping_correctness"])
        receipt = json.loads(path.read_text())
        receipt["building_surface_evidence"]["samples"][1]["cooked_hit_cm"][2] += 0.06
        self._artifact(self.machine, "shipping_correctness", receipt, "kazan")
        with self.assertRaisesRegex(ValueError, "building"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_ray_only_or_invalid_capsule_evidence_cannot_close_building_acceptance(self) -> None:
        path = Path(self.machine["artifacts"]["shipping_correctness"])
        original = json.loads(path.read_text())
        for key, value in (("ray_query_status", "miss"), ("capsule_query_status", "no_blocking_hit"), ("capsule_start_penetrating", True),
                           ("capsule_hit_normal", [0, 0, 0]), ("cooked_hit_normal", [float("nan"), 0, 1]),
                           ("capsule_hit_mesh", "another mesh"), ("capsule_source_plane_error_cm", 0.0)):
            with self.subTest(key=key):
                receipt = json.loads(json.dumps(original))
                receipt["building_surface_evidence"]["samples"][0][key] = value
                self._artifact(self.machine, "shipping_correctness", receipt, "kazan")
                with self.assertRaises(ValueError):
                    self.module.validate_shipping_cities(self.machine, self.root)
        receipt = json.loads(json.dumps(original))
        receipt["building_surface_evidence"].pop("capsule_query")
        self._artifact(self.machine, "shipping_correctness", receipt, "kazan")
        with self.assertRaisesRegex(ValueError, "building"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_native_water_mask_or_pixel_budget_changes_refuse(self) -> None:
        path = Path(self.machine["artifacts"]["shipping_water_temporal"])
        original = json.loads(path.read_text())
        for key, value in (("capture_encoding", "isolated_water_occupancy"),
                           ("gates", {"minimum_blue_pixels": 0}),
                           ("metrics", {"invalid_mask_pixels": 0, "blue_pixel_union": 0,
                                        "blue_classification_flip_ratio": 0, "mean_blue_channel_delta": 0})):
            with self.subTest(key=key):
                self._artifact(self.machine, "shipping_water_temporal", {**original, key: value}, "kazan")
                with self.assertRaisesRegex(ValueError, "Water"):
                    self.module.validate_shipping_cities(self.machine, self.root)

    def test_cache_measurements_from_an_old_source_candidate_refuse(self) -> None:
        path = Path(self.machine["artifacts"]["development_performance_aggregate"])
        aggregate = json.loads(path.read_text())
        aggregate["source_revision"] = "0" * 40
        self._artifact(self.machine, "development_performance_aggregate", aggregate, "kazan")
        with self.assertRaisesRegex(ValueError, "another source/package"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_manhattan_wrong_map_refuses_after_inventory_is_reauthenticated(self) -> None:
        path = Path(self.machine["artifacts"]["manhattan_shipping_proof"])
        document = json.loads(path.read_text())
        correctness = json.loads(Path(document["artifacts"]["shipping_correctness"]).read_text())
        correctness["map_package"] = self.module.CITIES["kazan"][0]
        self._artifact(document, "shipping_correctness", correctness, "manhattan")
        self._artifact(self.machine, "manhattan_shipping_proof", document, "manhattan")
        with self.assertRaisesRegex(ValueError, "gameplay/map/runtime"):
            self.module.validate_shipping_cities(self.machine, self.root)

    def test_other_pixel_paths_cannot_replace_the_native_capture_artifacts(self) -> None:
        path = Path(self.machine["artifacts"]["shipping_water_proof"])
        water = json.loads(path.read_text())
        water["reference_screenshot"] = self.machine["artifacts"]["shipping_water_product"]
        self._artifact(self.machine, "shipping_water_proof", water, "kazan")
        with self.assertRaisesRegex(ValueError, "another capture"):
            self.module.validate_shipping_cities(self.machine, self.root)


if __name__ == "__main__":
    unittest.main()
