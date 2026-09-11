"""Post-enrollment acceptance: locked package + gate, one evidence-chain receipt.

The frozen acceptance sequence (Plugins/World/ProjectWorld/docs/
territory_generation.md) ends with a read-only package and packaged
Presentation Gate proving the ENROLLED tree. Three properties have to hold,
and none of them is provable by prose:

1. Serialization. The audits, the package, and the gate must observe ONE
   coherent tree, so this command owns the project-global content mutation
   lock for the whole window. Child commands (the PowerShell audit) honor the
   delegated owner token instead of self-acquiring.
2. Provenance. A run ID typed on the command line is a label, not evidence.
   Each supplied run must actually be an accepted result for the expected
   profile, and every active manifest's input_identity must be traceable to a
   realization receipt inside those runs - otherwise the chain can certify
   manifests built from inputs nobody gated.
3. Linkage and durability. The E2E runs, both durable audits, the package,
   the IoStore inspection, and the gate are joined in one schema-validated
   record, written atomically, and a REJECTED receipt is preserved on every
   failure path so a refusal leaves evidence rather than a console message.
"""

from __future__ import annotations

import json
import os
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from .checks import common_contract_hash
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
    REPO_ROOT,
    _content_mutation_lock,
    _powershell,
    _presentation_profile_contract,
    _run,
    _runtime_profile_contract,
    _world_data_roots,
)
from .package_gate import _required_map_cooked, package_and_inspect
from .presentation import run_packaged_gate
from .validation import _forbidden_files, _validate_package_map_argument


AUDIT_SCRIPT = REPO_ROOT / "scripts" / "ue" / "world" / "audit_generated_authority.ps1"
ACCEPTANCE_ROOT = REPO_ROOT / "Saved" / "Validation" / "WorldAuthority"
PIPELINE_ROOT = REPO_ROOT / "Saved" / "Validation" / "WorldPipeline"


def _write_receipt(path: Path, document: dict[str, Any]) -> None:
    validate_against(document, "acceptance-chain.schema.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(".json.tmp")
    staging.write_text(json.dumps(document, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    staging.replace(path)


def accepted_run_evidence(run_id: str, expected_profile_id: str) -> dict[str, Any]:
    """Load one E2E run and prove it is what the caller claims it is."""
    result_path = PIPELINE_ROOT / run_id / "result.json"
    if not result_path.exists():
        raise ValidationFailure(
            "e2e_run_missing", "The acceptance chain references a run with no result receipt", run_id=run_id
        )
    document = read_json(result_path)
    if document.get("status") != "accepted":
        raise ValidationFailure(
            "e2e_run_not_accepted",
            "The acceptance chain requires accepted P0 and representative runs",
            run_id=run_id,
            status=document.get("status"),
        )
    if document.get("profile_id") != expected_profile_id:
        raise ValidationFailure(
            "e2e_run_profile_mismatch",
            "The supplied run does not belong to the profile it was passed as",
            run_id=run_id,
            expected_profile_id=expected_profile_id,
            actual_profile_id=document.get("profile_id"),
        )
    profile_contract = document.get("evidence", {}).get("validation_profile", {})
    recorded_profile_path = profile_contract.get("path")
    recorded_profile_hash = profile_contract.get("sha256")
    if not isinstance(recorded_profile_path, str) or Path(recorded_profile_path).is_absolute():
        raise ValidationFailure(
            "validation_profile_evidence_invalid",
            "The matrix run has no repository-relative validation-profile identity",
            run_id=run_id,
        )
    resolved_profile_path = profile_path(recorded_profile_path)
    canonical_profile_path = resolved_profile_path.relative_to(REPO_ROOT).as_posix()
    if (
        recorded_profile_path != canonical_profile_path
        or not isinstance(recorded_profile_hash, str)
        or file_hash(resolved_profile_path) != recorded_profile_hash
    ):
        raise ValidationFailure(
            "validation_profile_changed",
            "The matrix validation profile is missing, changed, or non-canonical",
            run_id=run_id,
        )
    try:
        loaded_profile = load_profile(resolved_profile_path)
    except ValidationFailure as error:
        raise ValidationFailure(
            "validation_profile_evidence_invalid",
            "The Matrix validation-profile evidence is not a valid validation profile",
            run_id=run_id,
            reason=error.code,
        ) from error
    if loaded_profile.get("profile_id") != expected_profile_id:
        raise ValidationFailure(
            "validation_profile_identity_mismatch",
            "The Matrix validation file does not declare the claimed profile ID",
            run_id=run_id,
            expected_profile_id=expected_profile_id,
            actual_profile_id=loaded_profile.get("profile_id"),
        )
    recorded_input_contract = document.get("evidence", {}).get("profile_input_contract")
    if recorded_input_contract != validation_profile_contract(resolved_profile_path):
        raise ValidationFailure(
            "validation_profile_inputs_changed",
            "A Matrix transitive profile input is missing, changed, or unrecorded",
            run_id=run_id,
        )
    common = document.get("evidence", {}).get("common_checks", {})
    common_path = Path(common.get("receipt", ""))
    expected_common_hash = common.get("receipt_sha256")
    if not common_path.is_file() or not expected_common_hash or file_hash(common_path) != expected_common_hash:
        raise ValidationFailure(
            "common_checks_evidence_changed",
            "The matrix run's common-check receipt is missing or changed",
            run_id=run_id,
        )
    common_document = read_json(common_path)
    validate_against(common_document, "validation-result.schema.json")
    if common_document.get("status") != "accepted" or common_document.get("profile_id") != "common_checks":
        raise ValidationFailure(
            "common_checks_not_accepted",
            "The matrix run references a non-accepted common-check receipt",
            run_id=run_id,
        )
    tested_contract = common_document.get("evidence", {}).get("common_contract_sha256")
    if (
        common.get("common_contract_sha256") != tested_contract
        or tested_contract != common_contract_hash()
    ):
        raise ValidationFailure(
            "common_checks_stale",
            "World code or contracts changed after the shared common checks",
            run_id=run_id,
        )
    return {
        "run_id": run_id,
        "profile_id": document["profile_id"],
        "result_path": str(result_path),
        "result_sha256": file_hash(result_path),
        "common_checks_sha256": expected_common_hash,
        "validation_profile": profile_contract,
        "profile_input_contract": recorded_input_contract,
        "document": document,
    }


IDENTITY_FIELDS = (
    "compile_result_sha256",
    "presentation_profile_sha256",
    "runtime_profile_sha256",
    "authored_overlay_profile_sha256",
)


def _digest_or_none(value: Any) -> str | None:
    """Normalize an identity field to a digest or nothing.

    input_identity uses the literal sentinel "none" for fields that do not
    apply to a scope: P0 legs declare no runtime profile, and the
    presentation scope is profile-owned so it has no compile-result identity.
    A sentinel is an absence, not a claim, and must compare equal to an
    absent field on the evidence side.
    """
    if isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value):
        return value
    return None


def gated_leg_records(runs: list[dict[str, Any]]) -> dict[str, dict[str, str | None]]:
    """Build map_package -> exact identity tuple from AUTHENTICATED evidence.

    Two properties matter and neither is free:

    * The realization receipts are verified against the hashes frozen into the
      accepted run before a single provenance field is read. Following a path
      recorded in `result.json` and trusting whatever bytes sit there today
      would let a mutated child receipt launder itself into "accepted
      provenance" while the parent hash still validated.
    * Identity is kept as a TUPLE per map package, not merged into global sets
      of valid hashes. A union only proves "these hashes occurred somewhere",
      which a map carrying another leg's legitimate compile hash would satisfy.
    """
    records: dict[str, dict[str, str | None]] = {}
    for run in runs:
        profiles = run["document"].get("evidence", {}).get("profiles", {})
        for leg_name, leg in profiles.items():
            for key in (
                "unreal_first",
                "unreal_second",
                "unreal_incremental",
                "unreal_rejected_apply",
                "unreal_clean_rebuild",
            ):
                receipt_path = leg.get(key)
                if not receipt_path:
                    continue
                expected = leg.get(f"{key}_sha256")
                if not expected:
                    raise ValidationFailure(
                        "e2e_evidence_unauthenticated",
                        "The accepted run does not pin the hash of its realization receipts",
                        run_id=run["run_id"],
                        leg=leg_name,
                        receipt=key,
                    )
                path = Path(receipt_path)
                if not path.exists():
                    raise ValidationFailure(
                        "e2e_realization_evidence_missing",
                        "Run evidence references a realization receipt that no longer exists",
                        run_id=run["run_id"],
                        leg=leg_name,
                        receipt=str(path),
                    )
                actual = file_hash(path)
                if actual != expected:
                    raise ValidationFailure(
                        "e2e_realization_evidence_tampered",
                        "A realization receipt changed after its run was accepted",
                        run_id=run["run_id"],
                        leg=leg_name,
                        receipt=str(path),
                        expected_sha256=expected,
                        actual_sha256=actual,
                    )
                receipt = read_json(path)
                if key in ("unreal_incremental", "unreal_rejected_apply"):
                    continue
                map_package = receipt.get("map_package")
                if not map_package:
                    raise ValidationFailure(
                        "e2e_realization_evidence_incomplete",
                        "A realization receipt declares no map package",
                        run_id=run["run_id"],
                        leg=leg_name,
                        receipt=str(path),
                    )
                identity = {
                    "compile_result_sha256": _digest_or_none(receipt.get("input_sha256")),
                    "presentation_profile_sha256": _digest_or_none(receipt.get("presentation_profile_sha256")),
                    "runtime_profile_sha256": _digest_or_none(receipt.get("runtime_profile_sha256")),
                    "authored_overlay_profile_sha256": _digest_or_none(
                        receipt.get("authored_overlay_set_sha256")
                    ),
                }
                existing = records.get(map_package)
                if existing is not None and existing != identity:
                    raise ValidationFailure(
                        "e2e_map_provenance_ambiguous",
                        "One map package was produced from two different input tuples",
                        map_package=map_package,
                        run_id=run["run_id"],
                        leg=leg_name,
                    )
                records[map_package] = identity
    return records


def verify_manifest_provenance(
    scopes: list[dict[str, Any]],
    leg_records: dict[str, dict[str, str | None]],
) -> list[str]:
    """Bind each enrolled scope to the exact accepted leg that produced it.

    A map-owned scope must match its leg's WHOLE identity tuple, selected by
    its own map_package - not merely carry hashes that exist somewhere in the
    accepted evidence. The shared presentation scope owns no map, so its
    provenance is derived from the presentation input its consumer maps
    actually used, which must be unanimous.
    """
    problems: list[str] = []
    by_id = {scope.get("scope_id"): scope for scope in scopes}

    for scope in scopes:
        scope_id = scope.get("scope_id")
        identity = scope.get("input_identity") or {}
        layer = scope.get("owning_layer")

        if layer == "presentation":
            consumers = [c for c in (scope.get("consumer_references") or [])]
            if not consumers:
                problems.append(f"{scope_id} has no consumer scope to derive its provenance from")
                continue
            expected: set[str | None] = set()
            for consumer_id in consumers:
                consumer = by_id.get(consumer_id)
                record = leg_records.get((consumer or {}).get("input_identity", {}).get("map_package"))
                if record is None:
                    problems.append(f"{scope_id} consumer {consumer_id} maps to no accepted leg")
                    continue
                expected.add(record["presentation_profile_sha256"])
            if len(expected) > 1:
                problems.append(f"{scope_id} consumers disagree on the presentation input that was gated")
            elif expected:
                declared = _digest_or_none(identity.get("presentation_profile_sha256"))
                if declared is None or declared not in expected:
                    problems.append(
                        f"{scope_id}.presentation_profile_sha256 is not the input its consumer maps were gated with"
                    )
            continue

        map_package = identity.get("map_package")
        record = leg_records.get(map_package)
        if record is None:
            problems.append(f"{scope_id} declares map package '{map_package}', which no accepted leg produced")
            continue
        for field in IDENTITY_FIELDS:
            declared = _digest_or_none(identity.get(field))
            gated = record.get(field)
            if declared != gated:
                problems.append(
                    f"{scope_id}.{field} does not match the accepted leg for {map_package}"
                )
    return problems


def required_map_owner(profile: dict[str, Any]) -> str:
    required_map = profile["package"]["required_map"]
    owners = {
        settings["world_data_plugin"]
        for settings in profile["profiles"].values()
        if settings["map_package"] == required_map
    }
    if len(owners) != 1:
        raise ValidationFailure(
            "required_map_owner_ambiguous",
            "Package required_map must resolve to exactly one world-data owner",
            required_map=required_map,
            owners=sorted(owners),
        )
    return next(iter(owners))


def accept(
    p0_run_id: str,
    representative_run_id: str,
    profile_value: str = "representative_v1",
    bootstrap_preflight: str | None = None,
) -> dict[str, Any]:
    stamp = datetime.now(UTC).strftime("%Y%m%dT%H%M%SZ")
    operation_id = f"accept-{stamp}"
    evidence_root = ACCEPTANCE_ROOT / operation_id
    logs = evidence_root / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    receipt_path = evidence_root / "acceptance_chain.json"

    document: dict[str, Any] = {
        "$schema": "https://alis.world/schemas/world-validation/acceptance-chain-v1.json",
        "schema_version": 1,
        "operation_id": operation_id,
        "status": "rejected",
        "e2e_runs": {},
        "errors": [],
    }

    def _reject(error: ValidationFailure) -> ValidationFailure:
        document["status"] = "rejected"
        document["errors"] = [{"code": error.code, "message": str(error), "details": error.details}]
        _write_receipt(receipt_path, document)
        return error

    try:
        resolved_profile_path = profile_path(profile_value)
        acceptance_profile_hash = file_hash(resolved_profile_path)
        profile = load_profile(resolved_profile_path)
        if file_hash(resolved_profile_path) != acceptance_profile_hash:
            raise ValidationFailure(
                "acceptance_profile_changed",
                "The acceptance profile changed while it was being loaded",
            )
        profile_id = profile["profile_id"]
        authority_owner = required_map_owner(profile)
        document["world_data_plugin"] = authority_owner
        if bootstrap_preflight:
            preflight_path = Path(bootstrap_preflight)
            preflight = read_json(preflight_path)
            validate_against(preflight, "bootstrap-preflight.schema.json")
            document["environment"] = {
                "bootstrap_preflight": str(preflight_path),
                "bootstrap_preflight_sha256": file_hash(preflight_path),
            }

        runs = [
            accepted_run_evidence(p0_run_id, "p0"),
            accepted_run_evidence(representative_run_id, profile_id),
        ]
        expected_profile_contract = {
            "path": resolved_profile_path.relative_to(REPO_ROOT).as_posix(),
            "sha256": acceptance_profile_hash,
        }
        if runs[1]["validation_profile"] != expected_profile_contract:
            raise ValidationFailure(
                "representative_profile_mismatch",
                "The representative Matrix did not gate the supplied acceptance profile",
            )
        if len({run["common_checks_sha256"] for run in runs}) != 1:
            raise ValidationFailure(
                "common_checks_inconsistent",
                "P0 and representative matrices must share one accepted common-check receipt",
            )
        document["e2e_runs"] = {
            run["profile_id"]: {
                "run_id": run["run_id"],
                "result_path": run["result_path"],
                "result_sha256": run["result_sha256"],
            }
            for run in runs
        }
        leg_records = gated_leg_records(runs)

        records: dict[str, dict[str, Any]] = {}
        for name, settings in profile["profiles"].items():
            owner = settings["world_data_plugin"]
            record = {
                "presentation_profile": _presentation_profile_contract(
                    settings["presentation_profile"], owner
                )
            }
            if settings.get("runtime_profile"):
                record["runtime_profile"] = _runtime_profile_contract(
                    settings["runtime_profile"], owner
                )
            records[name] = record

        powershell = _powershell()
        package_root = evidence_root / "package"
        iostore_result = evidence_root / "iostore_inspection.json"
        pre_receipt = evidence_root / "pre_package_audit.json"
        post_receipt = evidence_root / "post_package_audit.json"

        with _content_mutation_lock():
            pre_audit = _run_audit(
                powershell, logs, "pre_package_audit", pre_receipt, authority_owner
            )
            document["pre_package_audit"] = {
                "receipt": str(pre_receipt),
                "receipt_sha256": file_hash(pre_receipt),
                "active_set_sha256": pre_audit["active_set"]["sha256"],
            }

            problems = verify_manifest_provenance(pre_audit.get("scopes", []), leg_records)
            if problems:
                raise ValidationFailure(
                    "manifest_provenance_unproven",
                    "Enrolled manifests reference inputs that neither accepted run produced",
                    problems=problems,
                )

            _, iostore = package_and_inspect(
                powershell,
                package_root,
                profile["package"]["required_map"],
                logs,
                iostore_result,
                stage_prefix="acceptance_",
            )
            package_log = logs / "acceptance_package.log"
            if not _required_map_cooked(package_log, profile["package"]["required_map"]):
                raise ValidationFailure(
                    "world_map_not_cooked",
                    "The required production map was not observed in the cook",
                )
            _validate_package_map_argument(package_log, profile["package"]["required_map"])
            if iostore.get("status") != "accepted":
                raise ValidationFailure(
                    "acceptance_iostore_rejected",
                    "The packaged containers do not carry the required map as expected",
                    errors=iostore.get("errors", []),
                )
            content_root = _world_data_roots(authority_owner)[0]
            forbidden = _forbidden_files(
                [content_root, package_root],
                set(profile["package"]["forbidden_suffixes"]),
                set(profile["package"]["forbidden_names"]),
            )
            if forbidden:
                raise ValidationFailure(
                    "forbidden_payload_shipped",
                    "Raw or provider-normalized data reached a distributable root",
                    paths=forbidden,
                )
            gate = run_packaged_gate(profile, records, package_root, evidence_root, logs, operation_id)
            if gate is None:
                raise ValidationFailure(
                    "acceptance_gate_missing",
                    "The acceptance profile declares no Presentation Gate",
                    profile_id=profile_id,
                )
            document["package"] = {
                "operation_id": operation_id,
                "root": str(package_root),
                "required_map": iostore["required_package"],
                "iostore_receipt": str(iostore_result),
                "iostore_receipt_sha256": file_hash(iostore_result),
                "iostore_listed_bytes": iostore.get("project_world_listed_bytes"),
                "shipping_executable_sha256": gate.get("shipping_executable_sha256"),
                "package_summary_sha256": (
                    file_hash(package_root / "package_summary.txt")
                    if (package_root / "package_summary.txt").exists()
                    else None
                ),
            }
            document["presentation_gate"] = {
                "operation_id": gate.get("operation_id"),
                "receipt": gate.get("result"),
                "receipt_sha256": file_hash(Path(gate["result"])),
            }

            post_audit = _run_audit(
                powershell, logs, "post_package_audit", post_receipt, authority_owner
            )
            document["post_package_audit"] = {
                "receipt": str(post_receipt),
                "receipt_sha256": file_hash(post_receipt),
                "active_set_sha256": post_audit["active_set"]["sha256"],
            }

        pre_active = document["pre_package_audit"]["active_set_sha256"]
        post_active = document["post_package_audit"]["active_set_sha256"]
        if pre_active != post_active:
            raise ValidationFailure(
                "acceptance_tree_changed",
                "The generated tree changed across the packaging window",
                pre_active_set=pre_active,
                post_active_set=post_active,
            )
        if file_hash(resolved_profile_path) != acceptance_profile_hash:
            raise ValidationFailure(
                "acceptance_profile_changed",
                "The acceptance profile changed during packaging or the rendered gate",
            )
    except ValidationFailure as error:
        raise _reject(error) from None
    except Exception as error:
        # A programming or environment fault must still leave an artifact.
        # BaseException (KeyboardInterrupt, SystemExit) is deliberately not
        # caught. A dead disk can still defeat the write; everything handled
        # here produces a receipt.
        failure = ValidationFailure(
            "acceptance_internal_error",
            "Unexpected failure while proving the acceptance chain",
            exception_type=type(error).__name__,
            exception_message=str(error),
        )
        raise _reject(failure) from error

    document["status"] = "accepted"
    document["errors"] = []
    _write_receipt(receipt_path, document)
    return {"receipt": str(receipt_path), "active_set_sha256": document["post_package_audit"]["active_set_sha256"]}


def _run_audit(
    powershell: str,
    logs: Path,
    name: str,
    receipt: Path,
    world_data_plugin: str,
    allow_rejected_receipt: bool = False,
) -> dict[str, Any]:
    try:
        _run(
            name,
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(AUDIT_SCRIPT),
                "-WorldDataPlugin",
                world_data_plugin,
                "-EvidencePath",
                str(receipt),
            ],
            logs,
            timeout=1800,
        )
    except ValidationFailure as error:
        if (
            not allow_rejected_receipt
            or error.code != "stage_failed"
            or error.details.get("returncode") != 1
            or not receipt.is_file()
        ):
            raise
    document = read_json(receipt)
    if document.get("status") != "accepted" and not allow_rejected_receipt:
        raise ValidationFailure(
            "durable_authority_rejected",
            "The durable authority audit rejected the tracked generated tree",
            stage=name,
            failures=document.get("failures", []),
        )
    return document
