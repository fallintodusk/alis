#!/usr/bin/env python3
"""Refresh tracked public asset authority from Unreal Asset Registry evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath


class AuthorityError(RuntimeError):
    pass


def digest(path: Path, algorithm: str = "sha256") -> str:
    value = hashlib.new(algorithm)
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def normalized_json_md5(path: Path) -> str:
    text = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.md5(text.encode("utf-8"), usedforsecurity=False).hexdigest()


def resolve(repo_root: Path, raw: str) -> Path:
    path = (repo_root / Path(*PurePosixPath(raw.replace("\\", "/")).parts)).resolve()
    try:
        path.relative_to(repo_root.resolve())
    except ValueError as error:
        raise AuthorityError(f"Path escapes repository: {raw}") from error
    return path


def resolve_under(root: Path, raw: str) -> Path:
    path = (root / Path(*PurePosixPath(raw.replace("\\", "/")).parts)).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError as error:
        raise AuthorityError(f"Path escapes declared root: {raw}") from error
    return path


def collection_for(record: dict, collections: list[dict]) -> dict | None:
    package_name = str(record.get("package_name", ""))
    owned = [item for item in collections if package_name.startswith(item["package_root"].rstrip("/") + "/")]
    matches = [item for item in owned if item["asset_class"] == record.get("asset_class")]
    if not owned:
        return None
    if len(matches) != 1:
        raise AuthorityError(f"Unapproved or ambiguous generated class: {record.get('asset_class')}")
    return matches[0]


def build(repo_root: Path, inventory_path: Path, contract_path: Path, owner: str) -> tuple[Path, dict]:
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    authorities = [item for item in contract.get("asset_authorities", []) if item.get("owner") == owner]
    if len(authorities) != 1:
        raise AuthorityError(f"Expected one declared asset authority for {owner}")
    authority = authorities[0]
    inventory = json.loads(inventory_path.read_text(encoding="utf-8-sig"))
    assets = []
    inventory_sources: dict[str, set[str]] = {}
    for record in inventory.get("assets", []):
        collection = collection_for(record, authority["collections"])
        if collection is None:
            continue
        package_name = str(record.get("package_name", ""))
        package_root = collection["package_root"].rstrip("/")
        if not package_name.startswith(package_root + "/"):
            raise AuthorityError(f"Package is outside declared root: {package_name}")
        relative_package = package_name[len(package_root) + 1 :]
        if ".." in PurePosixPath(relative_package).parts:
            raise AuthorityError(f"Unsafe generated package name: {package_name}")
        source_root = resolve(repo_root, collection["source_root"])
        artifact_root = resolve(repo_root, collection["artifact_root"])
        source_path = resolve_under(source_root, str(record.get("source_json_path", "")))
        artifact_path = resolve_under(artifact_root, relative_package + ".uasset")
        for path in (source_path, artifact_path):
            if not path.is_file():
                raise AuthorityError(f"Generated pair is incomplete: {path}")
        source_md5 = normalized_json_md5(source_path)
        if source_md5 != str(record.get("source_json_hash", "")).lower():
            raise AuthorityError(f"Asset Registry source hash is stale: {package_name}")
        inventory_sources.setdefault(collection["source_root"], set()).add(
            source_path.relative_to(source_root).as_posix()
        )
        assets.append(
            {
                "asset_class": record["asset_class"],
                "package_name": package_name,
                "artifact_path": artifact_path.relative_to(repo_root).as_posix(),
                "artifact_sha256": digest(artifact_path),
                "source_path": source_path.relative_to(repo_root).as_posix(),
                "source_sha256": digest(source_path),
                "source_json_hash_md5": source_md5,
                "schema_path": collection["schema_path"],
            }
        )
    for source_root_raw in sorted({item["source_root"] for item in authority["collections"]}):
        source_root = resolve(repo_root, source_root_raw)
        actual = {path.relative_to(source_root).as_posix() for path in source_root.rglob("*.json")}
        recorded = inventory_sources.get(source_root_raw, set())
        if actual != recorded:
            missing = sorted(actual - recorded)
            stale = sorted(recorded - actual)
            raise AuthorityError(
                f"Generated source inventory is incomplete for {source_root_raw}; "
                f"missing={missing}, stale={stale}"
            )
    assets.sort(key=lambda item: item["package_name"])
    if not assets:
        raise AuthorityError("Asset Registry evidence contains no approved generated assets")
    output = resolve(repo_root, authority["manifest_path"])
    manifest = {
        "$schema": "../Schemas/public_generated_definition_manifest.schema.json",
        "schema_version": 1,
        "manifest_id": "project_object_public_generated_definitions_v1",
        "release_contract_sha256": digest(contract_path),
        "owner": authority["owner"],
        "assets": assets,
    }
    return output, manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--owner", default="ProjectObject")
    args = parser.parse_args()
    root = args.repo_root.resolve()
    contract = args.contract or root / "scripts/git/mirror/developer_asset_release.json"
    try:
        output, manifest = build(root, args.inventory.resolve(), contract.resolve(), args.owner)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (AuthorityError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"[FAIL] {error}")
        return 1
    print(f"[OK] Public generated asset authority: {output} ({len(manifest['assets'])} assets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
