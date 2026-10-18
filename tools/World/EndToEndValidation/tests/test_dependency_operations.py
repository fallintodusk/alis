"""Operation identities and dispatch follow actual consumed inputs."""
from copy import deepcopy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.CanonicalCompilation.app import pipeline
from World.CanonicalCompilation.refinement.app import identity
from World.EndToEndValidation.app import execution, immutable_canonical
from World.EndToEndValidation.app.contracts import ValidationFailure


class DependencyOperationTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / "tmp/world/dependency_operation_tests"
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_refinement_changes_own_identity_without_rewriting_base_producer(self):
        owner = self.root / "CanonicalCompilation"
        shutil.copytree(ROOT / "tools/World/CanonicalCompilation", owner, ignore=shutil.ignore_patterns("__pycache__", "tests"))
        with patch.object(pipeline, "compilation_root", return_value=owner), \
                patch.object(identity, "__file__", str(owner / "refinement/app/identity.py")):
            original_base, original_refinement = pipeline.compiler_implementation_hash(), identity.implementation_identity()
            child = owner / "refinement/app/geometry.py"
            child.write_bytes(child.read_bytes() + b"\n")
            self.assertEqual(original_base, pipeline.compiler_implementation_hash())
            self.assertNotEqual(original_refinement, identity.implementation_identity())
            previous = identity.implementation_identity()
            shared = owner / "app/terrain_surface.py"
            shared.write_bytes(shared.read_bytes() + b"\n")
            self.assertNotEqual(original_base, pipeline.compiler_implementation_hash())
            self.assertNotEqual(previous, identity.implementation_identity())

    def test_current_reader_identity_changes_with_consumed_helper(self):
        for owner in ("CanonicalCompilation", "SourceIngestion", "ExecutionEnvironment", "EndToEndValidation"):
            shutil.copytree(ROOT / f"tools/World/{owner}", self.root / f"tools/World/{owner}",
                            ignore=shutil.ignore_patterns("__pycache__", "tests"))
        path = self.root / "tools/World/EndToEndValidation/app/immutable_canonical.py"
        with patch.object(immutable_canonical, "__file__", str(path)):
            before = immutable_canonical.consumer_identity()
            helper = path.parent / "profile_inputs.py"
            helper.write_bytes(helper.read_bytes() + b"\n")
            self.assertNotEqual(before, immutable_canonical.consumer_identity())

    def test_immutable_dispatch_never_calls_source_or_base_compiler(self):
        settings = {"world_data_plugin": "ProjectWorldData", "source_profile": "fixture", "compiler_profile": "fixture",
                    "canonical_authority": True, "presentation_profile": "fixture"}
        forbidden = AssertionError("Upstream producer execution is forbidden")
        class ReachedWorld(Exception):
            pass
        with patch.object(execution, "_world_data_roots", return_value=(self.root, self.root)), \
                patch.object(execution, "_run", side_effect=forbidden), \
                patch.object(execution, "canonical_active_path", return_value=ROOT / "fixture-active.json"), \
                patch.object(execution, "validate_canonical_authority", return_value={"authority_id": "fixture", "inputs_hash": "0"*64,
                             "bundle": {"sha256": "1"*64}, "compile_result_sha256": "2"*64}), \
                patch.object(execution, "materialize_canonical", return_value=self.root / "compile_result.json") as materialize, \
                patch.object(execution, "file_hash", return_value="3"*64), \
                patch.object(execution, "_realization_evidence_path", return_value=self.root / "evidence/result.json"), \
                patch.object(execution, "_presentation_profile_contract", side_effect=ReachedWorld):
            with self.assertRaises(ReachedWorld):
                execution._run_profile("city", settings, self.root, self.root, "powershell", "immutable_canonical")
        materialize.assert_called_once()
        self.assertFalse((self.root / "city/source").exists())
        self.assertFalse((self.root / "city/compile_first").exists())

    def test_unknown_mode_refuses_before_read_or_write(self):
        with patch.object(execution, "_powershell", side_effect=AssertionError("Dispatch must refuse first")):
            with self.assertRaises(ValidationFailure):
                execution.execute({}, "unknown", self.root, self.root / "absent.json", {}, "unknown")
        self.assertEqual([], list(self.root.iterdir()))

    def test_persisted_unknown_mode_cannot_reach_acceptance(self):
        for mode in (None, "unknown", "immutable-canonical"):
            with self.subTest(mode=mode), self.assertRaises(ValidationFailure):
                immutable_canonical.validate_execution(self.root, {}, {"matrix_mode": mode})


if __name__ == "__main__":
    unittest.main()
