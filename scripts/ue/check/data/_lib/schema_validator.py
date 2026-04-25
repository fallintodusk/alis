"""Inline-$schema validation for plugin Data JSON files.

Rule (from AGENTS.md):
    Every JSON data file MUST include `$schema` with relative path to its
    schema file. No IDE-specific config -- validation is universal via
    inline `$schema`.

Object-root JSONs
    Must have a top-level `"$schema"` field. Value is a relative path
    resolved against the JSON file's directory.

Array-root JSONs (unavoidable: JSON arrays cannot have a `$schema` field)
    Matched by filename convention via ARRAY_ROOT_SCHEMA_MAP below. The
    mapping is explicit so a new array-shaped data file must register
    here -- otherwise it fails fast with an actionable error.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

import jsonschema

from .discovery import repo_root


# ---------------------------------------------------------------------------
# Array-root files: map basename -> schema path (relative to repo root).
# JSON arrays cannot embed $schema, so these map by convention.
# ---------------------------------------------------------------------------
ARRAY_ROOT_SCHEMA_MAP: dict[str, str] = {
    "ui_definitions.json": "Plugins/UI/ProjectUI/Data/Schemas/ui_definitions.schema.json",
}


# ---------------------------------------------------------------------------
# Files we intentionally skip (not valid JSON data files; owner-flagged).
# ---------------------------------------------------------------------------
SKIP_FILES: set[str] = {
    # SunSky.json contains a stray plain-text note; it is not JSON data and
    # is not read by any runtime loader. See architect audit notes.
    "Plugins/World/ProjectWorld/Data/SunSky.json",
}


@dataclass
class SchemaError:
    file: Path
    pointer: str
    message: str

    def format(self) -> str:
        rel = self.file.relative_to(repo_root()) if self.file.is_absolute() else self.file
        pointer = self.pointer if self.pointer else "<root>"
        return f"{rel}: {pointer} -- {self.message}"


def _discover_data_files() -> list[Path]:
    """Scan Plugins/*/Data/**/*.json excluding Schemas/ subtree and skip list."""
    root = repo_root()
    skip_abs = {(root / p).resolve() for p in SKIP_FILES}
    results: list[Path] = []
    plugins = root / "Plugins"
    if not plugins.exists():
        return results
    for data_dir in plugins.glob("*/*/Data"):
        if not data_dir.is_dir():
            continue
        for path in data_dir.rglob("*.json"):
            # Skip schema files
            if "Schemas" in path.parts:
                continue
            if path.resolve() in skip_abs:
                continue
            results.append(path)
    return sorted(results)


def _resolve_schema_path(json_path: Path, schema_ref: str) -> Path:
    """Resolve $schema value relative to the JSON file's directory."""
    return (json_path.parent / schema_ref).resolve()


def _format_jsonschema_error(err: jsonschema.ValidationError) -> tuple[str, str]:
    """Return (json_pointer, message) from a jsonschema error."""
    pointer = "/" + "/".join(str(p) for p in err.absolute_path) if err.absolute_path else ""
    msg = err.message
    if err.validator == "required":
        # Surface which field is missing more clearly.
        msg = f"missing required field: {err.message}"
    return pointer, msg


def validate_file(json_path: Path) -> list[SchemaError]:
    """Validate a single JSON data file. Returns list of errors (empty=green)."""
    errors: list[SchemaError] = []
    rel = json_path.relative_to(repo_root()) if json_path.is_absolute() else json_path

    try:
        with json_path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except json.JSONDecodeError as ex:
        errors.append(SchemaError(json_path, "", f"invalid JSON: {ex.msg} (line {ex.lineno}, col {ex.colno})"))
        return errors
    except OSError as ex:
        errors.append(SchemaError(json_path, "", f"cannot read file: {ex}"))
        return errors

    # Resolve schema path.
    schema_path: Path | None = None
    if isinstance(data, dict):
        schema_ref = data.get("$schema")
        if not isinstance(schema_ref, str) or not schema_ref.strip():
            errors.append(SchemaError(
                json_path, "",
                "missing $schema (object-root JSON must include a top-level \"$schema\" field "
                "with a relative path to its schema file)"
            ))
            return errors
        # Allow the standard JSON Schema URL form used by our draft-2020-12 schema
        # files themselves. Data files MUST NOT use the URL form -- they point at
        # a concrete schema file.
        if schema_ref.startswith("http://") or schema_ref.startswith("https://"):
            errors.append(SchemaError(
                json_path, "/$schema",
                f"$schema must be a relative path to a schema file, not a URL ({schema_ref})"
            ))
            return errors
        schema_path = _resolve_schema_path(json_path, schema_ref)
    elif isinstance(data, list):
        mapped = ARRAY_ROOT_SCHEMA_MAP.get(json_path.name)
        if not mapped:
            errors.append(SchemaError(
                json_path, "",
                f"array-root JSON has no entry in ARRAY_ROOT_SCHEMA_MAP "
                f"(file base name '{json_path.name}'). Register its schema there or "
                f"convert the file to an object with a $schema field."
            ))
            return errors
        schema_path = (repo_root() / mapped).resolve()
    else:
        errors.append(SchemaError(json_path, "", f"unsupported root type: {type(data).__name__}"))
        return errors

    # Load schema.
    if not schema_path.exists():
        errors.append(SchemaError(
            json_path, "/$schema" if isinstance(data, dict) else "",
            f"schema file not found: {schema_path.relative_to(repo_root())}"
        ))
        return errors
    try:
        with schema_path.open("r", encoding="utf-8") as fh:
            schema = json.load(fh)
    except (OSError, json.JSONDecodeError) as ex:
        errors.append(SchemaError(
            json_path, "/$schema" if isinstance(data, dict) else "",
            f"cannot load schema {schema_path.name}: {ex}"
        ))
        return errors

    # Validate.
    validator_cls = jsonschema.validators.validator_for(schema)
    try:
        validator_cls.check_schema(schema)
    except jsonschema.SchemaError as ex:
        errors.append(SchemaError(schema_path, "", f"schema is itself invalid: {ex.message}"))
        return errors
    validator = validator_cls(schema)

    for err in sorted(validator.iter_errors(data), key=lambda e: tuple(e.absolute_path)):
        pointer, msg = _format_jsonschema_error(err)
        errors.append(SchemaError(json_path, pointer, msg))

    return errors


def validate_all_data_files() -> tuple[int, list[SchemaError]]:
    """Run schema validation on every discovered data JSON.

    Returns (file_count, errors).
    """
    files = _discover_data_files()
    all_errors: list[SchemaError] = []
    for f in files:
        all_errors.extend(validate_file(f))
    return len(files), all_errors


def report(errors: Iterable[SchemaError], label: str, file_count: int) -> int:
    errors_list = list(errors)
    if not errors_list:
        print(f"{label} passed ({file_count} files).")
        return 0
    print(f"\n{label} FAILED ({len(errors_list)} errors across {file_count} files):")
    for e in errors_list:
        print(f"  [X] {e.format()}")
    return 1
