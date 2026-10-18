"""Refinement-owned documents, retaining the parent compiler's error/hash contracts."""
from functools import lru_cache
import json
import os
from pathlib import Path

from jsonschema import Draft202012Validator
from ...app.contracts import CompilerError, file_hash, read_json, resolve_owned_path
from ...app.contracts import validate_document as validate_base_document


ROOT = Path(__file__).resolve().parents[1]


@lru_cache(maxsize=None)
def _schemas() -> dict:
    return {value["$id"]: value for path in (ROOT / "contracts").glob("*.schema.json")
            for value in [json.loads(path.read_text(encoding="utf-8"))]}


def validate_document(document: dict, path: Path) -> None:
    schema = _schemas().get(document.get("$schema"))
    if schema is None:
        validate_base_document(document, path)
        return
    error = next(Draft202012Validator(schema).iter_errors(document), None)
    if error is not None:
        raise CompilerError("contract_violation", "Refinement document violates its contract",
                            path=str(path), pointer="/" + "/".join(map(str, error.absolute_path)), reason=error.message)


def write_json(path: Path, document: dict) -> None:
    validate_document(document, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    staged = path.with_suffix(path.suffix + ".tmp")
    staged.write_text(json.dumps(document, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    os.replace(staged, path)


def existing_result(output_root: Path, inputs_hash: str) -> dict | None:
    path = output_root / "refinement_result.json"
    if not path.is_file():
        return None
    result = read_json(path)
    validate_document(result, path)
    if result.get("operation") != "refine" or result.get("status") != "accepted" or result.get("inputs_hash") != inputs_hash:
        raise CompilerError("output_conflict", "Output belongs to another or incomplete refinement")
    for entry in result["outputs"]:
        payload = resolve_owned_path(output_root, entry["path"])
        if payload.stat().st_size != entry["byte_size"] or file_hash(payload) != entry["sha256"]:
            raise CompilerError("accepted_output_changed", "Accepted refinement output changed", path=entry["path"])
    return result
