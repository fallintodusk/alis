from __future__ import annotations

import hashlib
import json
import os
from functools import lru_cache
from pathlib import Path
from typing import Any

import jsonschema


SCHEMA_ROOT = Path(__file__).resolve().parents[1] / "contracts"


class CompilerError(RuntimeError):
    def __init__(self, code: str, message: str, **details: Any) -> None:
        super().__init__(message)
        self.code = code
        self.details = details


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CompilerError("invalid_json", "Cannot read JSON document", path=str(path), type=type(exc).__name__) from exc
    if not isinstance(value, dict):
        raise CompilerError("invalid_json", "Expected a JSON object", path=str(path))
    return value


@lru_cache(maxsize=None)
def _schema(reference: str) -> dict[str, Any]:
    candidate = next(
        (
            path
            for path in SCHEMA_ROOT.glob("*.schema.json")
            if json.loads(path.read_text(encoding="utf-8")).get("$id") == reference
        ),
        None,
    )
    if candidate is None:
        raise CompilerError("schema_missing", "Compiler schema is unavailable", schema=reference)
    try:
        value = json.loads(candidate.read_text(encoding="utf-8"))
        jsonschema.validators.validator_for(value).check_schema(value)
    except (OSError, json.JSONDecodeError, jsonschema.SchemaError) as exc:
        raise CompilerError("schema_invalid", "Compiler schema is invalid", schema=str(candidate)) from exc
    return value


def validate_document(value: dict[str, Any], path: Path) -> None:
    reference = value.get("$schema")
    if not isinstance(reference, str) or not reference:
        raise CompilerError("contract_violation", "Compiler document does not declare a schema", path=str(path))
    if reference.startswith(("http://", "https://")):
        schema = _schema(reference)
    else:
        schema_path = (path.parent / reference).resolve()
        if not schema_path.is_file():
            raise CompilerError("schema_missing", "Compiler schema is unavailable", schema=reference)
        try:
            schema = json.loads(schema_path.read_text(encoding="utf-8"))
            jsonschema.validators.validator_for(schema).check_schema(schema)
        except (OSError, json.JSONDecodeError, jsonschema.SchemaError) as exc:
            raise CompilerError("schema_invalid", "Compiler schema is invalid", schema=str(schema_path)) from exc
    validator = jsonschema.validators.validator_for(schema)(schema, format_checker=jsonschema.FormatChecker())
    error = next(iter(validator.iter_errors(value)), None)
    if error is None:
        return
    pointer = "/" + "/".join(str(item) for item in error.absolute_path) if error.absolute_path else "<root>"
    raise CompilerError(
        "contract_violation",
        "Compiler document violates its schema",
        path=str(path),
        pointer=pointer,
        reason=error.message,
    )


def write_json(path: Path, value: dict[str, Any]) -> None:
    validate_document(value, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    staging = path.with_suffix(path.suffix + ".tmp")
    staging.write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    os.replace(staging, path)


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")


def canonical_hash(value: Any) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def resolve_owned_path(root: Path, relative: str, *, must_exist: bool = True) -> Path:
    candidate = (root / relative).resolve()
    if not candidate.is_relative_to(root.resolve()):
        raise CompilerError("path_escape", "Compiler-owned path escaped its root", path=relative)
    if must_exist and not candidate.is_file():
        raise CompilerError("input_missing", "Compiler input is missing", path=relative)
    return candidate
