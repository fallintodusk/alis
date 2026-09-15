#!/usr/bin/env python3
"""Bind a reviewed unsigned release to an identical final public Git tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path

import prepare_release as release


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def replace_release_guide_names(path: Path, replacements: dict[str, str]) -> None:
    text = path.read_text(encoding="ascii")
    for old, new in replacements.items():
        if old not in text:
            raise release.ReleaseError(f"Release guide does not name rebound artifact: {old}")
        text = text.replace(old, new)
    path.write_text(text, encoding="ascii")


def artifact_path(root: Path, manifest: dict, suffix: str) -> Path:
    matches = [root / item["name"] for item in manifest["artifacts"] if item["name"].endswith(suffix)]
    if len(matches) != 1:
        raise release.ReleaseError(f"Expected exactly one release artifact ending in {suffix!r}")
    return matches[0]


def payload_identity(payload: dict, revision: str, branch: str) -> tuple[str, dict]:
    owners = payload.get("owners")
    if not isinstance(owners, list) or not owners:
        authority = payload.get("manifest_authority")
        if not isinstance(authority, dict) or not authority:
            raise release.ReleaseError("Developer payload does not declare its owners")
        owners = sorted(authority)
    source = {
        "revision": revision,
        "branch": branch,
        "tag": payload["public_source"]["tag"],
    }
    identity = {
        "release_version": payload["release_version"],
        "public_source": source,
        "owners": owners,
        "entries": payload["entries"],
    }
    digest = hashlib.sha256(
        json.dumps(identity, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    return digest, source


def rename_payload_files(root: Path, payload_path: Path, payload: dict, payload_id: str) -> tuple[Path, Path]:
    payload_root = payload_path.parent
    stem = f"ALIS_DeveloperProject_{release.normalize_version(payload['release_version'])}_{payload_id[:12]}"
    new_logical = f"{stem}.zip"
    for index, part in enumerate(payload["archive"]["parts"], start=1):
        old_name = release.require_safe_file_name(part.get("name"), "developer archive part name")
        suffix = "" if len(payload["archive"]["parts"]) == 1 else f".{index:03d}"
        new_name = new_logical + suffix
        old_path = payload_root / old_name
        new_path = payload_root / new_name
        if old_path != new_path:
            if new_path.exists():
                raise release.ReleaseError(f"Rebound developer archive already exists: {new_path}")
            os.replace(old_path, new_path)
        part["name"] = new_name
    payload["archive"]["logical_name"] = new_logical

    notice_path = artifact_path(root, release.read_json(root / "release_manifest.json"), ".notices.json")
    new_payload_path = payload_root / f"{stem}.developer-payload.json"
    new_notice_path = payload_root / f"{stem}.notices.json"
    if payload_path != new_payload_path:
        os.replace(payload_path, new_payload_path)
    if notice_path != new_notice_path:
        os.replace(notice_path, new_notice_path)
    return new_payload_path, new_notice_path


def rebind_release(root: Path, final_public_source: Path, branch: str) -> Path:
    root = root.resolve()
    final_public_source = final_public_source.resolve()
    manifest = release.verify_release_manifest(root)
    release.require_equal(manifest.get("status"), "pending_owner_approval", "release rebind state")

    status = release.git_value(final_public_source, "status", "--porcelain=v1", "--untracked-files=all")
    if status:
        raise release.ReleaseError("Final public source checkout is not clean")
    final_revision = release.git_value(final_public_source, "rev-parse", "HEAD")
    final_tree = release.git_value(final_public_source, "rev-parse", "HEAD^{tree}")
    reviewed = manifest["public_source"]
    if final_tree != reviewed["tree"]:
        raise release.ReleaseError(
            "Final public source tree differs from the reviewed unsigned release; "
            "rerun make release X.Y.Z RELEASE_SIGN=0"
        )
    if final_revision == reviewed["revision"] and reviewed.get("branch", branch) == branch:
        return root / "release_manifest.json"

    payload_path = artifact_path(root, manifest, ".developer-payload.json")
    payload = release.read_json(payload_path)
    old_payload_name = payload_path.name
    old_part_names = [part["name"] for part in payload["archive"]["parts"]]
    payload_id, source = payload_identity(payload, final_revision, branch)
    payload["payload_id"] = payload_id
    payload["public_source"] = source
    payload["owners"] = payload.get("owners") or sorted(payload["manifest_authority"])
    payload_path, notice_path = rename_payload_files(root, payload_path, payload, payload_id)
    write_json(payload_path, payload)
    replacements = {old_payload_name: payload_path.name}
    replacements.update(
        {old: part["name"] for old, part in zip(old_part_names, payload["archive"]["parts"], strict=True)}
    )
    replace_release_guide_names(root / "README.txt", replacements)

    notice = release.read_json(notice_path)
    notice["payload_id"] = payload_id
    write_json(notice_path, notice)

    component_path = root / "effective-component-manifest.json"
    component = release.read_json(component_path)
    component["source_commit"] = final_revision
    component["source_tree"] = final_tree
    write_json(component_path, component)

    manifest["public_source"] = {
        "revision": final_revision,
        "tree": final_tree,
        "branch": branch,
        "reviewed_revision": reviewed["revision"],
    }
    manifest["public_source_rebind"] = {
        "status": "content_identical",
        "reviewed_tree": reviewed["tree"],
        "developer_archive_sha256": payload["archive"]["sha256"],
        "player_package_tree_sha256": manifest["player_source"]["package_tree_sha256"],
    }
    manifest["release_documents"]["notices_artifact"] = notice_path.name
    manifest["artifacts"] = [
        {
            "name": path.relative_to(root).as_posix(),
            "byte_size": path.stat().st_size,
            "sha256": release.sha256_file(path),
        }
        for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix())
        if path.is_file() and path.name != "release_manifest.json"
    ]
    manifest_path = root / "release_manifest.json"
    write_json(manifest_path, manifest)
    release.verify_release_manifest(root)
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-dir", type=Path, required=True)
    parser.add_argument("--final-public-source", type=Path, required=True)
    parser.add_argument("--branch", default="main")
    args = parser.parse_args()
    try:
        result = rebind_release(args.release_dir, args.final_public_source, args.branch)
    except (release.ReleaseError, OSError, ValueError, KeyError) as error:
        print(f"[FAIL] {error}", file=__import__("sys").stderr)
        return 1
    print(f"[OK] final public source binding: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
