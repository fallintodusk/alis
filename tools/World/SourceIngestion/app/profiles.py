from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import IngestionError, canonical_hash, validate_document
from .coverage import build_coverage_evidence


ALLOWED_ADAPTERS = {"synthetic_json", "geofabrik_osm", "copernicus_dem"}
COPERNICUS_LICENSE_FIELDS = {
    "terms_url", "terms_version", "terms_accessed_on", "terms_sha256",
    "original_notice", "modified_notice", "liability_notice", "citation_notice",
    "downstream_obligations", "no_endorsement",
}
PUBLICATION_BOUNDARIES = {
    "tracked_canonical_data", "generated_unreal_assets", "packaged_commercial_distribution"
}


def area_fingerprint(area: dict[str, Any]) -> str:
    spatial_contract = {
        "geometry_type": "bbox",
        "bbox": [float(value) for value in area["bbox"]],
        "axis_order": area["axis_order"],
        "crs": area["crs"],
    }
    return f"sha256:{canonical_hash(spatial_contract)}"


def resolved_area(area: dict[str, Any]) -> dict[str, Any]:
    return {**area, "area_fingerprint": area_fingerprint(area)}


def _valid_bbox(value: Any) -> bool:
    return (
        isinstance(value, list)
        and len(value) == 4
        and all(isinstance(item, (int, float)) and not isinstance(item, bool) for item in value)
        and value[0] < value[2]
        and value[1] < value[3]
        and -180 <= value[0] <= 180
        and -180 <= value[2] <= 180
        and -90 <= value[1] <= 90
        and -90 <= value[3] <= 90
    )


def _intersects(left: list[float], right: list[float]) -> bool:
    return not (left[2] <= right[0] or left[0] >= right[2] or left[3] <= right[1] or left[1] >= right[3])


def _validate_copernicus_rights(
    source_id: str,
    license_data: dict[str, Any],
    admission: dict[str, Any],
) -> None:
    missing = [field for field in COPERNICUS_LICENSE_FIELDS if not license_data.get(field)]
    if missing or license_data.get("no_endorsement") is not True:
        raise IngestionError(
            "incomplete_license_evidence",
            "Copernicus terms identity, notices, and obligations are incomplete",
            source_id=source_id,
            missing=sorted(missing),
        )
    boundaries = admission.get("boundaries")
    if not isinstance(boundaries, dict):
        raise IngestionError("incomplete_admission", "Copernicus use boundaries are missing", source_id=source_id)
    accepted = {"approved", "approved_with_obligations"}
    if boundaries.get("local_verification_cache") not in accepted or any(
        boundaries.get(boundary) not in accepted for boundary in PUBLICATION_BOUNDARIES
    ):
        raise IngestionError(
            "incomplete_admission",
            "Copernicus cache and derived-publication boundaries must be approved",
            source_id=source_id,
        )
    if boundaries.get("public_repository_raw_payload") not in {*accepted, "not_distributed"}:
        raise IngestionError(
            "incomplete_admission", "Copernicus raw-payload repository policy is unresolved", source_id=source_id
        )


def validate_profile(profile: dict[str, Any], profile_path: Path, repo_root: Path) -> None:
    validate_document(profile, profile_path)
    profile_id = profile.get("profile_id")
    if (
        profile.get("schema_version") != 1
        or not isinstance(profile_id, str)
        or not profile_id
        or any(not (character.islower() or character.isdigit() or character == "_") for character in profile_id)
    ):
        raise IngestionError("unsupported_profile", "Source profile version or ID is unsupported")
    area = profile.get("area")
    area_id = area.get("area_id") if isinstance(area, dict) else None
    if (
        not isinstance(area, dict)
        or not isinstance(area_id, str)
        or not area_id
        or any(not (character.islower() or character.isdigit() or character == "_") for character in area_id)
        or not isinstance(area.get("label"), str)
        or not area["label"]
        or area.get("axis_order") != "longitude_latitude"
        or area.get("crs") != "EPSG:4326"
        or not _valid_bbox(area.get("bbox"))
    ):
        raise IngestionError("invalid_area", "Profile area must contain a valid longitude/latitude bbox")
    budget = profile.get("network_budget_bytes")
    if not isinstance(budget, int) or budget < 0:
        raise IngestionError("invalid_budget", "Network budget must be a non-negative integer")
    sources = profile.get("sources")
    if not isinstance(sources, list) or not sources:
        raise IngestionError("missing_sources", "Source profile has no sources")
    dem_count = sum(source.get("adapter") == "copernicus_dem" for source in sources)
    if dem_count > 1 and not isinstance(profile.get("raster_mosaic"), dict):
        raise IngestionError(
            "raster_mosaic_contract_missing",
            "Multiple DEM sources require an explicit mosaic contract",
        )
    projected_coverage = profile.get("projected_coverage")
    if projected_coverage is not None:
        bounds = projected_coverage.get("technical_bounds") if isinstance(projected_coverage, dict) else None
        if (
            not isinstance(bounds, list)
            or len(bounds) != 4
            or any(isinstance(value, bool) or not isinstance(value, (int, float)) for value in bounds)
            or bounds[0] >= bounds[2]
            or bounds[1] >= bounds[3]
        ):
            raise IngestionError("invalid_projected_coverage", "Projected technical bounds are invalid")

    seen: set[str] = set()
    for source in sources:
        source_id = source.get("source_id")
        if (
            not isinstance(source_id, str)
            or not source_id
            or source_id in seen
            or any(not (character.islower() or character.isdigit() or character == "_") for character in source_id)
        ):
            raise IngestionError("invalid_source_id", "Source IDs must be present and unique", source_id=source_id)
        seen.add(source_id)
        if source.get("adapter") not in ALLOWED_ADAPTERS:
            raise IngestionError("unsupported_adapter", "Source adapter is unsupported", source_id=source_id)
        if not source.get("provider") or not source.get("dataset"):
            raise IngestionError("missing_provider", "Source provider and dataset are required", source_id=source_id)
        if (
            not source.get("release")
            or not isinstance(source.get("expected_bytes"), int)
            or source["expected_bytes"] < 0
            or Path(str(source.get("file_name", ""))).name != source.get("file_name")
        ):
            raise IngestionError(
                "unsupported_release", "Source release, byte size, or file name is invalid", source_id=source_id
            )
        hashes = source.get("hashes")
        if not isinstance(hashes, dict) or len(str(hashes.get("sha256", ""))) != 64:
            raise IngestionError("missing_hash", "Source SHA-256 is required", source_id=source_id)
        accuracy = source.get("accuracy")
        if not isinstance(accuracy, dict) or not accuracy.get("confidence"):
            raise IngestionError("missing_accuracy", "Source accuracy and confidence are required", source_id=source_id)
        for axis in ("horizontal_accuracy_m", "vertical_accuracy_m"):
            value = accuracy.get(axis)
            if value is not None and (
                isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0
            ):
                raise IngestionError("invalid_accuracy", "Source accuracy must be non-negative or unknown", source_id=source_id, axis=axis)
        if source["adapter"] in {"synthetic_json", "copernicus_dem"} and accuracy.get("vertical_accuracy_m") is None:
            raise IngestionError("missing_accuracy", "Terrain sources require qualified vertical accuracy", source_id=source_id)
        license_data = source.get("license")
        admission = source.get("admission")
        if not isinstance(license_data, dict) or not license_data.get("id"):
            raise IngestionError("missing_license", "Source license metadata is required", source_id=source_id)
        if not isinstance(admission, dict) or admission.get("policy_result") != "approved":
            raise IngestionError("policy_rejected", "Source is not approved by policy", source_id=source_id)
        if (
            admission.get("commercial_transformation") is not True
            or not admission.get("generated_artifact_publication")
            or not admission.get("acquisition")
        ):
            raise IngestionError(
                "incomplete_admission", "Source use and acquisition decisions are incomplete", source_id=source_id
            )
        if source["adapter"] == "geofabrik_osm" and (
            license_data.get("id") != "ODbL-1.0"
            or not license_data.get("attribution")
            or not license_data.get("source_or_alteration_offer")
        ):
            raise IngestionError(
                "missing_license", "OSM attribution and ODbL offer metadata are required", source_id=source_id
            )
        if source["adapter"] == "copernicus_dem":
            _validate_copernicus_rights(source_id, license_data, admission)
        coverage = source.get("coverage_bbox")
        if coverage is not None and (not _valid_bbox(coverage) or not _intersects(area["bbox"], coverage)):
            raise IngestionError("out_of_area", "Source does not intersect the requested area", source_id=source_id)
        if source["adapter"] == "synthetic_json":
            local = (profile_path.parent / source.get("local_path", "")).resolve()
            if not local.is_relative_to(repo_root.resolve()) or not local.is_file():
                raise IngestionError(
                    "invalid_local_source", "Synthetic source must be a repository file", source_id=source_id
                )
        elif not str(source.get("url", "")).startswith("https://"):
            raise IngestionError("invalid_url", "Remote source URL must use HTTPS", source_id=source_id)


def build_plan(profile: dict[str, Any], repo_root: Path | None = None) -> dict[str, Any]:
    network_sources = [source for source in profile["sources"] if source["adapter"] != "synthetic_json"]
    total = sum(source["expected_bytes"] for source in network_sources)
    if total > profile["network_budget_bytes"]:
        raise IngestionError(
            "budget_exceeded", "Planned provider payload exceeds the profile budget", planned_bytes=total
        )
    inputs = [{
        "source_id": source["source_id"],
        "adapter": source["adapter"],
        "release": source["release"],
        "expected_bytes": source["expected_bytes"],
        "auth_mode": source["admission"]["acquisition"],
        "hashes": source["hashes"],
        "license": source["license"],
        "admission": source["admission"],
    } for source in profile["sources"]]
    plan = {
        "$schema": "https://alis.world/schemas/world-source/source-plan-v1.json",
        "schema_version": 1,
        "profile_id": profile["profile_id"],
        "operation_id": f"plan:{profile['profile_id']}:{canonical_hash(profile)[:12]}",
        "area": resolved_area(profile["area"]),
        "network_budget_bytes": profile["network_budget_bytes"],
        "planned_network_bytes": total,
        "inputs": inputs,
    }
    if "projected_coverage" in profile:
        if repo_root is None:
            raise IngestionError(
                "coverage_authority_required", "Projected source coverage requires the repository toolchain authority"
            )
        plan["coverage"] = build_coverage_evidence(repo_root, profile)
    return plan
