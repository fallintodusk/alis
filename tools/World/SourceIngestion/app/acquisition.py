from __future__ import annotations

import http.client
import os
import shutil
import urllib.request
from pathlib import Path
from typing import Any

from .contracts import IngestionError, exclusive_file_lock, file_hash
from .profiles import resolved_area


LEDGER_SCHEMA = "https://alis.world/schemas/world-source/source-ledger-v1.json"


def verify_source_file(source: dict[str, Any], path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise IngestionError("missing_payload", "Source payload is missing", source_id=source["source_id"])
    size = path.stat().st_size
    if size != source["expected_bytes"]:
        raise IngestionError(
            "wrong_size", "Source payload byte size differs", source_id=source["source_id"], actual=size
        )
    hashes = {"sha256": file_hash(path, "sha256")}
    if "md5" in source["hashes"] or "etag_md5" in source["hashes"]:
        hashes["md5"] = file_hash(path, "md5")
    if hashes["sha256"] != source["hashes"]["sha256"]:
        raise IngestionError("wrong_hash", "Source SHA-256 differs", source_id=source["source_id"])
    expected_md5 = source["hashes"].get("md5") or source["hashes"].get("etag_md5")
    if expected_md5 and hashes["md5"] != expected_md5:
        raise IngestionError("wrong_hash", "Source MD5 differs", source_id=source["source_id"])
    return {"byte_size": size, "hashes": hashes}


def _download(source: dict[str, Any], target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_suffix(target.suffix + ".partial")
    if partial.exists() and partial.stat().st_size >= source["expected_bytes"]:
        partial.unlink()
    offset = partial.stat().st_size if partial.exists() else 0
    request = urllib.request.Request(source["url"], headers={"User-Agent": "ALIS-WorldSource/1"})
    if offset:
        request.add_header("Range", f"bytes={offset}-")
    try:
        response = urllib.request.urlopen(request, timeout=120)
    except OSError as exc:
        raise IngestionError(
            "fetch_failed", "Source download failed", source_id=source["source_id"], error=str(exc)
        ) from exc
    status = getattr(response, "status", 200)
    content_range = response.headers.get("Content-Range", "")
    append = offset > 0 and status == 206 and content_range.startswith(f"bytes {offset}-")
    mode = "ab" if append else "wb"
    try:
        with response, partial.open(mode) as stream:
            shutil.copyfileobj(response, stream, length=1024 * 1024)
    except (OSError, http.client.HTTPException) as exc:
        raise IngestionError(
            "fetch_interrupted", "Source download was interrupted", source_id=source["source_id"], error=str(exc)
        ) from exc
    verification = verify_source_file(source, partial)
    if verification["byte_size"] != source["expected_bytes"]:
        raise IngestionError("fetch_incomplete", "Downloaded payload is incomplete", source_id=source["source_id"])
    os.replace(partial, target)


def source_cache_path(cache_root: Path, source: dict[str, Any]) -> Path:
    return cache_root / "sha256" / source["hashes"]["sha256"] / source["file_name"]


def materialize_sources(
    profile: dict[str, Any], profile_path: Path, repo_root: Path, cache_root: Path
) -> dict[str, Path]:
    paths: dict[str, Path] = {}
    for source in profile["sources"]:
        if source["adapter"] == "synthetic_json":
            path = (profile_path.parent / source["local_path"]).resolve()
        else:
            path = source_cache_path(cache_root, source)
            lock_path = cache_root / "locks" / f"{source['hashes']['sha256']}.lock"
            with exclusive_file_lock(lock_path):
                try:
                    verify_source_file(source, path)
                except IngestionError:
                    _download(source, path)
        verify_source_file(source, path)
        paths[source["source_id"]] = path
    return paths


def locate_sources(profile: dict[str, Any], profile_path: Path, cache_root: Path) -> dict[str, Path]:
    paths: dict[str, Path] = {}
    for source in profile["sources"]:
        if source["adapter"] == "synthetic_json":
            path = (profile_path.parent / source["local_path"]).resolve()
        else:
            path = source_cache_path(cache_root, source)
        paths[source["source_id"]] = path
    return paths


def build_ledger(profile: dict[str, Any], paths: dict[str, Path]) -> dict[str, Any]:
    snapshots = []
    for source in profile["sources"]:
        path = paths[source["source_id"]]
        verification = verify_source_file(source, path)
        sha256 = verification["hashes"]["sha256"]
        snapshots.append({
            "snapshot_id": f"{source['source_id']}:{sha256[:16]}",
            "source_id": source["source_id"],
            "provider": source["provider"],
            "dataset": source["dataset"],
            "release": source["release"],
            "byte_size": verification["byte_size"],
            "hashes": verification["hashes"],
            "accuracy": source["accuracy"],
            "crs": source.get("crs"),
            "license": source["license"],
            "admission": source["admission"],
            "policy_result": "approved",
        })
    return {
        "$schema": LEDGER_SCHEMA,
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        "area": resolved_area(profile["area"]),
        "snapshots": snapshots,
        "policy_result": "approved",
    }
