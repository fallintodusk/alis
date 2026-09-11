from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator

from World.CanonicalCompilation.api import (
    CanonicalCompilationError,
    terrain_impact_cells_for_profile,
)

from .profile_inputs import ProfileInputError, profile_input_contract
from .roots import REPO_ROOT, WorldDataRootError, resolve_owned_data_path, resolve_world_data_roots


COMPONENT_ROOT = Path(__file__).resolve().parents[1]


class ValidationFailure(RuntimeError):
    def __init__(self, code: str, message: str, **details: object) -> None:
        super().__init__(message)
        self.code = code
        self.details = details


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationFailure("json_read_failed", "Required validation JSON is unavailable", path=str(path)) from error
    if not isinstance(value, dict):
        raise ValidationFailure("json_shape_invalid", "Validation JSON root must be an object", path=str(path))
    return value


def validate_against(document: dict[str, Any], schema_name: str) -> None:
    schema_path = COMPONENT_ROOT / "contracts" / schema_name
    schema = read_json(schema_path)
    errors = sorted(Draft202012Validator(schema).iter_errors(document), key=lambda item: list(item.path))
    if errors:
        first = errors[0]
        location = "/".join(str(item) for item in first.path) or "$"
        raise ValidationFailure(
            "schema_validation_failed",
            "World validation contract is invalid",
            schema=schema_name,
            location=location,
            reason=first.message,
        )


def load_profile(path: Path) -> dict[str, Any]:
    profile = read_json(path)
    validate_against(profile, "validation-profile.schema.json")
    owner_data_roots: dict[str, Path] = {}
    presentation_contracts: dict[str, tuple[str, str]] = {}
    for name, settings in profile["profiles"].items():
        owner = settings["world_data_plugin"]
        try:
            roots = resolve_world_data_roots(owner)
        except WorldDataRootError as error:
            raise ValidationFailure(
                "world_data_plugin_invalid",
                str(error),
                profile=name,
                owner=owner,
            ) from error
        owner_data_roots[owner] = roots.data_root.resolve()
        if not settings["map_package"].startswith(f"/{owner}/Generated/"):
            raise ValidationFailure(
                "world_data_owner_mismatch",
                "Validation map does not belong to its declared world-data plugin",
                profile=name,
            )
        for field in (
            "presentation_profile",
            "runtime_profile",
            "realization_profile",
            "authored_overlay_profile",
            "authored_overlay_sabotage_profile",
            "source_profile_path",
            "compiler_profile_path",
        ):
            value = settings.get(field)
            if value is None:
                continue
            try:
                resolve_owned_data_path(roots.data_root, value)
            except WorldDataRootError as error:
                raise ValidationFailure(
                    "world_data_owner_mismatch",
                    "Profile path does not belong to its descriptor-derived Data root",
                    profile=name,
                    field=field,
                ) from error
        presentation_path = resolve_owned_data_path(roots.data_root, settings["presentation_profile"])
        presentation = read_json(presentation_path)
        presentation_contract = (str(presentation.get("profile_id", "")), file_hash(presentation_path))
        prior_contract = presentation_contracts.setdefault(owner, presentation_contract)
        if prior_contract != presentation_contract:
            raise ValidationFailure(
                "presentation_profile_owner_conflict",
                "Profiles sharing one world-data owner must use one presentation profile",
                owner=owner,
                profile=name,
            )
        _validate_owned_profile_pair(name, settings, roots.data_root)
        _validate_realization_profile(name, settings, roots.data_root)
    required_map = profile["package"]["required_map"]
    required_profiles = [
        settings for settings in profile["profiles"].values()
        if settings["map_package"] == required_map
    ]
    if len(required_profiles) != 1:
        raise ValidationFailure(
            "required_map_unknown",
            "Package required_map must name exactly one validated world profile map",
        )
    production_root = owner_data_roots[required_profiles[0]["world_data_plugin"]]
    if not path.resolve().is_relative_to(production_root):
        raise ValidationFailure(
            "world_data_owner_mismatch",
            "Production validation profile must live under its world-data Data root",
            path=str(path),
        )
    return profile


def _validate_realization_profile(
    name: str,
    settings: dict[str, Any],
    data_root: Path,
) -> None:
    value = settings.get("realization_profile")
    if value is None:
        return
    path = resolve_owned_data_path(data_root, value)
    document = read_json(path)
    if (
        not isinstance(document.get("profile_id"), str)
        or document.get("world_data_plugin") != settings["world_data_plugin"]
        or document.get("canonical_profile_id") != settings["compiler_profile"]
        or document.get("map_package") != settings["map_package"]
    ):
        raise ValidationFailure(
            "realization_profile_mismatch",
            "Realization profile does not match the validation owner, compiler, and map",
            profile=name,
        )


def _validate_owned_profile_pair(
    name: str,
    settings: dict[str, Any],
    data_root: Path,
) -> None:
    missing = [field for field in ("source_profile_path", "compiler_profile_path") if not settings.get(field)]
    if missing:
        raise ValidationFailure(
            "owned_profile_path_missing",
            "World data requires explicit source and compiler profile paths",
            profile=name,
            fields=missing,
        )
    try:
        source_path = resolve_owned_data_path(data_root, settings["source_profile_path"])
        compiler_path = resolve_owned_data_path(data_root, settings["compiler_profile_path"])
    except WorldDataRootError as error:
        raise ValidationFailure(
            "world_data_owner_mismatch",
            "Source/compiler profiles escape the declared Data root",
            profile=name,
        ) from error
    source = read_json(source_path)
    compiler = read_json(compiler_path)
    if source.get("profile_id") != settings["source_profile"]:
        raise ValidationFailure(
            "source_profile_mismatch",
            "Validation source ID does not match the owned source profile",
            profile=name,
        )
    if compiler.get("profile_id") != settings["compiler_profile"]:
        raise ValidationFailure(
            "compiler_profile_mismatch",
            "Validation compiler ID does not match the owned compiler profile",
            profile=name,
        )
    if compiler.get("world_data_plugin") != settings["world_data_plugin"]:
        raise ValidationFailure(
            "world_data_owner_mismatch",
            "Compiler profile belongs to another world-data owner",
            profile=name,
        )
    if compiler.get("source_profile_id") != source.get("profile_id"):
        raise ValidationFailure(
            "source_profile_mismatch",
            "Compiler source ID does not match the owned source profile",
            profile=name,
        )
    compiler_source = compiler.get("source_profile")
    if not isinstance(compiler_source, str):
        raise ValidationFailure(
            "source_profile_mismatch",
            "Compiler profile has no explicit source profile path",
            profile=name,
        )
    candidate = (REPO_ROOT / compiler_source).resolve()
    if candidate != source_path:
        raise ValidationFailure(
            "source_profile_mismatch",
            "Compiler source path does not identify the owned source profile",
            profile=name,
        )
    bounds = tuple(float(value) for value in settings["incremental_bounds"])
    try:
        impacted = terrain_impact_cells_for_profile(compiler_path, bounds)
    except CanonicalCompilationError as error:
        raise ValidationFailure(
            "incremental_scope_invalid",
            "Incremental bounds cannot be evaluated against the compiler profile",
            profile=name,
            reason=str(error),
        ) from error
    if len(impacted) != 1:
        raise ValidationFailure(
            "incremental_scope_invalid",
            "Incremental bounds must rebuild exactly one terrain cell after compiler halos",
            profile=name,
            impacted_cells=[list(cell) for cell in impacted],
        )


def profile_path(value: str) -> Path:
    supplied = Path(value)
    if supplied.suffix == ".json" or supplied.is_absolute() or "/" in value or "\\" in value:
        candidate = supplied if supplied.is_absolute() else REPO_ROOT / supplied
    else:
        candidate = COMPONENT_ROOT / "profiles" / f"{value}.validation.json"
    resolved = candidate.resolve()
    if not resolved.is_relative_to(REPO_ROOT.resolve()) or not resolved.is_file():
        raise ValidationFailure(
            "validation_profile_missing",
            "Validation profile must be an existing repository-owned JSON file",
            profile=value,
        )
    return resolved


def validation_profile_contract(path: Path) -> dict[str, object]:
    try:
        return profile_input_contract(path, REPO_ROOT)
    except ProfileInputError as error:
        raise ValidationFailure(
            "validation_profile_contract_invalid",
            "A validation profile transitive input is missing or invalid",
            reason=str(error),
        ) from error


def write_result(path: Path, result: dict[str, Any]) -> None:
    validate_against(result, "validation-result.schema.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(path.suffix + ".tmp")
    staging.write_text(json.dumps(result, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    os.replace(staging, path)


def canonical_hash(value: object) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_size(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())
