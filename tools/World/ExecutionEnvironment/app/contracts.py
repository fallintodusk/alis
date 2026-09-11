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


CONTRACT_ROOT = Path(__file__).resolve().parents[1] / "contracts"


class ExecutionEnvironmentError(RuntimeError):
    def __init__(self, code: str, message: str, **details: Any) -> None:
        super().__init__(message)
        self.code = code
        self.details = details


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ExecutionEnvironmentError(
            "invalid_json", "Cannot read execution-environment JSON", path=str(path)
        ) from exc
    if not isinstance(value, dict):
        raise ExecutionEnvironmentError("invalid_json", "Expected a JSON object", path=str(path))
    return value


@lru_cache(maxsize=None)
def _schema(reference: str) -> dict[str, Any]:
    for path in CONTRACT_ROOT.glob("*.schema.json"):
        value = json.loads(path.read_text(encoding="utf-8"))
        if value.get("$id") == reference:
            return value
    raise ExecutionEnvironmentError("schema_missing", "Execution-environment schema is unavailable", schema=reference)


def validate_document(value: dict[str, Any], path: Path) -> None:
    reference = value.get("$schema")
    if not isinstance(reference, str) or not reference:
        raise ExecutionEnvironmentError("contract_violation", "Document does not declare a schema", path=str(path))
    if reference.startswith(("http://", "https://")):
        schema = _schema(reference)
    else:
        schema_path = (path.parent / reference).resolve()
        if not schema_path.is_file():
            raise ExecutionEnvironmentError("schema_missing", "Execution-environment schema is unavailable")
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    validator_type = jsonschema.validators.validator_for(schema)
    try:
        validator_type.check_schema(schema)
    except jsonschema.SchemaError as exc:
        raise ExecutionEnvironmentError("schema_invalid", "Execution-environment schema is invalid") from exc
    error = next(iter(validator_type(schema).iter_errors(value)), None)
    if error is not None:
        raise ExecutionEnvironmentError(
            "contract_violation", "Execution-environment document violates its schema", reason=error.message
        )


def write_json(path: Path, value: dict[str, Any]) -> None:
    validate_document(value, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(path.suffix + ".tmp")
    staging.write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    os.replace(staging, path)


@contextmanager
def exclusive_file_lock(path: Path, timeout_seconds: float = 120.0):
    import msvcrt

    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    if path.stat().st_size == 0:
        handle.write(b"0")
        handle.flush()
    deadline = time.monotonic() + timeout_seconds
    while True:
        try:
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            break
        except OSError:
            if time.monotonic() >= deadline:
                handle.close()
                raise ExecutionEnvironmentError("lock_timeout", "Timed out waiting for execution-environment lock")
            time.sleep(0.1)
    try:
        yield
    finally:
        handle.seek(0)
        msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        handle.close()
