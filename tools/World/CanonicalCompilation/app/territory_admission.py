from __future__ import annotations

from pathlib import Path
from typing import Any

from World.SourceIngestion.api import (
    build_plan,
    read_json as read_source_json,
    run_contract as source_run_contract,
    validate_document as validate_source_document,
    validate_profile,
    write_json as write_source_json,
)

from .contracts import CompilerError, canonical_hash, file_hash, read_json, resolve_owned_path, validate_document, write_json
from .control_network import qualify_control_network
from .pipeline import load_profile
from .spatial import cell_bounds


RECEIPT_SCHEMA = "https://alis.world/schemas/world-compiler/territory-admission-receipt-v1.json"


def owner_data_root(repo_root: Path, owner: str) -> Path:
    descriptors = list((repo_root / "Plugins").glob(f"*/{owner}/{owner}.uplugin"))
    if len(descriptors) != 1:
        raise CompilerError("world_data_owner_invalid", "World-data plugin descriptor is missing or ambiguous", owner=owner)
    descriptor = read_json(descriptors[0])
    if descriptor.get("CanContainContent") is not True:
        raise CompilerError("world_data_owner_invalid", "Canonical authority owner must contain content", owner=owner)
    return descriptors[0].parent / "Data"


def _owned_path(repo_root: Path, data_root: Path, relative: str, category: str) -> Path:
    path = resolve_owned_path(repo_root, relative)
    if not path.is_relative_to(data_root.resolve()):
        raise CompilerError("territory_contract_owner_mismatch", "Territory contract belongs to another data owner", category=category)
    return path


def validate_profile_ownership(repo_root: Path, profile_path: Path, profile: dict[str, Any]) -> Path:
    data_root = owner_data_root(repo_root, profile["world_data_plugin"]).resolve()
    expected_parent = data_root / "Profiles" / "CanonicalCompilation"
    if profile_path.resolve().parent != expected_parent:
        raise CompilerError("compiler_profile_owner_mismatch", "Compiler profile is outside its owner data root")
    for field in ("source_profile", "authored_overlay", "fixture_features", "budget_profile", "control_profile"):
        relative = profile.get(field)
        if relative is not None:
            _owned_path(repo_root, data_root, relative, field)
    return data_root


def load_budget(repo_root: Path, profile: dict[str, Any]) -> tuple[dict[str, Any], Path] | None:
    relative = profile.get("budget_profile")
    if relative is None:
        return None
    path = resolve_owned_path(repo_root, relative)
    budget = read_json(path)
    validate_document(budget, path)
    identities = {
        budget.get("budget_id"), budget.get("compiler_profile_id"), budget.get("validation_profile_id")
    }
    if identities != {profile["profile_id"]} or budget.get("source_profile_id") != profile["source_profile_id"]:
        raise CompilerError("promotion_budget_mismatch", "Territory budget identities differ from compiler profile")
    return budget, path


def current_external_inputs(
    repo_root: Path,
    profile_path: Path,
    profile: dict[str, Any],
    include_source_runtime: bool = True,
) -> dict[str, Any]:
    validate_profile_ownership(repo_root, profile_path, profile)
    source_path = resolve_owned_path(repo_root, profile["source_profile"])
    source = read_source_json(source_path)
    if include_source_runtime:
        validate_profile(source, source_path, repo_root)
    if source["profile_id"] != profile["source_profile_id"]:
        raise CompilerError("territory_source_mismatch", "Source profile identity differs from compiler profile")
    overlay_path = resolve_owned_path(repo_root, profile["authored_overlay"])
    fixture_path = resolve_owned_path(repo_root, profile["fixture_features"]) if profile.get("fixture_features") else None
    budget_record = load_budget(repo_root, profile)
    control_path = resolve_owned_path(repo_root, profile["control_profile"]) if profile.get("control_profile") else None
    identity = {
        "source_profile_path": profile["source_profile"],
        "source_profile_sha256": file_hash(source_path),
        "authored_overlay_path": profile["authored_overlay"],
        "authored_overlay_sha256": file_hash(overlay_path),
        "fixture_features_path": profile.get("fixture_features"),
        "fixture_features_sha256": file_hash(fixture_path) if fixture_path else None,
        "budget_profile_path": profile.get("budget_profile"),
        "budget_profile_sha256": file_hash(budget_record[1]) if budget_record else None,
        "control_profile_path": profile.get("control_profile"),
        "control_profile_sha256": file_hash(control_path) if control_path else None,
    }
    if include_source_runtime:
        identity["source_run_inputs_hash"] = source_run_contract(repo_root, source)["run_inputs_hash"]
    return identity


def _technical_bounds(profile: dict[str, Any]) -> list[float]:
    bounds = [cell_bounds(profile["grid"], item["x"], item["y"]) for item in profile["target_cells"]]
    return [
        min(item[0] for item in bounds),
        min(item[1] for item in bounds),
        max(item[2] for item in bounds),
        max(item[3] for item in bounds),
    ]


def _equal_coordinates(left: list[float], right: list[float], tolerance: float) -> bool:
    return len(left) == len(right) and all(abs(float(a) - float(b)) <= tolerance for a, b in zip(left, right))


def _write_immutable_json(
    path: Path,
    value: dict[str, Any],
    validator=validate_document,
    writer=write_json,
) -> None:
    if path.is_file():
        existing = read_json(path)
        validator(existing, path)
        if canonical_hash(existing) != canonical_hash(value):
            raise CompilerError("territory_admission_conflict", "Admission evidence differs at the same identity")
        return
    writer(path, value)


def admit_territory(repo_root: Path, profile_path: Path) -> tuple[dict[str, Any], Path]:
    profile = load_profile(profile_path)
    current_external_inputs(repo_root, profile_path, profile)
    if "budget_profile" not in profile or "control_profile" not in profile:
        raise CompilerError("territory_contract_missing", "Territory admission requires explicit budget and control profiles")

    source_path = resolve_owned_path(repo_root, profile["source_profile"])
    source = read_source_json(source_path)
    validate_profile(source, source_path, repo_root)
    budget_record = load_budget(repo_root, profile)
    if budget_record is None:
        raise CompilerError("promotion_budget_missing", "Territory admission requires a budget profile")
    budget, budget_path = budget_record
    control_path = resolve_owned_path(repo_root, profile["control_profile"])
    control = read_json(control_path)
    validate_document(control, control_path)

    if source["profile_id"] != profile["source_profile_id"]:
        raise CompilerError("territory_source_mismatch", "Source profile identity differs from compiler profile")
    if (
        control["control_network_id"] != profile["profile_id"]
        or control["source_profile_id"] != source["profile_id"]
        or control["compiler_profile_id"] != profile["profile_id"]
    ):
        raise CompilerError("territory_control_mismatch", "Control-network identities differ from territory profiles")
    coverage = source.get("projected_coverage")
    if not isinstance(coverage, dict):
        raise CompilerError("territory_coverage_missing", "Territory source has no projected coverage contract")
    if (
        control["horizontal_crs"] != source["area"]["crs"]
        or control["projected_crs"] != profile["grid"]["canonical_crs"]
        or control["vertical_datum"] != profile["grid"]["vertical_datum"]
        or control["transform_id"] != profile["grid"]["coordinate_transform"]
        or coverage["projected_crs"] != profile["grid"]["canonical_crs"]
    ):
        raise CompilerError("territory_geospatial_mismatch", "CRS, transform, or datum differs across territory contracts")

    derived_bounds = _technical_bounds(profile)
    tolerance = float(profile["grid"]["coordinate_quantization"])
    terrain_halo = (
        profile["grid"]["operation_halos"]["terrain_resampling"]
        * max(float(value) for value in profile["grid"]["sample_spacing"])
    )
    center = next(item for item in control["controls"] if item["control_id"] == control["product_center_control_id"])
    if (
        not _equal_coordinates(derived_bounds, coverage["technical_bounds"], tolerance)
        or abs(float(coverage["terrain_resampling_halo_m"]) - terrain_halo) > tolerance
        or [round(value) for value in center["expected_projected"]] != profile["engine_georeference_origin"]
    ):
        raise CompilerError("territory_geometry_mismatch", "Bounds, halo, or engine georeference origin differs")

    plan = build_plan(source, repo_root)
    cell_count = len(profile["target_cells"])
    for name, actual, unit in (
        ("source_bytes", plan["planned_network_bytes"], "bytes"),
        ("cells", cell_count, "canonical_cells"),
    ):
        ceiling = budget["ceilings"][name]
        if ceiling["kind"] != "hard" or ceiling["unit"] != unit:
            raise CompilerError("promotion_budget_invalid", "Territory admission budget has invalid semantics", ceiling=name)
        if actual > ceiling["max"]:
            raise CompilerError("promotion_budget_exceeded", "Territory admission exceeds its frozen budget", ceiling=name, actual=actual)

    control_receipt, control_receipt_path = qualify_control_network(repo_root, control_path)
    source_contract = source_run_contract(repo_root, source)
    plan_hash = canonical_hash(plan)
    contract_hash = canonical_hash({
        "source": file_hash(source_path),
        "compiler": file_hash(profile_path),
        "control": file_hash(control_path),
        "budget": file_hash(budget_path),
        "control_receipt": file_hash(control_receipt_path),
        "source_run_inputs": source_contract["run_inputs_hash"],
        "source_plan": plan_hash,
    })
    output_root = (
        repo_root / "tmp" / "world" / "canonical_compilation" / "territory_admission"
        / profile["profile_id"] / contract_hash
    )
    plan_path = output_root / "source_plan.json"
    copied_control_path = output_root / "control_network_receipt.json"
    _write_immutable_json(plan_path, plan, validate_source_document, write_source_json)
    _write_immutable_json(copied_control_path, control_receipt)
    receipt = {
        "$schema": RECEIPT_SCHEMA,
        "schema_version": 1,
        "operation": "admit_territory",
        "status": "accepted",
        "profile_id": profile["profile_id"],
        "contract_hash": contract_hash,
        "source_profile_sha256": file_hash(source_path),
        "compiler_profile_sha256": file_hash(profile_path),
        "control_profile_sha256": file_hash(control_path),
        "budget_profile_sha256": file_hash(budget_path),
        "control_receipt_sha256": file_hash(control_receipt_path),
        "source_run_inputs_hash": source_contract["run_inputs_hash"],
        "source_plan_sha256": plan_hash,
        "cell_count": cell_count,
        "planned_source_bytes": plan["planned_network_bytes"],
        "technical_bounds": derived_bounds,
        "coverage": plan["coverage"],
        "evidence": [
            {"path": "source_plan.json", "sha256": file_hash(plan_path), "byte_size": plan_path.stat().st_size},
            {
                "path": "control_network_receipt.json",
                "sha256": file_hash(copied_control_path),
                "byte_size": copied_control_path.stat().st_size,
            },
        ],
        "errors": [],
    }
    output = output_root / "territory_admission_receipt.json"
    _write_immutable_json(output, receipt)
    return receipt, output
