from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from contextlib import ExitStack
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import validation
from World.EndToEndValidation.app.contracts import ValidationFailure


class SourcePayloadBudgetTests(unittest.TestCase):
    def setUp(self) -> None:
        scratch = REPO_ROOT / "tmp/world/end_to_end_validation/tests/source_payload"
        scratch.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = {"source_id": "pinned_provider", "expected_bytes": 8,
                       "hashes": {"sha256": hashlib.sha256(b"provider").hexdigest()}}
        self._write("source.json", {"profile_id": "city", "sources": [self.source]})
        self.ledger = {"profile_id": "city", "snapshots": [
            {"source_id": self.source["source_id"], "byte_size": 8,
             "hashes": self.source["hashes"]}]}
        self._save_source_result()
        self._write("canonical/coverage.json", {"feature_count": 1})
        self._write("reports/metrics.json", {"duration_ms": 1, "canonical_bytes": 1})

    def _write(self, relative: str, document: dict) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def _save_source_result(self) -> None:
        ledger_path = self._write("source_ledger.json", self.ledger)
        self._write("run_result.json", {
            "$schema": "https://alis.world/schemas/world-source/operation-result-v1.json",
            "schema_version": 1, "status": "accepted", "operation": "run", "profile_id": "city",
            "outputs": [{"kind": "source_ledger", "path": "source_ledger.json",
                         "sha256": hashlib.sha256(ledger_path.read_bytes()).hexdigest(),
                         "byte_size": ledger_path.stat().st_size}],
        })

    def _validate(self, cache_bytes: int = 10**12, budget_bytes: int = 9) -> dict:
        settings = {"world_data_plugin": "ProjectWorldData", "source_profile": "city",
                    "source_profile_path": "source.json", "expected_features": 1,
                    "realization_profile": "realization.json"}
        profile = {"thresholds": {"full_compile_seconds": 1, "incremental_compile_seconds": 1,
                   "canonical_bytes": 10, "unreal_import_seconds": 1, "generated_packages": 10,
                   "provider_payload_bytes": budget_bytes}, "profiles": {"kazan": settings},
                   "package": {"forbidden_suffixes": [], "forbidden_names": []}}
        record = {name: str(self.root) for name in (
            "compile_first_root", "compile_second_root", "incremental_root", "source_root")}
        record.update({name: str(self.root / "unused.json") for name in (
            "unreal_first", "unreal_second", "unreal_clean_rebuild", "unreal_incremental",
            "unreal_road_locality", "unreal_rejected_apply")})
        record.update({"source_result": str(self.root / "run_result.json"), "source_seconds": 0,
                       "generated_package_count": 1, "presentation_profile": {}, "runtime_profile": {},
                       "realization_profile": {}, "road_locality_dirty_unit": "cell",
                       "authored_overlay_profile": {}, "authored_package_hashes": {},
                       "rejected_generated_hashes": {}, "realization_compile_result_sha256": "a" * 64})
        execution = {"matrix_mode": "full", "test_suites": [], "profiles": {"kazan": record},
                     "generated_roots": [], "generated_tree_restoration": {"status": "accepted",
                     "owners": {"ProjectWorldData": {"before_sha256": "same", "after_sha256": "same",
                                                       "file_count": 0}}},
                     "environment": {"bootstrap_preflight": {"all_absent": False, "observations": []},
                                     "immutable_cache_bytes": cache_bytes}}
        with ExitStack() as stack:
            stack.enter_context(mock.patch.object(validation, "REPO_ROOT", self.root))
            for name, result in (("_determinism", {}), ("_validate_incremental", {}),
                                 ("_validate_canonical_integrity", {}),
                                 ("_validate_realization", {"verified_output_count": 1}),
                                 ("_validate_notices", {"entries": 1})):
                stack.enter_context(mock.patch.object(validation, name, return_value=result))
            return validation.validate(profile, execution)[1]

    def test_unrelated_cache_occupancy_does_not_reject_consumed_sources(self) -> None:
        metrics = self._validate()
        self.assertEqual(8, metrics["profiles"]["kazan"]["provider_payload_bytes"])

    def test_consumed_payload_over_budget_rejects_even_with_an_empty_cache(self) -> None:
        with self.assertRaises(ValidationFailure) as raised:
            self._validate(cache_bytes=0, budget_bytes=7)
        self.assertEqual("provider_budget_exceeded", raised.exception.code)

    def test_source_ledger_tampering_cannot_lower_the_budget_measurement(self) -> None:
        self.ledger["snapshots"][0]["byte_size"] = 0
        self._write("source_ledger.json", self.ledger)
        with self.assertRaises(ValidationFailure) as raised:
            self._validate(cache_bytes=0)
        self.assertEqual("source_payload_ledger_changed", raised.exception.code)

    def test_authenticated_ledger_must_match_every_declared_source_pin(self) -> None:
        self.ledger["snapshots"][0]["hashes"] = {"sha256": "b" * 64}
        self._save_source_result()
        with self.assertRaises(ValidationFailure) as raised:
            self._validate(cache_bytes=0)
        self.assertEqual("source_payload_identity_mismatch", raised.exception.code)

    def test_missing_consumed_source_cannot_read_as_zero(self) -> None:
        self.ledger["snapshots"] = []
        self._save_source_result()
        with self.assertRaises(ValidationFailure) as raised:
            self._validate(cache_bytes=0)
        self.assertEqual("source_payload_identity_mismatch", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
