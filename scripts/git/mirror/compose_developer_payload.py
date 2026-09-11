#!/usr/bin/env python3
"""Compose the public developer payload from accepted production authority."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any


PART_LIMIT_MIB = 1700
MAX_PART_MIB = 1900
MAX_RELEASE_PARTS = 990
ASSET_RELEASE_CONTRACT = "scripts/git/mirror/developer_asset_release.json"
GENERATED_ROOTS = (
    "Content/Generated/",
    "Content/__ExternalActors__/Generated/",
    "Content/__ExternalObjects__/Generated/",
)


class PayloadError(RuntimeError):
    pass


@dataclass(frozen=True)
class Entry:
    path: str
    sha256: str
    byte_size: int
    kind: str
    owner: str


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalized_json_md5(path: Path) -> str:
    text = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.md5(text.encode("utf-8"), usedforsecurity=False).hexdigest()


def normalized_json_sha256(path: Path) -> str:
    text = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise PayloadError(f"Invalid JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise PayloadError(f"Expected a JSON object: {path}")
    return value


def safe_relative(raw: str) -> str:
    normalized = raw.replace("\\", "/")
    path = PurePosixPath(normalized)
    if path.is_absolute() or not normalized or ".." in path.parts:
        raise PayloadError(f"Unsafe project-relative path: {raw}")
    return path.as_posix()


def resolve_repo_file(repo_root: Path, raw: str) -> tuple[str, Path]:
    relative = safe_relative(raw)
    resolved = (repo_root / Path(*PurePosixPath(relative).parts)).resolve()
    try:
        resolved.relative_to(repo_root.resolve())
    except ValueError as error:
        raise PayloadError(f"Path escapes repository: {raw}") from error
    if not resolved.is_file():
        raise PayloadError(f"Required payload file is missing: {relative}")
    return relative, resolved


def plugin_root(repo_root: Path, owner: str) -> Path:
    matches = list(repo_root.glob(f"Plugins/*/{owner}/{owner}.uplugin"))
    if len(matches) != 1:
        raise PayloadError(f"Expected one descriptor for owner {owner}, found {len(matches)}")
    return matches[0].parent


def add_entry(entries: dict[str, Entry], repo_root: Path, raw: str, kind: str, owner: str) -> Entry:
    relative, source = resolve_repo_file(repo_root, raw)
    entry = Entry(relative, sha256_file(source), source.stat().st_size, kind, owner)
    prior = entries.get(relative)
    if prior and prior != entry:
        raise PayloadError(f"Conflicting payload ownership for {relative}")
    entries[relative] = entry
    return entry


def collect_manifest_authority(
    repo_root: Path,
    owner: str,
    entries: dict[str, Entry],
    manifest_root: Path | None = None,
) -> list[dict[str, Any]]:
    root = plugin_root(repo_root, owner)
    owned_manifest_root = root / "Data" / "Manifests"
    manifests = manifest_root.resolve() if manifest_root else owned_manifest_root
    if not manifests.is_dir():
        raise PayloadError(f"Public World manifest root is missing: {manifests}")
    active_path = manifests / "active_set.json"
    active = read_json(active_path)

    selected: list[dict[str, Any]] = []
    for scope in active.get("scopes", []):
        relative_manifest = safe_relative(str(scope.get("manifest_path", "")))
        manifest_path = manifests / Path(*PurePosixPath(relative_manifest).parts)
        expected_manifest_hash = str(scope.get("manifest_sha256", "")).lower()
        if sha256_file(manifest_path) != expected_manifest_hash:
            raise PayloadError(f"Active manifest hash mismatch: {manifest_path}")
        manifest = read_json(manifest_path)
        manifest_relative = (owned_manifest_root.relative_to(repo_root) / Path(*PurePosixPath(relative_manifest).parts)).as_posix()
        selected.append({"scope_id": scope.get("scope_id"), "manifest": manifest_relative})

        plugin_prefix = root.relative_to(repo_root).as_posix() + "/"
        for artifact in manifest.get("artifacts", []):
            artifact_path = safe_relative(str(artifact.get("path", "")))
            if not artifact_path.startswith(plugin_prefix):
                raise PayloadError(f"Artifact is outside owner {owner}: {artifact_path}")
            owner_relative = artifact_path[len(plugin_prefix) :]
            if not any(owner_relative.startswith(prefix) for prefix in GENERATED_ROOTS):
                raise PayloadError(f"Artifact is outside generated roots: {artifact_path}")
            if PurePosixPath(artifact_path).suffix.lower() not in {".uasset", ".umap"}:
                raise PayloadError(f"Unsupported generated artifact type: {artifact_path}")
            if "hlod" in artifact_path.lower():
                raise PayloadError(f"Production payload cannot contain HLOD artifacts: {artifact_path}")
            entry = add_entry(entries, repo_root, artifact_path, str(artifact.get("kind", "generated_asset")), owner)
            if str(artifact.get("digest_kind", "")) != "sha256" or entry.sha256 != str(artifact.get("digest", "")).lower():
                raise PayloadError(f"Generated artifact hash mismatch: {artifact_path}")
    return selected


def collect_canonical_authority(
    repo_root: Path,
    owner: str,
    entries: dict[str, Entry],
    profile_ids: list[str],
) -> list[dict[str, Any]]:
    root = plugin_root(repo_root, owner)
    canonical_root = root / "Data" / "Canonical"
    selected: list[dict[str, Any]] = []
    if len(profile_ids) != len(set(profile_ids)) or not profile_ids:
        raise PayloadError(f"Public canonical profile selection is empty or duplicated: {owner}")
    for profile_id in sorted(profile_ids):
        if not re.fullmatch(r"[a-z0-9_]+", profile_id):
            raise PayloadError(f"Invalid public canonical profile id: {profile_id}")
        active_path = canonical_root / profile_id / "active.json"
        active = read_json(active_path)
        if active.get("profile_id") != profile_id:
            raise PayloadError(f"Canonical profile identity mismatch: {active_path}")
        bundle = active.get("bundle")
        if not isinstance(bundle, dict):
            raise PayloadError(f"Canonical authority has no bundle: {active_path}")
        bundle_relative = safe_relative(str(bundle.get("path", "")))
        bundle_path = active_path.parent / Path(*PurePosixPath(bundle_relative).parts)
        bundle_entry = add_entry(entries, repo_root, bundle_path.relative_to(repo_root).as_posix(), "canonical_bundle", owner)
        if bundle_entry.sha256 != str(bundle.get("sha256", "")).lower() or bundle_entry.byte_size != int(bundle.get("byte_size", -1)):
            raise PayloadError(f"Canonical bundle identity mismatch: {bundle_path}")
        selected.append(
            {
                "profile_id": active.get("profile_id"),
                "authority_id": active.get("authority_id"),
                "active_path": active_path.relative_to(repo_root).as_posix(),
                "bundle_path": bundle_entry.path,
            }
        )
    return selected


def collect_generated_definition_authority(
    repo_root: Path,
    entries: dict[str, Entry],
    authority: dict[str, Any],
    manifest: dict[str, Any],
    manifest_relative: str,
) -> int:
        owner = str(authority["owner"])
        collection_list = authority.get("collections", [])
        collections = {item.get("asset_class"): item for item in collection_list}
        if not collections or None in collections or len(collections) != len(collection_list):
            raise PayloadError(f"Public asset authority has invalid collections: {owner}")
        recorded_sources: dict[str, set[str]] = {}
        packages: set[str] = set()
        asset_count = 0
        manifest_assets = manifest.get("assets", [])
        if not manifest_assets:
            raise PayloadError(f"Public asset authority is empty: {manifest_relative}")
        for asset in manifest_assets:
            asset_class = asset.get("asset_class")
            collection = collections.get(asset_class)
            if not collection:
                raise PayloadError(f"Unapproved public asset class {asset_class}: {manifest_relative}")
            source_relative, source_path = resolve_repo_file(repo_root, str(asset.get("source_path", "")))
            artifact_relative, artifact_path = resolve_repo_file(repo_root, str(asset.get("artifact_path", "")))
            source_root = safe_relative(collection["source_root"]).rstrip("/")
            artifact_root = safe_relative(collection["artifact_root"]).rstrip("/")
            if not source_relative.startswith(source_root + "/") or not source_relative.endswith(".json"):
                raise PayloadError(f"Generated source is outside its declared collection: {source_relative}")
            if not artifact_relative.startswith(artifact_root + "/") or not artifact_relative.endswith(".uasset"):
                raise PayloadError(f"Generated asset is outside its declared collection: {artifact_relative}")
            package_name = str(asset.get("package_name", ""))
            if package_name in packages:
                raise PayloadError(f"Duplicate generated package authority: {package_name}")
            packages.add(package_name)
            package_root = str(collection["package_root"]).rstrip("/")
            if not package_name.startswith(package_root + "/"):
                raise PayloadError(f"Generated package is outside its declared root: {package_name}")
            if ".." in PurePosixPath(package_name.lstrip("/")).parts:
                raise PayloadError(f"Unsafe generated package name: {package_name}")
            expected_artifact = f"{artifact_root}/{package_name[len(package_root) + 1:]}.uasset"
            if artifact_relative != expected_artifact:
                raise PayloadError(f"Generated package/path mismatch: {package_name}")
            if asset.get("schema_path") != collection.get("schema_path"):
                raise PayloadError(f"Generated source schema classification drift: {source_relative}")
            if normalized_json_sha256(source_path) != asset.get("source_sha256"):
                raise PayloadError(f"Generated source hash mismatch: {source_relative}")
            if normalized_json_md5(source_path) != asset.get("source_json_hash_md5"):
                raise PayloadError(f"Generated source metadata mismatch: {source_relative}")
            if sha256_file(artifact_path) != asset.get("artifact_sha256"):
                raise PayloadError(f"Generated asset hash mismatch: {artifact_relative}")
            lowered = artifact_relative.lower()
            if "thirdparty" in lowered or "projectworldtestdata" in lowered or "hlod" in lowered:
                raise PayloadError(f"Forbidden public asset payload: {artifact_relative}")
            add_entry(entries, repo_root, artifact_relative, "generated_definition_asset", owner)
            recorded_sources.setdefault(source_root, set()).add(source_relative)
            asset_count += 1

        declared_source_roots = {safe_relative(item["source_root"]).rstrip("/") for item in collection_list}
        for source_root in declared_source_roots:
            recorded = recorded_sources.get(source_root, set())
            root = repo_root / Path(*PurePosixPath(source_root).parts)
            actual = {path.relative_to(repo_root).as_posix() for path in root.rglob("*.json")}
            if actual != recorded:
                raise PayloadError(f"Public source authority is incomplete under {source_root}")
        return asset_count


def collect_material_authority(
    repo_root: Path,
    entries: dict[str, Entry],
    authority: dict[str, Any],
    manifest: dict[str, Any],
) -> int:
    owner = str(authority["owner"])
    recipe_root = safe_relative(str(authority.get("recipe_root", ""))).rstrip("/")
    artifact_root = safe_relative(str(authority.get("artifact_root", ""))).rstrip("/")
    package_root = str(authority.get("package_root", "")).rstrip("/")
    records = manifest.get("records", [])
    if not records:
        raise PayloadError(f"Public material authority is empty: {owner}")

    verifier_path = repo_root / "Plugins/Resources/ProjectMaterial/Tools/verify_material_authority.py"
    spec = importlib.util.spec_from_file_location("project_material_authority_verifier", verifier_path)
    if spec is None or spec.loader is None:
        raise PayloadError("ProjectMaterial authority verifier could not be loaded")
    verifier = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(verifier)
        verified_recipes = verifier.verify_material_authority(
            repo_root / Path(*PurePosixPath(recipe_root).parts),
            repo_root / Path(*PurePosixPath(str(authority["manifest_path"])).parts),
        )
    except Exception as error:
        raise PayloadError(f"ProjectMaterial semantic authority rejected: {error}") from error

    recorded_recipes: set[str] = set()
    recorded_packages: dict[str, str] = {}
    for record in records:
        recipe_relative = f"{recipe_root}/{safe_relative(str(record.get('recipe_path', '')))}"
        resolve_repo_file(repo_root, recipe_relative)
        recipe_path = safe_relative(str(record.get("recipe_path", "")))
        if verified_recipes.get(recipe_path) != str(record.get("recipe_sha256", "")):
            raise PayloadError(f"Material semantic recipe identity is invalid: {recipe_relative}")

        object_path = str(record.get("output_object_path", ""))
        package_name, separator, object_name = object_path.partition(".")
        if not separator or not object_name or not package_name.startswith(package_root + "/"):
            raise PayloadError(f"Material output is outside its declared package root: {object_path}")
        artifact_relative = f"{artifact_root}/{package_name[len(package_root) + 1:]}.uasset"
        entry = add_entry(entries, repo_root, artifact_relative, "generated_material_asset", owner)
        if entry.sha256 != str(record.get("package_sha256", "")).lower():
            raise PayloadError(f"Generated material hash mismatch: {artifact_relative}")
        if package_name in recorded_packages:
            raise PayloadError(f"Duplicate generated material package: {package_name}")
        recorded_packages[package_name] = entry.sha256
        recorded_recipes.add(recipe_relative)

    for record in records:
        dependency = record.get("dependency_object_path")
        if dependency:
            dependency_package = str(dependency).partition(".")[0]
            if recorded_packages.get(dependency_package) != str(record.get("dependency_package_sha256", "")).lower():
                raise PayloadError(f"Generated material dependency is outside accepted authority: {dependency}")

    actual_recipes = {
        path.relative_to(repo_root).as_posix()
        for path in (repo_root / Path(*PurePosixPath(recipe_root).parts)).rglob("*.material.json")
    }
    if actual_recipes != recorded_recipes:
        raise PayloadError(f"Public material recipe authority is incomplete under {recipe_root}")
    return len(records)


def collect_public_asset_authority(repo_root: Path, entries: dict[str, Entry]) -> list[dict[str, Any]]:
    contract_relative, contract_path = resolve_repo_file(repo_root, ASSET_RELEASE_CONTRACT)
    contract = read_json(contract_path)
    contract_hash = sha256_file(contract_path)
    selected: list[dict[str, Any]] = []
    for authority in contract.get("asset_authorities", []):
        owner = str(authority.get("owner", ""))
        if not owner or "test" in owner.lower():
            raise PayloadError(f"Invalid public asset authority owner: {owner}")
        if authority.get("dependency_payload_policy") != "references_only":
            raise PayloadError(f"Public authority must not copy dependency payloads: {owner}")
        if authority.get("license_id") not in {"MPL-2.0", "CC-BY-4.0", "CC0-1.0"}:
            raise PayloadError(f"Unsupported public asset license: {owner}")
        manifest_relative, manifest_path = resolve_repo_file(repo_root, str(authority.get("manifest_path", "")))
        manifest = read_json(manifest_path)

        kind = authority.get("authority_kind")
        if kind == "generated_definition_manifest":
            if manifest.get("owner") != owner or manifest.get("release_contract_sha256") != contract_hash:
                raise PayloadError(f"Public asset authority is stale or has the wrong owner: {manifest_relative}")
            asset_count = collect_generated_definition_authority(
                repo_root, entries, authority, manifest, manifest_relative
            )
        elif kind == "material_manifest":
            asset_count = collect_material_authority(repo_root, entries, authority, manifest)
        else:
            raise PayloadError(f"Unsupported public asset authority kind for {owner}: {kind}")

        selected.append(
            {
                "owner": owner,
                "authority_kind": kind,
                "manifest_path": manifest_relative,
                "license_id": authority.get("license_id"),
                "distribution_class": authority.get("distribution_class"),
                "dependency_payload_policy": authority.get("dependency_payload_policy"),
                "asset_count": asset_count,
            }
        )
    return selected


def ensure_tracked(repo_root: Path, entries: list[Entry]) -> None:
    command = ["git", "-c", "core.fsmonitor=false", "-C", str(repo_root), "ls-files", "--error-unmatch"]
    for entry in entries:
        result = subprocess.run(command + [entry.path], capture_output=True, text=True, check=False)
        if result.returncode != 0:
            raise PayloadError(f"Payload authority is not tracked by git: {entry.path}")


def require_clean(repo_root: Path) -> None:
    result = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", "-C", str(repo_root), "status", "--porcelain=v1", "--untracked-files=all"],
        capture_output=True,
        text=True,
        check=True,
    )
    if result.stdout.strip():
        raise PayloadError("Developer payload requires a clean tracked source tree")


def require_git_ref(repo_root: Path, ref_name: str, label: str) -> None:
    result = subprocess.run(
        ["git", "-C", str(repo_root), "check-ref-format", ref_name],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise PayloadError(f"Public source {label} is not a valid Git ref: {ref_name}")


def write_bundle(repo_root: Path, output: Path, entries: list[Entry]) -> None:
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9, allowZip64=True) as bundle:
        for entry in entries:
            info = zipfile.ZipInfo(entry.path, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            with (repo_root / Path(*PurePosixPath(entry.path).parts)).open("rb") as source:
                with bundle.open(info, "w", force_zip64=True) as target:
                    shutil.copyfileobj(source, target, 1024 * 1024)


def split_archive(archive: Path, max_bytes: int) -> list[Path]:
    if archive.stat().st_size <= max_bytes:
        return [archive]
    parts: list[Path] = []
    with archive.open("rb") as source:
        index = 1
        remaining_total = archive.stat().st_size
        while remaining_total:
            part = archive.with_name(f"{archive.name}.{index:03d}")
            remaining_part = min(max_bytes, remaining_total)
            with part.open("wb") as target:
                while remaining_part:
                    chunk = source.read(min(8 * 1024 * 1024, remaining_part))
                    if not chunk:
                        raise PayloadError(f"Unexpected end of archive while writing {part.name}")
                    target.write(chunk)
                    remaining_part -= len(chunk)
                    remaining_total -= len(chunk)
            parts.append(part)
            index += 1
    archive.unlink()
    return parts


def sanitize_version(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9._-]+", "-", value).strip("-.")
    if not sanitized:
        raise PayloadError("Release version must contain a portable character")
    return sanitized


def write_notices(
    repo_root: Path,
    output: Path,
    payload_id: str,
    authorities: dict[str, Any],
    public_assets: list[dict[str, Any]],
) -> None:
    notices: list[dict[str, Any]] = []
    for owner, selected in authorities.items():
        for authority in selected:
            bundle_path = repo_root / Path(*PurePosixPath(authority["bundle_path"]).parts)
            with zipfile.ZipFile(bundle_path) as bundle:
                try:
                    attribution = json.loads(bundle.read("reports/attribution.json").decode("utf-8-sig"))
                except (KeyError, UnicodeDecodeError, json.JSONDecodeError) as error:
                    raise PayloadError(f"Canonical bundle has no valid attribution report: {bundle_path}") from error
            notices.append(
                {
                    "owner": owner,
                    "profile_id": authority["profile_id"],
                    "authority_id": authority["authority_id"],
                    "attribution": attribution,
                }
            )
    value = {
        "schema_version": 1,
        "payload_id": payload_id,
        "canonical_authorities": notices,
        "public_asset_authorities": public_assets,
    }
    output.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def compose(
    repo_root: Path,
    output_dir: Path,
    release_version: str,
    public_source_tag: str,
    owners: list[str],
    part_size_mib: int,
    allow_dirty: bool,
    public_source_revision: str,
    public_source_branch: str,
    world_manifest_root: Path | None = None,
) -> Path:
    repo_root = repo_root.resolve()
    if not re.fullmatch(r"[0-9a-f]{40}", public_source_revision):
        raise PayloadError("Public source revision must be one full lowercase Git commit SHA")
    require_git_ref(repo_root, f"refs/heads/{public_source_branch}", "branch")
    require_git_ref(repo_root, f"refs/tags/{public_source_tag}", "tag")
    if not allow_dirty:
        require_clean(repo_root)
    if output_dir.exists() and any(output_dir.iterdir()):
        raise PayloadError(f"Output directory must be empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    entries: dict[str, Entry] = {}
    manifest_authority: dict[str, Any] = {}
    canonical_authority: dict[str, Any] = {}
    release_contract = read_json(repo_root / ASSET_RELEASE_CONTRACT)
    world_authorities = {
        str(item.get("owner", "")): item
        for item in release_contract.get("world_authorities", [])
        if isinstance(item, dict)
    }
    for owner in owners:
        authority = world_authorities.get(owner)
        if not authority:
            raise PayloadError(f"No public World authority selection exists for owner: {owner}")
        manifest_authority[owner] = collect_manifest_authority(
            repo_root, owner, entries, world_manifest_root if owner == "ProjectWorldData" else None
        )
        canonical_authority[owner] = collect_canonical_authority(
            repo_root, owner, entries, list(authority.get("canonical_profiles", []))
        )
    public_asset_authority = collect_public_asset_authority(repo_root, entries)
    ordered = sorted(entries.values(), key=lambda item: item.path)
    non_binary_entries = [
        entry.path
        for entry in ordered
        if PurePosixPath(entry.path).suffix.lower() not in {".uasset", ".umap", ".zip"}
    ]
    if non_binary_entries:
        raise PayloadError(f"Developer payload contains public-source text: {non_binary_entries}")
    if not allow_dirty:
        ensure_tracked(repo_root, ordered)

    identity = {
        "release_version": release_version,
        "public_source": {
            "revision": public_source_revision,
            "branch": public_source_branch,
            "tag": public_source_tag,
        },
        "owners": owners,
        "entries": [entry.__dict__ for entry in ordered],
    }
    payload_id = hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    stem = f"ALIS_DeveloperProject_{sanitize_version(release_version)}_{payload_id[:12]}"
    archive = output_dir / f"{stem}.zip"
    write_bundle(repo_root, archive, ordered)
    archive_size = archive.stat().st_size
    archive_hash = sha256_file(archive)
    required_parts = (archive_size + part_size_mib * 1024 * 1024 - 1) // (part_size_mib * 1024 * 1024)
    if required_parts > MAX_RELEASE_PARTS:
        raise PayloadError(f"Developer payload needs {required_parts} parts; maximum is {MAX_RELEASE_PARTS}")
    parts = split_archive(archive, part_size_mib * 1024 * 1024)

    project_markers = []
    marker_owners = sorted(set(owners) | {item["owner"] for item in public_asset_authority})
    for marker in ("Alis.uproject", *[plugin_root(repo_root, owner).joinpath(f"{owner}.uplugin").relative_to(repo_root).as_posix() for owner in marker_owners]):
        relative, path = resolve_repo_file(repo_root, marker)
        project_markers.append({"path": relative})
    manifest = {
        "schema_version": 2,
        "payload_id": payload_id,
        "release_version": release_version,
        "public_source": identity["public_source"],
        "project_markers": project_markers,
        "manifest_authority": manifest_authority,
        "canonical_authority": canonical_authority,
        "public_asset_authority": public_asset_authority,
        "archive": {
            "logical_name": f"{stem}.zip",
            "sha256": archive_hash,
            "byte_size": archive_size,
            "parts": [
                {"name": part.name, "sha256": sha256_file(part), "byte_size": part.stat().st_size}
                for part in parts
            ],
        },
        "entries": [entry.__dict__ for entry in ordered],
    }
    manifest_path = output_dir / f"{stem}.developer-payload.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_notices(
        repo_root,
        output_dir / f"{stem}.notices.json",
        payload_id,
        canonical_authority,
        public_asset_authority,
    )
    scripts = Path(__file__).resolve().parent
    shutil.copy2(scripts / "install_developer_payload.ps1", output_dir / "INSTALL_ALIS_DEVELOPER_PROJECT.ps1")
    shutil.copy2(scripts / "install_developer_payload.bat", output_dir / "INSTALL_ALIS_DEVELOPER_PROJECT.bat")
    shutil.copy2(repo_root / "scripts" / "ue" / "package" / "verify_release.ps1", output_dir / "VERIFY_RELEASE.ps1")
    shutil.copy2(repo_root / "LICENSE", output_dir / "LICENSE.txt")
    shutil.copy2(repo_root / "LICENSES" / "MPL-2.0.txt", output_dir / "LICENSE_MPL-2.0.txt")
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--release-version", required=True)
    parser.add_argument("--public-source-tag", required=True)
    parser.add_argument("--public-source-revision", required=True)
    parser.add_argument("--public-source-branch", required=True)
    parser.add_argument("--world-manifest-root", type=Path)
    parser.add_argument("--owner", action="append", default=[])
    parser.add_argument("--part-size-mib", type=int, default=PART_LIMIT_MIB)
    parser.add_argument("--allow-dirty", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 1 <= args.part_size_mib <= MAX_PART_MIB:
        parser.error(f"--part-size-mib must be between 1 and {MAX_PART_MIB}")
    try:
        result = compose(
            args.repo_root,
            args.output_dir,
            args.release_version,
            args.public_source_tag,
            args.owner or ["ProjectWorldData"],
            args.part_size_mib,
            args.allow_dirty,
            args.public_source_revision,
            args.public_source_branch,
            args.world_manifest_root,
        )
    except (PayloadError, OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] Developer payload manifest: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
