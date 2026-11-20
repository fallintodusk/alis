#!/usr/bin/env python3
"""Refresh and verify tracked public asset authority from Unreal Asset Registry evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath


GENERATED_AUTHORITY_KIND = "generated_definition_manifest"


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


def normalized_json_sha256(path: Path) -> str:
    text = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


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


def generated_authorities(contract: dict, owner: str | None = None) -> list[dict]:
    items = [
        item
        for item in contract.get("asset_authorities", [])
        if item.get("authority_kind") == GENERATED_AUTHORITY_KIND
    ]
    owners = [str(item.get("owner", "")) for item in items]
    if len(owners) != len(set(owners)):
        raise AuthorityError("Generated-definition authorities declare duplicate owners")
    if owner is not None:
        matches = [item for item in items if item.get("owner") == owner]
        if not matches:
            raise AuthorityError(f"Owner {owner} is not a declared generated-definition authority")
        return matches
    if not items:
        raise AuthorityError("No generated-definition authorities are declared")
    return items


def build(repo_root: Path, inventory_path: Path, contract_path: Path, owner: str) -> tuple[Path, dict]:
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    authorities = [item for item in contract.get("asset_authorities", []) if item.get("owner") == owner]
    if len(authorities) != 1:
        raise AuthorityError(f"Expected one declared asset authority for {owner}")
    authority = authorities[0]
    if authority.get("authority_kind") != GENERATED_AUTHORITY_KIND:
        raise AuthorityError(f"Owner {owner} is not a generated-definition authority")
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
                "source_sha256": normalized_json_sha256(source_path),
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
        "$schema": authority["manifest_schema_ref"],
        "schema_version": 1,
        "manifest_id": authority["manifest_id"],
        "release_contract_sha256": digest(contract_path),
        "owner": authority["owner"],
        "assets": assets,
    }
    return output, manifest


def check(repo_root: Path, contract_path: Path, owner: str | None = None) -> list[str]:
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    authorities = generated_authorities(contract, owner)
    contract_sha = digest(contract_path)
    failures: list[str] = []

    def fail(message: str) -> None:
        failures.append(message)

    for authority in authorities:
        owner_name = str(authority.get("owner", ""))
        manifest_raw = str(authority.get("manifest_path", ""))
        try:
            manifest_path = resolve(repo_root, manifest_raw)
        except AuthorityError as error:
            fail(f"{owner_name}: {error}")
            continue
        if not manifest_path.is_file():
            fail(f"{owner_name}: manifest is missing: {manifest_raw}")
            continue
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError) as error:
            fail(f"{owner_name}: manifest is unreadable: {error}")
            continue
        if manifest.get("$schema") != authority.get("manifest_schema_ref"):
            fail(
                f"{owner_name}: manifest $schema mismatch: "
                f"expected {authority.get('manifest_schema_ref')!r}, got {manifest.get('$schema')!r}"
            )
        if manifest.get("manifest_id") != authority.get("manifest_id"):
            fail(
                f"{owner_name}: manifest_id mismatch: "
                f"expected {authority.get('manifest_id')!r}, got {manifest.get('manifest_id')!r}"
            )
        if manifest.get("owner") != owner_name:
            fail(f"{owner_name}: manifest owner mismatch: got {manifest.get('owner')!r}")
        if manifest.get("release_contract_sha256") != contract_sha:
            fail(
                f"{owner_name}: release_contract_sha256 mismatch: "
                f"expected {contract_sha}, got {manifest.get('release_contract_sha256')!r}"
            )

        collections = authority.get("collections", [])
        collection_by_class: dict[str, dict] = {}
        source_root_raws: set[str] = set()
        if not isinstance(collections, list) or not collections:
            fail(f"{owner_name}: no generated-definition collections are declared")
        else:
            for collection in collections:
                if not isinstance(collection, dict):
                    fail(f"{owner_name}: invalid collection entry")
                    continue
                asset_class = collection.get("asset_class")
                if not asset_class:
                    fail(f"{owner_name}: collection has no asset_class")
                    continue
                if asset_class in collection_by_class:
                    fail(f"{owner_name}: duplicate collection for asset class {asset_class}")
                    continue
                collection_by_class[asset_class] = collection
                source_root = collection.get("source_root")
                if not source_root:
                    fail(f"{owner_name}: collection {asset_class} has no source_root")
                else:
                    source_root_raws.add(str(source_root))
                for key in ("artifact_root", "package_root", "schema_path"):
                    if not collection.get(key):
                        fail(f"{owner_name}: collection {asset_class} has no {key}")

        assets = manifest.get("assets", [])
        if not isinstance(assets, list) or not assets:
            fail(f"{owner_name}: manifest has no assets")
            assets = []

        packages: set[str] = set()
        recorded_by_root: dict[str, set[str]] = {}
        for index, asset in enumerate(assets):
            label = f"{owner_name} asset[{index}]"
            if not isinstance(asset, dict):
                fail(f"{label}: invalid asset entry")
                continue
            asset_class = asset.get("asset_class")
            collection = collection_by_class.get(asset_class) if asset_class else None
            if collection is None:
                fail(f"{label}: unapproved asset class {asset_class!r}")
                continue
            package_name = str(asset.get("package_name", ""))
            label = f"{owner_name} {package_name or f'asset[{index}]'}"
            package_root = str(collection.get("package_root", "")).rstrip("/")
            if not package_root or not package_name.startswith(package_root + "/"):
                fail(f"{label}: package is outside declared root: {package_name!r}")
                continue
            relative_package = package_name[len(package_root) + 1 :]
            if not relative_package or ".." in PurePosixPath(relative_package).parts:
                fail(f"{label}: unsafe generated package name: {package_name!r}")
                continue
            if package_name in packages:
                fail(f"{label}: duplicate package name")
                continue
            packages.add(package_name)
            expected_artifact = (
                f"{str(collection.get('artifact_root', '')).rstrip('/')}/{relative_package}.uasset"
            )
            artifact_raw = str(asset.get("artifact_path", ""))
            if artifact_raw != expected_artifact:
                fail(
                    f"{label}: artifact path mismatch: "
                    f"expected {expected_artifact!r}, got {artifact_raw!r}"
                )
            if asset.get("schema_path") != collection.get("schema_path"):
                fail(
                    f"{label}: schema_path mismatch: "
                    f"expected {collection.get('schema_path')!r}, got {asset.get('schema_path')!r}"
                )

            source_raw = str(asset.get("source_path", ""))
            try:
                source_path = resolve(repo_root, source_raw)
                source_root = resolve(repo_root, str(collection.get("source_root", "")))
                source_rel = source_path.relative_to(source_root).as_posix()
            except (AuthorityError, ValueError) as error:
                fail(f"{label}: source path is invalid: {error}")
                source_path = None
                source_rel = None
            try:
                artifact_path = resolve(repo_root, artifact_raw)
                artifact_root = resolve(repo_root, str(collection.get("artifact_root", "")))
                artifact_path.relative_to(artifact_root)
            except (AuthorityError, ValueError) as error:
                fail(f"{label}: artifact path is invalid: {error}")
                artifact_path = None

            if source_path is not None:
                source_root_raw = str(collection.get("source_root", ""))
                if source_rel is not None:
                    recorded_by_root.setdefault(source_root_raw, set()).add(source_rel)
                if not source_path.is_file():
                    fail(f"{label}: source is missing: {source_raw}")
                else:
                    actual_md5 = normalized_json_md5(source_path)
                    if actual_md5 != str(asset.get("source_json_hash_md5", "")).lower():
                        fail(
                            f"{label}: {source_raw}: source_json_hash_md5 mismatch: "
                            f"expected {asset.get('source_json_hash_md5')!r}, got {actual_md5}"
                        )
                    actual_source_sha = normalized_json_sha256(source_path)
                    if actual_source_sha != asset.get("source_sha256"):
                        fail(
                            f"{label}: {source_raw}: source_sha256 mismatch: "
                            f"expected {asset.get('source_sha256')!r}, got {actual_source_sha}"
                        )
            if artifact_path is not None:
                if not artifact_path.is_file():
                    fail(f"{label}: artifact is missing: {artifact_raw}")
                else:
                    actual_artifact_sha = digest(artifact_path)
                    if actual_artifact_sha != asset.get("artifact_sha256"):
                        fail(
                            f"{label}: {artifact_raw}: artifact_sha256 mismatch: "
                            f"expected {asset.get('artifact_sha256')!r}, got {actual_artifact_sha}"
                        )

        for source_root_raw in sorted(source_root_raws):
            try:
                source_root = resolve(repo_root, source_root_raw)
            except AuthorityError as error:
                fail(f"{owner_name}: source root is invalid: {error}")
                continue
            if not source_root.is_dir():
                fail(f"{owner_name}: source root is missing: {source_root_raw}")
                continue
            actual = {path.relative_to(source_root).as_posix() for path in source_root.rglob("*.json")}
            recorded = recorded_by_root.get(source_root_raw, set())
            if actual != recorded:
                missing = sorted(actual - recorded)
                stale = sorted(recorded - actual)
                fail(
                    f"{owner_name}: source inventory is incomplete for {source_root_raw}; "
                    f"missing={missing}, stale={stale}"
                )
    return failures


def refresh(
    repo_root: Path,
    inventory_path: Path,
    contract_path: Path,
    owner: str | None = None,
) -> list[tuple[Path, dict]]:
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    authorities = generated_authorities(contract, owner)
    results: list[tuple[Path, dict]] = []
    for authority in authorities:
        owner_name = str(authority.get("owner", ""))
        output, manifest = build(repo_root, inventory_path, contract_path, owner_name)
        results.append((output, manifest))
    for output, manifest in results:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
    failures = check(repo_root, contract_path, owner)
    if failures:
        raise AuthorityError("Refreshed authority does not verify:\n" + "\n".join(failures))
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--owner")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check and args.inventory:
        parser.error("--check cannot be combined with --inventory")
    if not args.check and args.inventory is None:
        parser.error("--inventory is required unless --check is used")
    root = args.repo_root.resolve()
    contract = (args.contract or root / "scripts/git/mirror/developer_asset_release.json").resolve()
    try:
        if args.check:
            failures = check(root, contract, args.owner)
            if failures:
                for failure in failures:
                    print(f"[FAIL] {failure}")
                print(f"[FAIL] Generated authority check found {len(failures)} problem(s).")
                return 1
            print("[OK] Generated definition authority is current.")
            return 0
        results = refresh(root, args.inventory.resolve(), contract, args.owner)
        for output, manifest in results:
            print(
                f"[OK] {manifest['owner']}: {output} "
                f"({len(manifest['assets'])} assets)"
            )
        print("[OK] Generated definition authority refreshed and verified.")
        return 0
    except (AuthorityError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"[FAIL] {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
