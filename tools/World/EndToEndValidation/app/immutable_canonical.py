"""Authenticate immutable canonical inputs without accepting a new producer run."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

from World.CanonicalCompilation.api import CanonicalCompilationError, canonical_active_path, validate_canonical_authority
from .contracts import ValidationFailure, canonical_hash, file_hash, read_json, validate_against
from .canonical_mode import _profile_upstream_projection


MODE = "immutable_canonical"
SCHEMA = "https://alis.world/schemas/world-validation/immutable-canonical-selection-v1.json"


def _refuse(message: str, **details: object) -> None:
    raise ValidationFailure("immutable_canonical_input_unproven", message, **details)


def _git(repo_root: Path, arguments: list[str]) -> bytes:
    result = subprocess.run(["git", *arguments], cwd=repo_root, capture_output=True, check=False)
    if result.returncode:
        _refuse("The true immutable-input history is unavailable")
    return result.stdout


def _unchanged_file(repo_root: Path, relative: str, base: str) -> tuple[Path, str]:
    path = (repo_root / relative).resolve()
    if not path.is_relative_to(repo_root.resolve()) or not path.is_file():
        _refuse("A selected canonical input is missing or escapes its owner", path=relative)
    historical = hashlib.sha256(_git(repo_root, ["show", f"{base}:{relative}"])).hexdigest()
    current = file_hash(path)
    if historical != current:
        _refuse("A selected canonical authority or geographic input changed; use its upstream gate", path=relative)
    return path, current


def consumer_identity() -> str:
    """Current reader/contract identity, distinct from the frozen producing compiler."""
    world = Path(__file__).resolve().parents[2]
    files = []
    for owner in ("CanonicalCompilation", "SourceIngestion", "ExecutionEnvironment", "EndToEndValidation"):
        root = world / owner
        files.extend(root.glob("api.py"))
        files.extend(root.glob("app/*.py"))
        files.extend(root.glob("contracts/*.json"))
    return canonical_hash({path.relative_to(world).as_posix(): file_hash(path) for path in sorted(files)})


def selected_profiles(profile: dict) -> dict:
    selected = {name: settings for name, settings in profile["profiles"].items()
                if settings.get("canonical_authority") is True}
    if not selected:
        _refuse("No immutable accepted canonical input was selected")
    return selected


def _selected_inputs(repo_root: Path, settings: dict, base: str) -> tuple:
    compiler_relative = settings["compiler_profile_path"]
    compiler_path, compiler_hash = _unchanged_file(repo_root, compiler_relative, base)
    try:
        active_path = canonical_active_path(repo_root, compiler_path)
    except CanonicalCompilationError as error:
        _refuse("The selected canonical owner/profile is invalid", reason=error.code)
    active_relative = active_path.relative_to(repo_root.resolve()).as_posix()
    _, active_hash = _unchanged_file(repo_root, active_relative, base)
    pins = {compiler_relative: compiler_hash}
    external = read_json(active_path)["external_inputs"]
    if settings["source_profile_path"] != external["source_profile_path"]:
        _refuse("The selected source path differs from the immutable authority")
    for key, value in external.items():
        if key.endswith("_path") and value is not None:
            _, pins[value] = _unchanged_file(repo_root, value, base)
    if read_json(repo_root / settings["source_profile_path"])["profile_id"] != settings["source_profile"]:
        _refuse("The logical source profile differs from the selected immutable source file")
    return compiler_path, active_path, active_relative, active_hash, pins


def snapshot_selection(repo_root: Path, profile: dict, base: str, validation_profile_path: Path) -> dict:
    if not isinstance(base, str) or not base:
        _refuse("Immutable canonical consumption requires the true reviewed slice base")
    revision = _git(repo_root, ["rev-parse", "--verify", "--end-of-options", f"{base}^{{commit}}"])
    resolved_base = revision.decode("ascii").strip()
    if not validation_profile_path.resolve().is_relative_to(repo_root.resolve()):
        _refuse("The validation profile escapes its repository owner")
    profile_relative = validation_profile_path.resolve().relative_to(repo_root.resolve()).as_posix()
    historical_profile = _git(repo_root, ["show", f"{resolved_base}:{profile_relative}"])
    current_document = read_json(validation_profile_path)
    previous = _profile_upstream_projection(json.loads(historical_profile))
    current = _profile_upstream_projection(current_document)
    if previous["profile_id"] != current["profile_id"] or previous["thresholds"] != current["thresholds"] or any(
            current["profiles"].get(name) != settings for name, settings in previous["profiles"].items()):
        _refuse("An existing upstream entry, selection or validation budget changed; use its upstream gate")
    if selected_profiles(profile) != selected_profiles(current_document):
        _refuse("The selected World inputs differ from the current validation profile")
    selected = selected_profiles(profile)
    # Refuse any changed geographic pin before loading either city's large bundle.
    inputs = {name: _selected_inputs(repo_root, settings, resolved_base) for name, settings in selected.items()}
    records = {}
    for name, settings in selected.items():
        compiler_path, active_path, active_relative, active_hash, pins = inputs[name]
        try:
            authority = validate_canonical_authority(repo_root, active_path, compiler_path)
        except CanonicalCompilationError as error:
            _refuse("The selected canonical authority failed authentication", profile=name, reason=error.code)
        if 'selected_refinement' in authority:
            _refuse('A selected refinement needs explicit effective-input acceptance; base-only consumption cannot certify it', profile=name)
        if (authority["world_data_plugin"] != settings["world_data_plugin"] or
                authority["profile_id"] != settings["compiler_profile"] or
                authority["external_inputs"]["source_profile_path"] != settings["source_profile_path"]):
            _refuse("The selected canonical artifact belongs to different source/compiler inputs", profile=name)
        archive = active_path.parent / authority["bundle"]["path"]
        with zipfile.ZipFile(archive) as bundle:
            contract_bytes = bundle.read("run_contract.json")
            contract = json.loads(contract_bytes)
            result = json.loads(bundle.read("compile_result.json"))
        if (contract["run_inputs_hash"] != authority["inputs_hash"] or
                result["inputs_hash"] != authority["inputs_hash"] or result["operation"] != "compile"):
            _refuse("Historical base result and producing run contract disagree", profile=name)
        records[name] = {
            "authority_id": authority["authority_id"], "active_path": active_relative,
            "active_sha256": active_hash, "bundle_sha256": authority["bundle"]["sha256"],
            "compile_result_sha256": authority["compile_result_sha256"],
            "run_contract_sha256": hashlib.sha256(contract_bytes).hexdigest(),
            "historical_producer_implementation_sha256": contract["implementation_sha256"],
            "historical_source_inputs_hash": contract["source_inputs_hash"],
            "historical_execution_identity": contract["execution_environment"]["identity_sha256"],
            "inputs_hash": authority["inputs_hash"], "geographic_input_files": pins,
        }
    selection = {
        "$schema": SCHEMA, "schema_version": 1, "mode": MODE,
        "changed_base": resolved_base, "profiles": records,
        "validation_profile_path": profile_relative, "validation_profile_sha256": file_hash(validation_profile_path),
        "base_validation_profile_sha256": hashlib.sha256(historical_profile).hexdigest(),
        "current_consumer_implementation_sha256": consumer_identity(),
        "executed_upstream_operations": {"source_acquisition": False, "base_compilation": False,
                                         "canonical_promotion": False},
    }
    validate_against(selection, "immutable-canonical-selection.schema.json")
    return selection


def validate_selection(repo_root: Path, profile: dict, selection: dict) -> None:
    validate_against(selection, "immutable-canonical-selection.schema.json")
    current = snapshot_selection(repo_root, profile, selection["changed_base"], repo_root / selection["validation_profile_path"])
    if current != selection:
        _refuse("Immutable base selection or current consuming implementation changed after admission")


def validate_execution(repo_root: Path, profile: dict, execution: dict) -> None:
    mode = execution.get("matrix_mode")
    if mode not in {"full", "canonical_authority", MODE}:
        _refuse("Stored World acceptance has an unknown operation mode")
    if mode != MODE:
        return
    selection = execution.get("immutable_canonical_selection")
    if not isinstance(selection, dict):
        _refuse("Immutable canonical execution has no authenticated selected-input record")
    validate_selection(repo_root, profile, selection)
    if set(execution.get("profiles", {})) != set(selection["profiles"]):
        _refuse("The World proof omits a selected immutable canonical input")
    for name, record in execution["profiles"].items():
        if any(record.get(key) is not None for key in
               ("source_root", "source_result", "compile_first_root", "compile_second_root", "incremental_root")):
            _refuse("An immutable artifact-consumer receipt contains fresh producer execution", profile=name)
        authority = record.get("canonical_authority", {})
        if any(authority.get(key) != selection["profiles"][name][key] for key in
               ("authority_id", "active_path", "active_sha256", "bundle_sha256", "compile_result_sha256")):
            _refuse("A World leg consumed a different canonical authority", profile=name)
