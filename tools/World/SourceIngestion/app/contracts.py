from __future__ import annotations

import hashlib
import json
import os
import time
from contextlib import contextmanager
from functools import lru_cache
from pathlib import Path
from typing import Any

import jsonschema


RESULT_SCHEMA = "https://alis.world/schemas/world-source/operation-result-v1.json"
SCHEMA_ROOT = Path(__file__).resolve().parents[1] / "contracts"


class IngestionError(RuntimeError):
    def __init__(self, code: str, message: str, **details: Any) -> None:
        super().__init__(message)
        self.code = code
        self.details = details


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise IngestionError("invalid_json", f"Cannot read JSON: {path}", error=str(exc)) from exc
    if not isinstance(value, dict):
        raise IngestionError("invalid_json", f"Expected JSON object: {path}")
    return value


@lru_cache(maxsize=None)
def _load_schema(reference: str, document_parent: str) -> dict[str, Any]:
    if reference.startswith(("http://", "https://")):
        candidates = SCHEMA_ROOT.glob("*.schema.json")
        schema_path = next(
            (
                path
                for path in candidates
                if json.loads(path.read_text(encoding="utf-8")).get("$id") == reference
            ),
            None,
        )
    else:
        schema_path = (Path(document_parent) / reference).resolve()
    if schema_path is None or not schema_path.is_file():
        raise IngestionError("schema_missing", "JSON contract schema is unavailable", schema=reference)
    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        jsonschema.validators.validator_for(schema).check_schema(schema)
    except (OSError, json.JSONDecodeError, jsonschema.SchemaError) as exc:
        raise IngestionError(
            "schema_invalid", "JSON contract schema is invalid", schema=str(schema_path), error=str(exc)
        ) from exc
    return schema


def validate_document(value: dict[str, Any], document_path: Path) -> None:
    reference = value.get("$schema")
    if not isinstance(reference, str) or not reference:
        raise IngestionError("contract_violation", "JSON document does not declare a schema", path=str(document_path))
    schema = _load_schema(reference, str(document_path.parent.resolve()))
    validator_class = jsonschema.validators.validator_for(schema)
    validator = validator_class(schema, format_checker=jsonschema.FormatChecker())
    error = next(iter(validator.iter_errors(value)), None)
    if error is not None:
        pointer = "/" + "/".join(str(item) for item in error.absolute_path) if error.absolute_path else "<root>"
        raise IngestionError(
            "contract_violation",
            "JSON document violates its declared schema",
            path=str(document_path),
            pointer=pointer,
            reason=error.message,
        )


def write_json(path: Path, value: dict[str, Any]) -> None:
    validate_document(value, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(path.suffix + ".tmp")
    with staging.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=True)
        stream.write("\n")
    os.replace(staging, path)


def file_hash(path: Path, algorithm: str = "sha256") -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


@contextmanager
def exclusive_file_lock(path: Path, timeout_seconds: float = 120.0):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    if path.stat().st_size == 0:
        handle.write(b"0")
        handle.flush()
    deadline = time.monotonic() + timeout_seconds
    while True:
        try:
            handle.seek(0)
            if os.name == "nt":
                import msvcrt

                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl

                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except OSError as exc:
            if time.monotonic() >= deadline:
                handle.close()
                raise IngestionError(
                    "materialization_busy", "Timed out waiting for materialization lock", path=str(path)
                ) from exc
            time.sleep(0.05)
    try:
        yield
    finally:
        handle.seek(0)
        if os.name == "nt":
            import msvcrt

            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
        handle.close()


def canonical_hash(value: dict[str, Any]) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()
