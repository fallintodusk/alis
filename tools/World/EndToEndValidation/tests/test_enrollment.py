from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash
from World.EndToEndValidation.app import enrollment


MAP = "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory"
IDENTITY = {
    "compile_result_sha256": "a" * 64,
    "presentation_profile_sha256": "b" * 64,
    "runtime_profile_sha256": "none",
    "authored_overlay_profile_sha256": "c" * 64,
    "map_package": MAP,
}


def scope(scope_id: str, owning_layer: str, manifest_sha: str, identity: dict | None = None) -> dict:
    return {
        "scope_id": scope_id,
        "owning_layer": owning_layer,
        "manifest_sha256": manifest_sha,
        "input_identity": identity or {},
        "consumer_references": [],
        "artifact_set_sha256": "d" * 64,
    }


class EnrollmentTests(unittest.TestCase):
    def test_wrapper_receipt_uses_realization_evidence_root(self) -> None:
        receipt = enrollment._wrapper_receipt_path("enroll-test")

        self.assertEqual(
            enrollment.REALIZATION_EVIDENCE_ROOT / "enroll-test" / "wrapper.json",
            receipt,
        )

    def test_pre_enrollment_allows_only_target_scope_fingerprint_drift(self) -> None:
        vegetation = scope("layer_territory_vegetation", "vegetation", "1" * 64, IDENTITY)
        vegetation["generator_fingerprint_is_current"] = False
        audit = {
            "status": "rejected",
            "checks": [{"name": "generator_fingerprint_current", "passed": False}],
            "failures": ["generator_fingerprint_current: vegetation stale"],
            "scopes": [vegetation],
        }

        enrollment._validate_pre_enrollment_audit(audit, MAP, {"vegetation"})

        unrelated = dict(vegetation, scope_id="layer_other_vegetation")
        unrelated["input_identity"] = {"map_package": "/ProjectWorldData/Generated/Other"}
        audit["scopes"] = [unrelated]
        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_pre_enrollment_audit(audit, MAP, {"vegetation"})
        self.assertEqual("enrollment_pre_audit_rejected", raised.exception.code)

    def test_pre_enrollment_keeps_non_fingerprint_audit_failures_closed(self) -> None:
        audit = {
            "status": "rejected",
            "checks": [{"name": "artifacts_intact", "passed": False}],
            "failures": ["artifacts_intact: changed bytes"],
            "scopes": [],
        }

        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_pre_enrollment_audit(audit, MAP, {"vegetation"})
        self.assertEqual("enrollment_pre_audit_rejected", raised.exception.code)

    def test_post_enrollment_adds_only_exact_territory_scopes_and_shared_consumer(self) -> None:
        old = scope("map_p0", "map", "1" * 64, {"map_package": "/ProjectWorldData/Generated/P0/L_Old"})
        prior_presentation = scope("presentation_shared", "presentation", "2" * 64, {
            "map_package": "shared", "presentation_profile_sha256": "b" * 64,
        })
        prior_presentation["consumer_references"] = ["map_p0"]
        territory_map = scope("map_territory", "map", "3" * 64, IDENTITY)
        terrain = scope("layer_territory_terrain", "terrain", "4" * 64, IDENTITY)
        water = scope("layer_territory_water", "water", "5" * 64, IDENTITY)
        presentation = scope("presentation_shared", "presentation", "6" * 64, {
            "map_package": "shared", "presentation_profile_sha256": "b" * 64,
        })
        presentation["consumer_references"] = ["map_p0", "map_territory"]
        leg_records = {MAP: {
            "compile_result_sha256": "a" * 64,
            "presentation_profile_sha256": "b" * 64,
            "runtime_profile_sha256": None,
            "authored_overlay_profile_sha256": "c" * 64,
        }}
        result = enrollment._validate_post_enrollment(
            {"scopes": [old, prior_presentation]},
            {"scopes": [old, territory_map, terrain, water, presentation]},
            MAP,
            {"terrain", "water"},
            "b" * 64,
            leg_records,
        )
        self.assertEqual(
            ["layer_territory_terrain", "layer_territory_water", "map_territory"], result
        )

        changed_presentation_artifacts = dict(presentation, artifact_set_sha256="e" * 64)
        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_post_enrollment(
                {"scopes": [old, prior_presentation]},
                {"scopes": [old, territory_map, terrain, water, changed_presentation_artifacts]},
                MAP,
                {"terrain", "water"},
                "b" * 64,
                leg_records,
            )
        self.assertEqual("enrollment_existing_scope_changed", raised.exception.code)

        changed_old = dict(old, manifest_sha256="f" * 64)
        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_post_enrollment(
                {"scopes": [old, prior_presentation]},
                {"scopes": [changed_old, territory_map, terrain, water, presentation]},
                MAP,
                {"terrain", "water"},
                "b" * 64,
                leg_records,
            )
        self.assertEqual("enrollment_existing_scope_changed", raised.exception.code)

    def test_post_enrollment_rejects_presentation_change_for_existing_map(self) -> None:
        territory_map = scope("map_territory", "map", "1" * 64, IDENTITY)
        terrain = scope("layer_territory_terrain", "terrain", "2" * 64, IDENTITY)
        roads = scope("layer_territory_roads", "roads", "3" * 64, IDENTITY)
        prior_presentation = scope("presentation_shared", "presentation", "4" * 64, {
            "map_package": "shared", "presentation_profile_sha256": "b" * 64,
        })
        prior_presentation["consumer_references"] = ["map_territory"]
        changed_presentation = dict(prior_presentation, manifest_sha256="5" * 64)
        leg_records = {MAP: {
            "compile_result_sha256": "a" * 64,
            "presentation_profile_sha256": "b" * 64,
            "runtime_profile_sha256": None,
            "authored_overlay_profile_sha256": "c" * 64,
        }}

        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_post_enrollment(
                {"scopes": [territory_map, terrain, roads, prior_presentation]},
                {"scopes": [territory_map, terrain, roads, changed_presentation]},
                MAP,
                {"terrain", "roads"},
                "b" * 64,
                leg_records,
            )

        self.assertEqual("enrollment_existing_scope_changed", raised.exception.code)

    def test_post_enrollment_allows_only_matrix_proven_existing_map_scopes(self) -> None:
        prior_map = scope("map_territory", "map", "1" * 64, IDENTITY)
        prior_roads = scope("layer_territory_roads", "roads", "2" * 64, IDENTITY)
        presentation = scope("presentation_shared", "presentation", "3" * 64, {
            "map_package": "shared", "presentation_profile_sha256": "b" * 64,
        })
        presentation["consumer_references"] = ["map_territory"]
        unrelated = scope("map_p0", "map", "4" * 64, {
            "map_package": "/ProjectWorldData/Generated/P0/L_Old",
        })
        current_map = dict(prior_map, manifest_sha256="5" * 64)
        current_roads = dict(prior_roads, manifest_sha256="6" * 64)
        leg_records = {MAP: {
            "compile_result_sha256": "a" * 64,
            "presentation_profile_sha256": "b" * 64,
            "runtime_profile_sha256": None,
            "authored_overlay_profile_sha256": "c" * 64,
        }}

        result = enrollment._validate_post_enrollment(
            {"scopes": [prior_map, prior_roads, presentation, unrelated]},
            {"scopes": [current_map, current_roads, presentation, unrelated]},
            MAP,
            {"roads"},
            "b" * 64,
            leg_records,
        )
        self.assertEqual(["layer_territory_roads", "map_territory"], result)

        changed_unrelated = dict(unrelated, manifest_sha256="7" * 64)
        with self.assertRaises(ValidationFailure) as raised:
            enrollment._validate_post_enrollment(
                {"scopes": [prior_map, prior_roads, presentation, unrelated]},
                {"scopes": [current_map, current_roads, presentation, changed_unrelated]},
                MAP,
                {"roads"},
                "b" * 64,
                leg_records,
            )
        self.assertEqual("enrollment_existing_scope_changed", raised.exception.code)

    def test_matrix_canonical_authority_change_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            active = root / "active.json"
            materialized = root / "compile_result.json"
            active.write_text("active", encoding="utf-8")
            materialized.write_text("compile", encoding="utf-8")
            authority = {
                "authority_id": "territory:authority",
                "inputs_hash": "d" * 64,
                "bundle": {"sha256": "e" * 64},
                "compile_result_sha256": file_hash(materialized),
            }
            record = {
                "canonical_authority": {
                    "active_path": active.relative_to(REPO_ROOT).as_posix(),
                    "active_sha256": file_hash(active),
                    "authority_id": authority["authority_id"],
                    "inputs_hash": authority["inputs_hash"],
                    "bundle_sha256": authority["bundle"]["sha256"],
                    "compile_result_sha256": authority["compile_result_sha256"],
                },
                "realization_compile_result_sha256": file_hash(materialized),
            }
            enrollment._validate_matrix_authority(record, active, authority, materialized)
            active.write_text("changed", encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                enrollment._validate_matrix_authority(record, active, authority, materialized)
        self.assertEqual("matrix_canonical_authority_stale", raised.exception.code)

    def test_stale_matrix_is_rejected_before_wrapper_mutation(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory, patch.object(
            enrollment, "ENROLLMENT_ROOT", Path(directory)
        ), patch.object(
            enrollment,
            "accepted_run_evidence",
            side_effect=ValidationFailure("common_checks_stale", "stale"),
        ), patch.object(enrollment, "_run") as run:
            with self.assertRaises(ValidationFailure) as raised:
                enrollment.enroll(
                    "run-stale",
                    "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json",
                )
            receipts = list(Path(directory).rglob("result.json"))
            receipt_status = json.loads(receipts[0].read_text(encoding="utf-8"))["status"]
        self.assertEqual("common_checks_stale", raised.exception.code)
        self.assertEqual(1, len(receipts))
        self.assertEqual("rejected", receipt_status)
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
