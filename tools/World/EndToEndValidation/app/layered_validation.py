from __future__ import annotations

from typing import Any

from .contracts import ValidationFailure


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
    require_incremental: bool = True,
) -> dict[str, Any]:
    required_legs = ("first", "second", "road_locality", "clean")
    if require_incremental:
        required_legs += ("incremental",)
    _require(
        all(leg in receipts for leg in required_legs),
        "layered_realization_leg_missing",
        "Layered realization is missing a required World leg",
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
            changes.get("road_sections") == 0
            and changes.get("building_sections") == 0,
            "layered_output_topology_mismatch",
            "Retired P0 section counters are nonzero",
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
            expected_artifact_count = (
                contract.get("producer_artifact_count", contract["artifact_count"])
                if leg in ("first", "clean") else contract["artifact_count"]
            )
            _require(
                record.get("generator_id") == contract["generator_id"]
                and record.get("generator_version") == contract["generator_version"]
                and record.get("artifact_root") == contract["artifact_root"]
                and len(record.get("canonical_inputs", [])) == contract["canonical_input_count"]
                and len(record.get("artifacts", [])) == expected_artifact_count,
                "layer_inventory_contract_mismatch",
                "Layer generator, root, inputs, or artifacts differ from the profile",
                leg=leg,
                layer_id=layer_id,
            )
            metrics = record.get("metrics")
            expected_metrics = contract["metrics"]
            _require(
                isinstance(metrics, dict)
                and set(metrics) == set(expected_metrics)
                and all(
                    metrics[key] == (value if leg in ("first", "clean") or not key.endswith("_written") else 0)
                    for key, value in expected_metrics.items()
                ),
                "layer_metrics_mismatch",
                "Layer metrics differ from the profile or rewrite work occurred on a no-op leg",
                leg=leg,
                layer_id=layer_id,
            )
        _require(
            len({record.get("normalized_layer_contract_sha256") for record in records.values()}) == 1,
            "layer_contract_hash_drift",
            "Normalized layer contract changed across one Matrix",
            layer_id=layer_id,
        )
        first_artifacts = records["first"].get("artifacts", [])
        first_paths = tuple(sorted(item.get("path") for item in first_artifacts))
        second_paths = tuple(sorted(item.get("path") for item in records["second"].get("artifacts", [])))
        mesh_terrain = layer_id == "terrain" and records["first"].get("generator_id") == "project_mesh_terrain"
        for leg, record in records.items():
            artifacts = record.get("artifacts", [])
            paths = tuple(sorted(item.get("path") for item in artifacts))
            if leg == "clean" and mesh_terrain:
                first_semantics = [item.get("semantic_sha256") for item in first_artifacts]
                clean_semantics = [item.get("semantic_sha256") for item in artifacts]
                _require(
                    all(isinstance(value, str) and len(value) == 64 for value in first_semantics + clean_semantics)
                    and sorted(first_semantics) == sorted(clean_semantics),
                    "layer_reconstruction_semantic_drift",
                    "Clean Mesh Terrain allocation changed base or partition semantic output",
                    layer_id=layer_id,
                )
            else:
                _require(
                    paths == (first_paths if not mesh_terrain or leg == "first" else second_paths),
                    "layer_artifact_path_churn",
                    "Layer artifact paths changed outside clean Mesh Terrain allocation",
                    layer_id=layer_id,
                    leg=leg,
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
            not require_incremental or
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

    return {
        "canonical_cells": topology["canonical_cells"],
        "road_locality_dirty_unit": road_dirty_unit,
        "layers": {
            layer_id: {
                "canonical_inputs": contract["canonical_input_count"],
                "artifacts": contract["artifact_count"],
                "metrics": contract["metrics"],
            }
            for layer_id, contract in sorted(layer_contracts.items())
        },
    }
