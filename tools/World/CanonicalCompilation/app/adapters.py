from __future__ import annotations

import re
from typing import Any

from .contracts import CompilerError
from .spatial import transform_geometry


SOURCE_ID = re.compile(r"^(node|way|relation)/(\d+)$")
ROAD_WIDTHS = {
    "motorway": 14.0,
    "trunk": 12.0,
    "primary": 10.0,
    "secondary": 8.0,
    "tertiary": 7.0,
    "residential": 6.0,
    "service": 4.0,
    "living_street": 4.0,
    "footway": 2.0,
    "path": 1.5,
}


def _positive_number(value: Any) -> float | None:
    if isinstance(value, (int, float)) and float(value) > 0:
        return float(value)
    if not isinstance(value, str):
        return None
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*(?:m)?\s*", value)
    return float(match.group(1)) if match and float(match.group(1)) > 0 else None


def _nonnegative_number(value: Any) -> float | None:
    if isinstance(value, (int, float)) and float(value) >= 0:
        return float(value)
    if not isinstance(value, str):
        return None
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*(?:m)?\s*", value)
    return float(match.group(1)) if match else None


def canonical_feature_id(namespace: str, source_identity: str) -> str:
    match = SOURCE_ID.fullmatch(source_identity)
    if match is None:
        raise CompilerError("source_identity_invalid", "Provider feature identity is not stable", identity=source_identity)
    return f"alis:{namespace}:{match.group(1)}:{match.group(2)}"


def _building_attributes(properties: dict[str, Any]) -> dict[str, Any]:
    levels_value = _positive_number(properties.get("building:levels"))
    minimum_levels_value = _nonnegative_number(properties.get("building:min_level"))
    height_value = _positive_number(properties.get("height"))
    minimum_height_value = _nonnegative_number(properties.get("min_height"))
    building_class = str(properties.get("building") or properties.get("building:part") or "yes")
    if height_value is not None:
        height, basis = height_value, "source-derived"
    elif levels_value is not None:
        height, basis = levels_value * 3.0, "inferred_from_levels"
    else:
        defaults = {"house": 6.0, "industrial": 8.0, "apartments": 12.0}
        height, basis = defaults.get(building_class, 9.0), "procedural_default"
    if minimum_height_value is not None:
        minimum_height, minimum_basis = minimum_height_value, "source-derived"
    elif minimum_levels_value is not None:
        minimum_height, minimum_basis = minimum_levels_value * 3.0, "inferred_from_min_level"
    else:
        minimum_height, minimum_basis = 0.0, "grounded_default"
    attributes: dict[str, Any] = {
        "building_class": building_class,
        "height_m": round(height, 2),
        "height_basis": basis,
        "min_height_m": round(minimum_height, 2),
        "min_height_basis": minimum_basis,
    }
    if levels_value is not None:
        attributes["levels"] = int(levels_value)
        attributes["levels_basis"] = "source-derived"
    if minimum_levels_value is not None:
        attributes["min_level"] = int(minimum_levels_value)
        attributes["min_level_basis"] = "source-derived"
    return attributes


def _road_attributes(properties: dict[str, Any]) -> dict[str, Any]:
    road_class = str(properties.get("highway", "unclassified"))
    attributes: dict[str, Any] = {
        "road_class": road_class,
        "width_m": ROAD_WIDTHS.get(road_class, 5.0),
        "width_basis": "inferred_from_road_class",
    }
    if properties.get("name"):
        attributes["name"] = str(properties["name"])
        attributes["name_basis"] = "source-derived"
    return attributes


def _world_surface_attributes(provider_class: str, properties: dict[str, Any]) -> dict[str, Any]:
    if provider_class == "water":
        return {
            "water_class": str(
                properties.get("water") or properties.get("waterway") or properties.get("natural") or "water"
            ),
        }
    if provider_class == "land_cover":
        attributes = {
            "land_cover_class": str(properties.get("landuse") or properties.get("natural") or "unspecified"),
        }
        if properties.get("surface"):
            attributes["surface"] = str(properties["surface"])
        return attributes
    if provider_class == "vegetation_area":
        attributes = {
            "vegetation_class": str(properties.get("landuse") or properties.get("natural") or "vegetation"),
        }
    else:
        attributes = {
            "foliage_class": str(properties.get("natural") or "tree"),
        }
    for key in ("leaf_type", "leaf_cycle", "species"):
        if properties.get(key):
            attributes[key] = str(properties[key])
    return attributes


def adapt_feature(
    feature: dict[str, Any],
    namespace: str,
    grid: dict[str, Any],
    snapshot: dict[str, Any],
    projected_geometry: dict[str, Any] | None = None,
) -> tuple[dict[str, Any] | None, str | None]:
    provider_class = feature["provider_class"]
    geometry_type = feature["geometry"].get("type")
    if provider_class == "building":
        if geometry_type not in {"Polygon", "MultiPolygon"}:
            return None, "unsupported_building_representation"
        feature_class = "building"
        attributes = _building_attributes(feature["properties"])
    elif provider_class == "highway":
        if geometry_type not in {"LineString", "MultiLineString"}:
            return None, "unsupported_road_representation"
        feature_class = "road"
        attributes = _road_attributes(feature["properties"])
    elif provider_class == "water":
        if geometry_type not in {"LineString", "MultiLineString", "Polygon", "MultiPolygon"}:
            return None, "unsupported_water_representation"
        feature_class = "water"
        attributes = _world_surface_attributes(provider_class, feature["properties"])
    elif provider_class in {"land_cover", "vegetation_area"}:
        if geometry_type not in {"Polygon", "MultiPolygon"}:
            return None, f"unsupported_{provider_class}_representation"
        feature_class = provider_class
        attributes = _world_surface_attributes(provider_class, feature["properties"])
    elif provider_class == "foliage_point":
        if geometry_type not in {"Point", "MultiPoint"}:
            return None, "unsupported_foliage_point_representation"
        feature_class = "foliage_point"
        attributes = _world_surface_attributes(provider_class, feature["properties"])
    else:
        return None, "unsupported_feature_class"
    source_identity = feature["provider_feature_id"]
    return {
        "feature_id": canonical_feature_id(namespace, source_identity),
        "feature_class": feature_class,
        "geometry": projected_geometry or transform_geometry(feature["geometry"], grid),
        "attributes": attributes,
        "source_refs": [{
            "snapshot_id": snapshot["snapshot_id"],
            "provider": snapshot["provider"],
            "release": snapshot["release"],
            "provider_feature_id": source_identity,
        }],
        "provenance_result": "accepted",
        "authored_overlay_ids": [],
    }, None
