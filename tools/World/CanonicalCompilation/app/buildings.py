from __future__ import annotations

import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, require_tools, run_tool

from .adapters import _building_attributes, canonical_feature_id
from .contracts import CompilerError
from .geometry import geometry_rejection_reason


@dataclass(frozen=True)
class PreparedBuildings:
    source_features: list[dict[str, Any]]
    rejections: list[dict[str, str]]
    excluded_counts: dict[str, int]
    consumed_source_ids: list[str]


def _polygons(geometry: dict[str, Any]) -> list[list[list[list[float]]]]:
    return geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]


def _combined_geometry(geometries: list[dict[str, Any]]) -> dict[str, Any]:
    values = [polygon for geometry in geometries for polygon in _polygons(geometry)]
    return {
        "type": "Polygon" if len(values) == 1 else "MultiPolygon",
        "coordinates": values[0] if len(values) == 1 else values,
    }


def _orientation(a: list[float], b: list[float], c: list[float]) -> float:
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def _on_segment(a: list[float], b: list[float], point: list[float]) -> bool:
    return _orientation(a, b, point) == 0 and (
        min(a[0], b[0]) <= point[0] <= max(a[0], b[0])
        and min(a[1], b[1]) <= point[1] <= max(a[1], b[1])
    )


def _point_in_ring(point: list[float], ring: list[list[float]]) -> bool:
    inside = False
    for start, end in zip(ring, ring[1:], strict=False):
        if _on_segment(start, end, point):
            return True
        if (start[1] > point[1]) != (end[1] > point[1]):
            crossing = start[0] + (point[1] - start[1]) * (end[0] - start[0]) / (end[1] - start[1])
            if crossing > point[0]:
                inside = not inside
    return inside


def _contains(container: dict[str, Any], child: dict[str, Any]) -> bool:
    containers = _polygons(container)
    return all(
        any(
            all(
                _point_in_ring(point, candidate[0])
                and not any(_point_in_ring(point, hole) for hole in candidate[1:])
                for point in child_polygon[0][:-1]
            )
            for candidate in containers
        )
        for child_polygon in _polygons(child)
    )


def _ring_area(ring: list[list[float]]) -> float:
    return abs(sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(ring, ring[1:], strict=False))) * 0.5


def _area(geometry: dict[str, Any]) -> float:
    return sum(
        _ring_area(polygon[0]) - sum(_ring_area(hole) for hole in polygon[1:])
        for polygon in _polygons(geometry)
    )


def _fixture_containers(
    projected: dict[str, dict[str, Any]],
    outlines: set[str],
    parts: set[str],
) -> dict[str, list[str]]:
    return {
        part: sorted(
            outline
            for outline in outlines
            if outline != part and _contains(projected[outline], projected[part])
        )
        for part in sorted(parts)
    }


def _write_spatial_features(
    path: Path,
    identities: set[str],
    projected: dict[str, dict[str, Any]],
) -> None:
    path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"source_id": identity},
                "geometry": projected[identity],
            }
            for identity in sorted(identities)
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")


def _production_containers(
    repo_root: Path,
    scratch_root: Path,
    canonical_crs: str,
    projected: dict[str, dict[str, Any]],
    outlines: set[str],
    parts: set[str],
) -> dict[str, list[str]]:
    if not outlines or not parts:
        return {part: [] for part in parts}
    scratch_root.mkdir(parents=True, exist_ok=True)
    outlines_path = scratch_root / "outlines.geojson"
    parts_path = scratch_root / "parts.geojson"
    database_path = scratch_root / "association.gpkg"
    result_path = scratch_root / "association.json"
    _write_spatial_features(outlines_path, outlines, projected)
    _write_spatial_features(parts_path, parts, projected)
    try:
        require_tools(repo_root)
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GPKG", str(database_path), str(outlines_path),
            "-nln", "outlines", "-a_srs", canonical_crs,
            "-lco", "SPATIAL_INDEX=YES",
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-update", "-f", "GPKG", str(database_path), str(parts_path),
            "-nln", "parts", "-a_srs", canonical_crs,
            "-lco", "SPATIAL_INDEX=YES",
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GeoJSON", str(result_path), str(database_path),
            "-dialect", "SQLite", "-sql",
            "SELECT p.source_id AS part_id, o.source_id AS outline_id "
            "FROM parts p JOIN outlines o ON p.source_id <> o.source_id "
            "AND o.fid IN (SELECT id FROM rtree_outlines_geom "
            "WHERE maxx >= ST_MinX(p.geom) AND minx <= ST_MaxX(p.geom) "
            "AND maxy >= ST_MinY(p.geom) AND miny <= ST_MaxY(p.geom)) "
            "AND ST_Covers(o.geom, p.geom)",
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    try:
        output = json.loads(result_path.read_text(encoding="utf-8"))
        containers = {part: [] for part in parts}
        for item in output["features"]:
            properties = item["properties"]
            containers[properties["part_id"]].append(properties["outline_id"])
        return {key: sorted(set(value)) for key, value in containers.items()}
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise CompilerError("building_association_failed", "Pinned OGR building association is invalid") from exc


def _coverage_complete_fixture(
    outline_geometries: list[dict[str, Any]],
    part_geometries: list[dict[str, Any]],
    tolerance: float,
) -> bool:
    outline = _combined_geometry(outline_geometries)
    return (
        all(
            any(_point_in_ring(point, polygon[0]) for geometry in part_geometries for polygon in _polygons(geometry))
            for polygon in _polygons(outline)
            for point in polygon[0][:-1]
        )
        and sum(_area(value) for value in part_geometries) >= _area(outline) - tolerance
    )


def _production_complete_groups(
    repo_root: Path,
    scratch_root: Path,
    canonical_crs: str,
    groups: dict[str, tuple[set[str], set[str]]],
    projected: dict[str, dict[str, Any]],
    tolerance: float,
) -> set[str]:
    eligible = {key: value for key, value in groups.items() if value[0] and value[1]}
    if not eligible:
        return set()
    scratch_root.mkdir(parents=True, exist_ok=True)
    source_path = scratch_root / "coverage.geojson"
    database_path = scratch_root / "coverage.gpkg"
    unions_path = scratch_root / "coverage_unions.gpkg"
    result_path = scratch_root / "coverage_result.json"
    source_path.write_text(json.dumps({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"logical_id": logical_id, "role": role},
                "geometry": projected[identity],
            }
            for logical_id, (outlines, parts) in sorted(eligible.items())
            for role, identities in (("outline", outlines), ("part", parts))
            for identity in sorted(identities)
        ],
    }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    try:
        require_tools(repo_root)
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GPKG", str(database_path), str(source_path),
            "-nln", "coverage", "-a_srs", canonical_crs,
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GPKG", str(unions_path), str(database_path),
            "-dialect", "SQLite", "-nln", "unions", "-sql",
            "SELECT logical_id, role, ST_Union(geom) AS geom "
            "FROM coverage GROUP BY logical_id, role",
        ])
        run_tool(repo_root, "ogr2ogr", [
            "-overwrite", "-f", "GeoJSON", str(result_path), str(unions_path),
            "-dialect", "SQLite", "-sql",
            "SELECT o.logical_id, o.geom AS geom, ST_Area(o.geom) AS outline_area, "
            "COALESCE(ST_Area(ST_Difference(o.geom, p.geom)), 0.0) AS uncovered_area "
            "FROM unions o JOIN unions p ON o.logical_id = p.logical_id "
            "WHERE o.role = 'outline' AND p.role = 'part'",
        ])
    except ExecutionEnvironmentError as exc:
        raise CompilerError(exc.code, str(exc), **exc.details) from exc
    try:
        result = json.loads(result_path.read_text(encoding="utf-8"))
        return {
            item["properties"]["logical_id"]
            for item in result["features"]
            if float(item["properties"]["outline_area"]) > 0.0
            and float(item["properties"]["uncovered_area"]) <= tolerance
        }
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise CompilerError(
            "building_coverage_failed",
            "Pinned OGR building coverage output is invalid",
            parser_error=str(exc),
        ) from exc


def _vertical_valid(attributes: dict[str, Any], maximum_height: float) -> bool:
    minimum = attributes.get("min_height_m", 0.0)
    height = attributes.get("height_m", 0.0)
    return 0.0 <= minimum < height <= maximum_height


def prepare_buildings(
    source_features: list[dict[str, Any]],
    projected: dict[str, dict[str, Any]],
    namespace: str,
    snapshot: dict[str, Any],
    settings: dict[str, Any],
    grid: dict[str, Any],
    repo_root: Path,
    scratch_root: Path,
    industrial_rejections: set[str],
    target_source_ids: set[str] | None = None,
) -> PreparedBuildings:
    buildings = {
        item["provider_feature_id"]: item
        for item in source_features
        if item["provider_class"] == "building"
    }
    rejected_geometry = {
        identity
        for identity, feature in buildings.items()
        if identity in industrial_rejections
        or identity not in projected
        or geometry_rejection_reason(projected[identity], "building", check_self_intersections=False)
    }
    usable = set(buildings) - rejected_geometry
    outlines = {identity for identity in usable if buildings[identity]["properties"].get("building") not in {None, ""}}
    parts = {identity for identity in usable if buildings[identity]["properties"].get("building:part") not in {None, ""}}
    relation_outlines: dict[str, set[str]] = defaultdict(set)
    relation_parts: dict[str, set[str]] = defaultdict(set)
    for identity in usable:
        for membership in buildings[identity].get("relation_memberships", []):
            relation_id = membership["provider_relation_id"]
            if membership["role"] == "outline" and identity in outlines:
                relation_outlines[relation_id].add(identity)
            elif membership["role"] == "part" and identity in parts:
                relation_parts[relation_id].add(identity)

    outline_relations = {
        identity: {relation for relation, values in relation_outlines.items() if identity in values}
        for identity in outlines
    }
    ambiguous_relation_outlines = {
        identity for identity, relations in outline_relations.items() if len(relations) > 1
    }
    invalid_ambiguous_relations = {
        relation
        for identity in ambiguous_relation_outlines
        for relation in outline_relations[identity]
    }
    valid_relations = {
        relation
        for relation, values in relation_outlines.items()
        if values and relation not in invalid_ambiguous_relations
    }
    part_relations = {
        identity: {relation for relation, values in relation_parts.items() if identity in values}
        for identity in parts
    }
    ambiguous_parts = {
        identity
        for identity, relations in part_relations.items()
        if len(relations) > 1 or any(relation in invalid_ambiguous_relations for relation in relations)
    }
    missing_outline_parts = {
        identity
        for identity, relations in part_relations.items()
        if len(relations) == 1 and not (relations & valid_relations) and identity not in ambiguous_parts
    }
    logical_outlines: dict[str, set[str]] = {identity: {identity} for identity in outlines}
    outline_to_logical = {identity: identity for identity in outlines}
    for relation_id in sorted(valid_relations):
        members = relation_outlines.get(relation_id, set())
        if not members:
            continue
        for identity in members:
            previous = outline_to_logical.get(identity)
            if previous is not None:
                logical_outlines.pop(previous, None)
            outline_to_logical[identity] = relation_id
        logical_outlines[relation_id] = set(members)

    explicit_parts: dict[str, set[str]] = defaultdict(set)
    assigned_parts: set[str] = set()
    for relation_id, values in relation_parts.items():
        if relation_id not in valid_relations or relation_id not in logical_outlines:
            continue
        for identity in values - ambiguous_parts:
            explicit_parts[relation_id].add(identity)
            assigned_parts.add(identity)

    related_parts = {identity for values in relation_parts.values() for identity in values}
    remaining_parts = parts - related_parts
    if grid["coordinate_transform"] == "fixture_affine":
        containers = _fixture_containers(projected, outlines, remaining_parts)
    else:
        containers = _production_containers(
            repo_root, scratch_root / "association", grid["canonical_crs"], projected, outlines, remaining_parts
        )
    containment_parts: dict[str, set[str]] = defaultdict(set)
    orphan_parts: set[str] = set()
    ambiguous_containment: set[str] = set()
    for identity, candidates in containers.items():
        logical_candidates = sorted({outline_to_logical[item] for item in candidates})
        if len(logical_candidates) == 1:
            containment_parts[logical_candidates[0]].add(identity)
        elif logical_candidates:
            ambiguous_containment.add(identity)
        else:
            orphan_parts.add(identity)

    maximum_height = float(settings["maximum_height_m"])
    tolerance = float(settings["coverage_tolerance_square_m"])
    grouped_parts = {
        logical_id: (
            member_outlines,
            explicit_parts.get(logical_id, set()) | containment_parts.get(logical_id, set()),
        )
        for logical_id, member_outlines in logical_outlines.items()
    }
    if grid["coordinate_transform"] == "fixture_affine":
        complete_groups = {
            logical_id
            for logical_id, (member_outlines, member_parts) in grouped_parts.items()
            if member_parts and _coverage_complete_fixture(
                [projected[identity] for identity in sorted(member_outlines)],
                [projected[identity] for identity in sorted(member_parts)],
                tolerance,
            )
        }
    else:
        complete_groups = _production_complete_groups(
            repo_root,
            scratch_root / "coverage",
            grid["canonical_crs"],
            grouped_parts,
            projected,
            tolerance,
        )
    candidates: list[dict[str, Any]] = []
    rejections = [
        {"source_identity": identity, "reason_code": "invalid_building_geometry", "message": "Building source geometry is invalid"}
        for identity in sorted(rejected_geometry)
        if target_source_ids is None or identity in target_source_ids
    ]
    rejections.extend(
        {"source_identity": identity, "reason_code": reason, "message": "Building part association is not unique"}
        for reason, identities in (
            ("ambiguous_building_relation", ambiguous_parts),
            ("building_relation_outline_missing", missing_outline_parts),
            ("ambiguous_building_containment", ambiguous_containment),
            ("orphan_building_part", orphan_parts),
        )
        for identity in sorted(identities)
        if target_source_ids is None or identity in target_source_ids
    )
    fallback_count = 0
    for logical_id, member_outlines in sorted(logical_outlines.items()):
        member_parts = sorted(explicit_parts.get(logical_id, set()) | containment_parts.get(logical_id, set()))
        source_ids = sorted(set(member_outlines) | set(member_parts))
        if logical_id.startswith("relation/"):
            source_ids = sorted(set([*source_ids, logical_id]))
        if target_source_ids is not None and not (set(source_ids) & target_source_ids):
            continue
        outline_features = [buildings[identity] for identity in sorted(member_outlines)]
        outline_geometries = [projected[identity] for identity in sorted(member_outlines)]
        part_geometries = [projected[identity] for identity in member_parts]
        part_attributes = [_building_attributes(buildings[identity]["properties"]) for identity in member_parts]
        complete = logical_id in complete_groups and all(
            _vertical_valid(value, maximum_height) for value in part_attributes
        )
        selected_ids = member_parts if complete else sorted(member_outlines)
        selection = "complete_parts" if complete else "outline_no_parts"
        if member_parts and not complete:
            selection = "outline_fallback_incomplete_parts"
            fallback_count += 1
        selected_attributes = [_building_attributes(buildings[identity]["properties"]) for identity in selected_ids]
        if not selected_ids or not all(_vertical_valid(value, maximum_height) for value in selected_attributes):
            rejections.append({
                "source_identity": logical_id,
                "reason_code": "building_vertical_range_invalid",
                "message": "Logical building has no safe vertical fallback",
            })
            continue
        logical_feature_id = canonical_feature_id(namespace, logical_id)
        volumes = [
            {
                "volume_id": f"{logical_feature_id}:volume:{identity.replace('/', '_')}",
                "geometry": projected[identity],
                "min_height_m": attributes["min_height_m"],
                "height_m": attributes["height_m"],
                "height_basis": attributes["height_basis"],
                "min_height_basis": attributes["min_height_basis"],
                "source_feature_id": identity,
            }
            for identity, attributes in zip(selected_ids, selected_attributes, strict=True)
        ]
        effective_geometry = _combined_geometry([item["geometry"] for item in volumes])
        attributes = dict(selected_attributes[0])
        attributes.update({
            "building_massing_version": 2,
            "logical_building_id": logical_id,
            "volume_selection": selection,
            "height_m": max(item["height_m"] for item in volumes),
            "effective_volumes": volumes,
        })
        candidate = {
            "feature_id": logical_feature_id,
            "feature_class": "building",
            "geometry": effective_geometry,
            "attributes": attributes,
            "source_refs": [
                {
                    "snapshot_id": snapshot["snapshot_id"],
                    "provider": snapshot["provider"],
                    "release": snapshot["release"],
                    "provider_feature_id": identity,
                }
                for identity in source_ids
            ],
            "provenance_result": "accepted",
            "authored_overlay_ids": [],
        }
        candidates.append({
            "provider_feature_id": logical_id,
            "provider_class": "building",
            "geometry": effective_geometry,
            "properties": {},
            "prepared_canonical_feature": candidate,
        })
    excluded_counts = {
        "invalid_building_geometry": len(rejected_geometry),
        "ambiguous_building_relation": len(ambiguous_parts),
        "building_relation_outline_missing": len(missing_outline_parts),
        "ambiguous_building_containment": len(ambiguous_containment),
        "orphan_building_part": len(orphan_parts),
        "outline_fallback_incomplete_parts": fallback_count,
    }
    return PreparedBuildings(
        candidates,
        rejections,
        {key: value for key, value in excluded_counts.items() if value},
        sorted(buildings if target_source_ids is None else set(buildings) & target_source_ids),
    )
