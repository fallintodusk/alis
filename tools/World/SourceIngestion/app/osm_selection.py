from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, run_tool

from .contracts import canonical_hash, file_hash


_LAYERS = ("points", "lines", "multipolygons")


def _run_stage(repo_root: Path, stage: str, tool: str, arguments: list[str]) -> str:
    try:
        return run_tool(repo_root, tool, arguments)
    except ExecutionEnvironmentError as error:
        raise ExecutionEnvironmentError(error.code, str(error), stage=stage, **error.details) from error


def _tag_value(properties: dict[str, Any], key: str) -> str | None:
    direct = properties.get(key)
    if direct is not None:
        return str(direct)
    pattern = re.compile(rf'(?:^|,)"{re.escape(key)}"=>"((?:\\.|[^"])*)"')
    match = pattern.search(properties.get("other_tags") or "")
    return match[1] if match else None


def _matches_filters(properties: dict[str, Any], object_type: str, filters: list[str]) -> bool:
    for expression in filters:
        object_types, separator, predicate = expression.partition("/")
        if not separator or object_type not in object_types:
            continue
        key, equals, values = predicate.partition("=")
        value = _tag_value(properties, key)
        if value is None:
            continue
        if not equals or value in values.split(","):
            return True
    return False


def _provider_id(layer: str, properties: dict[str, Any]) -> tuple[str, str] | None:
    if layer == "points":
        return "n", str(properties["osm_id"])
    if layer == "lines":
        return "w", str(properties["osm_id"])
    way_id = properties.get("osm_way_id")
    if way_id not in {None, ""}:
        return "w", str(way_id)
    relation_id = properties.get("osm_id")
    return ("r", str(relation_id)) if relation_id not in {None, ""} else None


def _select_layer_ids(
    repo_root: Path,
    source_path: Path,
    layer: str,
    filters: list[str],
    bbox: list[float],
    output_path: Path,
) -> set[str]:
    _run_stage(repo_root, f"select_{layer}", "ogr2ogr", [
        "-overwrite", "-f", "GeoJSON", "-spat", *(str(value) for value in bbox),
        "-spat_srs", "EPSG:4326", str(output_path), str(source_path), layer,
    ])
    document = json.loads(output_path.read_text(encoding="utf-8"))
    selected = set()
    for feature in document.get("features", []):
        properties = feature.get("properties", {})
        provider_id = _provider_id(layer, properties)
        if provider_id and _matches_filters(properties, provider_id[0], filters):
            selected.add("".join(provider_id))
    return selected


def select_complete_geometries(
    repo_root: Path,
    source_path: Path,
    filters: list[str],
    bbox: list[float],
    output_root: Path,
) -> tuple[Path, Path, dict[str, Any]]:
    output_root.mkdir(parents=True, exist_ok=True)
    candidate_source = output_root / "candidate_complete.osm.pbf"
    candidate_paths = {layer: output_root / f"candidate_{layer}.geojson" for layer in _LAYERS}
    membership_ids = output_root / "selected_membership.ids"
    selected_pbf = output_root / "selected_complete.osm.pbf"
    complete_geojson = output_root / "selected_complete.geojson"
    intersecting_geojson = output_root / "selected_intersecting.geojson"
    for generated in (
        candidate_source, *candidate_paths.values(), membership_ids,
        selected_pbf, complete_geojson, intersecting_geojson,
    ):
        generated.unlink(missing_ok=True)

    _run_stage(repo_root, "filter_complete_candidates", "osmium", [
        "tags-filter", "-o", str(candidate_source), str(source_path), *filters,
    ])
    selected_ids: set[str] = set()
    for layer, path in candidate_paths.items():
        selected_ids.update(_select_layer_ids(repo_root, candidate_source, layer, filters, bbox, path))
    if not selected_ids:
        raise ExecutionEnvironmentError("empty_osm_membership", "Exact OSM membership selected no features")
    membership_ids.write_text("".join(f"{value}\n" for value in sorted(selected_ids)), encoding="ascii")
    candidate_hashes = {layer: file_hash(path) for layer, path in candidate_paths.items()}

    _run_stage(repo_root, "close_selected_references", "osmium", [
        "getid", "-r", "-t", "-i", str(membership_ids), "-o", str(selected_pbf), str(source_path),
    ])
    reference_check = _run_stage(repo_root, "validate_selected_references", "osmium", [
        "check-refs", "-r", str(selected_pbf),
    ])
    _run_stage(repo_root, "export_selected_geometries", "osmium", [
        "export", "-u", "type_id", "-a", "id,type,version,timestamp,changeset",
        "-f", "geojson", "-o", str(complete_geojson), str(selected_pbf),
    ])
    _run_stage(repo_root, "validate_final_intersection", "ogr2ogr", [
        "-overwrite", "-f", "GeoJSON", "-spat", *(str(value) for value in bbox),
        "-spat_srs", "EPSG:4326", str(intersecting_geojson), str(complete_geojson),
    ])
    evidence = {
        "membership_contract_id": canonical_hash({
            "version": 4,
            "source": "full_pinned_snapshot",
            "filters": filters,
            "tag_selection": "osmium_tags_filter_with_reference_completion",
            "geometry_selection": "ogr_layer_closed_bbox_intersects",
            "reference_completion": "osmium_getid_recursive_from_exact_membership",
            "reference_validation": "osmium_check_refs_relations",
            "bbox": bbox,
        }),
        "bbox": bbox,
        "filters": filters,
        "candidate_layers_sha256": candidate_hashes,
        "membership_ids_sha256": file_hash(membership_ids),
        "selected_membership_count": len(selected_ids),
        "selected_pbf_sha256": file_hash(selected_pbf),
        "intersecting_geojson_sha256": file_hash(intersecting_geojson),
        "reference_check": reference_check.strip(),
    }
    for transient in (candidate_source, *candidate_paths.values(), membership_ids, complete_geojson):
        transient.unlink()
    return selected_pbf, intersecting_geojson, evidence
