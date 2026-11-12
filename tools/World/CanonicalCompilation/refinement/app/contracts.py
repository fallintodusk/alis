"""Refinement-owned documents, retaining the parent compiler's error/hash contracts."""
from functools import lru_cache
import json
import math
import os
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Resource
from referencing.exceptions import Unresolvable
from ...app.contracts import CompilerError, file_hash, read_json, resolve_owned_path
from ...app.contracts import validate_document as validate_base_document
from ...app.contracts import schema_registry


ROOT = Path(__file__).resolve().parents[1]


def strict_json_document(payload, path):
    def invalid_number(_):
        raise CompilerError('invalid_json', 'Refinement input contains a nonfinite JSON number', path=str(path))
    def finite_float(token):
        value = float(token)
        if not math.isfinite(value):
            invalid_number(token)
        return value
    try:
        document = json.loads(payload, parse_constant=invalid_number, parse_float=finite_float)
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as error:
        raise CompilerError('invalid_json', 'Refinement input is not a readable JSON document', path=str(path)) from error
    if not isinstance(document, dict):
        raise CompilerError('invalid_json', 'Refinement input must be a JSON object', path=str(path))
    return document


def operation_input_descriptors(operation):
    if operation['kind'] == 'authored_dry_terrain':
        return [(operation['input'], 'authored')]
    if operation['kind'] == 'identified_basin':
        return [(operation['input'], 'basin')]
    if operation['kind'] != 'constructed_surface_import':
        return [(operation['context'], 'source')]
    inputs = operation['inputs']
    result = [(inputs['model'], 'model'), (inputs['policy'], 'policy')]
    for context in inputs['source_contexts']:
        result.append((context['normalized'], 'source'))
        if context['native'] is not None:
            result.append((context['native'], 'native'))
    return result


def request_input_descriptors(request):
    result = {}
    for operation in request['operations']:
        for descriptor, role in operation_input_descriptors(operation):
            if operation['kind'] in ('constructed_surface_import', 'identified_basin', 'authored_dry_terrain') and descriptor['path'] != f"refinement/inputs/{descriptor['sha256']}.json":
                raise CompilerError('refinement_input_conflict', 'Retained model input is not content addressed')
            value = descriptor, role
            path = descriptor['path']
            if path in result and result[path] != value:
                raise CompilerError('refinement_input_conflict', 'Retained input has contradictory declared roles', path=path)
            result[path] = value
    return result


def validate_native_snapshot(payload, descriptor):
    document = strict_json_document(payload, descriptor['path'])
    if descriptor['format'] == 'sanitized_osm_full_response':
        document = document.get('document') if isinstance(document, dict) else None
    if (not isinstance(document, dict) or document.get('version') != '0.6' or
            not isinstance(document.get('elements'), list)):
        raise CompilerError('refinement_input_invalid', 'Retained native source differs from its declared format')


@lru_cache(maxsize=None)
def _schemas() -> dict:
    return {value["$id"]: value for path in (ROOT / "contracts").rglob("*.schema.json")
            for value in [json.loads(path.read_text(encoding="utf-8"))]}


def validate_document(document: dict, path: Path) -> None:
    schema = _schemas().get(document.get("$schema"))
    if schema is None:
        validate_base_document(document, path)
        return
    resources = {key: Resource.from_contents(value) for key, value in _schemas().items()}
    source_contracts = ROOT.parents[1]/'SourceIngestion/supplemental/contracts'
    for name in ('ground-grid.schema.json', 'derived-ground.schema.json'):
        value = json.loads((source_contracts/name).read_text(encoding='ascii'))
        resources[value['$id']] = Resource.from_contents(value)
    registry = schema_registry().with_resources(resources.items())
    try:
        error = next(Draft202012Validator(schema, registry=registry).iter_errors(document), None)
    except Unresolvable as error:
        raise CompilerError('schema_reference_unknown', 'Refinement schema reference is unavailable offline', path=str(path)) from error
    if error is not None:
        raise CompilerError("contract_violation", "Refinement document violates its contract",
                            path=str(path), pointer="/" + "/".join(map(str, error.absolute_path)), reason=error.message)


def write_json(path: Path, document: dict) -> None:
    validate_document(document, path)
    path.parent.mkdir(parents=True, exist_ok=True)
    staged = path.with_suffix(path.suffix + ".tmp")
    staged.write_text(json.dumps(document, indent=2, ensure_ascii=True, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
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
