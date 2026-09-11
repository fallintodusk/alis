from __future__ import annotations

import hashlib
import json
from collections.abc import Collection
from pathlib import Path
from typing import Any


class ProfileInputError(RuntimeError):
    pass


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _read_object(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ProfileInputError(f"Profile input is unreadable: {path}") from error
    if not isinstance(value, dict):
        raise ProfileInputError(f"Profile input root is not an object: {path}")
    return value


def _repo_file(repo_root: Path, value: str) -> Path:
    path = (repo_root / value).resolve()
    if not path.is_relative_to(repo_root.resolve()) or not path.is_file():
        raise ProfileInputError(f"Profile input escapes the repository or is missing: {value}")
    return path


def _relative_file(parent: Path, repo_root: Path, value: str) -> Path:
    path = (parent / value).resolve()
    if not path.is_relative_to(repo_root.resolve()) or not path.is_file():
        raise ProfileInputError(f"Profile-relative input escapes the repository or is missing: {value}")
    return path


def _object_definition_inputs(repo_root: Path, definition_ids: set[str]) -> set[Path]:
    if not definition_ids:
        return set()
    manifest_path = repo_root / (
        "Plugins/Resources/ProjectObject/Data/Manifests/public_generated_definitions.json"
    )
    manifest = _read_object(manifest_path)
    result = {manifest_path}
    unresolved = set(definition_ids)
    for asset in manifest.get("assets", []):
        if not isinstance(asset, dict) or asset.get("asset_class") != "ObjectDefinition":
            continue
        source_value = asset.get("source_path")
        artifact_value = asset.get("artifact_path")
        if not isinstance(source_value, str) or not isinstance(artifact_value, str):
            continue
        source = _repo_file(repo_root, source_value)
        definition_id = _read_object(source).get("id")
        typed_id = f"ObjectDefinition:{definition_id}"
        if typed_id in unresolved:
            result.add(source)
            result.add(_repo_file(repo_root, artifact_value))
            unresolved.remove(typed_id)
    if unresolved:
        raise ProfileInputError(
            f"Gameplay placement references unpublished ObjectDefinitions: {sorted(unresolved)}"
        )
    return result


def _authored_package_inputs(repo_root: Path, profile_paths: set[Path]) -> set[Path]:
    result: set[Path] = set()
    for profile_path in profile_paths:
        profile = _read_object(profile_path)
        plugin_name = profile.get("world_data_plugin")
        if not isinstance(plugin_name, str) or not plugin_name:
            raise ProfileInputError(
                f"Authored overlay profile has no world-data owner: {profile_path}"
            )
        descriptors = [
            path
            for path in (repo_root / "Plugins").rglob(f"{plugin_name}.uplugin")
            if path.stem == plugin_name
        ]
        if len(descriptors) != 1:
            raise ProfileInputError(
                f"Authored overlay owner must resolve to one plugin: {plugin_name}"
            )
        content_root = (descriptors[0].parent / "Content").resolve()
        package_prefix = f"/{plugin_name}/Authored/"
        for overlay in profile.get("overlays", []):
            package = overlay.get("authored_package") if isinstance(overlay, dict) else None
            if not isinstance(package, str) or not package.startswith(package_prefix):
                raise ProfileInputError(
                    f"Authored package escapes its world-data owner: {package}"
                )
            relative = Path(package[len(f"/{plugin_name}/"):])
            package_base = (content_root / relative).resolve()
            if not package_base.is_relative_to(content_root):
                raise ProfileInputError(
                    f"Authored package escapes its world-data content: {package}"
                )
            map_path = package_base.with_suffix(".umap")
            if not map_path.is_file():
                raise ProfileInputError(f"Authored package is missing: {package}")
            result.add(map_path)
            for external_root_name in ("__ExternalActors__", "__ExternalObjects__"):
                external_root = content_root / external_root_name / relative
                if external_root.is_dir():
                    result.update(path for path in external_root.rglob("*") if path.is_file())
    return result


def profile_input_files(validation_profile: Path, repo_root: Path) -> list[Path]:
    validation_profile = validation_profile.resolve()
    if not validation_profile.is_relative_to(repo_root.resolve()) or not validation_profile.is_file():
        raise ProfileInputError(f"Validation profile is outside the repository: {validation_profile}")
    profile = _read_object(validation_profile)
    files = {validation_profile}
    source_profiles: set[Path] = set()
    compiler_profiles: set[Path] = set()
    realization_profiles: set[Path] = set()
    authored_overlay_profiles: set[Path] = set()

    for settings in profile.get("profiles", {}).values():
        if not isinstance(settings, dict):
            continue
        for field in (
            "source_profile_path",
            "compiler_profile_path",
            "presentation_profile",
            "runtime_profile",
            "realization_profile",
            "authored_overlay_profile",
            "authored_overlay_sabotage_profile",
        ):
            value = settings.get(field)
            if isinstance(value, str) and value:
                path = _repo_file(repo_root, value)
                files.add(path)
                if field == "source_profile_path":
                    source_profiles.add(path)
                elif field == "compiler_profile_path":
                    compiler_profiles.add(path)
                elif field == "realization_profile":
                    realization_profiles.add(path)
                elif field in ("authored_overlay_profile", "authored_overlay_sabotage_profile"):
                    authored_overlay_profiles.add(path)

    for compiler_path in compiler_profiles:
        compiler = _read_object(compiler_path)
        for field in ("source_profile", "authored_overlay", "fixture_features", "budget_profile", "control_profile"):
            value = compiler.get(field)
            if isinstance(value, str) and value:
                files.add(_repo_file(repo_root, value))

    for source_path in source_profiles:
        source = _read_object(source_path)
        for source_entry in source.get("sources", []):
            if isinstance(source_entry, dict) and isinstance(source_entry.get("local_path"), str):
                files.add(_relative_file(source_path.parent, repo_root, source_entry["local_path"]))

    definition_ids: set[str] = set()
    for realization_path in realization_profiles:
        realization = _read_object(realization_path)
        for layer in realization.get("layers", []):
            settings = layer.get("settings", {}) if isinstance(layer, dict) else {}
            source_value = settings.get("placement_source") if isinstance(settings, dict) else None
            if not isinstance(source_value, str):
                continue
            placement_path = _relative_file(realization_path.parents[2], repo_root, source_value)
            files.add(placement_path)
            for placement in _read_object(placement_path).get("placements", []):
                if isinstance(placement, dict) and isinstance(placement.get("definition_id"), str):
                    definition_ids.add(placement["definition_id"])
    files.update(_object_definition_inputs(repo_root, definition_ids))
    files.update(_authored_package_inputs(repo_root, authored_overlay_profiles))
    return sorted(files)


def executable_profile_inputs(
    repo_root: Path,
    profile_ids: Collection[str] | None = None,
) -> dict[str, frozenset[str]]:
    result: dict[str, frozenset[str]] = {}
    selected = frozenset(profile_ids) if profile_ids is not None else None
    roots = (repo_root / "Plugins" / "World").glob(
        "*/Data/Profiles/EndToEndValidation/*.validation.json"
    )
    for validation_profile in sorted(roots):
        profile_id = _read_object(validation_profile).get("profile_id")
        if not isinstance(profile_id, str) or not profile_id:
            raise ProfileInputError(f"Executable validation profile ID is missing or duplicated: {validation_profile}")
        if selected is not None and profile_id not in selected:
            continue
        if profile_id in result:
            raise ProfileInputError(f"Executable validation profile ID is missing or duplicated: {validation_profile}")
        result[profile_id] = frozenset(
            path.relative_to(repo_root).as_posix()
            for path in profile_input_files(validation_profile, repo_root)
        )
    return result


def profile_input_contract(validation_profile: Path, repo_root: Path) -> dict[str, object]:
    files = profile_input_files(validation_profile, repo_root)
    records = []
    for path in files:
        records.append({
            "path": path.relative_to(repo_root).as_posix(),
            "sha256": _sha256(path),
        })
    return {"files": records}
