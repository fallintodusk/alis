from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from World.SourceIngestion.api import (
    IngestionError,
    default_output_root,
    file_hash as source_file_hash,
    read_json as read_source_json,
    run_contract,
    validate_document as validate_source_document,
    validate_profile,
)

from .contracts import CompilerError, file_hash, read_json, resolve_owned_path, validate_document


@dataclass(frozen=True)
class SourceBundle:
    root: Path
    result_path: Path
    result: dict[str, Any]
    source_profile: dict[str, Any]
    source_run_contract: dict[str, Any]
    ledger: dict[str, Any]
    feature_snapshot_id: str
    features: list[dict[str, Any]]
    raster_path: Path
    raster: dict[str, Any]


def _verified_output(result: dict[str, Any], root: Path, kind: str) -> Path:
    matches = [entry for entry in result["outputs"] if entry["kind"] == kind]
    if len(matches) != 1:
        raise CompilerError("source_contract_missing", "Source result does not contain one required output", kind=kind)
    entry = matches[0]
    path = resolve_owned_path(root, entry["path"])
    if path.stat().st_size != entry["byte_size"] or file_hash(path) != entry["sha256"]:
        raise CompilerError("source_output_changed", "Source output no longer matches its accepted receipt", kind=kind)
    return path


def _verify_all_outputs(result: dict[str, Any], root: Path) -> None:
    for entry in result["outputs"]:
        if "path" not in entry:
            continue
        path = resolve_owned_path(root, entry["path"])
        if path.stat().st_size != entry["byte_size"] or file_hash(path) != entry["sha256"]:
            raise CompilerError("source_output_changed", "Accepted source output changed", kind=entry["kind"])


def _load_feature_shards(manifest_path: Path) -> tuple[str, list[dict[str, Any]]]:
    manifest = read_source_json(manifest_path)
    validate_source_document(manifest, manifest_path)
    features: list[dict[str, Any]] = []
    for shard in manifest["shards"]:
        path = (manifest_path.parent / shard["path"]).resolve()
        if not path.is_relative_to(manifest_path.parent.resolve()) or not path.is_file():
            raise CompilerError("source_shard_missing", "Provider feature shard is unavailable", shard=shard["shard_id"])
        if path.stat().st_size <= 0 or source_file_hash(path) != shard["sha256"]:
            raise CompilerError("source_shard_changed", "Provider feature shard differs from its manifest", shard=shard["shard_id"])
        document = read_source_json(path)
        validate_source_document(document, path)
        if document["snapshot_id"] != manifest["snapshot_id"] or len(document["features"]) != shard["count"]:
            raise CompilerError("source_shard_conflict", "Provider feature shard conflicts with its manifest")
        features.extend(document["features"])
    if len(features) != manifest["total_features"]:
        raise CompilerError("source_feature_count_mismatch", "Provider feature count differs from its manifest")
    return manifest["snapshot_id"], features


def _current_source_result(
    repo_root: Path, compilation_root: Path, compiler_profile: dict[str, Any], explicit: Path | None
) -> tuple[Path, Path, dict[str, Any], dict[str, Any]]:
    source_profile_path = resolve_owned_path(repo_root, compiler_profile["source_profile"])
    source_profile = read_source_json(source_profile_path)
    validate_profile(source_profile, source_profile_path, repo_root)
    if source_profile.get("profile_id") != compiler_profile["source_profile_id"]:
        raise CompilerError(
            "source_profile_mismatch",
            "Compiler source profile path resolves to another profile",
            expected=compiler_profile["source_profile_id"],
            actual=source_profile.get("profile_id"),
        )
    current_run_contract = run_contract(repo_root, source_profile)
    if explicit is None:
        source_root = default_output_root(
            repo_root, source_profile, current_run_contract["run_inputs_hash"], "run"
        )
        result_path = source_root / "run_result.json"
    else:
        result_path = explicit.resolve()
        source_root = result_path.parent
    if not result_path.is_file():
        raise CompilerError(
            "source_run_missing",
            "Run the accepted source-ingestion profile before compilation",
            profile=source_profile["profile_id"],
        )
    result = read_source_json(result_path)
    validate_source_document(result, result_path)
    if result.get("operation") != "run" or result.get("status") != "accepted":
        raise CompilerError("source_run_rejected", "Source result is not an accepted complete run")
    if result.get("profile_id") != source_profile["profile_id"] or result.get("inputs_hash") != current_run_contract["run_inputs_hash"]:
        raise CompilerError("source_run_stale", "Source result does not certify the current source contract")
    return source_root, result_path, result, source_profile


def load_source_bundle(
    repo_root: Path,
    compilation_root: Path,
    compiler_profile: dict[str, Any],
    explicit_result: Path | None = None,
) -> SourceBundle:
    try:
        root, result_path, result, source_profile = _current_source_result(
            repo_root, compilation_root, compiler_profile, explicit_result
        )
        _verify_all_outputs(result, root)
        source_run_contract_path = _verified_output(result, root, "run_contract")
        ledger_path = _verified_output(result, root, "source_ledger")
        manifest_path = _verified_output(result, root, "provider_features_manifest")
        raster_path = _verified_output(result, root, "raster_layer")
        ledger = read_source_json(ledger_path)
        validate_source_document(ledger, ledger_path)
        source_run_contract = read_source_json(source_run_contract_path)
        validate_source_document(source_run_contract, source_run_contract_path)
        if ledger.get("policy_result") != "approved" or any(
            snapshot.get("policy_result") != "approved" for snapshot in ledger.get("snapshots", [])
        ):
            raise CompilerError("source_rights_unresolved", "Source ledger is not approved")
        feature_snapshot_id, features = _load_feature_shards(manifest_path)
        fixture_path_value = compiler_profile.get("fixture_features")
        if fixture_path_value:
            fixture_path = resolve_owned_path(repo_root, fixture_path_value)
            fixture = read_source_json(fixture_path)
            validate_source_document(fixture, fixture_path)
            features.extend(fixture["features"])
        raster = read_source_json(raster_path)
        validate_source_document(raster, raster_path)
        return SourceBundle(
            root,
            result_path,
            result,
            source_profile,
            source_run_contract,
            ledger,
            feature_snapshot_id,
            features,
            raster_path,
            raster,
        )
    except IngestionError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
