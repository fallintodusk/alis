from __future__ import annotations

import json
import shutil
import tempfile
import threading
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.artifacts import accepted_base, output_descriptor
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash, read_json, write_json
from World.CanonicalCompilation.app.promotion import (
    materialize_canonical,
    promote_canonical,
    validate_canonical_authority,
)
from World.CanonicalCompilation.app.pipeline import (
    compiler_implementation_hash,
    compiler_run_inputs_hash,
    load_profile,
)
from World.CanonicalCompilation.app.territory_admission import current_external_inputs


class CanonicalPromotionTests(unittest.TestCase):
    PROFILE_PATH = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Profiles" / "CanonicalCompilation" / "kazan_territory_v1.compile.json"
    )

    def _candidate(
        self,
        root: Path,
        duration_ms: int = 1,
        feature_count: int = 0,
        source_inputs_hash: str | None = None,
        overlay_sha256: str | None = None,
    ) -> Path:
        zero_hash = "0" * 64
        profile = load_profile(self.PROFILE_PATH)
        external = current_external_inputs(REPO_ROOT, self.PROFILE_PATH, profile)
        contract = {
            "$schema": "https://alis.world/schemas/world-compiler/run-inputs-v1.json",
            "schema_version": 1,
            "profile_id": "kazan_territory_v1",
            "grid_id": "grid_413718bc833994e5",
            "overlay_document": {},
            "base_reuse": {"decision": "not_requested", "reasons": []},
            "authoritative_change_scope": {
                "new_target_cells": [], "terrain_bounds": [], "provider_feature_ids": [],
                "source_result_changed": False, "source_overlap_verified": False,
            },
            "source_inputs_hash": source_inputs_hash or external["source_run_inputs_hash"],
            "source_result_sha256": zero_hash,
            "feature_set_semantic_sha256": zero_hash,
            "profile_sha256": file_hash(self.PROFILE_PATH),
            "overlay_sha256": overlay_sha256 or external["authored_overlay_sha256"],
            "execution_environment": {
                "python_runtime": "test",
                "python_dependency_lock_sha256": zero_hash,
                "toolchain_lock_sha256": zero_hash,
                "toolchain_receipt_sha256": None,
                "implementation_sha256": zero_hash,
                "identity_sha256": zero_hash,
            },
            "fixture_features_sha256": None,
            "base_result_sha256": None,
            "change_scope": {"terrain_bounds": [], "provider_feature_ids": []},
            "implementation_sha256": compiler_implementation_hash(),
        }
        run_hash = compiler_run_inputs_hash(contract)
        contract["run_inputs_hash"] = run_hash
        metrics = {
            "$schema": "https://alis.world/schemas/world-compiler/metrics-report-v1.json",
            "schema_version": 1,
            "profile_id": "kazan_territory_v1",
            "determinism_level": "observational_excluded_from_d1",
            "duration_ms": duration_ms,
            "canonical_bytes": 1,
            "cell_count": 210,
            "terrain_processed_cells": 210,
            "feature_processed_count": 0,
            "feature_count": feature_count,
            "rejection_count": 0,
        }
        write_json(root / "run_contract.json", contract)
        write_json(root / "reports" / "metrics.json", metrics)
        outputs = [
            output_descriptor("compiler_document", path, root)
            for path in (root / "run_contract.json", root / "reports" / "metrics.json")
        ]
        result = {
            "$schema": "https://alis.world/schemas/world-compiler/compile-result-v1.json",
            "schema_version": 1,
            "operation_id": "compile:kazan_territory_v1:" + run_hash[:12],
            "operation": "compile",
            "status": "accepted",
            "profile_id": "kazan_territory_v1",
            "inputs_hash": run_hash,
            "path_base": "output_root",
            "outputs": outputs,
            "errors": [],
        }
        result_path = root / "compile_result.json"
        write_json(result_path, result)
        return result_path

    def test_promoted_bundle_is_deterministic_valid_and_reusable(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            result_path = self._candidate(root / "candidate")
            equivalent_result_path = self._candidate(root / "equivalent_candidate", duration_ms=99)
            authority_root = root / "authority"
            first, active_path = promote_canonical(
                REPO_ROOT, self.PROFILE_PATH, result_path, authority_root
            )
            self.assertNotEqual(file_hash(result_path), file_hash(equivalent_result_path))
            second, _ = promote_canonical(
                REPO_ROOT, self.PROFILE_PATH, equivalent_result_path, authority_root
            )
            self.assertEqual(first["bundle"]["sha256"], second["bundle"]["sha256"])
            self.assertEqual(first, validate_canonical_authority(REPO_ROOT, active_path, self.PROFILE_PATH))
            materialized = materialize_canonical(REPO_ROOT, active_path, self.PROFILE_PATH)
            _, result = accepted_base(materialized)
            self.assertEqual("kazan_territory_v1", result["profile_id"])

            changed_profile = root / "changed.compile.json"
            changed = read_json(self.PROFILE_PATH)
            changed["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
            changed["algorithm_version"] = "changed"
            changed_profile.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(CompilerError) as raised:
                validate_canonical_authority(REPO_ROOT, active_path, changed_profile)
            self.assertEqual("canonical_profile_changed", raised.exception.code)

    def test_promotion_streams_files_and_commits_active_last(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            result_path = self._candidate(root / "candidate")
            authority_root = root / "authority"
            original_read_bytes = Path.read_bytes

            def guarded_read_bytes(path: Path) -> bytes:
                if path.resolve().is_relative_to(result_path.parent.resolve()):
                    raise AssertionError("eager canonical read")
                return original_read_bytes(path)

            with patch.object(Path, "read_bytes", guarded_read_bytes):
                _, active_path = promote_canonical(
                    REPO_ROOT, self.PROFILE_PATH, result_path, authority_root
                )
            accepted_active = active_path.read_bytes()
            with patch(
                "World.CanonicalCompilation.app.promotion.validate_canonical_authority",
                side_effect=CompilerError("candidate_invalid", "sabotage"),
            ):
                with self.assertRaises(CompilerError):
                    promote_canonical(REPO_ROOT, self.PROFILE_PATH, result_path, authority_root)
            self.assertEqual(accepted_active, active_path.read_bytes())

    def test_concurrent_materialization_reuses_one_complete_tree(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            result_path = self._candidate(root / "candidate")
            authority, active_path = promote_canonical(
                REPO_ROOT, self.PROFILE_PATH, result_path, root / "authority"
            )
            output_root = (
                REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "materialized"
                / authority["profile_id"] / authority["bundle"]["sha256"]
            )
            if output_root.exists():
                shutil.rmtree(output_root)
            outputs: list[Path] = []
            errors: list[Exception] = []

            def materialize() -> None:
                try:
                    outputs.append(materialize_canonical(REPO_ROOT, active_path, self.PROFILE_PATH))
                except Exception as error:
                    errors.append(error)

            workers = [threading.Thread(target=materialize) for _ in range(2)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join()
            self.assertFalse(errors)
            expected_result = output_root / "compile_result.json"
            self.assertEqual([expected_result, expected_result], sorted(outputs))
            accepted_base(output_root / "compile_result.json")

    def test_hard_budget_is_enforced_before_promotion(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            result_path = self._candidate(root / "candidate", feature_count=100001)
            with self.assertRaises(CompilerError) as raised:
                promote_canonical(REPO_ROOT, self.PROFILE_PATH, result_path, root / "authority")
            self.assertEqual("promotion_budget_exceeded", raised.exception.code)

    def test_stale_external_inputs_and_external_profile_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            for name, options in (
                ("source", {"source_inputs_hash": "2" * 64}),
                ("overlay", {"overlay_sha256": "2" * 64}),
            ):
                result_path = self._candidate(root / name, **options)
                with self.assertRaises(CompilerError) as raised:
                    promote_canonical(REPO_ROOT, self.PROFILE_PATH, result_path, root / f"authority_{name}")
                self.assertEqual("promotion_profile_changed", raised.exception.code)

            external_profile = root / "external.compile.json"
            external = read_json(self.PROFILE_PATH)
            external["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
            external_profile.write_text(json.dumps(external), encoding="utf-8")
            result_path = self._candidate(root / "external_candidate")
            with self.assertRaises(CompilerError) as raised:
                promote_canonical(REPO_ROOT, external_profile, result_path, root / "external_authority")
            self.assertEqual("compiler_profile_owner_mismatch", raised.exception.code)

    def test_changed_candidate_and_provider_payload_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            result_path = self._candidate(root / "candidate")
            (result_path.parent / "reports" / "metrics.json").write_text("{}", encoding="utf-8")
            with self.assertRaises(CompilerError) as raised:
                promote_canonical(REPO_ROOT, self.PROFILE_PATH, result_path, root / "authority")
            self.assertEqual("promotion_output_changed", raised.exception.code)

        for suffix in (".tif", ".bin", ""):
            with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
                root = Path(directory)
                result_path = self._candidate(root / "candidate")
                provider = result_path.parent / f"raw{suffix}"
                provider.write_bytes(b"provider")
                result = read_json(result_path)
                result["outputs"].append(output_descriptor("provider", provider, result_path.parent))
                write_json(result_path, result)
                with self.assertRaises(CompilerError) as raised:
                    promote_canonical(REPO_ROOT, self.PROFILE_PATH, result_path, root / "authority")
                self.assertEqual("promotion_output_invalid", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
