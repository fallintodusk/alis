from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import canonical_mode
from World.EndToEndValidation.app.canonical_mode import CanonicalModeError, require_world_only_changes
from World.EndToEndValidation.app.contract_inputs import common_contract_files
from World.EndToEndValidation.app.planning import plan_for_paths
from World.EndToEndValidation.tests import proof_input_fixture


class CanonicalConsumerScopeTests(unittest.TestCase):
    def setUp(self) -> None:
        scratch = REPO_ROOT / "tmp/world/end_to_end_validation/canonical_scope"
        scratch.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        proof_input_fixture.write_producers(self.root)
        proof_input_fixture.write_target_export(self.root)
        self.production = "Plugins/World/ProjectWorldData/Data/"
        self.synthetic = "Plugins/World/ProjectWorldTestData/Data/"
        self.compiler = self.production + "Profiles/CanonicalCompilation/production.compile.json"
        self.twin = self.synthetic + "Profiles/CanonicalCompilation/twin.compile.json"
        self.control = self.production + "Controls/heights.json"
        self.raw = self.production + "Fixtures/raw.json"
        self.validation = self.production + "Profiles/EndToEndValidation/kazan_territory_v1.validation.json"
        self.document = {
            "profile_id": "kazan_territory_v1", "thresholds": {}, "profiles": {
                "production": {
                    "canonical_authority": True, "world_data_plugin": "ProjectWorldData",
                    "source_profile_path": self.production + "Profiles/SourceIngestion/production.source.json",
                    "compiler_profile_path": self.compiler,
                },
                "twin": {
                    "world_data_plugin": "ProjectWorldTestData",
                    "source_profile_path": self.synthetic + "Profiles/SourceIngestion/twin.source.json",
                    "compiler_profile_path": self.twin,
                },
            },
        }
        self.write(self.validation, self.document)
        self.write(self.compiler, {"control_profile": self.control})
        self.write(self.twin, {})
        self.write(self.control, {})
        self.write(self.raw, {})
        self.write(self.document["profiles"]["production"]["source_profile_path"], {
            "sources": [{"local_path": "../../Fixtures/raw.json"}],
        })
        self.write(self.document["profiles"]["twin"]["source_profile_path"], {"sources": []})
        self.commit_base()

    def write(self, path: str, document: dict) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(document), encoding="utf-8")

    def commit_base(self) -> None:
        for arguments in (
            ("init", "-q"), ("add", "."),
            ("-c", "user.name=WorldTest", "-c", "user.email=world@example.invalid", "commit", "-qm", "base"),
        ):
            subprocess.run(("git", *arguments), cwd=self.root, check=True, capture_output=True)

    def guard(self, paths: list[str], base: str = "HEAD") -> None:
        require_world_only_changes(paths, base, self.root, validation_profile=self.root / self.validation)

    def test_unconsumed_twin_input_retains_common_check_and_l1(self) -> None:
        self.guard([self.twin])
        plan = plan_for_paths([self.twin], self.root, "HEAD")
        self.assertTrue(plan["l1_required"])
        self.assertEqual({"kazan_territory_v1": "canonical_authority"}, plan["l2_matrix_modes"])
        self.assertIn(self.root / self.twin, common_contract_files(self.root))

    def test_safe_changed_inventory_loads_authority_once(self) -> None:
        paths = [self.twin] + [
            f"Plugins/World/ProjectWorldData/Content/Generated/Buildings/Cells/SM_{index}.uasset"
            for index in range(10)
        ]
        with mock.patch.object(
            canonical_mode, "profile_input_files", wraps=canonical_mode.profile_input_files,
        ) as input_reads:
            self.guard(paths)
        self.assertEqual(input_reads.call_count, 1)

    def test_mixed_inventory_refusal_reports_only_upstream_offenders(self) -> None:
        generated = "Plugins/World/ProjectWorldData/Content/Generated/Buildings/Cells/SM_safe.uasset"
        with self.assertRaises(CanonicalModeError) as rejected:
            self.guard([generated, self.twin, self.compiler, self.raw])
        self.assertEqual(
            str(rejected.exception),
            "Changed source or canonical inputs require the full Matrix: "
            + ", ".join(sorted([self.compiler, self.raw])),
        )

    def test_consumed_inputs_and_shared_implementations_still_refuse(self) -> None:
        for path in (
            self.compiler, self.control, self.raw,
            self.production + "Canonical/kazan_territory_v1/new-active.json",
            "Plugins/World/ProjectWorldData/Data/Canonical/kazan_territory_v1/active.json",
            "tools/World/CanonicalCompilation/app/pipeline.py",
            "tools/World/SourceIngestion/app/pipeline.py",
            "tools/World/ExecutionEnvironment/bootstrap.py",
        ):
            with self.subTest(path=path), self.assertRaises(CanonicalModeError):
                self.guard([path])

    def test_production_can_consume_test_data_without_an_exemption(self) -> None:
        self.document["profiles"]["production"]["compiler_profile_path"] = self.twin
        self.write(self.validation, self.document)
        self.commit_base()
        with self.assertRaises(CanonicalModeError):
            self.guard([self.twin])

    def test_unknown_history_and_unreadable_inputs_fail_closed(self) -> None:
        with self.assertRaises(CanonicalModeError):
            self.guard([self.twin], "missing-base")
        (self.root / self.compiler).write_text("{bad", encoding="utf-8")
        with self.assertRaises(CanonicalModeError):
            self.guard([self.twin])

    def test_changed_authority_selection_or_missing_references_fail_closed(self) -> None:
        self.document["profiles"]["twin"]["canonical_authority"] = True
        self.write(self.validation, self.document)
        with self.assertRaises(CanonicalModeError):
            self.guard([self.twin])
        self.document["profiles"]["production"].pop("compiler_profile_path")
        self.write(self.validation, self.document)
        self.commit_base()
        with self.assertRaises(CanonicalModeError):
            self.guard([self.twin])

    def test_matrix_modes_follow_each_authority_consumer(self) -> None:
        alternate = json.loads(json.dumps(self.document))
        alternate["profile_id"] = "other_v1"
        alternate["profiles"]["production"]["compiler_profile_path"] = self.twin
        alternate_path = self.production + "Profiles/EndToEndValidation/other_v1.validation.json"
        self.write(alternate_path, alternate)
        self.commit_base()
        with mock.patch("World.EndToEndValidation.app.planning.COMMON_TEST_PROFILE_IDS", {
            "kazan_territory_v1", "other_v1",
        }):
            plan = plan_for_paths([self.twin], self.root, "HEAD")
        self.assertEqual({"kazan_territory_v1": "canonical_authority", "other_v1": "full"}, plan["l2_matrix_modes"])


if __name__ == "__main__":
    unittest.main()
