from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Any

from .contracts import ValidationFailure, file_hash, read_json, tree_size, validate_against
from .layered_validation import validate_layered_realization


REPO_ROOT = Path(__file__).resolve().parents[4]


def _require(condition: bool, code: str, message: str, **details: object) -> None:
    if not condition:
        raise ValidationFailure(code, message, **details)


def _accepted(path: Path, expected_schema: str) -> dict[str, Any]:
    value = read_json(path)
    _require(value.get("$schema") == expected_schema, "evidence_schema_invalid", "Evidence schema is incompatible", path=str(path))
    _require(value.get("schema_version") == 1, "evidence_version_invalid", "Evidence version is incompatible", path=str(path))
    _require(value.get("status") == "accepted", "evidence_rejected", "Required evidence is not accepted", path=str(path))
    return value


def _output_hashes(result: dict[str, Any]) -> dict[str, str]:
    return {item["path"]: item["sha256"] for item in result["outputs"]}


def _determinism(first_root: Path, second_root: Path) -> dict[str, Any]:
    first_result = _accepted(
        first_root / "compile_result.json",
        "https://alis.world/schemas/world-compiler/compile-result-v1.json",
    )
    second_result = _accepted(
        second_root / "compile_result.json",
        "https://alis.world/schemas/world-compiler/compile-result-v1.json",
    )
    first_hashes = _output_hashes(first_result)
    second_hashes = _output_hashes(second_result)
    d1_paths = sorted(set(first_hashes) - {"reports/metrics.json"})
    _require(set(first_hashes) == set(second_hashes), "d1_output_set_mismatch", "Compiler output sets differ")
    _require(
        all(first_hashes[path] == second_hashes[path] for path in d1_paths),
        "d1_hash_mismatch",
        "Deterministic canonical documents differ",
    )
    d2_paths = sorted(
        path for path in d1_paths if path.startswith("canonical/terrain/") or path.startswith("canonical/features/")
    )
    _require(d2_paths, "d2_artifacts_missing", "No declared D2 artifacts were found")
    _require(
        all(first_hashes[path] == second_hashes[path] for path in d2_paths),
        "d2_hash_mismatch",
        "Declared engine-independent artifacts differ",
    )
    first_coverage = read_json(first_root / "canonical" / "coverage.json")
    second_coverage = read_json(second_root / "canonical" / "coverage.json")
    _require(
        first_coverage["semantic_hash"] == second_coverage["semantic_hash"],
        "d0_hash_mismatch",
        "Canonical semantic hashes differ",
    )
    return {
        "D0": first_coverage["semantic_hash"],
        "D1_document_count": len(d1_paths),
        "D2_artifact_count": len(d2_paths),
    }


def _validate_canonical_authority_candidate(
    candidate_root: Path,
    canonical_result_path: Path,
    authority: dict[str, Any],
) -> dict[str, Any]:
    candidate = read_json(candidate_root / "compile_result.json")
    canonical = read_json(canonical_result_path)

    def semantic_contract(root: Path, result: dict[str, Any]) -> dict[str, Any]:
        coverage = read_json(root / "canonical" / "coverage.json")
        artifacts = sorted(
            (entry["path"], entry["sha256"])
            for entry in result["outputs"]
            if entry["path"].startswith(("canonical/terrain/", "canonical/features/"))
        )
        for relative, digest in artifacts:
            _require(
                file_hash(root / relative) == digest,
                "canonical_candidate_artifact_hash_mismatch",
                "Canonical semantic artifact bytes differ from their receipt",
                path=relative,
            )
        return {
            "profile_id": coverage.get("profile_id"),
            "world_data_plugin": coverage.get("world_data_plugin"),
            "engine_georeference_origin": coverage.get("engine_georeference_origin"),
            "algorithm_version": coverage.get("algorithm_version"),
            "grid_id": coverage.get("grid_id"),
            "grid": coverage.get("grid"),
            "area_fingerprints": coverage.get("area_fingerprints"),
            "cells": sorted((item["cell_id"], item["path"]) for item in coverage["cells"]),
            "feature_count": coverage.get("feature_count"),
            "rejected_feature_count": coverage.get("rejected_feature_count"),
            "semantic_hash": coverage.get("semantic_hash"),
            "artifacts": artifacts,
        }

    candidate_contract = semantic_contract(candidate_root, candidate)
    canonical_contract = semantic_contract(canonical_result_path.parent, canonical)
    _require(
        canonical.get("inputs_hash") == authority.get("inputs_hash")
        and file_hash(canonical_result_path) == authority.get("compile_result_sha256")
        and candidate_contract == canonical_contract,
        "canonical_candidate_mismatch",
        "Fresh compile semantics differ from the canonical authority used by Unreal",
    )
    return {
        "inputs_hash": authority["inputs_hash"],
        "candidate_inputs_hash": candidate.get("inputs_hash"),
        "compile_result_sha256": authority["compile_result_sha256"],
        "semantic_hash": canonical_contract["semantic_hash"],
        "semantic_artifact_count": len(canonical_contract["artifacts"]),
        "provenance_drift": candidate.get("inputs_hash") != canonical.get("inputs_hash"),
        "output_count": len(canonical["outputs"]),
    }


def _validate_incremental(base_root: Path, incremental_root: Path, threshold: float) -> dict[str, Any]:
    metrics = read_json(incremental_root / "reports" / "metrics.json")
    diff = read_json(incremental_root / "reports" / "diff.json")
    validation = read_json(incremental_root / "reports" / "validation.json")
    _require(metrics["duration_ms"] / 1000.0 <= threshold, "incremental_budget_exceeded", "Incremental compile exceeded its frozen budget")
    _require(len(diff["terrain_rebuilt_cells"]) == 1, "incremental_scope_invalid", "Incremental run must rebuild exactly one terrain cell")
    base_coverage = read_json(base_root / "canonical" / "coverage.json")
    expected_reused = len(base_coverage["cells"]) - 1
    _require(
        len(diff["reused_cells"]) == expected_reused,
        "incremental_reuse_invalid",
        "Incremental run must reuse every untouched cell",
    )
    _require(metrics["feature_processed_count"] == 0, "incremental_vector_scope_invalid", "Terrain-only run processed vector inputs")
    checks = {item["check"]: item for item in validation["checks"]}
    _require(checks["boundary"]["details"]["mismatches"] == 0, "boundary_mismatch", "Incremental output has a boundary mismatch")
    base_hashes = _output_hashes(read_json(base_root / "compile_result.json"))
    incremental_hashes = _output_hashes(read_json(incremental_root / "compile_result.json"))
    reused_id = diff["reused_cells"][0]
    cell = next(item for item in base_coverage["cells"] if item["cell_id"] == reused_id)
    _require(base_hashes[cell["path"]] == incremental_hashes[cell["path"]], "untouched_core_changed", "Untouched cell manifest changed")
    return {
        "duration_seconds": metrics["duration_ms"] / 1000.0,
        "rebuilt_cell": diff["terrain_rebuilt_cells"][0],
        "reused_cell": reused_id,
        "boundary_mismatches": 0,
    }


def _validate_canonical_integrity(root: Path) -> dict[str, int]:
    report = read_json(root / "reports" / "validation.json")
    checks = {item["check"]: item for item in report["checks"]}
    boundary_mismatches = checks["boundary"]["details"]["mismatches"]
    duplicate_ids = checks["topology"]["details"]["duplicate_ids"]
    dangling_references = checks["ownership"]["details"]["dangling_references"]
    _require(boundary_mismatches == 0, "boundary_mismatch", "Canonical output has a boundary mismatch")
    _require(duplicate_ids == 0, "duplicate_identity", "Canonical output has duplicate identities")
    _require(dangling_references == 0, "owned_output_orphan", "Canonical output has dangling ownership")
    return {
        "boundary_mismatches": boundary_mismatches,
        "duplicate_ids": duplicate_ids,
        "dangling_references": dangling_references,
    }


def _validate_notices(root: Path) -> dict[str, Any]:
    entries = read_json(root / "reports" / "attribution.json")["entries"]
    licenses = {entry["license"]["id"]: entry["license"] for entry in entries}
    _require("ODbL-1.0" in licenses, "odbl_notice_missing", "OSM attribution entry is missing")
    _require(
        bool(licenses["ODbL-1.0"].get("attribution")) and bool(licenses["ODbL-1.0"].get("source_or_alteration_offer")),
        "odbl_offer_missing",
        "OSM attribution or source/alteration offer is missing",
    )
    copernicus = next((value for key, value in licenses.items() if key.startswith("LicenseRef-COP-DEM")), None)
    _require(copernicus is not None and bool(copernicus.get("modified_notice")), "copernicus_notice_missing", "Copernicus modified-output notice is missing")
    return {"entries": len(entries), "license_ids": sorted(licenses)}


def _validate_manifest_lifecycle(
    first_path: Path,
    second_path: Path,
    clean_path: Path,
    incremental_path: Path | None = None,
    road_locality_path: Path | None = None,
) -> dict[str, Any]:
    """Prove the sandboxed manifest lifecycle ran on real content.

    The first Apply enrolls; the second Apply must have passed preflight
    against the enrolled manifests on an unchanged tree (the live drift
    proof); the clean rebuild must have used the explicit reconstruction
    route against the same active set, never re-enrollment.
    """
    sidecars = []
    artifact_paths: list[dict[str, tuple[str, ...]]] = []
    legs = [("first", first_path), ("second", second_path)]
    if road_locality_path is not None:
        legs.append(("road_locality", road_locality_path))
    if incremental_path is not None:
        legs.append(("incremental", incremental_path))
    legs.append(("clean", clean_path))
    for leg, path in legs:
        sidecar_path = Path(f"{path}.manifests.json")
        _require(sidecar_path.is_file(), "manifest_evidence_missing", "A realization leg has no manifest lifecycle evidence", leg=leg)
        sidecar = read_json(sidecar_path)
        scopes_valid = len(sidecar.get("scopes", [])) >= 1 and all(
            re.fullmatch(r"scopes/(map|presentation|layer)_[a-z0-9_]+\.[0-9]+\.json", str(scope.get("manifest_path", "")))
            and re.fullmatch(r"[a-f0-9]{64}", str(scope.get("manifest_sha256", "")))
            for scope in sidecar.get("scopes", [])
        )
        _require(
            re.fullmatch(r"[a-f0-9]{64}", str(sidecar.get("active_set_sha256", ""))) is not None and scopes_valid,
            "manifest_evidence_invalid",
            "A realization leg has malformed manifest lifecycle evidence",
            leg=leg,
        )
        sidecars.append(sidecar)
        manifest_root = Path(sidecar["manifest_root"])
        artifacts_by_scope: dict[str, tuple[str, ...]] = {}
        for scope in sidecar["scopes"]:
            manifest = read_json(manifest_root / scope["manifest_path"])
            artifacts_by_scope[scope["scope_id"]] = tuple(
                sorted(str(artifact["path"]) for artifact in manifest.get("artifacts", []))
            )
        artifact_paths.append(artifacts_by_scope)
    by_leg = {leg: sidecar for (leg, _), sidecar in zip(legs, sidecars, strict=True)}
    _require(by_leg["first"]["route"] == "enroll" and by_leg["first"]["enrolled"] is True, "manifest_enrollment_missing", "The first Apply did not enroll the sandbox scopes")
    _require(by_leg["second"]["route"] == "apply" and by_leg["second"]["enrolled"] is False, "manifest_preflight_unproven", "The unchanged Apply bypassed manifest preflight")
    if road_locality_path is not None:
        _require(by_leg["road_locality"]["route"] == "apply" and by_leg["road_locality"]["enrolled"] is False, "manifest_road_locality_unproven", "The road-locality Apply bypassed manifest preflight")
    if incremental_path is not None:
        _require(by_leg["incremental"]["route"] == "apply" and by_leg["incremental"]["enrolled"] is False, "manifest_incremental_unproven", "The incremental Apply bypassed manifest preflight")
    _require(by_leg["clean"]["route"] == "reconstruct" and by_leg["clean"]["enrolled"] is False, "manifest_reconstruction_unproven", "The clean rebuild did not use explicit reconstruction")
    _require(
        len({sidecar["manifest_root"] for sidecar in sidecars}) == 1,
        "manifest_root_inconsistent",
        "Realization legs used different manifest authority roots",
    )
    _require(
        len({tuple(sorted(scope["scope_id"] for scope in sidecar["scopes"])) for sidecar in sidecars}) == 1,
        "manifest_scope_set_inconsistent",
        "Realization legs report different participating scope sets",
    )
    _require(
        all(
            sidecars[index]["prior_active_set_sha256"] == sidecars[index - 1]["active_set_sha256"]
            for index in range(1, len(sidecars))
        ),
        "manifest_continuity_broken",
        "Active-set continuity is broken across the realization legs",
    )
    _require(
        all(paths == artifact_paths[0] for paths in artifact_paths[1:]),
        "generated_package_path_churn",
        "Regeneration changed generated package paths for an unchanged scope set",
    )
    return {
        "enrolled_scopes": sorted(scope["scope_id"] for scope in sidecars[0]["scopes"]),
        "activated_scopes": {scope["scope_id"]: scope["manifest_path"] for scope in by_leg["clean"]["scopes"]},
        "active_set_sha256": by_leg["clean"]["active_set_sha256"],
        "stable_artifact_paths": sum(len(paths) for paths in artifact_paths[0].values()),
    }


def _validate_realization(
    first_path: Path,
    second_path: Path,
    clean_path: Path,
    expected: dict[str, Any],
    presentation: dict[str, str],
    threshold: float,
    runtime: dict[str, str] | None = None,
    realization_profile: dict[str, str] | None = None,
    incremental_path: Path | None = None,
    road_locality_path: Path | None = None,
    road_locality_dirty_unit: str | None = None,
    rejected_path: Path | None = None,
    authored: dict[str, Any] | None = None,
    authored_package_hashes: dict[str, dict[str, str]] | None = None,
    rejected_generated_hashes: dict[str, dict[str, str]] | None = None,
    realization_compile_result_sha256: str | None = None,
) -> dict[str, Any]:
    schema = "https://alis.world/schemas/world-realization/realization-result-v1.json"
    first = _accepted(first_path, schema)
    second = _accepted(second_path, schema)
    clean = _accepted(clean_path, schema)
    incremental = _accepted(incremental_path, schema) if incremental_path is not None else None
    road_locality = _accepted(road_locality_path, schema) if road_locality_path is not None else None
    manifest_evidence = _validate_manifest_lifecycle(
        first_path, second_path, clean_path, incremental_path, road_locality_path
    )
    accepted_receipts = tuple(
        receipt for receipt in (first, second, road_locality, incremental, clean)
        if receipt is not None
    )
    authored_expected = expected.get("expected_authored", {
        "resolved": 3,
        "placed": 2,
        "masks": 1,
        "minimum_package_files": 1,
    })
    if realization_compile_result_sha256 is not None:
        _require(
            all(
                receipt.get("input_sha256") == realization_compile_result_sha256
                for receipt in (first, second, clean)
            ),
            "realization_compile_result_mismatch",
            "Full, unchanged, or reconstructed Unreal evidence used another compile result",
        )
    for receipt in accepted_receipts:
        _require(
            receipt.get("presentation_profile") == presentation["profile_id"] and
            receipt.get("presentation_profile_sha256") == presentation["sha256"],
            "presentation_profile_mismatch",
            "Unreal evidence does not belong to the pinned presentation input",
        )
        if runtime is not None:
            _require(
                receipt.get("runtime_profile") == runtime["profile_id"] and
                receipt.get("runtime_profile_sha256") == runtime["sha256"],
                "runtime_profile_mismatch",
                "Unreal evidence does not belong to the pinned runtime input",
            )
            _require(
                receipt.get("runtime_route_collision_probed") is True and
                receipt.get("runtime_collision_probe_count") == expected["expected_road_fragments"] and
                receipt.get("runtime_route_collision_orientation_probed") is True and
                receipt.get("runtime_collision_orientation_probe_count") == expected["expected_road_fragments"] and
                receipt.get("runtime_navigation_probed") is True and
                receipt.get("runtime_streaming_policy_probed") is True and
                receipt.get("runtime_nanite_policy_probed") is True and
                receipt.get("runtime_instancing_policy_probed") is True and
                receipt.get("runtime_hlod_policy_probed") is True and
                receipt.get("hlod_proxy_actor_count") == 0 and
                receipt.get("hlod_layer_reference_count") == 0 and
                receipt.get("hlod_eligible_generated_actor_count") == 0 and
                receipt.get("runtime_structural_budgets_passed") is True,
                "runtime_route_unproven",
                "Unreal evidence does not prove the accepted gameplay route",
            )
        if authored is not None:
            _require(
                receipt.get("authored_overlay_set") == authored["profile_id"]
                and receipt.get("authored_overlay_set_sha256") == authored["sha256"],
                "authored_overlay_profile_mismatch",
                "Unreal evidence does not belong to the pinned authored overlay input",
            )
            _require(
                receipt.get("authored_anchor_resolved_count") == authored_expected["resolved"]
                and receipt.get("authored_anchor_refused_count") == 0
                and receipt.get("authored_anchor_placed_count") == authored_expected["placed"]
                and receipt.get("authored_mask_count") == authored_expected["masks"],
                "authored_anchor_resolution_unproven",
                "Unreal evidence does not match the authored anchor contract",
            )
    if authored is not None:
        anchor_resolutions = [receipt.get("authored_anchor_resolutions") for receipt in accepted_receipts]
        _require(
            all(value == anchor_resolutions[0] for value in anchor_resolutions[1:]),
            "authored_anchor_transform_drift",
            "Authored anchor bindings or derived transforms changed across regeneration routes",
        )
        _require(
            authored_package_hashes is not None
            and len(authored_package_hashes) >= 6
            and len(authored_package_hashes.get("before", {}))
            >= authored_expected["minimum_package_files"]
            and all(value == authored_package_hashes["before"] for value in authored_package_hashes.values()),
            "authored_package_mutated",
            "An authored package changed during regeneration",
        )
    if rejected_path is not None:
        rejected = read_json(rejected_path)
        _require(rejected.get("status") == "rejected", "authored_sabotage_not_rejected", "Sabotaged anchor resolution was not rejected")
        errors = rejected.get("errors", [])
        _require(
            rejected.get("authored_anchor_refused_count", 0) >= 1
            and any(error.get("code") == "authored-overlay-resolution" for error in errors),
            "authored_sabotage_reason_unproven",
            "Sabotaged Apply did not fail closed on semantic anchor resolution",
        )
        _require(
            rejected_generated_hashes is not None
            and rejected_generated_hashes.get("before")
            and rejected_generated_hashes.get("before") == rejected_generated_hashes.get("after"),
            "rejected_apply_rollback_mismatch",
            "Rejected Apply did not restore the exact generated tree",
        )
    _require(max(receipt["duration_seconds"] for receipt in accepted_receipts) <= threshold, "unreal_budget_exceeded", "Unreal import exceeded its frozen budget")
    _require(
        len({receipt["semantic_fingerprint"] for receipt in accepted_receipts}) == 1,
        "d3_mismatch",
        "Repeated, locality, incremental, or clean-rebuilt Unreal worlds differ semantically",
    )
    if realization_profile is not None:
        _require(incremental is not None, "layered_realization_leg_missing", "Layered Matrix has no incremental realization")
        _require(road_locality is not None and road_locality_dirty_unit is not None, "layered_realization_leg_missing", "Layered Matrix has no road-locality realization")
        layered = validate_layered_realization(
            {"first": first, "second": second, "road_locality": road_locality, "incremental": incremental, "clean": clean},
            expected,
            realization_profile,
            road_locality_dirty_unit,
        )
        return {
            "D3": second["semantic_fingerprint"],
            "manifest_lifecycle": manifest_evidence,
            "duration_seconds": second["duration_seconds"],
            "generated_source_bytes": second["generated_source_bytes"],
            "verified_output_count": second["verified_output_count"],
            "authored_layer_preserved": second["authored_correction_layer_preserved"],
            **layered,
        }
    changes = second["changes"]
    _require(changes["road_sections"] == expected["expected_road_fragments"], "road_realization_mismatch", "Road fragment count differs from the contract")
    _require(changes["building_sections"] == expected["expected_buildings"], "building_realization_mismatch", "Building realization count differs from the contract")
    _require(changes["cross_cell_road_shared_boundary_points"] >= 1, "road_boundary_missing", "Cross-cell road has no shared boundary coordinate")
    _require(changes["updated_landscape_components"] == 0, "unchanged_landscape_updated", "Unchanged import updated Landscape components")
    if incremental is not None:
        _require(
            incremental.get("input_sha256") != second.get("input_sha256"),
            "incremental_realization_input_unproven",
            "Incremental realization reused the full compile receipt",
        )
        if expected["require_landscape"]:
            _require(
                incremental["changes"]["updated_landscape_components"] == 0
                and incremental["changes"]["updated_actors"] >= 1,
                "incremental_landscape_noop_unproven",
                "Content-identical incremental terrain rewrote Landscape data or refreshed no generated actors",
            )
        else:
            _require(
                incremental["changes"]["updated_actors"] >= 1,
                "incremental_actor_scope_unproven",
                "Incremental realization did not update a generated actor",
            )
    if expected["require_landscape"]:
        _require(second["authored_correction_layer_preserved"], "authored_layer_lost", "Authored Landscape layer was not preserved")
        _require(second["georeferencing_placement_error_m"] == 0, "placement_error", "GeoReferencing placement is not exact")
    return {
        "D3": second["semantic_fingerprint"],
        "manifest_lifecycle": manifest_evidence,
        "duration_seconds": second["duration_seconds"],
        "generated_source_bytes": second["generated_source_bytes"],
        "verified_output_count": second["verified_output_count"],
        "road_fragments": changes["road_sections"],
        "building_sections": changes["building_sections"],
        "authored_layer_preserved": second["authored_correction_layer_preserved"],
        "runtime_route": second.get("runtime_route", ""),
        "runtime_navigation_path_m": second.get("runtime_navigation_path_m", 0),
        "runtime_collision_probe_count": second.get("runtime_collision_probe_count", 0),
        "runtime_collision_orientation_probe_count": second.get("runtime_collision_orientation_probe_count", 0),
        "procedural_mesh_buffer_bytes": second.get("procedural_mesh_buffer_bytes", 0),
        "mesh_section_draw_call_upper_bound": second.get("procedural_mesh_section_draw_call_upper_bound", 0),
    }


def _forbidden_files(roots: list[Path], suffixes: set[str], names: set[str]) -> list[str]:
    matches: list[str] = []
    for root in roots:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and (path.suffix.lower() in suffixes or path.name.lower() in names):
                matches.append(str(path))
    return sorted(matches)


def _validate_bootstrap(environment: dict[str, Any]) -> dict[str, Any]:
    preflight = environment["bootstrap_preflight"]
    clean = bool(preflight["all_absent"])
    if clean:
        _require(not any(item["present"] for item in preflight["observations"]), "clean_preflight_conflict", "Clean bootstrap preflight contains existing state")
        _require(environment["python_environment_bytes"] > 0, "python_bootstrap_missing", "Clean bootstrap did not create the hash-locked Python environment")
        _require(environment["native_tools_bytes"] > 0, "native_bootstrap_missing", "Clean bootstrap did not install the pinned native tools")
        _require(environment["immutable_cache_bytes"] > 0, "source_reacquisition_missing", "Clean bootstrap did not reacquire source payloads")
        _require(environment["network_transfer_bytes"] > 0, "source_network_evidence_missing", "Clean bootstrap did not record source acquisition into the empty cache")
    return {
        "mode": "clean-bootstrap" if clean else "prepared-machine",
        "all_required_roots_absent": clean,
        "observation_count": len(preflight["observations"]),
    }


def _validate_package_map_argument(package_log: Path, required_map: str) -> list[str]:
    """Prove the cook map list at both AutomationTool layers.

    UE 5.8 AutomationTool ProjectParams.cs parses -MapsToCook=<A>+<B> from the
    BuildCookRun command line into ProjectParams.MapsToCook; CookCommand then
    launches the cook commandlet with the translated -Map=<A>+<B> argument.
    Both layers must show the complete configured-plus-required map set.
    """
    text = package_log.read_text(encoding="utf-8", errors="replace")
    configured_text = (REPO_ROOT / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
    configured = re.findall(r'^\+MapsToCook=\(FilePath="(?P<map>/[A-Za-z0-9_/-]+)"\)\s*$', configured_text, re.MULTILINE)
    expected = set(configured)
    expected.add(required_map)

    parsed = next((line for line in text.splitlines() if line.startswith("Parsing command line:")), "")
    uat_match = re.search(r"(?i)(?:^|\s)-MapsToCook=([^\s]+)", parsed)
    _require(uat_match is not None, "package_map_argument_missing", "UAT did not receive the explicit -MapsToCook map-list argument")
    uat_maps = uat_match.group(1).split("+")
    _require(expected.issubset(set(uat_maps)), "package_map_set_incomplete", "The focused cook omitted a configured or required release map", missing=sorted(expected - set(uat_maps)))

    cook_line = next((line for line in text.splitlines() if re.search(r"(?i)-run=Cook(?:\s|$)", line)), "")
    cook_match = re.search(r"(?i)(?:^|\s)-Map=([^\s]+)", cook_line)
    _require(cook_match is not None, "package_cook_map_argument_missing", "The cook commandlet invocation carries no translated -Map list")
    cook_maps = cook_match.group(1).split("+")
    _require(expected.issubset(set(cook_maps)), "package_cook_map_set_incomplete", "The cook commandlet omitted a configured or required release map", missing=sorted(expected - set(cook_maps)))
    return uat_maps


def _validate_presentation_gate(profile: dict[str, Any], execution: dict[str, Any]) -> dict[str, Any] | None:
    settings = profile.get("presentation_gate")
    gate_evidence = execution.get("presentation_gate")
    if settings is None:
        _require(gate_evidence is None, "presentation_gate_unexpected", "Unrequested presentation evidence was attached")
        return None
    _require(gate_evidence is not None, "presentation_gate_missing", "The profile requires packaged rendered evidence")
    receipt_path = Path(gate_evidence["result"])
    receipt = read_json(receipt_path)
    validate_against(receipt, "presentation-result.schema.json")
    _require(receipt["status"] == "accepted", "presentation_gate_rejected", "Packaged rendered evidence is not accepted")
    _require("null" not in receipt["rhi"].lower() and receipt["rhi"] != "unavailable", "presentation_gate_null_rhi", "Rendered acceptance cannot use NullRHI")
    _require(receipt["operation_id"] == gate_evidence["operation_id"], "presentation_gate_operation_mismatch", "Rendered evidence belongs to another end-to-end operation")
    _require(receipt["build_configuration"] == "Shipping", "presentation_gate_configuration_mismatch", "The measured packaged process is not a Shipping build")
    reported_executable = os.path.normcase(str(Path(receipt["executable"]).resolve()))
    staged_executable = os.path.normcase(str(Path(gate_evidence["shipping_executable"]).resolve()))
    _require(reported_executable == staged_executable, "presentation_gate_executable_mismatch", "The measured process is not the staged Shipping executable")
    _require(re.fullmatch(r"[a-f0-9]{64}", gate_evidence.get("shipping_executable_sha256", "")) is not None, "presentation_gate_executable_hash_missing", "The staged Shipping executable hash was not recorded")
    world_name = settings["world_profile"]
    world_settings = profile["profiles"][world_name]
    world_record = execution["profiles"][world_name]
    presentation = world_record["presentation_profile"]
    runtime = world_record["runtime_profile"]
    _require(receipt["map_package"] == world_settings["map_package"], "presentation_gate_map_mismatch", "Rendered evidence belongs to another map")
    _require(receipt["presentation_profile"] == presentation["profile_id"] and receipt["presentation_profile_sha256"] == presentation["sha256"], "presentation_gate_profile_mismatch", "Rendered evidence belongs to another presentation profile")
    _require(receipt["runtime_profile"] == runtime["profile_id"] and receipt["runtime_profile_sha256"] == runtime["sha256"], "presentation_gate_runtime_mismatch", "Rendered evidence belongs to another runtime profile")
    resolution_x, resolution_y = settings["resolution"]
    _require(receipt["machine_profile_id"] == settings["machine_profile_id"], "presentation_gate_machine_mismatch", "Rendered evidence used another machine profile")
    _require(receipt["resolution_x"] == resolution_x and receipt["resolution_y"] == resolution_y, "presentation_gate_resolution_mismatch", "Rendered evidence used another resolution")
    _require(receipt["scalability_level"] == settings["scalability_level"], "presentation_gate_scalability_mismatch", "Rendered evidence used another scalability level")
    _require(receipt["warmup_frames"] == settings["warmup_frames"] and receipt["sample_frames_per_camera"] == settings["sample_frames"], "presentation_gate_window_mismatch", "Rendered evidence used another warmup or sample window")
    _require(receipt["requested_camera_roles"] == settings["camera_roles"], "presentation_gate_camera_set_mismatch", "Rendered evidence used another camera set")
    _require(receipt["p95_frame_time_budget_ms"] == gate_evidence["frame_budget_ms"], "presentation_gate_budget_mismatch", "Rendered evidence checked another runtime budget")
    _require(receipt["worst_p95_frame_time_ms"] <= receipt["p95_frame_time_budget_ms"], "presentation_gate_frame_budget_exceeded", "Packaged p95 frame time exceeded the frozen runtime budget")

    capture_root = (receipt_path.parent / "captures").resolve()
    screenshots: list[dict[str, Any]] = []
    roles: list[str] = []
    for viewpoint in receipt["viewpoints"]:
        roles.append(viewpoint["camera_role"])
        _require(viewpoint["sample_count"] == settings["sample_frames"], "presentation_gate_sample_count_mismatch", "A viewpoint has an incomplete sample window", camera=viewpoint["camera_role"])
        screenshot = Path(viewpoint["screenshot"]).resolve()
        try:
            screenshot.relative_to(capture_root)
        except ValueError as error:
            raise ValidationFailure("presentation_gate_capture_scope_invalid", "A rendered capture escapes the evidence root", path=str(screenshot)) from error
        _require(screenshot.is_file() and screenshot.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", "presentation_gate_capture_invalid", "A rendered capture is missing or not PNG", path=str(screenshot))
        screenshots.append({"camera_role": viewpoint["camera_role"], "path": str(screenshot), "sha256": file_hash(screenshot), "byte_size": screenshot.stat().st_size})
    _require(roles == settings["camera_roles"], "presentation_gate_viewpoint_mismatch", "Rendered viewpoints do not match the requested order")
    return {
        "receipt": str(receipt_path),
        "executable": receipt["executable"],
        "build_configuration": receipt["build_configuration"],
        "shipping_executable_sha256": gate_evidence["shipping_executable_sha256"],
        "machine_profile_id": receipt["machine_profile_id"],
        "gpu_adapter": receipt["gpu_adapter"],
        "gpu_driver": receipt["gpu_driver"],
        "rhi": receipt["rhi"],
        "resolution": [receipt["resolution_x"], receipt["resolution_y"]],
        "scalability_level": receipt["scalability_level"],
        "warmup_frames": receipt["warmup_frames"],
        "samples_per_camera": receipt["sample_frames_per_camera"],
        "worst_p95_frame_time_ms": receipt["worst_p95_frame_time_ms"],
        "budget_ms": receipt["p95_frame_time_budget_ms"],
        "screenshots": screenshots,
    }


def validate(profile: dict[str, Any], execution: dict[str, Any]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    thresholds = profile["thresholds"]
    checks: list[dict[str, Any]] = []
    metrics: dict[str, Any] = {"profiles": {}}
    for suite in execution["test_suites"]:
        _require(suite["status"] == "accepted", "test_suite_failed", "Required failure or architecture tests failed", suite=suite["name"])
    checks.append({"check": "fault_injection_and_architecture", "status": "passed", "suites": len(execution["test_suites"])})
    bootstrap = _validate_bootstrap(execution["environment"])
    checks.append({"check": "bootstrap_environment", "status": "passed", **bootstrap})
    restoration = execution.get("generated_tree_restoration", {})
    restored_owners = restoration.get("owners", {})
    expected_owners = sorted({settings["world_data_plugin"] for settings in profile["profiles"].values()})
    _require(
        restoration.get("status") == "accepted"
        and sorted(restored_owners) == expected_owners
        and all(
            owner.get("before_sha256") == owner.get("after_sha256")
            and isinstance(owner.get("file_count"), int)
            for owner in restored_owners.values()
        ),
        "matrix_generated_restore_unproven",
        "Matrix did not prove restoration of every pre-run generated scope",
    )
    checks.append({"check": "generated_tree_restoration", "status": "passed"})

    for name, expected in profile["profiles"].items():
        record = execution["profiles"][name]
        source_result = _accepted(Path(record["source_result"]), "https://alis.world/schemas/world-source/operation-result-v1.json")
        _require(source_result["profile_id"] == expected["source_profile"], "source_profile_mismatch", "Source evidence belongs to another profile")
        roots = [Path(record["compile_first_root"]), Path(record["compile_second_root"])]
        determinism = _determinism(*roots)
        compile_metrics = read_json(roots[0] / "reports" / "metrics.json")
        _require(compile_metrics["duration_ms"] / 1000.0 <= thresholds["full_compile_seconds"], "compile_budget_exceeded", "Full compile exceeded its frozen budget")
        _require(compile_metrics["canonical_bytes"] <= thresholds["canonical_bytes"], "canonical_size_exceeded", "Canonical output exceeded its frozen budget")
        coverage = read_json(roots[0] / "canonical" / "coverage.json")
        _require(coverage["feature_count"] == expected["expected_features"], "feature_count_mismatch", "Canonical feature count differs from the contract")
        integrity = _validate_canonical_integrity(roots[0])
        canonical_authority = None
        if expected.get("canonical_authority"):
            authority = record.get("canonical_authority")
            _require(
                isinstance(authority, dict),
                "canonical_authority_evidence_missing",
                "Territory Matrix did not authenticate canonical authority",
            )
            canonical_authority = _validate_canonical_authority_candidate(
                roots[0], Path(record["realization_compile_result"]), authority
            )
        incremental = _validate_incremental(roots[0], Path(record["incremental_root"]), thresholds["incremental_compile_seconds"])
        realization = _validate_realization(
            Path(record["unreal_first"]),
            Path(record["unreal_second"]),
            Path(record["unreal_clean_rebuild"]),
            expected,
            record["presentation_profile"],
            thresholds["unreal_import_seconds"],
            record.get("runtime_profile"),
            record.get("realization_profile"),
            incremental_path=Path(record["unreal_incremental"]),
            road_locality_path=Path(record["unreal_road_locality"]),
            road_locality_dirty_unit=record["road_locality_dirty_unit"],
            rejected_path=Path(record["unreal_rejected_apply"]),
            authored=record["authored_overlay_profile"],
            authored_package_hashes=record["authored_package_hashes"],
            rejected_generated_hashes=record["rejected_generated_hashes"],
            realization_compile_result_sha256=record["realization_compile_result_sha256"],
        )
        if "realization_profile" not in expected:
            _require(1 + realization["building_sections"] == expected["expected_realized_features"], "realized_feature_count_mismatch", "Realized feature identity count differs from the bounded adapter contract")
        _require(realization["verified_output_count"] <= thresholds["generated_packages"], "generated_output_budget_exceeded", "Verified Unreal output count exceeded its frozen budget")
        _require(record["generated_package_count"] <= thresholds["generated_packages"], "generated_package_budget_exceeded", "Generated Unreal package count exceeded its frozen budget")
        if name == "synthetic":
            rejections = read_json(roots[0] / "reports" / "rejections.json")["rejections"]
            expected_rejections = expected.get("expected_rejections", 1)
            _require(len(rejections) == expected_rejections, "invalid_fixture_mismatch", "Invalid fixture rejection count differs from the contract")
        metrics["profiles"][name] = {
            "source_seconds": record["source_seconds"],
            "source_output_bytes": tree_size(Path(record["source_root"])),
            "full_compile_seconds": compile_metrics["duration_ms"] / 1000.0,
            "canonical_bytes": compile_metrics["canonical_bytes"],
            "generated_package_count": record["generated_package_count"],
            **determinism,
            "integrity": integrity,
            "incremental": incremental,
            "canonical_authority": canonical_authority,
            "unreal": realization,
        }
        checks.append({"check": f"{name}_D0_D3", "status": "passed"})

    notices = _validate_notices(Path(execution["profiles"]["kazan"]["compile_first_root"]))
    checks.append({"check": "attribution_notices_and_offer", "status": "passed", **notices})
    immutable_bytes = execution["environment"]["immutable_cache_bytes"]
    _require(immutable_bytes <= thresholds["provider_payload_bytes"], "provider_budget_exceeded", "Immutable provider payload exceeded its frozen budget")
    forbidden = _forbidden_files(
        [Path(path) for path in execution["generated_roots"]],
        set(profile["package"]["forbidden_suffixes"]),
        set(profile["package"]["forbidden_names"]),
    )
    _require(not forbidden, "forbidden_generated_payload", "Raw or provider-normalized data reached a generated root", paths=forbidden)
    metrics["environment"] = execution["environment"]
    metrics["environment"]["mode"] = bootstrap["mode"]
    checks.append({"check": "generated_content_distribution_boundary", "status": "passed"})
    return checks, metrics
