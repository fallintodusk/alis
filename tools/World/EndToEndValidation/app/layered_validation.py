from __future__ import annotations

import re
from typing import Any

from .contracts import ValidationFailure


_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


def _is_sha256(value: object) -> bool:
    return isinstance(value, str) and _SHA256_PATTERN.fullmatch(value) is not None


def _require(condition: bool, code: str, message: str, **details: object) -> None:
    if not condition:
        raise ValidationFailure(code, message, **details)


def _inventory_by_id(receipt: dict[str, Any], leg: str) -> dict[str, dict[str, Any]]:
    inventories = receipt.get("layer_inventories", [])
    by_id = {
        item.get("layer_id"): item
        for item in inventories
        if isinstance(item, dict) and isinstance(item.get("layer_id"), str)
    }
    _require(
        len(by_id) == len(inventories),
        "layer_inventory_identity_invalid",
        "Layer inventory IDs are missing or duplicated",
        leg=leg,
    )
    return by_id


def _dependent_layer_closure(
    inventories: dict[str, dict[str, Any]], root_layer: str
) -> set[str]:
    closure = {root_layer}
    while True:
        previous_size = len(closure)
        for layer_id, record in inventories.items():
            dependencies = {
                unit_id[len("layer:"):-len(":contract")]
                for item in record.get("dependency_inputs", [])
                if isinstance(item, dict)
                and isinstance((unit_id := item.get("unit_id")), str)
                and unit_id.startswith("layer:")
                and unit_id.endswith(":contract")
            }
            if dependencies.intersection(closure):
                closure.add(layer_id)
        if len(closure) == previous_size:
            return closure


def validate_layered_realization(
    receipts: dict[str, dict[str, Any]],
    expected: dict[str, Any],
    realization_profile: dict[str, str],
    road_dirty_unit: str,
) -> dict[str, Any]:
    required_legs = ("first", "second", "road_locality", "incremental", "clean")
    _require(
        all(leg in receipts for leg in required_legs),
        "layered_realization_leg_missing",
        "Layered realization requires full, no-op, road-locality, incremental, and reconstruction evidence",
    )
    topology = expected["expected_topology"]
    layer_contracts = {item["layer_id"]: item for item in expected["expected_layers"]}
    _require(
        len(layer_contracts) == len(expected["expected_layers"]),
        "layer_expectation_identity_invalid",
        "Expected layer IDs are duplicated",
    )

    inventories: dict[str, dict[str, dict[str, Any]]] = {}
    for leg in required_legs:
        receipt = receipts[leg]
        _require(
            receipt.get("realization_profile") == realization_profile["profile_id"]
            and receipt.get("realization_profile_sha256") == realization_profile["sha256"],
            "realization_profile_mismatch",
            "Unreal evidence does not belong to the pinned realization profile",
            leg=leg,
        )
        _require(
            receipt.get("canonical_cell_count") == topology["canonical_cells"]
            and receipt.get("sample_spacing_m")
            == [topology["sample_spacing_m"], topology["sample_spacing_m"]]
            and receipt.get("world_partition") is True,
            "layered_topology_mismatch",
            "Canonical cells, sample spacing, or World Partition differ from the profile",
            leg=leg,
        )
        _require(
            receipt.get("georeferencing_placement_error_m", float("inf"))
            <= topology["maximum_georeferencing_error_m"],
            "layered_georeferencing_error",
            "GeoReferencing placement exceeds the profile tolerance",
            leg=leg,
        )
        # Elevation. GeoReferencing above proves XY placement only. These fields describe
        # the Generated Base SOURCE edit layer, which is an input to UE's edit-layer blend --
        # not the final composed heightmap that renders, collides, and sets component bounds.
        # The Kazan defect had a correct source layer and a flat final surface, so source
        # evidence alone must never accept terrain.
        terrain_samples = receipt.get("terrain_source_height_sample_count")
        terrain_expected = receipt.get("terrain_source_height_expected_sample_count")
        _require(
            isinstance(terrain_samples, int)
            and isinstance(terrain_expected, int)
            and terrain_expected > 0
            and terrain_samples == terrain_expected,
            "layered_terrain_source_height_coverage",
            "Realized terrain did not compare every canonical elevation sample",
            leg=leg,
        )
        _require(
            receipt.get("terrain_source_height_mismatch_count") == 0,
            "layered_terrain_source_height_mismatch",
            "Generated Base source layer does not match canonical elevation",
            leg=leg,
        )
        _require(
            receipt.get("terrain_source_height_max_error_m", float("inf"))
            <= receipt.get("terrain_source_height_tolerance_m", 0.0),
            "layered_terrain_source_height_error",
            "Source elevation error exceeds the admitted quantization tolerance",
            leg=leg,
        )
        _require(
            _is_sha256(receipt.get("terrain_source_height_semantic_sha256")),
            "layered_terrain_source_height_identity_invalid",
            "Source terrain height identity is not a valid SHA-256",
            leg=leg,
        )
        # ACCEPTANCE surface: the blended final/base heightmap that renders and collides.
        # Deliberately metric-based, never a "verified" boolean - the broken Kazan artifact
        # measured cleanly while every one of its 215,040 samples was wrong.
        final_samples = receipt.get("terrain_final_height_sample_count")
        final_expected = receipt.get("terrain_final_height_expected_sample_count")
        _require(
            isinstance(final_samples, int)
            and isinstance(final_expected, int)
            and final_expected > 0
            and final_samples == final_expected,
            "layered_terrain_final_height_coverage",
            "Final heightmap did not compare every canonical elevation sample",
            leg=leg,
        )
        _require(
            receipt.get("terrain_final_height_mismatch_count") == 0,
            "layered_terrain_final_height_mismatch",
            "Final composed Landscape heightmap does not match canonical elevation",
            leg=leg,
        )
        _require(
            receipt.get("terrain_final_height_max_error_m", float("inf"))
            <= receipt.get("terrain_final_height_tolerance_m", 0.0),
            "layered_terrain_final_height_error",
            "Final elevation error exceeds the admitted quantization tolerance",
            leg=leg,
        )
        _require(
            _is_sha256(receipt.get("terrain_final_height_semantic_sha256")),
            "layered_terrain_final_height_identity_invalid",
            "Final terrain height identity is not a valid SHA-256",
            leg=leg,
        )
        _require(
            receipt.get("hlod_proxy_actor_count") == 0
            and receipt.get("hlod_layer_reference_count") == 0
            and receipt.get("hlod_eligible_generated_actor_count") == 0,
            "layered_hlod_output_present",
            "Production layered realization must contain zero HLOD output",
            leg=leg,
        )
        changes = receipt.get("changes", {})
        _require(
            changes.get("landscape_components") == topology["landscape_components"]
            and changes.get("landscape_proxies") == topology["landscape_proxies"]
            and changes.get("water_cell_actors") == topology["water_cell_actors"]
            and changes.get("water_mesh_assets") == topology["water_mesh_assets"]
            and changes.get("road_cell_actors") == topology["road_cell_actors"]
            and changes.get("road_mesh_assets") == topology["road_mesh_assets"]
            and changes.get("vegetation_cell_actors") == topology["vegetation_cell_actors"]
            and changes.get("vegetation_components") == topology["vegetation_components"]
            and changes.get("vegetation_instances") == topology["vegetation_instances"]
            and changes.get("vegetation_candidates") == topology["vegetation_candidates"]
            and changes.get("vegetation_road_exclusions") == topology["vegetation_road_exclusions"]
            and changes.get("vegetation_water_exclusions") == topology["vegetation_water_exclusions"]
            and changes.get("vegetation_authored_mask_exclusions")
            == topology["vegetation_authored_mask_exclusions"]
            and changes.get("building_cell_actors") == topology["building_cell_actors"]
            and changes.get("building_mesh_assets") == topology["building_mesh_assets"]
            and changes.get("building_triangles") == topology["building_triangles"]
            and changes.get("building_candidate_fragments") == topology["building_candidate_fragments"]
            and changes.get("building_accepted_fragments") == topology["building_accepted_fragments"]
            and changes.get("building_duplicate_fragments") == topology["building_duplicate_fragments"]
            and changes.get("building_contained_fragments") == topology["building_contained_fragments"]
            and changes.get("building_conflict_fragments") == topology["building_conflict_fragments"]
            and changes.get("building_malformed_fragments") == topology["building_malformed_fragments"]
            and changes.get("building_authored_mask_exclusions")
            == topology["building_authored_mask_exclusions"]
            and changes.get("gameplay_placement_actors")
            == topology["gameplay_placement_actors"]
            and changes.get("road_sections") == 0
            and changes.get("building_sections") == 0,
            "layered_output_topology_mismatch",
            "Landscape, water, road, vegetation, or building topology differs from the layered profile",
            leg=leg,
        )
        inventories[leg] = _inventory_by_id(receipt, leg)
        _require(
            set(inventories[leg]) == set(layer_contracts),
            "layer_inventory_set_mismatch",
            "Realization produced an unexpected layer set",
            leg=leg,
        )

    road_locality_layers = _dependent_layer_closure(inventories["second"], "roads")
    for layer_id, contract in layer_contracts.items():
        records = {leg: inventories[leg][layer_id] for leg in required_legs}
        for leg, record in records.items():
            _require(
                record.get("generator_id") == contract["generator_id"]
                and record.get("generator_version") == contract["generator_version"]
                and record.get("artifact_root") == contract["artifact_root"]
                and len(record.get("canonical_inputs", [])) == contract["canonical_input_count"]
                and len(record.get("artifacts", [])) == contract["artifact_count"],
                "layer_inventory_contract_mismatch",
                "Layer generator, root, inputs, or artifacts differ from the profile",
                leg=leg,
                layer_id=layer_id,
            )
        _require(
            len({record.get("normalized_layer_contract_sha256") for record in records.values()}) == 1,
            "layer_contract_hash_drift",
            "Normalized layer contract changed across one Matrix",
            layer_id=layer_id,
        )
        artifact_paths = [
            tuple(sorted(item.get("path") for item in record.get("artifacts", [])))
            for record in records.values()
        ]
        _require(
            all(paths == artifact_paths[0] for paths in artifact_paths[1:]),
            "layer_artifact_path_churn",
            "Layer artifact paths changed across no-op or reconstruction",
            layer_id=layer_id,
        )
        _require(
            records["first"].get("final_dirty_units") == ["*"],
            "layer_initial_full_build_unproven",
            "Initial layered realization was not a full build",
            layer_id=layer_id,
        )
        _require(
            records["second"].get("final_dirty_units") == [],
            "layer_noop_dirty",
            "Unchanged layered realization retained dirty units",
            layer_id=layer_id,
        )
        _require(
            len(records["incremental"].get("final_dirty_units", [])) == contract["incremental_dirty_count"],
            "layer_incremental_dirty_mismatch",
            "Incremental dirty closure differs from the profile",
            layer_id=layer_id,
        )
        locality_dirty = records["road_locality"].get("final_dirty_units")
        if layer_id in road_locality_layers:
            _require(
                locality_dirty == [road_dirty_unit],
                "road_locality_dirty_mismatch",
                "Road-locality evidence did not reevaluate the selected unit through its dependency closure",
                layer_id=layer_id,
            )
        else:
            _require(
                locality_dirty == [],
                "road_locality_sibling_dirty",
                "Road-locality evidence dirtied a sibling generated layer",
                layer_id=layer_id,
            )
        if layer_id != "roads":
            _require(
                records["road_locality"].get("artifacts") == records["second"].get("artifacts"),
                "road_locality_sibling_artifact_drift",
                "Road-locality evidence changed sibling artifact bytes or paths",
                layer_id=layer_id,
            )

    for leg in ("first", "clean"):
        changes = receipts[leg]["changes"]
        _require(
            changes.get("updated_landscape_components") == topology["landscape_components"]
            and changes.get("water_triangles") == topology["water_triangles"],
            "layered_full_rebuild_unproven",
            "Full or reconstructed layered output did not rebuild exact terrain and water",
            leg=leg,
        )
        _require(
            changes.get("road_triangles") == topology["road_triangles"]
            and changes.get("vegetation_instance_rewrites") == topology["vegetation_instances"]
            and changes.get("building_triangle_rewrites") == topology["building_triangles"]
            and changes.get("gameplay_placement_rewrites") == topology["gameplay_placement_actors"],
            "layered_road_rebuild_unproven",
            "Full or reconstructed layered output did not rebuild exact roads and vegetation",
            leg=leg,
        )
    for leg in ("second", "road_locality", "incremental"):
        changes = receipts[leg]["changes"]
        _require(
            changes.get("updated_landscape_components") == 0
            and changes.get("water_triangles") == 0
            and changes.get("road_triangles") == 0
            and changes.get("vegetation_instance_rewrites") == 0
            and changes.get("building_triangle_rewrites") == 0
            and changes.get("gameplay_placement_rewrites") == 0,
            "layered_noop_rewrite",
            "No-op or content-identical incremental work rewrote layered geometry",
            leg=leg,
        )

    # Content-identical legs must realize byte-identical terrain. Reconstruction drift here
    # means the same canonical authority produced different elevation.
    for surface, key, code in (
        ("source", "terrain_source_height_semantic_sha256",
         "layered_terrain_source_height_identity_drift"),
        ("final", "terrain_final_height_semantic_sha256",
         "layered_terrain_final_height_identity_drift"),
    ):
        identities = {leg: receipts[leg].get(key) for leg in required_legs}
        _require(
            len(set(identities.values())) == 1,
            code,
            f"Content-identical legs realized different {surface} terrain elevation",
            identities=identities,
        )

    return {
        "canonical_cells": topology["canonical_cells"],
        "landscape_proxies": topology["landscape_proxies"],
        "water_cell_actors": topology["water_cell_actors"],
        "water_triangles": topology["water_triangles"],
        "road_cell_actors": topology["road_cell_actors"],
        "road_triangles": topology["road_triangles"],
        "vegetation_cell_actors": topology["vegetation_cell_actors"],
        "vegetation_components": topology["vegetation_components"],
        "vegetation_instances": topology["vegetation_instances"],
        "vegetation_candidates": topology["vegetation_candidates"],
        "vegetation_road_exclusions": topology["vegetation_road_exclusions"],
        "vegetation_water_exclusions": topology["vegetation_water_exclusions"],
        "vegetation_authored_mask_exclusions": topology["vegetation_authored_mask_exclusions"],
        "building_cell_actors": topology["building_cell_actors"],
        "building_triangles": topology["building_triangles"],
        "building_candidate_fragments": topology["building_candidate_fragments"],
        "building_accepted_fragments": topology["building_accepted_fragments"],
        "building_duplicate_fragments": topology["building_duplicate_fragments"],
        "building_contained_fragments": topology["building_contained_fragments"],
        "building_conflict_fragments": topology["building_conflict_fragments"],
        "building_malformed_fragments": topology["building_malformed_fragments"],
        "gameplay_placement_actors": topology["gameplay_placement_actors"],
        "road_locality_dirty_unit": road_dirty_unit,
        "terrain_source_height_sample_count": receipts["first"].get("terrain_source_height_sample_count"),
        "terrain_source_height_max_error_m": receipts["first"].get("terrain_source_height_max_error_m"),
        "terrain_source_relief_m": receipts["first"].get("terrain_source_relief_m"),
        "terrain_source_height_semantic_sha256": receipts["first"].get("terrain_source_height_semantic_sha256"),
        "terrain_final_height_mismatch_count": receipts["first"].get("terrain_final_height_mismatch_count"),
        "terrain_final_relief_m": receipts["first"].get("terrain_final_relief_m"),
        "terrain_final_height_semantic_sha256": receipts["first"].get("terrain_final_height_semantic_sha256"),
        "layers": {
            layer_id: {
                "canonical_inputs": contract["canonical_input_count"],
                "artifacts": contract["artifact_count"],
            }
            for layer_id, contract in sorted(layer_contracts.items())
        },
    }
