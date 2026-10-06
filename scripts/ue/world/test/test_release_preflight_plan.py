# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
"""Release impact and contract refusal proofs; no build or generated mutation."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from release_preflight_plan import REPO_ROOT, release_world_plan, world_owners
from validate_release_contracts import validate_contracts
from World.EndToEndValidation.tests.proof_input_fixture import write_producers
from World.EndToEndValidation.app.planning import plan_for_paths
from World.EndToEndValidation.app.contract_inputs import COMMON_TEST_PROFILE_IDS
from World.EndToEndValidation.app.profile_inputs import executable_profile_inputs


class ReleaseWorldImpactTests(unittest.TestCase):
    def test_production_plans_match_direct_world_planner(self):
        base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True).strip()
        paths = (
            "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldBuildingRealization.cpp",
            "scripts/ue/world/realize_canonical_world.ps1",
            "tools/World/CanonicalCompilation/app/pipeline.py",
            "tools/World/SourceIngestion/app/acquisition.py",
            "tools/World/EndToEndValidation/app/acceptance.py",
            "Plugins/World/ProjectWorldData/Content/Generated/Kazan.umap",
            "Plugins/World/ProjectWorldMeshTerrain/Content/Terrain/MPD_ProjectTerrain_Shared_v1.uasset",
            "Plugins/World/ProjectWorldData/Data/Manifests/active.json",
            "Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json",
            "Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_512_1536_v1.json",
            "tools/World/CanonicalCompilation/contracts/compiler-profile.schema.json",
            "tools/World/ExecutionEnvironment/requirements.txt",
            "scripts/ue/package/public_world_projection.ps1",
            "tools/World/CanonicalCompilation/testsupport/pipeline.py",
        )
        # Reuse the real immutable profile closure in this table. Only the
        # composition changes between cases; dependency discovery has its own tests.
        inputs = executable_profile_inputs(REPO_ROOT, COMMON_TEST_PROFILE_IDS)
        with mock.patch("World.EndToEndValidation.app.planning.executable_profile_inputs", return_value=inputs):
            for path in paths:
                snapshot = {"requested_base": "HEAD", "base": base, "changed_paths": [path]}
                with self.subTest(path=path), mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
                    plan = release_world_plan("HEAD")
                    expected = plan_for_paths([path], REPO_ROOT, base)
                    self.assertEqual(expected, plan["world_plan"])
                    if expected["l1_required"]:
                        self.assertIn("WorldContracts", plan["owners"])
                    self.assertFalse(plan["world_gates_executed"])

    def test_proof_only_and_mixed_paths_preserve_production_impact(self):
        proofs = ["scripts/ue/world/test/fixture.ps1", "scripts/ue/package/tests/fixture.py",
                  "tools/World/CanonicalCompilation/tests/test_pipeline.py",
                  "tools/World/EndToEndValidation/tests/test_planning.py",
                  "scripts/ue/test/unit/run_single.ps1", "docs/world/contract.md",
                  "todo/00_current/world.md", "scripts/ue/package/run_release_preflight.ps1"]
        snapshot = {"requested_base": "HEAD", "base": "HEAD", "changed_paths": proofs}
        with mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
            self.assertIsNone(release_world_plan("HEAD")["world_plan"])
        source = "tools/World/CanonicalCompilation/app/pipeline.py"
        snapshot["changed_paths"] = proofs + [source]
        with mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
            self.assertEqual(plan_for_paths([source], REPO_ROOT, "HEAD"), release_world_plan("HEAD")["world_plan"])

    def test_actual_git_profile_plans_select_public_runtime_and_schema_owners(self):
        scratch = REPO_ROOT / "tmp/world/release_preflight/tests"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            write_producers(root)
            for arguments in (("init", "--quiet"), ("add", "--", "."),
                              ("-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid",
                               "commit", "--quiet", "-m", "fixture")):
                subprocess.run(["git", "-C", str(root), *arguments], check=True, capture_output=True)
            for relative, expected in (
                ("Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_public_v1.realization.json", ["Projection", "WorldContracts"]),
                ("Plugins/World/ProjectWorldData/Data/Profiles/Realization/manhattan_showcase_public_v1.realization.json", ["Projection", "WorldContracts"]),
                ("Plugins/World/ProjectWorldData/Data/Runtime/runtime.json", ["PerformanceEvidence", "Projection", "WorldContracts", "WorldRuntime"]),
                ("Plugins/World/ProjectWorld/Data/Schemas/project_world_runtime_profile.schema.json", ["Projection", "WorldContracts"]),
                ("tools/World/CanonicalCompilation/contracts/compiler-profile.schema.json", ["WorldContracts"]),
            ):
                with self.subTest(path=relative):
                    path = root / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("{}", encoding="utf-8")
                    process = subprocess.run([sys.executable, str(Path(__file__).with_name("release_preflight_plan.py")),
                                              "--base=HEAD", "--repo-root", str(root)], check=True, capture_output=True, text=True)
                    plan = json.loads(process.stdout)
                    self.assertEqual([relative], plan["changed_paths"])
                    self.assertEqual(expected, plan["owners"])
                    self.assertFalse(plan["world_gates_executed"])
                    path.unlink()

    def test_public_private_and_future_realization_profiles_select_projection_not_private_pie(self):
        for name in ("kazan_territory_public_v1", "manhattan_showcase_public_v1", "kazan_territory_v1", "future_world"):
            path = f"Plugins/World/ProjectWorldData/Data/Profiles/Realization/{name}.realization.json"
            with self.subTest(path=path):
                self.assertEqual(["Projection", "WorldContracts"], world_owners([path]))

    def test_consumers_and_schemas_select_their_actual_checks(self):
        prefix = "Plugins/World/ProjectWorldData/Data/"
        projection = ["Projection", "WorldContracts"]
        runtime = ["PerformanceEvidence", "Projection", "WorldContracts", "WorldRuntime"]
        cases = {
            prefix + "Runtime/kazan_territory_512_1536_v1.json": runtime,
            prefix + "Runtime/manhattan_showcase_512_1536_v1.json": runtime,
            prefix + "Presentation/kazan_representative_v1.json": projection,
            prefix + "Authored/kazan_territory_public_v1.authored.json": projection,
            "Plugins/World/ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json": projection,
            "Plugins/World/ProjectWorld/Data/Schemas/project_world_runtime_profile.schema.json": projection,
            "Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json": projection,
            "Plugins/World/ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json": projection,
            prefix + "Profiles/EndToEndValidation/kazan_territory_v1.validation.json": ["WorldContracts"],
            "Plugins/World/ProjectWorld/Source/ProjectWorld/Private/Driver.cpp": ["PerformanceEvidence", "WorldRuntime"],
            "Plugins/World/ProjectWorld/ProjectWorld.uplugin": ["PerformanceEvidence", "WorldRuntime"],
            "Plugins/World/ProjectWorldData/Content/Generated/World.umap": ["PerformanceEvidence", "WorldRuntime"],
            "docs/build/packaging_guide.md": [],
            "scripts/ue/world/test/README.md": [],
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(expected, world_owners([path]))

    def test_profile_plan_preserves_existing_world_gate_requirements_without_executing_them(self):
        path = "Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json"
        snapshot = {"requested_base": "HEAD", "base": "a" * 40, "changed_paths": [path]}
        with mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
            plan = release_world_plan("HEAD")
        self.assertIn("kazan_territory_v1", plan["world_plan"]["l2_matrices"])
        self.assertFalse(plan["world_gates_executed"])
        self.assertNotIn("WorldRuntime", plan["owners"])

    def test_release_fixture_edit_does_not_select_a_generation_matrix(self):
        path = "scripts/ue/package/run_release_preflight.ps1"
        snapshot = {"requested_base": "HEAD", "base": "a" * 40, "changed_paths": [path]}
        with mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
            plan = release_world_plan("HEAD")
        self.assertIsNone(plan["world_plan"])
        self.assertFalse(plan["world_gates_executed"])

    def test_toolkit_schemas_preserve_existing_world_gate_metadata(self):
        for path in ("tools/World/CanonicalCompilation/contracts/compiler-profile.schema.json",
                     "tools/World/SourceIngestion/contracts/source-profile.schema.json",
                     "tools/World/EndToEndValidation/contracts/validation-profile.schema.json",
                     "tools/World/EndToEndValidation/contracts/territory-budget.schema.json"):
            snapshot = {"requested_base": "HEAD", "base": "a" * 40, "changed_paths": [path]}
            with self.subTest(path=path), mock.patch("release_preflight_plan.changed_path_snapshot", return_value=snapshot):
                plan = release_world_plan("HEAD")
                self.assertEqual(["WorldContracts"], plan["owners"])
                self.assertIn("kazan_territory_v1", plan["world_plan"]["l2_matrices"])
                self.assertFalse(plan["world_gates_executed"])


class WorldContractValidationTests(unittest.TestCase):
    def test_toolkit_metaschema_rejection_even_without_a_profile_reference(self):
        scratch = REPO_ROOT / "tmp/world/release_preflight/tests"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            data = root / "Plugins/World/Fixture/Data"
            (data / "Schemas").mkdir(parents=True)
            (data / "Runtime").mkdir()
            (data / "Schemas/runtime.schema.json").write_text('{"type":"object"}', encoding="utf-8")
            (data / "Runtime/profile.json").write_text('{"$schema":"../Schemas/runtime.schema.json"}', encoding="utf-8")
            toolkit = root / "tools/World/CanonicalCompilation/contracts"
            toolkit.mkdir(parents=True)
            (toolkit / "compiler-profile.schema.json").write_text('{"type":42}', encoding="utf-8")
            self.assertTrue(validate_contracts(root)[1])

    def test_changed_schema_validates_unchanged_consumers_and_rejects_unknown_inputs(self):
        scratch = REPO_ROOT / "tmp/world/release_preflight/tests"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            data = root / "Plugins/World/Fixture/Data"
            (data / "Schemas").mkdir(parents=True)
            (data / "Runtime").mkdir()
            schema_path = data / "Schemas/runtime.schema.json"
            schema = {"type": "object", "required": ["value"], "properties": {"value": {"type": "integer"}}}
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            profile = data / "Runtime/unchanged.json"
            profile.write_text(json.dumps({"$schema": "../Schemas/runtime.schema.json", "value": 1}), encoding="utf-8")
            count, errors = validate_contracts(root)
            self.assertEqual(2, count)
            self.assertEqual([], errors)
            schema["properties"]["value"]["maximum"] = 0
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            self.assertTrue(validate_contracts(root)[1])
            schema_path.write_text('{"type": 42}', encoding="utf-8")
            self.assertTrue(validate_contracts(root)[1])
            schema_path.unlink()
            with self.assertRaisesRegex(RuntimeError, "absent"):
                validate_contracts(root)
            schema_path.write_text(json.dumps(schema), encoding="utf-8")
            profile.write_text('{"$schema":"../Schemas/missing.json"}', encoding="utf-8")
            self.assertTrue(validate_contracts(root)[1])
            profile.write_text("invalid JSON", encoding="utf-8")
            self.assertTrue(validate_contracts(root)[1])


if __name__ == "__main__":
    unittest.main()
