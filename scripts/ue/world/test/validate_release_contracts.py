# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
"""Validate World profile/schema consumers without generation or Unreal startup."""
from __future__ import annotations

import json
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "scripts/ue/check/data"))

import jsonschema
from _lib.schema_validator import SchemaError, report, validate_file


def validate_contracts(root: Path = REPO_ROOT) -> tuple[int, list[SchemaError]]:
    files = []
    errors = []
    schemas = []
    for data in sorted((root / "Plugins/World").glob("*/Data")):
        for directory in ("Profiles", "Runtime", "Presentation", "Authored"):
            files.extend(sorted((data / directory).rglob("*.json")))
        schemas.extend(sorted((data / "Schemas").rglob("*.json")))
    for contracts in sorted((root / "tools/World").glob("*/contracts")):
        schemas.extend(sorted(contracts.rglob("*.schema.json")))
    if not files or not schemas:
        raise RuntimeError("World contracts or schemas are absent; validation cannot certify an empty input set")
    for path in schemas:
        try:
            schema = json.loads(path.read_text(encoding="utf-8"))
            jsonschema.validators.validator_for(schema).check_schema(schema)
        except (OSError, ValueError, jsonschema.SchemaError) as error:
            errors.append(SchemaError(path, "", f"invalid World schema: {error}"))
    # Validate extant consumers too: a well-formed changed schema can still
    # invalidate unchanged profiles. The inline-$schema owner resolves each one.
    for path in files:
        errors.extend(validate_file(path))
    return len(files) + len(schemas), errors


def main() -> int:
    try:
        count, errors = validate_contracts()
        return report(errors, "World release contracts", count)
    except (RuntimeError, OSError, ValueError) as error:
        print(f"World release contracts failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
