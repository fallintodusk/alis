from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import IngestionError, canonical_hash, file_hash, read_json, validate_document, write_json
from .osm_building_relations import extract_building_relation_memberships
from .profiles import resolved_area
from .osm_selection import select_complete_geometries
from .raster_mosaic import decode_raster_mosaic
from .tool_io import data_artifact as _data_artifact
from .tool_io import portable_metadata as _portable_metadata
from .tool_io import run_json as _run_json
from World.ExecutionEnvironment.api import require_tools, run_tool


FEATURE_SCHEMA = "https://alis.world/schemas/world-source/provider-feature-v1.json"
FEATURE_MANIFEST_SCHEMA = "https://alis.world/schemas/world-source/provider-feature-manifest-v1.json"
RASTER_SCHEMA = "https://alis.world/schemas/world-source/raster-layer-v1.json"
METADATA_SCHEMA = "https://alis.world/schemas/world-source/provider-metadata-v1.json"
OSM_SELECTION_SCHEMA = "https://alis.world/schemas/world-source/osm-selection-receipt-v1.json"


def _snapshot(ledger: dict[str, Any], source_id: str) -> dict[str, Any]:
    for item in ledger["snapshots"]:
        if item["source_id"] == source_id:
            return item
    raise IngestionError("missing_snapshot", "Verified source is absent from the ledger", source_id=source_id)


def _feature_collection(
    snapshot: dict[str, Any], source: dict[str, Any], area: dict[str, Any], features: list[dict[str, Any]]
) -> dict[str, Any]:
    return {
        "$schema": FEATURE_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "area": area,
        "shard_id": "0000",
        "features": sorted(features, key=lambda item: item["provider_feature_id"]),
        "license": source["license"],
    }


def _write_feature_set(base_path: Path, collection: dict[str, Any], kind: str) -> list[dict[str, Any]]:
    shard_path = base_path.with_name(f"{base_path.name}.0000.json")
    manifest_path = base_path.with_name(f"{base_path.name}.manifest.json")
    write_json(shard_path, collection)
    features = collection["features"]
    shard = {
        "shard_id": "0000",
        "path": shard_path.name,
        "sha256": file_hash(shard_path),
        "count": len(features),
        "first_provider_feature_id": features[0]["provider_feature_id"] if features else None,
        "last_provider_feature_id": features[-1]["provider_feature_id"] if features else None,
    }
    manifest = {
        "$schema": FEATURE_MANIFEST_SCHEMA,
        "schema_version": 1,
        "snapshot_id": collection["snapshot_id"],
        "provider": collection["provider"],
        "release": collection["release"],
        "area": collection["area"],
        "shard_strategy": {"kind": "provider_feature_id_range", "version": 1},
        "total_features": len(features),
        "shards": [shard],
        "license": collection["license"],
    }
    write_json(manifest_path, manifest)
    return [
        {"kind": f"{kind}_manifest", "path": manifest_path, "sha256": file_hash(manifest_path), "count": len(features)},
        {"kind": f"{kind}_shard", "path": shard_path, "sha256": shard["sha256"], "count": len(features)},
    ]


def _geometry_intersects_area(geometry: dict[str, Any], bbox: list[float]) -> bool:
    points: list[list[float]] = []

    def collect(value: Any) -> None:
        if (
            isinstance(value, list)
            and len(value) >= 2
            and all(isinstance(item, (int, float)) for item in value[:2])
        ):
            points.append([float(value[0]), float(value[1])])
            return
        if isinstance(value, list):
            for item in value:
                collect(item)

    collect(geometry.get("coordinates"))
    if not points:
        raise IngestionError("invalid_feature", "Synthetic provider geometry has no coordinates")
    bounds = (
        min(point[0] for point in points),
        min(point[1] for point in points),
        max(point[0] for point in points),
        max(point[1] for point in points),
    )
    return not (bounds[2] < bbox[0] or bounds[0] > bbox[2] or bounds[3] < bbox[1] or bounds[1] > bbox[3])


def _decode_synthetic(
    source: dict[str, Any], path: Path, snapshot: dict[str, Any], area: dict[str, Any], output_root: Path, repo_root: Path
) -> list[dict[str, Any]]:
    raw = read_json(path)
    validate_document(raw, path)
    if raw.get("provider") != "synthetic" or raw.get("release") != source["release"]:
        raise IngestionError("unsupported_release", "Synthetic provider fixture does not match its source profile")
    for feature in raw.get("features", []):
        required = {"provider_feature_id", "provider_class", "geometry", "properties", "precision", "confidence"}
        if not required.issubset(feature):
            raise IngestionError("invalid_feature", "Synthetic provider feature is incomplete")
    admitted_features = [
        feature
        for feature in raw["features"]
        if _geometry_intersects_area(feature["geometry"], area["bbox"])
    ]
    feature_outputs = _write_feature_set(
        output_root / "synthetic_features",
        _feature_collection(snapshot, source, area, admitted_features),
        "provider_features",
    )
    raster = raw["raster"]
    raster_value = {
        "$schema": RASTER_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "area": area,
        "raster_class": raster["raster_class"],
        "crs": source["crs"],
        "vertical_datum": {
            "id": source["crs"]["vertical"],
            "unit": source["crs"]["vertical_unit"],
        },
        "vertical_provenance": {
            "source_ref": snapshot["snapshot_id"],
            "source_accuracy_m": snapshot["accuracy"]["vertical_accuracy_m"],
            "confidence": snapshot["accuracy"]["confidence"],
        },
        "pixel_type": raster["pixel_type"],
        "size": raster["size"],
        "transform": raster["transform"],
        "nodata": raster["nodata"],
        "sample_storage": "embedded",
        "source_hashes": snapshot["hashes"],
        "license": source["license"],
        "provider_payload": {"samples": raster["samples"]},
    }
    raster_path = output_root / "synthetic_raster.json"
    write_json(raster_path, raster_value)
    return [*feature_outputs, {"kind": "raster_layer", "path": raster_path, "sha256": file_hash(raster_path)}]


def _convert_osmium_geojson(
    geojson_path: Path,
    snapshot: dict[str, Any],
    source: dict[str, Any],
    area: dict[str, Any],
    admitted_classes: set[str] | None = None,
    required_properties: dict[str, Any] | None = None,
) -> dict[str, Any]:
    raw = read_json(geojson_path)
    features_by_id: dict[str, dict[str, Any]] = {}
    for feature in raw.get("features", []):
        properties = feature.get("properties") or {}
        provider_type = properties.get("@type") or properties.get("type")
        provider_numeric_id = properties.get("@id", properties.get("id"))
        if provider_type and provider_numeric_id is not None:
            provider_id = f"{provider_type}/{provider_numeric_id}"
        else:
            provider_id = feature.get("id")
        geometry = feature.get("geometry")
        if not provider_id or not geometry:
            raise IngestionError("invalid_feature", "OSM export contains a feature without identity or geometry")
        provider_class = _classify_osm_feature(properties)
        if admitted_classes is not None and provider_class not in admitted_classes:
            continue
        if required_properties is not None and any(
            properties.get(key) != value for key, value in required_properties.items()
        ):
            continue
        normalized = {
            "provider_feature_id": str(provider_id),
            "provider_class": provider_class,
            "geometry": geometry,
            "properties": properties,
            "precision": {"coordinate_decimals": 7, "source_crs": "EPSG:4326"},
            "confidence": None,
        }
        existing = features_by_id.get(str(provider_id))
        if existing is None:
            features_by_id[str(provider_id)] = normalized
            continue
        if existing["properties"] != normalized["properties"] or existing["provider_class"] != provider_class:
            raise IngestionError(
                "duplicate_feature_conflict",
                "OSM export contains conflicting records for one provider identity",
                provider_feature_id=str(provider_id),
            )
        if existing["geometry"] == geometry:
            continue
        existing_rank = _osm_geometry_rank(provider_class, properties, existing["geometry"].get("type"))
        incoming_rank = _osm_geometry_rank(provider_class, properties, geometry.get("type"))
        if existing_rank == incoming_rank:
            raise IngestionError(
                "duplicate_feature_conflict",
                "OSM export contains conflicting geometries for one provider identity",
                provider_feature_id=str(provider_id),
            )
        if incoming_rank > existing_rank:
            features_by_id[str(provider_id)] = normalized
    return _feature_collection(snapshot, source, area, list(features_by_id.values()))


def _convert_demonstration_boundary(
    geojson_path: Path,
    snapshot: dict[str, Any],
    source: dict[str, Any],
    boundary_area: dict[str, Any],
) -> dict[str, Any]:
    """Preserve the exact boundary selected by the profile's OSM filter."""
    return _convert_osmium_geojson(
        geojson_path,
        snapshot,
        source,
        boundary_area,
        admitted_classes={"administrative_boundary"},
    )


def _classify_osm_feature(properties: dict[str, Any]) -> str:
    natural = properties.get("natural")
    landuse = properties.get("landuse")
    if "highway" in properties:
        return "highway"
    if "building" in properties or "building:part" in properties:
        return "building"
    if natural == "tree":
        return "foliage_point"
    if natural == "water" or landuse in {"reservoir", "basin"} or "waterway" in properties:
        return "water"
    if natural == "wood" or landuse in {"forest", "orchard", "vineyard"}:
        return "vegetation_area"
    if natural in {"grassland", "scrub", "heath", "wetland", "sand", "bare_rock"}:
        return "land_cover"
    if landuse in {"grass", "meadow", "recreation_ground"}:
        return "land_cover"
    if properties.get("boundary") == "administrative":
        return "administrative_boundary"
    return "provider_feature"


def _osm_geometry_rank(provider_class: str, properties: dict[str, Any], geometry_type: Any) -> int:
    area_classes = {"building", "land_cover", "vegetation_area"}
    water_is_area = (
        properties.get("natural") == "water"
        or properties.get("landuse") in {"reservoir", "basin"}
    )
    if provider_class in area_classes or (provider_class == "water" and water_is_area):
        return 2 if geometry_type in {"Polygon", "MultiPolygon"} else 1
    if provider_class in {"highway", "water"}:
        return 2 if geometry_type in {"LineString", "MultiLineString"} else 1
    if provider_class == "foliage_point":
        return 2 if geometry_type in {"Point", "MultiPoint"} else 1
    return 1


def _provider_feature_fingerprint(snapshot: dict[str, Any], feature: dict[str, Any]) -> str:
    return "sha256:" + canonical_hash({
        "snapshot_id": snapshot["snapshot_id"],
        "provider_feature_id": feature["provider_feature_id"],
        "geometry": feature["geometry"],
    })


def _decode_osm(
    source: dict[str, Any], path: Path, snapshot: dict[str, Any], area: dict[str, Any], output_root: Path, repo_root: Path
) -> list[dict[str, Any]]:
    require_tools(repo_root)
    osm_root = output_root / "osm"
    osm_root.mkdir(parents=True, exist_ok=True)
    boundary_pbf = osm_root / "demonstration_boundary.osm.pbf"
    boundary_geojson = osm_root / "demonstration_boundary.geojson"
    for generated in (boundary_pbf, boundary_geojson):
        if generated.exists():
            generated.unlink()

    fileinfo = _run_json(repo_root, "osmium", ["fileinfo", "-e", "-j", str(path)])
    object_counts = fileinfo.get("data", {}).get("count", {})
    if not isinstance(object_counts, dict) or sum(object_counts.values()) == 0:
        raise IngestionError("empty_source", "Geofabrik PBF contains no OSM objects")
    metadata_path = output_root / "geofabrik_source_metadata.json"
    write_json(metadata_path, {
        "$schema": METADATA_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "payload": _portable_metadata(fileinfo),
    })
    filters = source["decode"]["feature_filters"]
    selected_pbf, selected_geojson, selection_evidence = select_complete_geometries(
        repo_root, path, filters, area["bbox"], osm_root,
    )
    boundary_filter = area["demonstration_boundary"]["osm_filter"]
    run_tool(repo_root, "osmium", ["tags-filter", "-o", str(boundary_pbf), str(path), boundary_filter])
    run_tool(repo_root, "osmium", [
        "export", "-u", "type_id", "-a", "id,type,version,timestamp,changeset", "-f", "geojson",
        "-o", str(boundary_geojson), str(boundary_pbf)
    ])

    admitted_classes = {
        "building", "foliage_point", "highway", "land_cover", "vegetation_area", "water",
    }
    features = _convert_osmium_geojson(
        selected_geojson,
        snapshot,
        source,
        area,
        admitted_classes=admitted_classes,
    )
    relation_memberships = extract_building_relation_memberships(
        repo_root,
        path,
        {item["provider_feature_id"] for item in features["features"]},
        osm_root / "building_relations",
    )
    for feature in features["features"]:
        memberships = relation_memberships.get(feature["provider_feature_id"])
        if memberships:
            feature["relation_memberships"] = memberships
    boundary_area = {
        **area["demonstration_boundary"],
        "selection": boundary_filter,
    }
    boundary = _convert_demonstration_boundary(
        boundary_geojson,
        snapshot,
        source,
        boundary_area,
    )
    if not features["features"] or not boundary["features"]:
        raise IngestionError("empty_decode", "OSM decode did not produce the requested features and boundary")
    if len(boundary["features"]) != 1:
        raise IngestionError("ambiguous_boundary", "Boundary selection did not resolve to one provider feature")
    boundary_feature = boundary["features"][0]
    boundary["area"]["area_fingerprint"] = _provider_feature_fingerprint(snapshot, boundary_feature)
    boundary["area"]["fingerprint_basis"] = "provider_snapshot_feature_geometry"
    feature_outputs = _write_feature_set(output_root / "area_osm_features", features, "provider_features")
    boundary_outputs = _write_feature_set(output_root / "demonstration_boundary", boundary, "provider_boundary")
    selection_path = output_root / "osm_selection_receipt.json"
    write_json(selection_path, {
        "$schema": OSM_SELECTION_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        **selection_evidence,
        "selected_feature_count": len(features["features"]),
        "geometry_export_drop_count": max(
            0,
            selection_evidence["selected_membership_count"] - len(features["features"]),
        ),
        "selected_features_sha256": canonical_hash({"features": features["features"]}),
        "result": "accepted",
    })
    return [
        {"kind": "provider_metadata", "path": metadata_path, "sha256": file_hash(metadata_path)},
        *feature_outputs,
        *boundary_outputs,
        {"kind": "osm_selection_receipt", "path": selection_path, "sha256": file_hash(selection_path)},
        {"kind": "derived_pbf", "path": selected_pbf, "sha256": file_hash(selected_pbf), "byte_size": selected_pbf.stat().st_size},
    ]


def _decode_dem(
    source: dict[str, Any], path: Path, snapshot: dict[str, Any], area: dict[str, Any], output_root: Path, repo_root: Path
) -> list[dict[str, Any]]:
    require_tools(repo_root)
    metadata = _run_json(repo_root, "gdalinfo", ["-json", str(path)])
    epsg = metadata.get("stac", {}).get("proj:epsg")
    bands = metadata.get("bands", [])
    if epsg != 4326 or not bands or bands[0].get("type") != "Float32":
        raise IngestionError("unsupported_raster", "Copernicus raster CRS or pixel type differs from the pinned contract")
    metadata_path = output_root / "copernicus_source_metadata.json"
    write_json(metadata_path, {
        "$schema": METADATA_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "payload": _portable_metadata(metadata),
    })
    bbox = area["bbox"]
    normalized = output_root / "area_dem_provider_cog.tif"
    if normalized.exists():
        normalized.unlink()
    run_tool(repo_root, "gdal_translate", [
        "-projwin", str(bbox[0]), str(bbox[3]), str(bbox[2]), str(bbox[1]),
        "-projwin_srs", "EPSG:4326", "-of", "COG", "-co", "COMPRESS=DEFLATE",
        "-co", "PREDICTOR=YES", "-co", "BLOCKSIZE=512", "-co", "NUM_THREADS=1",
        str(path), str(normalized),
    ])
    clipped = _run_json(repo_root, "gdalinfo", ["-json", str(normalized)])
    raster_path = output_root / "area_dem_raster_layer.json"
    raster_value = {
        "$schema": RASTER_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "area": area,
        "raster_class": source["raster_class"],
        "crs": {"horizontal": "EPSG:4326", "axis_order": "longitude_latitude"},
        "vertical_datum": {"id": "EPSG:3855", "unit": "metre"},
        "vertical_provenance": {
            "source_ref": snapshot["snapshot_id"],
            "source_accuracy_m": snapshot["accuracy"]["vertical_accuracy_m"],
            "confidence": snapshot["accuracy"]["confidence"],
        },
        "pixel_type": clipped["bands"][0]["type"],
        "size": clipped["size"],
        "transform": clipped["geoTransform"],
        "nodata": clipped["bands"][0].get("noDataValue"),
        "sample_storage": "artifact",
        "data_artifact": _data_artifact(raster_path, normalized, "COG"),
        "source_hashes": snapshot["hashes"],
        "license": source["license"],
        "provider_payload": {"area_or_point": metadata.get("metadata", {}).get("", {}).get("AREA_OR_POINT")},
    }
    write_json(raster_path, raster_value)
    return [
        {"kind": "provider_metadata", "path": metadata_path, "sha256": file_hash(metadata_path)},
        {"kind": "provider_raster", "path": normalized, "sha256": file_hash(normalized), "byte_size": normalized.stat().st_size},
        {"kind": "raster_layer", "path": raster_path, "sha256": file_hash(raster_path)},
    ]


def decode_sources(
    profile: dict[str, Any], paths: dict[str, Path], ledger: dict[str, Any], output_root: Path, repo_root: Path
) -> list[dict[str, Any]]:
    output_root.mkdir(parents=True, exist_ok=True)
    outputs: list[dict[str, Any]] = []
    area = resolved_area(profile["area"])
    mosaic_sources = [source for source in profile["sources"] if source["adapter"] == "copernicus_dem"]
    if len(mosaic_sources) > 1:
        outputs.extend(decode_raster_mosaic(profile, paths, ledger, area, output_root, repo_root))
    for source in profile["sources"]:
        snapshot = _snapshot(ledger, source["source_id"])
        if source["adapter"] == "synthetic_json":
            decoded = _decode_synthetic(source, paths[source["source_id"]], snapshot, area, output_root, repo_root)
        elif source["adapter"] == "geofabrik_osm":
            decoded = _decode_osm(source, paths[source["source_id"]], snapshot, area, output_root, repo_root)
        elif source["adapter"] == "copernicus_dem":
            decoded = [] if len(mosaic_sources) > 1 else _decode_dem(
                source, paths[source["source_id"]], snapshot, area, output_root, repo_root
            )
        else:
            raise IngestionError("unsupported_adapter", "No decoder exists", source_id=source["source_id"])
        outputs.extend(decoded)
    return outputs
