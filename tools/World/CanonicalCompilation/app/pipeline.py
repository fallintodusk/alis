from __future__ import annotations

import os
import shutil
import time
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, execution_identity

from .adapters import canonical_feature_id
from .artifacts import accepted_base, existing_result, output_descriptor, promote_staging, write_cells
from .contracts import (
    CompilerError,
    canonical_hash,
    file_hash,
    read_json,
    resolve_owned_path,
    validate_document,
    write_json,
)
from .features import compile_features
from .incremental import (
    feature_rebuild_cells,
    load_base_features,
    load_base_terrain,
    merge_features,
    terrain_impact_cells,
)
from .lineage import (
    base_cells,
    build_lineages,
    derive_overlay_changes,
    reusable_layers,
)
from .projection import select_source_features
from .raster_dependencies import component_dirty_cells, derive_raster_dependencies
from .reports import write_reports
from .source import SourceBundle, load_source_bundle
from .spatial import cell_id, grid_id
from .terrain import build_terrain_cells
from .validation import build_validation_report
from .water import expand_water_change_ids, validate_water_profile


RUN_INPUTS_SCHEMA = "https://alis.world/schemas/world-compiler/run-inputs-v1.json"
RESULT_SCHEMA = "https://alis.world/schemas/world-compiler/compile-result-v1.json"
RUN_COMPONENT_FIELDS = (
    "source_inputs_hash",
    "source_result_sha256",
    "feature_set_semantic_sha256",
    "profile_sha256",
    "overlay_sha256",
    "execution_environment",
    "fixture_features_sha256",
    "base_result_sha256",
    "change_scope",
    "implementation_sha256",
)


def compilation_root() -> Path:
    return Path(__file__).resolve().parents[1]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[4]


def compiler_implementation_hash() -> str:
    root = compilation_root()
    implementation_files = [
        *sorted((root / "app").glob("*.py")),
        *sorted((root / "contracts").glob("*.schema.json")),
        root / "run.py",
        root / "bootstrap.py",
    ]
    return canonical_hash({
        "files": {
            path.relative_to(root).as_posix(): file_hash(path)
            for path in implementation_files
        }
    })


def compiler_run_inputs_hash(contract: dict[str, Any]) -> str:
    return canonical_hash({field: contract[field] for field in RUN_COMPONENT_FIELDS})


def profile_path(value: str) -> Path:
    root = compilation_root()
    supplied = Path(value)
    if supplied.suffix == ".json" or supplied.is_absolute() or "/" in value or "\\" in value:
        path = supplied.resolve() if supplied.is_absolute() else (repo_root() / supplied).resolve()
    else:
        path = root / "profiles" / f"{value}.compile.json"
    if not path.is_file():
        raise CompilerError("profile_missing", "Compiler profile does not exist", profile=value)
    return path


def load_profile(path: Path) -> dict[str, Any]:
    value = read_json(path)
    validate_document(value, path)
    if "target_cell_range" in value:
        selection = value["target_cell_range"]
        if selection["min_x"] > selection["max_x"] or selection["min_y"] > selection["max_y"]:
            raise CompilerError("profile_invalid", "Target cell range is inverted")
        value["target_cells"] = [
            {"x": x_value, "y": y_value}
            for y_value in range(selection["min_y"], selection["max_y"] + 1)
            for x_value in range(selection["min_x"], selection["max_x"] + 1)
        ]
    targets = {(item["x"], item["y"]) for item in value["target_cells"]}
    if len(targets) != len(value["target_cells"]):
        raise CompilerError("duplicate_target_cell", "Compiler profile repeats a target cell")
    if value["grid"]["coordinate_transform"] == "fixture_affine" and "fixture_scale_m" not in value["grid"]:
        raise CompilerError("profile_invalid", "Fixture affine grids require fixture_scale_m")
    if value["grid"]["coordinate_transform"] != "fixture_affine" and not value["grid"]["canonical_crs"].startswith("EPSG:"):
        raise CompilerError("profile_invalid", "Production coordinate transform requires an EPSG canonical CRS")
    if max(value["grid"]["operation_halos"].values(), default=0) > value["grid"]["halo_samples"]:
        raise CompilerError("profile_invalid", "Declared terrain halo is smaller than an operation halo")
    alignment = value["grid"]["alignment_cell_bounds"]
    if alignment[0] > alignment[2] or alignment[1] > alignment[3]:
        raise CompilerError("profile_invalid", "Raster alignment cell bounds are inverted")
    outside_alignment = any(
        not (alignment[0] <= x_value <= alignment[2] and alignment[1] <= y_value <= alignment[3])
        for x_value, y_value in targets
    )
    if outside_alignment:
        raise CompilerError("profile_invalid", "Target cell is outside the fixed raster alignment domain")
    validate_water_profile(value)
    quality_roles = [item["role"] for item in value.get("quality_cells", [])]
    if len(quality_roles) != len(set(quality_roles)):
        raise CompilerError("profile_invalid", "Quality cell roles must be unique")
    for quality in value.get("quality_cells", []):
        selected = {(item["x"], item["y"]) for item in quality["cells"]}
        if not selected.issubset(targets):
            raise CompilerError("profile_invalid", "Quality cells must belong to the target territory")
        if quality["role"] == "cross_cell_boundary" and len(selected) < 2:
            raise CompilerError("profile_invalid", "Cross-cell quality scope requires at least two cells")
    return value


def _run_contract(
    profile_path_value: Path,
    profile: dict[str, Any],
    overlay_path: Path,
    bundle: SourceBundle,
    base_result: Path | None,
    terrain_change_bounds: list[tuple[float, float, float, float]],
    feature_change_ids: list[str],
    overlay: dict[str, Any],
) -> dict[str, Any]:
    try:
        environment = execution_identity(
            repo_root(), profile["grid"]["coordinate_transform"] != "fixture_affine"
        )
    except (ExecutionEnvironmentError, RuntimeError, ValueError) as exc:
        raise CompilerError("execution_environment_invalid", str(exc)) from exc
    components: dict[str, Any] = {
        "source_inputs_hash": bundle.result["inputs_hash"],
        "source_result_sha256": file_hash(bundle.result_path),
        "feature_set_semantic_sha256": canonical_hash({"features": bundle.features}),
        "profile_sha256": file_hash(profile_path_value),
        "overlay_sha256": file_hash(overlay_path),
        "execution_environment": environment,
        "fixture_features_sha256": None,
        "base_result_sha256": file_hash(base_result) if base_result else None,
        "change_scope": {
            "terrain_bounds": [list(bounds) for bounds in sorted(set(terrain_change_bounds))],
            "provider_feature_ids": sorted(set(feature_change_ids)),
        },
        "implementation_sha256": compiler_implementation_hash(),
    }
    if profile.get("fixture_features"):
        fixture_path = resolve_owned_path(repo_root(), profile["fixture_features"])
        components["fixture_features_sha256"] = file_hash(fixture_path)
    return {
        "$schema": RUN_INPUTS_SCHEMA,
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        "grid_id": grid_id(profile["grid"]),
        "overlay_document": overlay,
        "base_reuse": {
            "decision": "reused" if base_result else "not_requested",
            "reasons": [],
        },
        "authoritative_change_scope": {
            "new_target_cells": [],
            "terrain_bounds": [],
            "provider_feature_ids": [],
            "source_result_changed": False,
            "source_overlap_verified": False,
        },
        **components,
        "run_inputs_hash": compiler_run_inputs_hash(components),
    }


def default_output_root(profile: dict[str, Any], inputs_hash: str) -> Path:
    return repo_root() / "tmp" / "world" / "canonical_compilation" / "runs" / profile["profile_id"] / inputs_hash / "run"


def _terrain_grid(profile: dict[str, Any]) -> dict[str, Any]:
    grid = dict(profile["grid"])
    sampling = profile.get("raster_sampling")
    if sampling:
        grid["resampling_scale"] = sampling["bilinear_warp_scale"]
    return grid


def _validate_base(base_result: Path | None, profile: dict[str, Any], identifier: str) -> Path | None:
    if base_result is None:
        return None
    base_root, base_receipt = accepted_base(base_result.resolve())
    if base_receipt.get("profile_id") != profile["profile_id"]:
        raise CompilerError("base_profile_conflict", "Incremental base belongs to another compiler profile")
    coverage_path = base_root / "canonical" / "coverage.json"
    coverage = read_json(coverage_path)
    validate_document(coverage, coverage_path)
    if coverage.get("grid_id") != identifier:
        raise CompilerError("base_grid_conflict", "Incremental base uses another canonical terrain grid")
    return base_root


def compile_world(
    profile_value: str,
    source_result: Path | None = None,
    output_root_value: Path | None = None,
    base_result: Path | None = None,
    terrain_change_bounds: list[tuple[float, float, float, float]] | None = None,
    feature_change_ids: list[str] | None = None,
) -> tuple[dict[str, Any], Path]:
    started = time.perf_counter()
    root = compilation_root()
    selected_profile_path = profile_path(profile_value)
    profile = load_profile(selected_profile_path)
    overlay_path = resolve_owned_path(repo_root(), profile["authored_overlay"])
    overlay = read_json(overlay_path)
    validate_document(overlay, overlay_path)
    bundle = load_source_bundle(repo_root(), root, profile, source_result)
    change_bounds = sorted(set(terrain_change_bounds or []))
    changed_feature_ids = sorted(set(feature_change_ids or []))
    has_scope = bool(change_bounds or changed_feature_ids)
    if base_result is None and has_scope:
        raise CompilerError(
            "incremental_arguments_invalid",
            "A declared change scope requires an accepted base",
        )
    identifier = grid_id(profile["grid"])
    base_root = _validate_base(base_result, profile, identifier)
    contract = _run_contract(
        selected_profile_path,
        profile,
        overlay_path,
        bundle,
        base_result.resolve() if base_result else None,
        change_bounds,
        changed_feature_ids,
        overlay,
    )
    targets = [(int(item["x"]), int(item["y"])) for item in profile["target_cells"]]
    target_set = set(targets)
    current_lineages = build_lineages(profile, bundle, contract, overlay)
    base_documents = base_cells(base_root) if base_root else {}
    new_targets = target_set - set(base_documents)
    removed_targets = set(base_documents) - target_set
    source_result_changed = False
    source_overlap_verified = False
    feature_semantic_changed = False
    overlay_bounds: list[tuple[float, float, float, float]] = []
    overlay_feature_ids: set[str] = set()
    current_dependencies = None
    layer_reuse = {
        "terrain": (False, ["base_not_requested"]),
        "feature": (False, ["base_not_requested"]),
    }
    if base_root:
        base_contract_path = base_root / "run_contract.json"
        base_contract = read_json(base_contract_path)
        validate_document(base_contract, base_contract_path)
        source_result_changed = (
            base_contract["source_result_sha256"]
            != current_lineages["terrain"]["source_result_sha256"]
        )
        layer_reuse = reusable_layers(base_documents, current_lineages, source_result_changed)
        feature_semantic_changed = (
            base_contract.get("feature_set_semantic_sha256")
            != contract["feature_set_semantic_sha256"]
        )
        if removed_targets:
            layer_reuse = {
                layer: (False, sorted(set([*decision[1], "target_cells_removed"])))
                for layer, decision in layer_reuse.items()
            }
        if any(decision[0] for decision in layer_reuse.values()):
            overlay_bounds, changed_canonical_ids = derive_overlay_changes(
                base_contract["overlay_document"], overlay
            )
            provider_by_canonical = {
                canonical_feature_id(profile["identity_namespace"], item["provider_feature_id"]):
                    item["provider_feature_id"]
                for item in bundle.features
            }
            for document in base_documents.values():
                feature_path = base_root / document["feature_artifact"]["path"]
                for feature in read_json(feature_path)["features"]:
                    provider_by_canonical[feature["feature_id"]] = feature["source_refs"][0]["provider_feature_id"]
            unknown_overrides = sorted(changed_canonical_ids - set(provider_by_canonical))
            if unknown_overrides:
                raise CompilerError(
                    "overlay_rebase_required",
                    "Changed overlay target is absent from old and current source",
                    missing_feature_ids=unknown_overrides,
                )
            overlay_feature_ids = {provider_by_canonical[item] for item in changed_canonical_ids}
            current_dependencies = derive_raster_dependencies(
                repo_root(), bundle.raster, identifier, _terrain_grid(profile), targets
            )
        else:
            base_root = None
    effective_bounds = sorted(set([*change_bounds, *overlay_bounds]))
    effective_feature_ids = expand_water_change_ids(
        profile, set(changed_feature_ids) | overlay_feature_ids
    )
    if base_root:
        contract["base_reuse"] = {
            "decision": "reused",
            "reasons": sorted({reason for decision in layer_reuse.values() for reason in decision[1]}),
            "layers": {
                layer: {
                    "decision": "reused" if decision[0] else "full_rebuild",
                    "reasons": decision[1],
                }
                for layer, decision in layer_reuse.items()
            },
        }
        source_overlap_verified = (
            source_result_changed and layer_reuse["feature"][0] and feature_semantic_changed
        )
    elif base_documents:
        contract["base_reuse"] = {
            "decision": "full_rebuild",
            "reasons": sorted({reason for decision in layer_reuse.values() for reason in decision[1]}),
            "layers": {
                layer: {"decision": "full_rebuild", "reasons": decision[1]}
                for layer, decision in layer_reuse.items()
            },
        }
    contract["authoritative_change_scope"] = {
        "new_target_cells": [list(item) for item in sorted(new_targets)] if base_root else [],
        "terrain_bounds": [list(item) for item in effective_bounds],
        "provider_feature_ids": effective_feature_ids,
        "source_result_changed": source_result_changed,
        "source_overlap_verified": source_overlap_verified,
    }
    output_root = output_root_value.resolve() if output_root_value else default_output_root(
        profile, contract["run_inputs_hash"]
    )
    existing = existing_result(output_root, contract["run_inputs_hash"]) if output_root.exists() else None
    if existing:
        return existing, output_root
    staging = output_root.with_name(output_root.name + f".staging-{os.getpid()}")
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    try:
        write_json(staging / "run_contract.json", contract)
        all_ids = {cell_id(identifier, *cell) for cell in targets}
        if base_root is None:
            terrain_coordinates = set(targets)
            terrain = build_terrain_cells(
                repo_root(), bundle, identifier, _terrain_grid(profile), targets, overlay,
                staging / "scratch" / "terrain",
            )
            compiled = compile_features(
                bundle, profile, identifier, overlay, repo_root(), staging / "scratch" / "features",
                terrain=terrain,
            )
            terrain_rebuilt_ids = set(all_ids)
            feature_rebuilt_ids = set(all_ids)
        else:
            if layer_reuse["terrain"][0]:
                if current_dependencies is None:
                    raise CompilerError("dependency_index_missing", "Reusable terrain requires current dependencies")
                component_dirty = component_dirty_cells(
                    base_documents, current_dependencies.cell_components, identifier
                )
                terrain_coordinates = (
                    set(new_targets) | component_dirty | terrain_impact_cells(profile, effective_bounds)
                )
            else:
                terrain_coordinates = set(targets)
            terrain = load_base_terrain(base_root, profile, identifier)
            if terrain_coordinates:
                terrain.update(build_terrain_cells(
                    repo_root(),
                    bundle,
                    identifier,
                    _terrain_grid(profile),
                    sorted(terrain_coordinates),
                    overlay,
                    staging / "scratch" / "terrain",
                ))
            terrain_rebuilt_ids = {cell_id(identifier, *cell) for cell in terrain_coordinates}
            if not layer_reuse["feature"][0]:
                compiled = compile_features(
                    bundle,
                    profile,
                    identifier,
                    overlay,
                    repo_root(),
                    staging / "scratch" / "features",
                    terrain=terrain,
                )
                feature_rebuilt_ids = set(all_ids)
            elif feature_semantic_changed and not effective_feature_ids:
                compiled = compile_features(
                    bundle,
                    profile,
                    identifier,
                    overlay,
                    repo_root(),
                    staging / "scratch" / "features",
                    terrain=terrain,
                )
                feature_rebuilt_ids = feature_rebuild_cells(
                    base_root, compiled, profile, identifier
                )
            else:
                base_features = load_base_features(base_root, profile, identifier)
                selected_for_growth = select_source_features(
                    repo_root(), bundle.features, profile["grid"], new_targets,
                    staging / "scratch" / "selection",
                )
                changed_ids = set(expand_water_change_ids(profile, set(effective_feature_ids) | {
                    item["provider_feature_id"] for item in selected_for_growth
                }))
                base_source_groups = [
                    {
                        source_ref["provider_feature_id"]
                        for source_ref in feature["source_refs"]
                    }
                    for features in base_features.by_owner.values()
                    for feature in features
                ]
                changed_ids.update({
                    source_id
                    for source_group in base_source_groups
                    if source_group & changed_ids
                    for source_id in source_group
                })
                if changed_ids:
                    selected_features = [
                        item for item in bundle.features if item["provider_feature_id"] in changed_ids
                    ]
                    partial = compile_features(
                        bundle, profile, identifier, overlay, repo_root(),
                        staging / "scratch" / "features", selected_features, False, terrain,
                    )
                    replacement_ids = changed_ids | set(partial.processed_source_ids)
                    compiled, _ = merge_features(
                        base_features, replacement_ids, partial, profile, identifier, overlay,
                    )
                    feature_rebuilt_ids = feature_rebuild_cells(
                        base_root, compiled, profile, identifier
                    )
                else:
                    compiled = base_features
                    feature_rebuilt_ids = set()
            feature_rebuilt_ids.update(cell_id(identifier, *item) for item in new_targets)
        rebuilt_ids = terrain_rebuilt_ids | feature_rebuilt_ids
        reused_ids = all_ids - rebuilt_ids
        validation = build_validation_report(profile, bundle.ledger["policy_result"], identifier, terrain, compiled)
        artifacts, cells, feature_ids, manifest_updated_ids = write_cells(
            staging,
            profile,
            identifier,
            terrain,
            compiled,
            terrain_rebuilt_ids,
            feature_rebuilt_ids,
            base_root,
            current_lineages,
        )
        semantic_hash = canonical_hash({
            "grid_id": identifier,
            "features": sorted(
                (item for values in compiled.by_owner.values() for item in values),
                key=lambda item: item["feature_id"],
            ),
            "terrain_core": {key: value["core_samples"] for key, value in sorted(terrain.items())},
            "references": compiled.references,
            "rejections": compiled.rejections,
        })
        coverage = {
            "$schema": "https://alis.world/schemas/world-compiler/coverage-manifest-v1.json",
            "schema_version": 1,
            "profile_id": profile["profile_id"],
            "world_data_plugin": profile["world_data_plugin"],
            "engine_georeference_origin": profile["engine_georeference_origin"],
            "algorithm_version": profile.get("algorithm_version", "alis-world-compiler-1"),
            "grid_id": identifier,
            "grid": profile["grid"],
            "area_fingerprints": sorted({
                entry[lineage_name]["source_area"]["area_fingerprint"]
                for entry in cells
                for lineage_name in ("terrain_lineage", "feature_lineage")
            }),
            "cells": cells,
            "feature_count": len(feature_ids),
            "rejected_feature_count": len(compiled.rejections),
            "artifact_descriptors": artifacts,
            "semantic_hash": semantic_hash,
        }
        write_json(staging / "canonical" / "coverage.json", coverage)
        write_reports(
            staging,
            profile,
            bundle,
            overlay,
            compiled,
            validation,
            feature_ids,
            rebuilt_ids,
            reused_ids,
            base_root,
            terrain_rebuilt_ids,
            feature_rebuilt_ids,
            cells,
            manifest_updated_ids,
        )
        if (staging / "scratch").exists():
            shutil.rmtree(staging / "scratch")
        deterministic_paths = sorted(
            path for path in staging.rglob("*.json") if path.name not in {"metrics.json", "compile_result.json"}
        )
        metrics = {
            "$schema": "https://alis.world/schemas/world-compiler/metrics-report-v1.json",
            "schema_version": 1,
            "profile_id": profile["profile_id"],
            "determinism_level": "observational_excluded_from_d1",
            "duration_ms": int((time.perf_counter() - started) * 1000),
            "canonical_bytes": sum(path.stat().st_size for path in deterministic_paths),
            "cell_count": len(cells),
            "terrain_processed_cells": len(terrain_coordinates),
            "feature_processed_count": len(compiled.processed_source_ids),
            "feature_count": len(feature_ids),
            "rejection_count": len(compiled.rejections),
        }
        write_json(staging / "reports" / "metrics.json", metrics)
        outputs = [
            output_descriptor("compiler_document", path, staging)
            for path in sorted(staging.rglob("*.json"))
        ]
        result = {
            "$schema": RESULT_SCHEMA,
            "schema_version": 1,
            "operation_id": f"compile:{profile['profile_id']}:{contract['run_inputs_hash'][:12]}",
            "operation": "compile",
            "status": "accepted",
            "profile_id": profile["profile_id"],
            "inputs_hash": contract["run_inputs_hash"],
            "path_base": "output_root",
            "outputs": outputs,
            "errors": [],
        }
        write_json(staging / "compile_result.json", result)
        promote_staging(staging, output_root)
        return result, output_root
    except Exception:
        if staging.exists():
            shutil.rmtree(staging)
        raise


def plan_world(profile_value: str, source_result: Path | None = None) -> dict[str, Any]:
    root = compilation_root()
    selected_profile_path = profile_path(profile_value)
    profile = load_profile(selected_profile_path)
    overlay_path = resolve_owned_path(repo_root(), profile["authored_overlay"])
    overlay = read_json(overlay_path)
    validate_document(overlay, overlay_path)
    bundle = load_source_bundle(repo_root(), root, profile, source_result)
    contract = _run_contract(selected_profile_path, profile, overlay_path, bundle, None, [], [], overlay)
    return {
        "$schema": RESULT_SCHEMA,
        "schema_version": 1,
        "operation_id": f"plan:{profile['profile_id']}:{contract['run_inputs_hash'][:12]}",
        "operation": "plan",
        "status": "accepted",
        "profile_id": profile["profile_id"],
        "inputs_hash": contract["run_inputs_hash"],
        "path_base": "output_root",
        "outputs": [],
        "errors": [],
    }
