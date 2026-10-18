"""Selected artifact consumption is distinct from accepting a fresh producer run."""
import importlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.EndToEndValidation.app.contracts import ValidationFailure

MODULE = "World.EndToEndValidation.app.immutable_canonical"
PROFILE = ROOT / "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json"


class ImmutableCanonicalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        parent = ROOT / "tmp/world/immutable_canonical_tests"
        parent.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=parent)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = Path(cls.temporary.name)
        cls.profile = json.loads(PROFILE.read_text())
        cls.profile_path = cls.root / PROFILE.relative_to(ROOT)
        cls.profile_path.parent.mkdir(parents=True)
        cls.profile_path.write_text(json.dumps(cls.profile))
        descriptor = Path("Plugins/World/ProjectWorldData/ProjectWorldData.uplugin")
        (cls.root / descriptor).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / descriptor, cls.root / descriptor)
        for owner in ("CanonicalCompilation", "SourceIngestion", "EndToEndValidation"):
            shutil.copytree(ROOT / f"tools/World/{owner}/contracts", cls.root / f"tools/World/{owner}/contracts")
        cls.active_paths = []
        cls.pin_paths = set()
        for settings in cls.profile["profiles"].values():
            if not settings.get("canonical_authority"):
                continue
            relative = f"Plugins/World/{settings['world_data_plugin']}/Data/Canonical/{settings['compiler_profile']}/active.json"
            active = json.loads((ROOT / relative).read_text())
            cls.active_paths.append(relative)
            paths = {relative, settings["compiler_profile_path"]}
            paths.update(value for key, value in active["external_inputs"].items()
                         if key.endswith("_path") and value)
            for name in paths:
                target = cls.root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, target)
            cls.pin_paths.update(paths)
            archive = Path(relative).parent / active["bundle"]["path"]
            (cls.root / archive).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / archive, cls.root / archive)
        for args in (("init", "-q"), ("add", "--", *sorted(cls.pin_paths)),
                     ("-c", "user.name=WorldTest", "-c", "user.email=world@example.invalid", "commit", "-qm", "accepted inputs")):
            subprocess.run(["git", *args], cwd=cls.root, check=True, capture_output=True)
        historical_profile = deepcopy(cls.profile)
        del historical_profile["profiles"]["manhattan"]
        cls.profile_path.write_text(json.dumps(historical_profile))
        subprocess.run(["git", "add", "--", str(cls.profile_path)], cwd=cls.root, check=True, capture_output=True)
        subprocess.run(["git", "-c", "user.name=WorldTest", "-c", "user.email=world@example.invalid", "commit", "-qm", "validation contract"],
                       cwd=cls.root, check=True, capture_output=True)
        cls.profile_path.write_text(json.dumps(cls.profile))

    def operation(self):
        self.assertIsNotNone(importlib.util.find_spec(MODULE),
                             "Immutable canonical consumption needs its own authenticated admission boundary")
        return importlib.import_module(MODULE)

    def test_both_real_authorities_admit_without_acquire_compile_or_promotion(self):
        operation = self.operation()
        forbidden = AssertionError("The immutable consumer must not invoke a producer")
        with patch("World.CanonicalCompilation.app.pipeline.compile_world", side_effect=forbidden), \
             patch("World.SourceIngestion.app.acquisition.materialize_sources", side_effect=forbidden), \
             patch("World.CanonicalCompilation.app.promotion.promote_canonical", side_effect=forbidden):
            selection = operation.snapshot_selection(self.root, self.profile, "HEAD", self.profile_path)
        self.assertEqual({"kazan", "manhattan"}, set(selection["profiles"]))
        self.assertEqual({"source_acquisition": False, "base_compilation": False, "canonical_promotion": False},
                         selection["executed_upstream_operations"])
        for city in selection["profiles"].values():
            self.assertRegex(city["historical_producer_implementation_sha256"], "^[a-f0-9]{64}$")
            self.assertRegex(city["run_contract_sha256"], "^[a-f0-9]{64}$")
        operation.validate_selection(self.root, self.profile, selection)

    def test_selected_child_cannot_be_silently_certified_as_base_only_consumption(self):
        operation = self.operation()
        # The authority boundary has already authenticated this return value.
        def authenticated(repo, active, compiler):
            return {**json.loads(active.read_text()), 'selected_refinement': {'effective_identity': 'a'*64}}
        with patch(MODULE + '.validate_canonical_authority', side_effect=authenticated), self.assertRaises(ValidationFailure):
            operation.snapshot_selection(self.root, self.profile, 'HEAD', self.profile_path)

    def test_changed_pins_and_authority_refuse_even_with_valid_json(self):
        operation = self.operation()
        for name in sorted(self.pin_paths):
            path = self.root / name
            original = path.read_bytes()
            try:
                path.write_bytes(original + b"\n")
                with self.subTest(path=name), self.assertRaises(ValidationFailure):
                    operation.snapshot_selection(self.root, self.profile, "HEAD", self.profile_path)
            finally:
                path.write_bytes(original)

    def test_missing_history_and_changed_bundle_refuse(self):
        operation = self.operation()
        with self.assertRaises(ValidationFailure):
            operation.snapshot_selection(self.root, self.profile, "missing-history", self.profile_path)
        active_path = self.root / self.active_paths[0]
        active = json.loads(active_path.read_text())
        archive = active_path.parent / active["bundle"]["path"]
        original = archive.read_bytes()
        try:
            with archive.open("ab") as stream:
                stream.write(b"changed bundle")
            with self.assertRaises(ValidationFailure):
                operation.snapshot_selection(self.root, self.profile, "HEAD", self.profile_path)
        finally:
            archive.write_bytes(original)

    def test_old_city_or_tampered_selection_cannot_cover_current_input(self):
        operation = self.operation()
        selection = operation.snapshot_selection(self.root, self.profile, "HEAD", self.profile_path)
        del selection["profiles"]["manhattan"]
        with self.assertRaises(ValidationFailure):
            operation.validate_selection(self.root, self.profile, selection)

    def test_upstream_budget_reference_and_selection_changes_refuse_before_bundle_reads(self):
        operation = self.operation()
        original = self.profile_path.read_bytes()
        cases = []
        for field in ("provider_payload_bytes", "full_compile_seconds", "incremental_compile_seconds"):
            document = deepcopy(self.profile)
            document["thresholds"][field] += 1
            cases.append(document)
        for field, value in (("source_profile", "wrong-logical-source"), ("source_profile_path", "missing-source"),
                             ("compiler_profile", "wrong-compiler"), ("canonical_authority", False)):
            document = deepcopy(self.profile)
            document["profiles"]["kazan"][field] = value
            cases.append(document)
        document = deepcopy(self.profile)
        del document["profiles"]["kazan"]
        cases.append(document)
        try:
            with patch.object(operation, "validate_canonical_authority", side_effect=AssertionError("Refuse before bundle reads")):
                for document in cases:
                    self.profile_path.write_text(json.dumps(document))
                    with self.subTest(document=document["profiles"].keys()), self.assertRaises(ValidationFailure):
                        operation.snapshot_selection(self.root, document, "HEAD", self.profile_path)
                document = deepcopy(self.profile)
                document["profiles"]["manhattan"]["source_profile"] = "wrong-added-source-id"
                self.profile_path.write_text(json.dumps(document))
                with self.assertRaisesRegex(ValidationFailure, "logical source profile"):
                    operation.snapshot_selection(self.root, document, "HEAD", self.profile_path)
        finally:
            self.profile_path.write_bytes(original)


if __name__ == "__main__":
    unittest.main()
