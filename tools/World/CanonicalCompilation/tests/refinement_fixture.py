"""Create a self-contained disposable canonical fixture through the public owner."""

from copy import deepcopy
from pathlib import Path
import sys
import shutil

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.CanonicalCompilation.api import prepare_terrain_refinement
from World.CanonicalCompilation.app.artifacts import output_descriptor
from World.CanonicalCompilation.app.contracts import canonical_hash, file_hash, read_json, write_json
from World.CanonicalCompilation.tests.test_terrain_refinement import fixture

PREFIX = "https://alis.world/schemas/world-compiler/"


def make_base(root: Path, cross_cell: bool = True):
    cells, water, policy, selected = fixture()
    if not cross_cell:
        water["geometry"]["coordinates"] = [[[8, 12], [22, 12], [22, 18], [8, 18], [8, 12]]]
        selected.pop(next(reversed(selected)))
    water.update(feature_id="alis:fixture:way:1", owner_cell_id=next(iter(cells)),
                 intersecting_cell_ids=list(cells), source_refs=[{"source_id": "fixture"}],
                 representations=[], provenance_result="accepted")
    water["attributes"].update(surface_group_id="fixture-water", surface_group_members=["way/1"],
                               surface_geometry="polygon", surface_behavior="standing", surface_role="area")
    water["attributes"]["surface_function"].update(function_id="standing_polygon_quantile", function_version=1)
    lineage = {name: "0" * 64 for name in ("implementation_sha256", "execution_environment_identity", "compiler_contract_sha256",
        "source_ingestion_contract_sha256", "source_semantic_contract_sha256", "source_result_sha256", "raster_source_sha256", "overlay_sha256")}
    lineage.update(grid_id=next(iter(cells.values()))["grid_id"], algorithm_version="fixture", identity_namespace="fixture",
                   source_snapshots=[{"snapshot_id": "fixture"}], overlay_id="fixture",
                   source_area={"area_fingerprint": "sha256:" + "0" * 64, "bbox": [0, 0, 60, 30], "axis_order": "xy", "crs": "fixture"})
    entries, artifacts = [], []
    for column, (identifier, terrain) in enumerate(cells.items()):
        terrain.update({"$schema": PREFIX + "terrain-cell-v2.json", "schema_version": 2,
                        "vertical_provenance": {"source_ref": "fixture", "vertical_datum": "fixture_height", "source_accuracy_m": 0,
                                                "confidence": "fixture", "sampling_quantization_residual_m": 0.05},
                        "raster_alignment": {"authority": "synthetic_fixture_affine", "source_crs": "fixture",
                                             "canonical_crs": "fixture", "resampling_method": "bilinear"},
                        "raster_component_dependencies": [], "halo_window": {}, "water_fit": {"core_samples": [[12, 12], [12, 12]], "halo_window": {}},
                        "height_edges": {}, "height_border_bands": {}, "weight_mask": {}, "authored_patch_ids": []})
        terrain["surface_semantics"].update(contract_id="terrain_surface_semantics", contract_version=1,
            hydro_transition_radius_m=60, water_clearance_m=0, contract_sha256="0" * 64,
            halo_samples={"ground": [], "hydro_transition": []}, edge_sha256={})
        path = root / f"canonical/terrain/cell_{column}.json"
        write_json(path, terrain)
        features_path = root / f"canonical/features/cell_{column}.json"
        write_json(features_path, {"$schema": PREFIX + "canonical-feature-v1.json", "schema_version": 1,
                                 "grid_id": terrain["grid_id"], "cell_id": identifier,
                                 "features": [water] if column == 0 else []})
        def descriptor(kind, file):
            return {"artifact_id": f"{kind}:{identifier}", "cell_id": identifier, "representation": kind,
                    "content_hash": file_hash(file), "byte_size": file.stat().st_size, "schema_version": 1,
                    "dependencies": [], "provenance_result": "accepted"}
        td, fd = descriptor("terrain", path), descriptor("features", features_path)
        artifacts.extend([td, fd])
        manifest_path = root / f"canonical/cells/cell_{column}.json"
        write_json(manifest_path, {"$schema": PREFIX + "cell-manifest-v1.json", "schema_version": 1,
            "grid_id": terrain["grid_id"], "cell_id": identifier, "cell_x": column, "cell_y": 0, "bounds": terrain["bounds"],
            "neighbors": {}, "owned_feature_ids": [water["feature_id"]] if column == 0 else [], "referenced_feature_ids": [],
            "terrain_artifact": {**td, "path": path.relative_to(root).as_posix()},
            "feature_artifact": {**fd, "path": features_path.relative_to(root).as_posix()},
            "terrain_component_dependencies": [], "terrain_lineage": lineage, "feature_lineage": lineage,
            "provenance_result": "accepted"})
        entries.append({"cell_id": identifier, "path": manifest_path.relative_to(root).as_posix(),
                        "sha256": file_hash(manifest_path), "byte_size": manifest_path.stat().st_size,
                        "terrain_lineage": lineage, "feature_lineage": lineage})
    coverage = {"$schema": PREFIX + "coverage-manifest-v1.json", "schema_version": 1, "profile_id": "refinement_fixture",
                "world_data_plugin": "ProjectWorldTestData", "engine_georeference_origin": [0, 0], "algorithm_version": "fixture",
                "grid_id": lineage["grid_id"], "area_fingerprints": ["sha256:" + "0" * 64], "cells": entries,
                "grid": {"canonical_crs": "fixture", "vertical_datum": "fixture_height", "coordinate_transform": "fixture_affine",
                         "origin": [0, 0], "sample_spacing": [30, 30], "cell_quads": [1, 1],
                         "coordinate_quantization": 0.01, "height_quantization": 0.1, "vertical_origin_m": 0},
                "feature_count": 1, "rejected_feature_count": 0, "artifact_descriptors": artifacts, "semantic_hash": canonical_hash(cells)}
    write_json(root / "canonical/coverage.json", coverage)
    write_json(root / "reports/provenance.json", {"$schema": PREFIX + "provenance-report-v1.json", "schema_version": 1,
        "profile_id": "refinement_fixture", "policy_result": "accepted", "sources": [{"source_id": "fixture"}],
        "feature_sources": [], "raster_authority": None, "cell_lineage": entries, "overlay": {}})
    write_json(root / "compile_result.json", {"$schema": PREFIX + "compile-result-v1.json", "schema_version": 1,
        "operation_id": "compile:refinement_fixture", "operation": "compile", "status": "accepted", "profile_id": "refinement_fixture",
        "inputs_hash": canonical_hash(cells), "path_base": "output_root", "errors": [],
        "outputs": [output_descriptor("compiler_document", p, root) for p in sorted(root.rglob("*.json"))]})
    return cells, water, policy, selected


def invalid_candidate(destination: Path, name: str, mutate):
    """Reauthenticate malformed geometry so native rejection must inspect the contract."""
    bad = destination / name
    shutil.copytree(destination / "candidate", bad)
    path = bad / "canonical/refined_terrain/cell_0.json"
    surface = read_json(path)
    mutate(surface["quads"][0])
    write_json(path, surface)
    manifest_path = bad / "canonical/cells/cell_0.json"
    manifest = read_json(manifest_path)
    manifest["terrain_artifact"].update(content_hash=file_hash(path), byte_size=path.stat().st_size)
    write_json(manifest_path, manifest)
    coverage = read_json(bad / "canonical/coverage.json")
    coverage["cells"][0].update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
    coverage["artifact_descriptors"][0].update(content_hash=file_hash(path), byte_size=path.stat().st_size)
    write_json(bad / "canonical/coverage.json", coverage)
    result = read_json(bad / "compile_result.json")
    result["outputs"] = [output_descriptor("compiler_document", p, bad) for p in sorted(bad.rglob("*.json")) if p.name != "compile_result.json"]
    write_json(bad / "compile_result.json", result)


def main(destination: Path):
    if destination.exists():
        raise RuntimeError("Fixture destination must be a new disposable directory")
    _, water, policy, selected = make_base(destination / "base")
    prepare_terrain_refinement(ROOT, destination / "base/compile_result.json", destination / "candidate", water, policy, selected)

    def missing(quad):
        quad["triangles"].pop()
        quad["water_triangles"] = [i for i in quad["water_triangles"] if i < len(quad["triangles"])]

    def weights(quad):
        point = next(v for v in quad["vertices"] if v[0] == 30 and 0 < v[1] < 30)
        point[3:] = [0, 1]

    def duplicate(quad):
        index = quad["triangles"][0][0]
        quad["vertices"].append(deepcopy(quad["vertices"][index]))
        quad["triangles"][0][0] = len(quad["vertices"]) - 1

    def crack(quad):
        # Split one interior edge on one face only: area is unchanged but topology cracks.
        for face in quad["triangles"]:
            for i in range(3):
                a, b, c = face[i:] + face[:i]
                p, q = quad["vertices"][a], quad["vertices"][b]
                if any(p[axis] == q[axis] == boundary for axis in (0, 1) for boundary in (0, 30)):
                    continue
                midpoint = [(x + y) / 2 for x, y in zip(p, q)]
                if any(v[:2] == midpoint[:2] for v in quad["vertices"]):
                    continue
                m = len(quad["vertices"])
                quad["vertices"].append(midpoint)
                face[:] = [a, m, c]
                quad["triangles"].append([m, b, c])
                return
        raise AssertionError("Fixture needs an interior edge")

    for name, mutation in (("bad_coverage", missing), ("bad_weights", weights),
                           ("bad_duplicate", duplicate), ("bad_crack", crack)):
        invalid_candidate(destination, name, mutation)


if __name__ == "__main__":
    main(Path(sys.argv[1]))
