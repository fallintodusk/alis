"""Authenticated disposable refinement publication; production promotion remains separate."""

from copy import deepcopy
import math
from pathlib import Path
import shutil
import tempfile

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, validate_triangle_cover
from .artifacts import accepted_base, existing_result, output_descriptor, promote_staging
from .contracts import CompilerError, canonical_hash, file_hash, read_json, resolve_owned_path, validate_document, write_json
from .terrain_refinement import EPSILON, _point, refine_terrain_cells, _domain
from .terrain_water_coverage import _intersects


def validate_refinement(repo_root: Path, base: dict, surface: dict) -> None:
    validate_document(surface, Path("refinement.json"))
    if surface["grid_id"] != base["grid_id"] or surface["cell_id"] != base["cell_id"]:
        raise CompilerError("refinement_identity_invalid", "Replacement identity differs from base grid")
    seen = set()
    for quad in surface["quads"]:
        x, y = quad["coordinate"]
        if (x, y) in seen or not (0 <= y < len(base["core_samples"]) - 1 and 0 <= x < len(base["core_samples"][0]) - 1):
            raise CompilerError("refinement_coverage_invalid", "Duplicate or out-of-grid replacement quad")
        seen.add((x, y))
        vertices, faces = quad["vertices"], quad["triangles"]
        for vertex in vertices:
            if any(not isinstance(v, (int, float)) or not math.isfinite(v) for v in vertex):
                raise CompilerError("refinement_value_invalid", "Replacement has nonfinite values")
            if any(v < -EPSILON or v > 1 + EPSILON for v in vertex[3:]) or abs(sum(vertex[3:]) - 1) > EPSILON:
                raise CompilerError("refinement_weights_invalid", "Replacement weights are not normalized")
        triangles = []
        for face in faces:
            if max(face) >= len(vertices):
                raise CompilerError("refinement_index_invalid", "Replacement triangle index is outside vertices")
            points = [vertices[i][:2] for i in face]
            a, b, c = points
            if (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]) <= 1e-12:
                raise CompilerError("refinement_coverage_invalid", "Replacement triangle is collapsed or reversed")
            triangles.append(points)
        if any(i >= len(faces) for i in quad["water_triangles"]):
            raise CompilerError("refinement_index_invalid", "Water triangle index is invalid")
        a, b, c, d = _point(base, x, y), _point(base, x + 1, y), _point(base, x, y + 1), _point(base, x + 1, y + 1)
        try:
            validate_triangle_cover(repo_root, {"type": "Polygon", "coordinates": [[a[:2], b[:2], d[:2], c[:2], a[:2]]]}, triangles)
        except ExecutionEnvironmentError as error:
            raise CompilerError(error.code, str(error), **error.details) from error


def prepare_terrain_refinement(repo_root: Path, base_result_path: Path, output_root: Path,
                               water: dict, policy: dict, selected: dict) -> tuple[dict, Path]:
    """Prepare an isolated final-surface bundle. No production authority is advanced."""
    scratch = (repo_root / "tmp").resolve()
    output_root = output_root.resolve()
    if not output_root.is_relative_to(scratch) or output_root == scratch:
        raise CompilerError("refinement_scope_invalid", "Refinement preparation is confined to project tmp")
    base_root, base_result = accepted_base(base_result_path.resolve())
    if output_root == base_root or output_root.is_relative_to(base_root) or base_root.is_relative_to(output_root):
        raise CompilerError("refinement_scope_invalid", "Candidate and base directories must be disjoint")
    coverage = read_json(base_root / "canonical/coverage.json")
    manifests = {entry["cell_id"]: read_json(resolve_owned_path(base_root, entry["path"])) for entry in coverage["cells"]}
    cells = {key: read_json(resolve_owned_path(base_root, value["terrain_artifact"]["path"])) for key, value in manifests.items()}
    features = [f for entry in base_result["outputs"] if entry["path"].startswith("canonical/features/")
                for f in read_json(resolve_owned_path(base_root, entry["path"]))["features"]]
    admitted = [f for f in features if f["feature_id"] == water["feature_id"]]
    if len(admitted) != 1 or canonical_hash(admitted[0]) != canonical_hash(water):
        raise CompilerError("refinement_water_identity", "Water is not the unchanged accepted base feature")
    domain = _domain(cells, selected)
    for feature in features:
        if feature["feature_class"] == "water" and feature["feature_id"] != water["feature_id"] and any(_intersects(feature, bounds) for bounds in domain):
            raise CompilerError("refinement_conflicting_water", "Additional Water influence needs an explicit joint policy")
    payloads = refine_terrain_cells(repo_root, cells, water, policy, selected)
    request_hash = canonical_hash({"base": file_hash(base_result_path), "surfaces": payloads,
                                   "water": water, "policy": policy, "selected": selected})
    if output_root.exists():
        previous = existing_result(output_root, request_hash)
        if previous is None:
            raise CompilerError("output_conflict", "Existing refinement directory has no accepted receipt")
        return previous, output_root
    output_root.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_root.parent, prefix="refinement-") as temporary:
        staging = Path(temporary) / "candidate"
        staging.mkdir()
        for entry in base_result["outputs"]:
            destination = resolve_owned_path(staging, entry["path"], must_exist=False)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(resolve_owned_path(base_root, entry["path"]), destination)
        changed_cells = []
        for identifier, payload in sorted(payloads.items()):
            manifest = deepcopy(manifests[identifier])
            base_descriptor = manifest["terrain_artifact"]
            payload["base"] = {"path": base_descriptor["path"], "sha256": base_descriptor["content_hash"],
                               "byte_size": base_descriptor["byte_size"]}
            validate_refinement(repo_root, cells[identifier], payload)
            relative = base_descriptor["path"].replace("canonical/terrain/", "canonical/refined_terrain/")
            target = staging / relative
            write_json(target, payload)
            manifest["terrain_artifact"] = {**base_descriptor, "path": relative, "content_hash": file_hash(target),
                                             "byte_size": target.stat().st_size,
                                             "dependencies": [*base_descriptor["dependencies"], f"refinement:{canonical_hash(payload['consumed'])}"]}
            entry = next(e for e in coverage["cells"] if e["cell_id"] == identifier)
            manifest_path = staging / entry["path"]
            write_json(manifest_path, manifest)
            entry.update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
            for descriptor in coverage["artifact_descriptors"]:
                if descriptor["artifact_id"] == base_descriptor["artifact_id"]:
                    descriptor.update({k: v for k, v in manifest["terrain_artifact"].items() if k != "path"})
            changed_cells.append(identifier)
        coverage["semantic_hash"] = canonical_hash({"base_semantic_hash": coverage["semantic_hash"],
                                                   "refinement": {key: canonical_hash(value) for key, value in sorted(payloads.items())}})
        write_json(staging / "canonical/coverage.json", coverage)
        receipt = {"$schema": "https://alis.world/schemas/world-compiler/refinement-report-v1.json", "schema_version": 1,
                   "base_result_sha256": file_hash(base_result_path), "inputs_hash": request_hash,
                   "changed_cells": changed_cells, "base_lineage_retained": True,
                   "production_adoption": False}
        write_json(staging / "reports/refinement.json", receipt)
        result = {**base_result, "operation": "refine", "operation_id": f"refine:{request_hash[:16]}", "inputs_hash": request_hash,
                  "outputs": [output_descriptor("compiler_document", path, staging) for path in sorted(staging.rglob("*.json"))]}
        write_json(staging / "compile_result.json", result)
        promote_staging(staging, output_root)
    return result, output_root
