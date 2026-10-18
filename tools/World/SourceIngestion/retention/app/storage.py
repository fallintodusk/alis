"""Content-addressed storage reuses source verification and exclusive locks."""
import json
import os
from pathlib import Path
import shutil
import sys

import jsonschema

from ...app.contracts import IngestionError, exclusive_file_lock, read_json


def _roots(repo_root: Path, *, required: bool) -> tuple[Path, ...]:
    config = repo_root / "scripts/config/world_source_storage.local.json"
    if not config.is_file():
        if required:
            raise IngestionError("source_storage_missing", "Configure durable source storage before accepting provider ingestion")
        return ()
    value = read_json(config)
    schema = json.loads((Path(__file__).resolve().parents[1] / "contracts/source-storage.schema.json").read_text())
    try:
        jsonschema.Draft202012Validator(schema).validate(value)
    except jsonschema.ValidationError as error:
        raise IngestionError("source_storage_invalid", "Invalid source storage configuration", reason=error.message) from error
    roots = tuple(Path(value[key]) for key in ("primary_root", "replica_root"))
    if any(not root.is_absolute() for root in roots):
        raise IngestionError("source_storage_invalid", "Storage roots must be absolute machine-local paths")
    roots = tuple(root.resolve() for root in roots)
    repo = repo_root.resolve()
    if any(root.is_relative_to(repo) or repo.is_relative_to(root) for root in roots) or \
            roots[0].is_relative_to(roots[1]) or roots[1].is_relative_to(roots[0]):
        raise IngestionError("source_storage_invalid", "Storage copies must be disjoint and outside the disposable checkout")
    return roots


def _verified_copy(source: dict, origin: Path, target: Path) -> None:
    from ...app.acquisition import verify_source_file
    verify_source_file(source, origin)
    target.parent.mkdir(parents=True, exist_ok=True)
    staging = target.with_name(target.name + ".retaining")
    try:
        with origin.open("rb") as incoming, staging.open("wb") as output:
            shutil.copyfileobj(incoming, output, 1024 * 1024)
            output.flush()
            os.fsync(output.fileno())
        verify_source_file(source, staging)
        os.replace(staging, target)
        verify_source_file(source, target)
    finally:
        staging.unlink(missing_ok=True)


def _valid(source: dict, path: Path) -> bool:
    from ...app.acquisition import verify_source_file
    try:
        verify_source_file(source, path)
        return True
    except IngestionError as error:
        if error.code not in {"missing_payload", "wrong_size", "wrong_hash"}:
            raise
        return False


def recover_source(repo_root: Path, source: dict, cache_path: Path) -> bool:
    """Caller owns the cache lock. Unknown storage failures remain failures."""
    from ...app.acquisition import source_cache_path
    for index, root in enumerate(_roots(repo_root, required=False)):
        with exclusive_file_lock(root / "locks" / f"{source['hashes']['sha256']}.lock"):
            origin = source_cache_path(root, source)
            if _valid(source, origin):
                _verified_copy(source, origin, cache_path)
                print(json.dumps({"event": "source_recovered", "source_id": source["source_id"],
                                  "sha256": source["hashes"]["sha256"], "copy": index}), file=sys.stderr)
                return True
    return False


def retain_sources(repo_root: Path, profile: dict, paths: dict[str, Path]) -> None:
    """Both verified copies are required before a provider operation is accepted."""
    from ...app.acquisition import source_cache_path, verify_source_file
    sources = [item for item in profile["sources"] if item["adapter"] != "synthetic_json"]
    if not sources:
        return
    roots = _roots(repo_root, required=True)
    for source in sources:
        verify_source_file(source, paths[source["source_id"]])
    for source in sources:
        for root in roots:
            with exclusive_file_lock(root / "locks" / f"{source['hashes']['sha256']}.lock"):
                target = source_cache_path(root, source)
                if not _valid(source, target):
                    _verified_copy(source, paths[source["source_id"]], target)
    print(json.dumps({"event": "source_retention_verified", "snapshots": len(sources), "copies": len(roots)}), file=sys.stderr)


def validate_storage(repo_root: Path, profile: dict) -> None:
    if any(source["adapter"] != "synthetic_json" for source in profile["sources"]):
        _roots(repo_root, required=True)


def validate_cache_root(repo_root: Path, cache_root: Path) -> None:
    cache = cache_root.resolve()
    if any(cache.is_relative_to(root) or root.is_relative_to(cache) for root in _roots(repo_root, required=False)):
        raise IngestionError("source_cache_storage_overlap", "Acquisition cache and durable archives must be disjoint")
