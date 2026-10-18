"""Authenticate both city proofs from the final shared Shipping candidate."""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

from release_performance import authenticate_artifacts, file_hash, performance_decision, read_json


CITIES = {
    "kazan": ("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory", "kazan_territory_512_1536_v1"),
    "manhattan": ("/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase", "manhattan_showcase_512_1536_v1"),
}
MATERIAL_MANIFEST = "Plugins/Resources/ProjectMaterial/Data/Manifests/Surfaces/accepted.surface-manifest.json"


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _number(value: object) -> float:
    _require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value),
             "Shipping evidence has an unknown numeric measurement")
    return float(value)


def _vector(value: object) -> list[float]:
    _require(isinstance(value, list) and len(value) == 3, "Shipping building ray vector is missing")
    return [_number(number) for number in value]


def _path(document: dict, key: str, root: Path) -> Path:
    recorded = document.get("artifacts", {}).get(key)
    _require(isinstance(recorded, str) and bool(recorded), f"Shipping artifact is missing: {key}")
    path = Path(recorded)
    return path if path.is_absolute() else root / path


def _same_path(recorded: object, expected: Path, root: Path) -> bool:
    if not isinstance(recorded, str) or not recorded:
        return False
    path = Path(recorded)
    return (path if path.is_absolute() else root / path).resolve() == expected.resolve()


def _building(correctness: dict, reference: dict, reference_hash: str, city: str) -> dict:
    evidence = correctness.get("building_surface_evidence", {})
    _require(evidence.get("schema") == "project-world-building-surface-evidence:v2" and
             evidence.get("status") == "accepted" and evidence.get("reference_sha256") == reference_hash and
             reference.get("schema") == "project-world-building-surface-reference:v1" and
             reference.get("status") == "accepted" and reference.get("map_package") == CITIES[city][0] and
             reference.get("runtime_profile_sha256") == correctness.get("runtime_profile_sha256") and
             evidence.get("reference_tolerance_cm") == 0.05, "Shipping building reference is unauthenticated")
    _require(evidence.get("capsule_query") == "UWorld::ComponentSweepMulti" and
             evidence.get("capsule_pawn_class") == "/Script/ProjectCharacter.DefinitionCharacter" and
             evidence.get("capsule_query_channel") == 2 and
             0 < _number(evidence.get("capsule_radius_cm")) <= _number(evidence.get("capsule_half_height_cm")),
             "Shipping building evidence lacks the actual player capsule query")
    samples, sources = evidence.get("samples"), reference.get("samples")
    _require(isinstance(samples, list) and isinstance(sources, list) and len(samples) == len(sources) == 6,
             "Shipping building wall/roof ray coverage is incomplete")
    keys, meshes, kinds = set(), set(), []
    maximum_reference, maximum_source = 0.0, 0.0
    for sample, source in zip(samples, sources, strict=True):
        _require(all(sample.get(key) == value for key, value in source.items()), "Shipping building source sample changed")
        mesh, kind = sample.get("mesh", ""), sample.get("kind")
        _require(isinstance(mesh, str) and mesh.startswith(CITIES[city][0].rsplit("/", 1)[0] + "/Buildings/") and
                 kind in ("wall", "roof") and (mesh, kind) not in keys and
                 isinstance(sample.get("source_triangle"), int) and sample["source_triangle"] >= 0,
                 "Shipping building named ray identity is invalid")
        keys.add((mesh, kind)); meshes.add(mesh); kinds.append(kind)
        cooked, expected, target = (_vector(sample.get(key)) for key in
                                    ("cooked_hit_cm", "reference_hit_cm", "source_target_cm"))
        reference_error, source_error = math.dist(cooked, expected), math.dist(cooked, target)
        _require(sample.get("ray_query_status") == "hit_named_component" and sample.get("ray_query_returned_hit") is True and
                 reference_error <= 0.05 and abs(reference_error - _number(sample.get("reference_error_cm"))) <= 1e-6 and
                 abs(source_error - _number(sample.get("cooked_source_error_cm"))) <= 1e-6,
                 "Shipping building cooked ray differs from its current source physics reference")
        contact = _vector(sample.get("capsule_hit_cm"))
        for key in ("cooked_hit_normal", "capsule_hit_normal"):
            normal = _vector(sample.get(key))
            _require(abs(sum(value * value for value in normal) - 1) <= 0.0001,
                     "Shipping building measured contact normal is invalid")
        _require(sample.get("capsule_query_status") == "blocked_named_component" and
                 sample.get("capsule_hit_mesh") == mesh and sample.get("capsule_blocking_hit") is True and
                 sample.get("capsule_start_penetrating") is False and
                 _number(sample.get("capsule_hit_count")) >= 1 and
                 _number(sample.get("capsule_other_blocking_hit_count")) == _number(sample.get("capsule_initial_penetration_count")) == 0 and
                 0 <= _number(sample.get("capsule_hit_time")) <= 1,
                 "Shipping building capsule did not block on the named surface")
        source_normal = _vector(source.get("normal"))
        for point, key in ((target, "capsule_source_plane_error_cm"), (expected, "capsule_reference_plane_error_cm")):
            distance = abs(sum((value - origin) * axis for value, origin, axis in zip(contact, point, source_normal, strict=True)))
            _require(abs(distance - _number(sample.get(key))) <= 1e-6,
                     "Shipping building capsule plane bias differs from the measured contact")
        maximum_reference, maximum_source = max(maximum_reference, reference_error), max(maximum_source, source_error)
    _require(len(meshes) == 3 and kinds.count("wall") == kinds.count("roof") == 3 and
             abs(maximum_reference - _number(evidence.get("maximum_reference_error_cm"))) <= 1e-6 and
             abs(maximum_source - _number(evidence.get("maximum_source_error_cm"))) <= 1e-6,
             "Shipping building aggregate differs from its named rays")
    return {"maximum_reference_error_cm": maximum_reference, "maximum_source_error_cm": maximum_source}


def _cache(correctness: dict, log: str, records: list[dict]) -> list[dict]:
    _require(correctness.get("material_texture_binding_scope") ==
             "engine_parameter_readback_not_rendered_consumption", "Shipping cache proof lacks explicit readback scope")
    budgets = re.findall(r"\[RuntimeCache::Initialize\] Fixed global_budget_bytes=(\d+)", log)
    _require(len(budgets) == 1 and int(budgets[0]) > 0, "Shipping cache global budget observation is missing")
    rows = []
    for line in log.splitlines():
        if "[RuntimeCache::ColdWarmProof]" in line:
            rows.append(dict(re.findall(r"(\w+)=([^\s]+)", line)))
    roles = {"terrain": "terrain.default", "roads": "road.default_asphalt", "buildings": "building.default_concrete"}
    measurements = []
    for role, binding in roles.items():
        accepted = [record for record in records if record.get("binding_id") == binding]
        _require(len(accepted) == 1, f"Shipping cache accepted material binding is missing: {role}")
        authority = accepted[0]
        consumers = [item for item in correctness.get("material_texture_bindings", []) if item.get("role") == role and
                     authority["output_object_path"] in item.get("material_chain", [])]
        primary = [item for item in consumers if item.get("texture") == authority["pattern_object_path"]]
        _require(primary and all(item.get("resource_ready") is True and _number(item.get("width")) > 0 and
                                _number(item.get("height")) > 0 for item in consumers),
                 f"Shipping cache bound live texture output is missing: {role}")
        bundle_package = authority["pattern_object_path"].partition(".")[0]
        bound_outputs = {item["texture"] for item in consumers if isinstance(item.get("texture"), str) and
                         item["texture"].partition(".")[0] == bundle_package}
        matches = [row for row in rows if row.get("identity") == authority["pattern_semantic_identity"] and
                   row.get("target") in bound_outputs]
        _require(matches, f"Shipping cache cold/warm generation proof is missing: {role}")
        for row in matches:
            try:
                numbers = {key: float(row[key]) for key in ("cold_request_to_ready_ms", "warm_request_to_ready_ms",
                    "cold_node_generations", "warm_node_generations", "warm_cache_hits", "same_resource", "warm_ready",
                    "resident_bytes", "peak_bytes")}
            except (KeyError, ValueError) as error:
                raise ValueError("Shipping cache measurement is malformed") from error
            _require(all(math.isfinite(value) and value >= 0 for value in numbers.values()) and
                     numbers["cold_node_generations"] > 0 and numbers["warm_node_generations"] == 0 and
                     numbers["warm_cache_hits"] > 0 and numbers["same_resource"] == numbers["warm_ready"] == 1 and
                     0 < numbers["resident_bytes"] <= numbers["peak_bytes"] <= int(budgets[0]),
                     f"Shipping cache warm reuse or resident budget failed: {role}")
            measurements.append({"role": role, "identity": row["identity"], "target": row["target"], **numbers})
    return measurements


def _water(document: dict, correctness: dict, root: Path, city: str) -> None:
    water = read_json(_path(document, "shipping_water_proof", root))
    authority = read_json(_path(document, "shipping_water_viewpoint_authority", root))
    temporal = read_json(_path(document, "shipping_water_temporal", root))
    _require(water.get("status") == "accepted" and water.get("build_configuration") == "Shipping" and
             all(water.get(key) == correctness.get(key) for key in
                 ("operation_id", "map_package", "runtime_profile", "runtime_profile_sha256")) and
             water.get("capture_encoding") == "visible_water_depth_mask_v1" and
             water.get("capture_source") == "SCS_SceneColorSceneDepth" and
             water.get("final_color_capture_source") == "SCS_FinalColorLDR" and
             all(water.get(key) is True for key in ("reference_captured", "repeat_captured", "final_color_captured",
                                                   "same_capture_session")) and
             0 <= _number(water.get("target_xy_error_cm")) <= _number(water.get("allowed_target_xy_error_cm")) == 25000 and
             authority.get("status") == "accepted" and authority.get("feature_class") == "water" and
             authority.get("feature_id") == ("alis:osm:relation:7493502" if city == "kazan" else "alis:osm:relation:2389611") and
             authority.get("target_unreal_xy_cm") == _vector(water.get("requested_water_target_cm"))[:2],
             f"Shipping native Water visibility/viewpoint contract failed: {city}")
    _require(temporal.get("status") == "accepted" and temporal.get("capture_encoding") == "visible_water_depth_mask_v1" and
             temporal.get("gates") == {"minimum_blue_pixels": 4096, "maximum_blue_classification_flip_ratio": 0.001,
                                      "maximum_mean_blue_channel_delta": 1.5} and not temporal.get("failures"),
             "Shipping Water temporal gate is missing or changed")
    metrics = temporal.get("metrics", {})
    _require(metrics.get("invalid_mask_pixels") == 0 and _number(metrics.get("blue_pixel_union")) >= 4096 and
             0 <= _number(metrics.get("blue_classification_flip_ratio")) <= 0.001 and
             0 <= _number(metrics.get("mean_blue_channel_delta")) <= 1.5,
             "Shipping Water visible pixel measurement failed")
    for arm in ("reference", "repeat"):
        key = f"shipping_water_{arm}"
        image = _path(document, key, root)
        _require(temporal.get(arm, {}).get("sha256") == document["artifact_sha256"][key] and
                 _same_path(temporal.get(arm, {}).get("path"), image, root) and
                 _same_path(water.get(f"{arm}_screenshot"), image, root),
                 "Shipping Water pixel receipt belongs to another capture")
    _require(_same_path(water.get("final_color_screenshot"), _path(document, "shipping_water_product", root), root),
             "Shipping Water product pixels belong to another capture")


def validate_shipping_cities(machine: dict, source_root: Path) -> dict:
    _require("manhattan_shipping_proof" in machine.get("artifacts", {}), "Manhattan Shipping proof is missing")
    authenticate_artifacts(machine, source_root)
    manhattan = read_json(_path(machine, "manhattan_shipping_proof", source_root))
    authenticate_artifacts(manhattan, source_root)
    materials = read_json(source_root / MATERIAL_MANIFEST).get("records", [])
    development = read_json(_path(machine, "manhattan_development_performance", source_root))
    authenticate_artifacts(development, source_root)
    result = {}
    for city, document in (("kazan", machine), ("manhattan", manhattan)):
        _require(all(document.get(key) == machine.get(key) and machine.get(key) is not None for key in
                     ("revision", "source_state_sha256", "shipping_executable_sha256", "shipping_package_sha256")) and
                 document.get("status") in ("accepted", "accepted_with_performance_waiver") and
                 document.get("shipping_exit_code") == document.get("shipping_water_exit_code") == 0,
                 f"Shipping city proof belongs to another candidate or failed: {city}")
        correctness = read_json(_path(document, "shipping_correctness", source_root))
        _require(correctness.get("status") == "accepted" and correctness.get("build_configuration") == "Shipping" and
                 correctness.get("material_texture_binding_scope") == "engine_parameter_readback_not_rendered_consumption" and
                 correctness.get("operation_id") == document.get("operation_id") and
                 correctness.get("map_package") == CITIES[city][0] and correctness.get("runtime_profile") == CITIES[city][1] and
                 correctness.get("runtime_profile_sha256") == document.get("runtime_profile_sha256") and
                 all(correctness.get(key) is True for key in ("project_loading_provenance", "possessed_player", "normal_movement",
                    "terrain_collision", "road_collision", "building_collision", "center_unloaded_at_edge", "edge_loaded",
                    "center_reloaded", "preview_flight_restored")) and
                 correctness.get("gameplay_interaction_required") is (city == "kazan") and
                 (city != "kazan" or correctness.get("gameplay_interaction") is True),
                 f"Shipping city gameplay/map/runtime proof failed: {city}")
        reference_path = _path(document, "shipping_building_reference", source_root)
        building = _building(correctness, read_json(reference_path), file_hash(reference_path), city)
        _water(document, correctness, source_root, city)
        _require(_same_path(correctness.get("screenshot"), _path(document, "shipping_product_screenshot", source_root), source_root),
                 "Shipping product viewport belongs to another process")
        dev_document = machine if city == "kazan" else development
        aggregate = (read_json(_path(machine, "development_performance_aggregate", source_root))
                     if city == "kazan" else development.get("performance", {}))
        performance_decision(aggregate, machine.get("status") == "accepted_with_performance_waiver")
        _require(all(aggregate.get(key) == machine.get(machine_key) and machine.get(machine_key) is not None
                     for key, machine_key in (("source_revision", "revision"), ("source_state_sha256", "source_state_sha256"),
                         ("development_package_sha256", "development_package_sha256"),
                         ("development_executable_sha256", "development_executable_sha256"))) and
                 aggregate.get("runtime_profile_sha256") == correctness.get("runtime_profile_sha256"),
                 "Shipping cache generation observation belongs to another source/package candidate")
        prefix = "development_run_01" if city == "kazan" else "run-01"
        dev_correctness = read_json(_path(dev_document, f"{prefix}_correctness", source_root))
        _require(dev_correctness.get("build_configuration") == "Development" and
                 all(dev_correctness.get(key) == correctness.get(key) for key in
                     ("map_package", "runtime_profile", "runtime_profile_sha256")), "Shipping cache observation belongs to another city")
        cache = _cache(dev_correctness, _path(dev_document, f"{prefix}_log", source_root).read_text(encoding="utf-8-sig"), materials)
        # Shipping readback must still bind the same accepted output paths proven generated in Development.
        for role in ("terrain", "roads", "buildings"):
            expected = {(item["parameter"], item["texture"]) for item in dev_correctness.get("material_texture_bindings", [])
                        if item.get("role") == role}
            actual = {(item.get("parameter"), item.get("texture")) for item in correctness.get("material_texture_bindings", [])
                      if item.get("role") == role and item.get("resource_ready") is True}
            _require(expected and expected == actual, "Shipping cache material outputs differ from measured Development bindings")
        result[city] = {**building, "cache_measurements": cache}
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(validate_shipping_cities(read_json(args.receipt), args.source_root)))
