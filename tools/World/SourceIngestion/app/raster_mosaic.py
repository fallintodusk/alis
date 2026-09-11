from __future__ import annotations

import hashlib
import json
import math
import struct
import tempfile
from pathlib import Path
from typing import Any

from World.ExecutionEnvironment.api import require_tools, run_tool

from .contracts import IngestionError, canonical_hash, file_hash, write_json
from .tool_io import data_artifact, portable_metadata, run_json


COMPONENT_SCHEMA = "https://alis.world/schemas/world-source/raster-component-v1.json"
MOSAIC_SCHEMA = "https://alis.world/schemas/world-source/raster-mosaic-layer-v1.json"
VALIDATION_SCHEMA = "https://alis.world/schemas/world-source/raster-mosaic-validation-v1.json"
METADATA_SCHEMA = "https://alis.world/schemas/world-source/provider-metadata-v1.json"


def _component_order(source: dict[str, Any], snapshot: dict[str, Any]) -> tuple[Any, ...]:
    return (*[float(value) for value in source["coverage_bbox"]], source["source_id"], snapshot["snapshot_id"])


def _bounds(metadata: dict[str, Any]) -> list[float]:
    transform = [float(value) for value in metadata["geoTransform"]]
    width, height = (int(value) for value in metadata["size"])
    return [transform[0], transform[3] + height * transform[5], transform[0] + width * transform[1], transform[3]]


def _canonical_sample_hash(repo_root: Path, raster_path: Path, metadata: dict[str, Any]) -> str:
    header = {
        "nodata": metadata["bands"][0].get("noDataValue"),
        "size": metadata["size"],
        "transform": metadata["geoTransform"],
    }
    digest = hashlib.sha256()
    digest.update(json.dumps(header, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8"))
    digest.update(b"\n")
    count = 0
    with tempfile.TemporaryDirectory(dir=raster_path.parent) as directory:
        raw_path = Path(directory) / "samples.bin"
        run_tool(repo_root, "gdal_translate", [
            "-q", "-of", "ENVI", "-ot", "Float32", "-co", "INTERLEAVE=BSQ",
            str(raster_path), str(raw_path),
        ])
        header_path = raw_path.with_suffix(".hdr")
        if "byte order = 0" not in header_path.read_text(encoding="ascii"):
            raise IngestionError("raster_decode_failed", "Semantic sample stream is not little-endian")
        with raw_path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                if len(block) % 4:
                    raise IngestionError("raster_decode_failed", "Semantic sample stream is misaligned")
                normalized = bytearray()
                for (value,) in struct.iter_unpack("<f", block):
                    if not math.isfinite(value):
                        raise IngestionError("raster_nodata", "Normalized raster contains a non-finite sample")
                    nodata = header["nodata"]
                    if nodata is not None and value == float(nodata):
                        raise IngestionError("raster_nodata", "Normalized raster contains nodata")
                    normalized.extend(struct.pack("<f", 0.0 if value == 0.0 else value))
                    count += 1
                digest.update(normalized)
    if count != int(metadata["size"][0]) * int(metadata["size"][1]):
        raise IngestionError("raster_decode_failed", "Normalized raster sample count differs from metadata")
    return digest.hexdigest()


def _validate_vrt_coverage(repo_root: Path, vrt: Path) -> int:
    metadata = run_json(repo_root, "gdalinfo", ["-json", str(vrt)])
    expected = int(metadata["size"][0]) * int(metadata["size"][1])
    checked = 0
    with tempfile.TemporaryDirectory(dir=vrt.parent) as directory:
        raw_path = Path(directory) / "alpha.bin"
        run_tool(repo_root, "gdal_translate", [
            "-q", "-b", "2", "-of", "ENVI", "-ot", "Byte", str(vrt), str(raw_path),
        ])
        with raw_path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                checked += len(block)
                if any(value != 255 for value in block):
                    raise IngestionError("raster_coverage_gap", "Constructed mosaic contains an uncovered pixel")
    if checked != expected:
        raise IngestionError("raster_decode_failed", "Mosaic coverage mask size differs from metadata")
    return checked


def _component(
    repo_root: Path,
    source: dict[str, Any],
    source_path: Path,
    snapshot: dict[str, Any],
    area: dict[str, Any],
    root: Path,
) -> tuple[dict[str, Any], Path, Path]:
    metadata = run_json(repo_root, "gdalinfo", ["-json", str(source_path)])
    bands = metadata.get("bands", [])
    transform = metadata.get("geoTransform", [])
    area_or_point = metadata.get("metadata", {}).get("", {}).get("AREA_OR_POINT")
    if (
        metadata.get("stac", {}).get("proj:epsg") != 4326
        or len(bands) != 1
        or bands[0].get("type") != "Float32"
        or len(transform) != 6
        or float(transform[2]) != 0.0
        or float(transform[4]) != 0.0
        or area_or_point != "Point"
    ):
        raise IngestionError("unsupported_raster", "Raster metadata differs from the mosaic contract")
    source_root = root / source["source_id"]
    source_root.mkdir(parents=True, exist_ok=True)
    normalized = source_root / "normalized.cog.tif"
    west, south, east, north = (float(value) for value in area["bbox"])
    coverage = [float(value) for value in source["coverage_bbox"]]
    clip = [max(west, coverage[0]), max(south, coverage[1]), min(east, coverage[2]), min(north, coverage[3])]
    run_tool(repo_root, "gdal_translate", [
        "-q", "-projwin", str(clip[0]), str(clip[3]), str(clip[2]), str(clip[1]),
        "-projwin_srs", "EPSG:4326", "-of", "COG", "-co", "COMPRESS=DEFLATE",
        "-co", "PREDICTOR=YES", "-co", "BLOCKSIZE=512", "-co", "NUM_THREADS=1",
        str(source_path), str(normalized),
    ])
    clipped = run_json(repo_root, "gdalinfo", ["-json", str(normalized)])
    realized = _bounds(clipped)
    identity = {
        "normalization_contract_version": 1,
        "original_snapshot_id": snapshot["snapshot_id"],
        "original_sha256": snapshot["hashes"]["sha256"],
        "requested_clip": clip,
        "realized_bounds": realized,
        "size": clipped["size"],
        "transform": clipped["geoTransform"],
        "vertical_datum": source["crs"]["vertical"],
        "accuracy": snapshot["accuracy"],
    }
    component_path = source_root / "component.json"
    component = {
        "$schema": COMPONENT_SCHEMA,
        "schema_version": 1,
        "component_id": canonical_hash(identity),
        "source_id": source["source_id"],
        "snapshot_id": snapshot["snapshot_id"],
        "requested_clip": clip,
        "realized_bounds": realized,
        "raster_class": source["raster_class"],
        "crs": {"horizontal": "EPSG:4326", "axis_order": "longitude_latitude"},
        "vertical_datum": {"id": source["crs"]["vertical"], "unit": source["crs"]["vertical_unit"]},
        "accuracy": snapshot["accuracy"],
        "pixel_type": "Float32",
        "size": clipped["size"],
        "transform": clipped["geoTransform"],
        "nodata": clipped["bands"][0].get("noDataValue"),
        "sample_semantic_sha256": _canonical_sample_hash(repo_root, normalized, clipped),
        "data_artifact": data_artifact(component_path, normalized, "COG"),
        "source_hashes": snapshot["hashes"],
    }
    write_json(component_path, component)
    metadata_path = source_root / "provider_metadata.json"
    write_json(metadata_path, {
        "$schema": METADATA_SCHEMA,
        "schema_version": 1,
        "snapshot_id": snapshot["snapshot_id"],
        "provider": source["provider"],
        "release": source["release"],
        "payload": portable_metadata(metadata),
    })
    return component, component_path, metadata_path


def _overlap_checks(repo_root: Path, components: list[tuple[dict[str, Any], Path]]) -> list[dict[str, Any]]:
    checks: list[dict[str, Any]] = []
    for index, (left, left_path) in enumerate(components):
        for right, right_path in components[index + 1:]:
            bounds = [
                max(left["realized_bounds"][0], right["realized_bounds"][0]),
                max(left["realized_bounds"][1], right["realized_bounds"][1]),
                min(left["realized_bounds"][2], right["realized_bounds"][2]),
                min(left["realized_bounds"][3], right["realized_bounds"][3]),
            ]
            if bounds[0] >= bounds[2] or bounds[1] >= bounds[3]:
                continue
            left_values = run_tool(repo_root, "gdal_translate", [
                "-q", "-projwin", str(bounds[0]), str(bounds[3]), str(bounds[2]), str(bounds[1]),
                "-projwin_srs", "EPSG:4326", "-of", "XYZ", str(left_path), "/vsistdout/",
            ])
            right_values = run_tool(repo_root, "gdal_translate", [
                "-q", "-projwin", str(bounds[0]), str(bounds[3]), str(bounds[2]), str(bounds[1]),
                "-projwin_srs", "EPSG:4326", "-of", "XYZ", str(right_path), "/vsistdout/",
            ])
            if left_values != right_values:
                raise IngestionError("raster_overlap_conflict", "Raster overlap samples differ")
            checks.append({"left": left["component_id"], "right": right["component_id"], "bounds": bounds})
    return checks


def _aligned_component_grid(reference: list[float], candidate: list[float]) -> bool:
    if len(reference) != 6 or len(candidate) != 6:
        return False
    if not all(math.isfinite(float(value)) for value in [*reference, *candidate]):
        return False
    if any(abs(float(transform[index])) > 1e-15 for transform in (reference, candidate) for index in (2, 4)):
        return False
    if not all(
        math.isclose(float(reference[index]), float(candidate[index]), rel_tol=1e-12, abs_tol=1e-15)
        for index in (1, 5)
    ):
        return False
    offsets = (
        (float(candidate[0]) - float(reference[0])) / float(reference[1]),
        (float(candidate[3]) - float(reference[3])) / float(reference[5]),
    )
    return all(abs(value - round(value)) <= 1e-7 for value in offsets)


def decode_raster_mosaic(
    profile: dict[str, Any],
    paths: dict[str, Path],
    ledger: dict[str, Any],
    area: dict[str, Any],
    output_root: Path,
    repo_root: Path,
) -> list[dict[str, Any]]:
    require_tools(repo_root)
    sources = [source for source in profile["sources"] if source["adapter"] == "copernicus_dem"]
    snapshots = {item["source_id"]: item for item in ledger["snapshots"]}
    ordered = sorted(sources, key=lambda item: _component_order(item, snapshots[item["source_id"]]))
    root = output_root / "raster"
    records: list[tuple[dict[str, Any], Path, Path]] = [
        _component(repo_root, source, paths[source["source_id"]], snapshots[source["source_id"]], area, root)
        for source in ordered
    ]
    first = records[0][0]
    compatibility = (
        first["raster_class"], first["crs"], first["vertical_datum"], first["accuracy"],
        first["pixel_type"],
    )
    if any((
        item[0]["raster_class"], item[0]["crs"], item[0]["vertical_datum"], item[0]["accuracy"],
        item[0]["pixel_type"],
    ) != compatibility or not _aligned_component_grid(first["transform"], item[0]["transform"])
        for item in records[1:]):
        raise IngestionError("raster_component_conflict", "Raster components are not mosaic-compatible")
    overlap_checks = _overlap_checks(repo_root, [(item[0], item[1].parent / item[0]["data_artifact"]["path"]) for item in records])
    mosaic_bounds = [
        min(item[0]["realized_bounds"][0] for item in records),
        min(item[0]["realized_bounds"][1] for item in records),
        max(item[0]["realized_bounds"][2] for item in records),
        max(item[0]["realized_bounds"][3] for item in records),
    ]
    contract_id = canonical_hash({
        "requested_area": area["bbox"],
        "realized_bounds": mosaic_bounds,
        **profile["raster_mosaic"],
    })
    authority_components = [
        {"component_id": item[0]["component_id"], "sample_semantic_sha256": item[0]["sample_semantic_sha256"]}
        for item in records
    ]
    authority_id = canonical_hash({"mosaic_contract_id": contract_id, "components": authority_components})
    vrt = root / "mosaic.vrt"
    component_paths = [item[1].parent / item[0]["data_artifact"]["path"] for item in records]
    run_tool(repo_root, "gdalbuildvrt", [
        "-q", "-strict", "-resolution", "same", "-addalpha", "-te",
        *(str(value) for value in mosaic_bounds), str(vrt), *(str(path) for path in component_paths),
    ])
    coverage_count = _validate_vrt_coverage(repo_root, vrt)
    mosaic_cog = root / "mosaic.cog.tif"
    run_tool(repo_root, "gdal_translate", [
        "-q", "-b", "1", "-of", "COG", "-co", "COMPRESS=DEFLATE", "-co", "PREDICTOR=YES",
        "-co", "BLOCKSIZE=512", "-co", "NUM_THREADS=1", str(vrt), str(mosaic_cog),
    ])
    mosaic_metadata = run_json(repo_root, "gdalinfo", ["-json", str(mosaic_cog)])
    mosaic_path = root / "mosaic.json"
    component_entries = [{
        "component_id": item[0]["component_id"],
        "source_id": item[0]["source_id"],
        "snapshot_id": item[0]["snapshot_id"],
        "realized_bounds": item[0]["realized_bounds"],
        "size": item[0]["size"],
        "transform": item[0]["transform"],
        "sample_semantic_sha256": item[0]["sample_semantic_sha256"],
        "artifact_sha256": item[0]["data_artifact"]["sha256"],
    } for item in records]
    mosaic = {
        "$schema": MOSAIC_SCHEMA,
        "schema_version": 1,
        "snapshot_id": f"mosaic:{authority_id[:16]}",
        "provider": "Composite admitted raster",
        "release": "+".join(item[0]["snapshot_id"] for item in records),
        "area": area,
        "realized_bounds": mosaic_bounds,
        "raster_class": first["raster_class"],
        "crs": first["crs"],
        "vertical_datum": first["vertical_datum"],
        "vertical_provenance": {
            "source_ref": f"mosaic:{authority_id}",
            "source_accuracy_m": max(float(item[0]["accuracy"]["vertical_accuracy_m"]) for item in records),
            "confidence": first["accuracy"]["confidence"],
        },
        "pixel_type": "Float32",
        "size": mosaic_metadata["size"],
        "transform": mosaic_metadata["geoTransform"],
        "nodata": mosaic_metadata["bands"][0].get("noDataValue"),
        "sample_storage": "artifact",
        "data_artifact": data_artifact(mosaic_path, mosaic_cog, "COG"),
        "source_hashes": {item[0]["source_id"]: item[0]["source_hashes"]["sha256"] for item in records},
        "license": {"component_license_ids": sorted({source["license"]["id"] for source in ordered})},
        "mosaic_contract_id": contract_id,
        "mosaic_authority_id": authority_id,
        "sample_semantic_sha256": _canonical_sample_hash(repo_root, mosaic_cog, mosaic_metadata),
        "components": component_entries,
    }
    write_json(mosaic_path, mosaic)
    validation_path = root / "mosaic_validation.json"
    write_json(validation_path, {
        "$schema": VALIDATION_SCHEMA,
        "schema_version": 1,
        "mosaic_contract_id": contract_id,
        "mosaic_authority_id": authority_id,
        "coverage_policy": "vrt_alpha_complete",
        "coverage_pixels_checked": coverage_count,
        "uncovered_pixels": 0,
        "overlap_policy": "require_equal_float32_samples",
        "overlap_checks": overlap_checks,
        "result": "accepted",
    })
    outputs = []
    for component, component_path, metadata_path in records:
        artifact_path = component_path.parent / component["data_artifact"]["path"]
        outputs.extend([
            {"kind": "provider_metadata", "path": metadata_path, "sha256": file_hash(metadata_path)},
            {"kind": "raster_component", "path": component_path, "sha256": file_hash(component_path)},
            {"kind": "provider_raster", "path": artifact_path, "sha256": file_hash(artifact_path), "byte_size": artifact_path.stat().st_size},
        ])
    return [
        *outputs,
        {"kind": "raster_mosaic_validation", "path": validation_path, "sha256": file_hash(validation_path)},
        {"kind": "provider_raster", "path": mosaic_cog, "sha256": file_hash(mosaic_cog), "byte_size": mosaic_cog.stat().st_size},
        {"kind": "raster_layer", "path": mosaic_path, "sha256": file_hash(mosaic_path)},
    ]
