from __future__ import annotations

import json
import shutil
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from World.CanonicalCompilation.api import (
    canonical_active_path,
    materialize_canonical,
    validate_canonical_authority,
)

from .acceptance import accepted_run_evidence, gated_leg_records, verify_manifest_provenance, _run_audit
from .contracts import (
    ValidationFailure,
    file_hash,
    load_profile,
    profile_path,
    read_json,
    validate_against,
    validation_profile_contract,
)
from .execution import (
    REALIZE_SCRIPT,
    REALIZATION_EVIDENCE_ROOT,
    REPO_ROOT,
    _authored_overlay_profile_contract,
    _content_mutation_lock,
    _powershell,
    _presentation_profile_contract,
    _realization_profile_contract,
    _run,
)


ENROLLMENT_ROOT = REPO_ROOT / "Saved" / "Validation" / "WorldEnrollment"


def _wrapper_receipt_path(operation_id: str) -> Path:
    return REALIZATION_EVIDENCE_ROOT / operation_id / "wrapper.json"


def _write_receipt(path: Path, document: dict[str, Any]) -> None:
    validate_against(document, "enrollment-result.schema.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(".json.tmp")
    staging.write_text(json.dumps(document, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    staging.replace(path)


def _required_profile(profile: dict[str, Any]) -> tuple[str, dict[str, Any]]:
    required_map = profile["package"]["required_map"]
    matches = [
        (name, settings)
        for name, settings in profile["profiles"].items()
        if settings["map_package"] == required_map
    ]
    if len(matches) != 1:
        raise ValidationFailure(
            "required_map_unknown",
            "Enrollment required_map must identify exactly one Matrix profile",
        )
    name, settings = matches[0]
    if not settings.get("canonical_authority") or not settings.get("realization_profile"):
        raise ValidationFailure(
            "enrollment_profile_not_durable",
            "L3 enrollment requires canonical authority and a layered realization profile",
            profile=name,
        )
    return name, settings


def _validate_matrix_authority(
    record: dict[str, Any],
    active_path: Path,
    authority: dict[str, Any],
    materialized_result: Path,
) -> None:
    recorded = record.get("canonical_authority") or {}
    expected = {
        "active_path": active_path.relative_to(REPO_ROOT).as_posix(),
        "active_sha256": file_hash(active_path),
        "authority_id": authority["authority_id"],
        "inputs_hash": authority["inputs_hash"],
        "bundle_sha256": authority["bundle"]["sha256"],
        "compile_result_sha256": authority["compile_result_sha256"],
    }
    if recorded != expected or record.get("realization_compile_result_sha256") != file_hash(materialized_result):
        raise ValidationFailure(
            "matrix_canonical_authority_stale",
            "Matrix evidence does not identify the current canonical authority",
        )


def _validate_pre_enrollment_audit(
    audit: dict[str, Any],
    map_package: str,
    layer_ids: set[str],
) -> None:
    if audit.get("status") == "accepted":
        return
    failed_checks = [check for check in audit.get("checks", []) if not check.get("passed")]
    stale_scopes = {
        scope["scope_id"]
        for scope in audit.get("scopes", [])
        if scope.get("generator_fingerprint_is_current") is False
    }
    replaceable_scopes = {
        scope["scope_id"]
        for scope in audit.get("scopes", [])
        if scope.get("input_identity", {}).get("map_package") == map_package
        and (scope.get("owning_layer") == "map" or scope.get("owning_layer") in layer_ids)
    }
    only_expected_fingerprint_drift = (
        audit.get("status") == "rejected"
        and len(failed_checks) == 1
        and failed_checks[0].get("name") == "generator_fingerprint_current"
        and bool(stale_scopes)
        and bool(audit.get("failures"))
        and stale_scopes <= replaceable_scopes
        and all(
            str(failure).startswith("generator_fingerprint_current:")
            for failure in audit.get("failures", [])
        )
    )
    if not only_expected_fingerprint_drift:
        raise ValidationFailure(
            "enrollment_pre_audit_rejected",
            "Pre-enrollment authority has failures outside the exact Matrix replacement scopes",
            failures=audit.get("failures", []),
            stale_scopes=sorted(stale_scopes),
        )


def _validate_post_enrollment(
    pre_audit: dict[str, Any],
    post_audit: dict[str, Any],
    map_package: str,
    layer_ids: set[str],
    presentation_sha256: str,
    leg_records: dict[str, dict[str, str | None]],
) -> list[str]:
    pre_scopes = {scope["scope_id"]: scope for scope in pre_audit["scopes"]}
    post_scopes = {scope["scope_id"]: scope for scope in post_audit["scopes"]}
    territory = [
        scope for scope in post_scopes.values()
        if scope.get("input_identity", {}).get("map_package") == map_package
    ]
    expected_layers = {"map", *layer_ids}
    if {scope.get("owning_layer") for scope in territory} != expected_layers:
        raise ValidationFailure(
            "enrollment_scope_set_mismatch",
            "Durable authority does not contain the exact map and generated layers",
        )
    problems = verify_manifest_provenance(territory, leg_records)
    if problems:
        raise ValidationFailure(
            "enrollment_provenance_mismatch",
            "Enrolled territory scopes do not match accepted Matrix evidence",
            problems=problems,
        )
    map_scope = next(scope for scope in territory if scope["owning_layer"] == "map")
    presentations = [
        scope for scope in post_scopes.values()
        if scope.get("owning_layer") == "presentation"
        and scope.get("input_identity", {}).get("presentation_profile_sha256") == presentation_sha256
    ]
    if len(presentations) != 1 or map_scope["scope_id"] not in presentations[0].get("consumer_references", []):
        raise ValidationFailure(
            "enrollment_presentation_consumer_missing",
            "Shared presentation authority does not consume the enrolled territory map",
        )
    territory_scope_ids = {scope["scope_id"] for scope in territory}
    new_map_admission = map_scope["scope_id"] not in pre_scopes
    for scope_id, prior in pre_scopes.items():
        current = post_scopes.get(scope_id)
        if scope_id in territory_scope_ids:
            # Exact Matrix provenance was already verified above. Only these
            # candidate map/layer scopes may advance during existing-map L3.
            continue
        if prior.get("owning_layer") == "presentation" and new_map_admission:
            expected_consumers = sorted({*prior.get("consumer_references", []), map_scope["scope_id"]})
            allowed_changes = {
                "manifest_path", "manifest_sha256", "generation",
                "accepted_operation_id", "consumer_references",
            }
            prior_stable = {key: value for key, value in prior.items() if key not in allowed_changes}
            current_stable = {
                key: value for key, value in (current or {}).items()
                if key not in allowed_changes
            }
            if (
                current is None
                or current_stable != prior_stable
                or sorted(current.get("consumer_references", [])) != expected_consumers
            ):
                raise ValidationFailure(
                    "enrollment_existing_scope_changed",
                    "New-map enrollment changed presentation beyond its consumer reference",
                    scope_id=scope_id,
                )
            continue
        if current is None or current.get("manifest_sha256") != prior.get("manifest_sha256"):
            raise ValidationFailure(
                "enrollment_existing_scope_changed",
                "Enrollment changed an unrelated durable scope",
                scope_id=scope_id,
            )
    return sorted(scope["scope_id"] for scope in territory)


def enroll(run_id: str, profile_value: str) -> dict[str, Any]:
    stamp = datetime.now(UTC).strftime("%Y%m%dT%H%M%SZ")
    operation_id = f"enroll-{stamp}"
    evidence_root = ENROLLMENT_ROOT / operation_id
    receipt_path = evidence_root / "result.json"
    resolved_profile = profile_path(profile_value)
    profile = load_profile(resolved_profile)
    world_name, settings = _required_profile(profile)
    document: dict[str, Any] = {
        "$schema": "https://alis.world/schemas/world-validation/enrollment-result-v1.json",
        "schema_version": 1,
        "operation_id": operation_id,
        "operation": "world_authority_enrollment",
        "status": "rejected",
        "profile_id": profile["profile_id"],
        "run_id": run_id,
        "world_data_plugin": settings["world_data_plugin"],
        "map_package": settings["map_package"],
        "evidence": {},
        "errors": [],
    }

    def reject(error: ValidationFailure) -> ValidationFailure:
        document["status"] = "rejected"
        document["errors"] = [{"code": error.code, "message": str(error), "details": error.details}]
        _write_receipt(receipt_path, document)
        return ValidationFailure(error.code, str(error), **error.details, receipt=str(receipt_path))

    try:
        run = accepted_run_evidence(run_id, profile["profile_id"])
        if run["validation_profile"]["path"] != resolved_profile.relative_to(REPO_ROOT).as_posix():
            raise ValidationFailure(
                "enrollment_validation_profile_mismatch",
                "Enrollment profile differs from the accepted Matrix profile",
            )
        record = run["document"]["evidence"]["profiles"][world_name]
        leg_records = gated_leg_records([run])
        compiler_path = (REPO_ROOT / settings["compiler_profile_path"]).resolve()
        active_path = canonical_active_path(REPO_ROOT, compiler_path)
        authority = validate_canonical_authority(REPO_ROOT, active_path, compiler_path)
        materialized = materialize_canonical(REPO_ROOT, active_path, compiler_path)
        _validate_matrix_authority(record, active_path, authority, materialized)
        presentation = _presentation_profile_contract(
            settings["presentation_profile"], settings["world_data_plugin"]
        )
        authored = _authored_overlay_profile_contract(
            settings["authored_overlay_profile"], settings["world_data_plugin"]
        )
        realization = _realization_profile_contract(
            settings["realization_profile"], settings["world_data_plugin"], settings["map_package"]
        )
        realization_document = read_json(REPO_ROOT / realization["path"])
        layer_ids = {layer["layer_id"] for layer in realization_document["layers"]}
        first_path = Path(record["unreal_first"])
        first = read_json(first_path)
        if (
            file_hash(first_path) != record["unreal_first_sha256"]
            or first.get("map_package") != settings["map_package"]
            or first.get("input_sha256") != file_hash(materialized)
            or first.get("realization_profile_sha256") != realization["sha256"]
        ):
            raise ValidationFailure(
                "enrollment_matrix_leg_mismatch",
                "Matrix realization evidence differs from the exact L3 inputs",
            )
        initial_contract = validation_profile_contract(resolved_profile)
        powershell = _powershell()
        logs = evidence_root / "logs"
        emitted_wrapper_receipt = _wrapper_receipt_path(operation_id)
        wrapper_receipt = evidence_root / "wrapper.json"
        with _content_mutation_lock():
            accepted_run_evidence(run_id, profile["profile_id"])
            if validation_profile_contract(resolved_profile) != initial_contract:
                raise ValidationFailure(
                    "enrollment_profile_changed",
                    "Validation inputs changed before L3 mutation",
                )
            _validate_matrix_authority(
                record,
                active_path,
                validate_canonical_authority(REPO_ROOT, active_path, compiler_path),
                materialized,
            )
            pre_audit_path = evidence_root / "pre_audit.json"
            pre_audit = _run_audit(
                powershell,
                logs,
                "enrollment_pre_audit",
                pre_audit_path,
                settings["world_data_plugin"],
                allow_rejected_receipt=True,
            )
            _validate_pre_enrollment_audit(pre_audit, settings["map_package"], layer_ids)
            command = [
                powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(REALIZE_SCRIPT),
                "-CompileResult", str(materialized), "-Mode", "Apply",
                "-WorldDataPlugin", settings["world_data_plugin"],
                "-PresentationProfile", str(REPO_ROOT / presentation["path"]),
                "-AuthoredOverlayProfile", str(REPO_ROOT / authored["path"]),
                "-RealizationProfile", str(REPO_ROOT / realization["path"]),
                "-Map", settings["map_package"], "-EvidencePath", str(emitted_wrapper_receipt),
                # This IS the sanctioned L3 command; the operator authorized this exact
                # operation before it ran. The wrapper refuses unattended durable
                # enrollment without this switch, so an ad-hoc agent invocation cannot
                # grant production authority by itself.
                "-EnrollManifests", "-NonInteractive", "-DurableEnrollmentAuthorized",
            ]
            if settings["require_landscape"]:
                command.append("-RequireLandscape")
            _run("enrollment_apply", command, logs, timeout=7200)
            shutil.copy2(emitted_wrapper_receipt, wrapper_receipt)
            shutil.copy2(
                Path(f"{emitted_wrapper_receipt}.manifests.json"),
                Path(f"{wrapper_receipt}.manifests.json"),
            )
            wrapper = read_json(wrapper_receipt)
            if wrapper.get("status") != "accepted" or wrapper.get("input_sha256") != file_hash(materialized):
                raise ValidationFailure(
                    "enrollment_wrapper_rejected",
                    "Durable realization wrapper did not accept the authenticated canonical input",
                )
            post_audit_path = evidence_root / "post_audit.json"
            post_audit = _run_audit(
                powershell, logs, "enrollment_post_audit", post_audit_path, settings["world_data_plugin"]
            )
            enrolled_scopes = _validate_post_enrollment(
                pre_audit,
                post_audit,
                settings["map_package"],
                layer_ids,
                presentation["sha256"],
                leg_records,
            )
            if validation_profile_contract(resolved_profile) != initial_contract:
                raise ValidationFailure(
                    "enrollment_profile_changed",
                    "Validation inputs changed during L3 mutation",
                )

        document["status"] = "accepted"
        document["errors"] = []
        document["evidence"] = {
            "matrix": {
                "result": run["result_path"],
                "sha256": run["result_sha256"],
                "profile_input_contract": run["profile_input_contract"],
            },
            "canonical_authority": record["canonical_authority"],
            "realization_profile": realization,
            "wrapper": {"path": str(wrapper_receipt), "sha256": file_hash(wrapper_receipt)},
            "pre_audit": {"path": str(pre_audit_path), "sha256": file_hash(pre_audit_path), "active_set_sha256": pre_audit["active_set"]["sha256"]},
            "post_audit": {"path": str(post_audit_path), "sha256": file_hash(post_audit_path), "active_set_sha256": post_audit["active_set"]["sha256"]},
            "enrolled_scopes": enrolled_scopes,
        }
        _write_receipt(receipt_path, document)
        return {"receipt": str(receipt_path), "active_set_sha256": post_audit["active_set"]["sha256"]}
    except ValidationFailure as error:
        raise reject(error) from error
    except Exception as error:
        failure = ValidationFailure(
            "enrollment_internal_error",
            "Unexpected failure while enrolling durable world authority",
            exception_type=type(error).__name__,
        )
        raise reject(failure) from error
