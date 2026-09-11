#!/usr/bin/env python3
"""Verify ProjectMaterial recipe semantics against its accepted manifest."""

from __future__ import annotations

import hashlib
import json
import math
import re
import struct
from pathlib import Path, PurePosixPath
from typing import Any


class MaterialAuthorityError(RuntimeError):
    pass


def _read_object(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise MaterialAuthorityError(f"Invalid ProjectMaterial JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise MaterialAuthorityError(f"Expected a JSON object: {path}")
    return value


def _number(value: Any, name: str, minimum: float, maximum: float) -> str:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise MaterialAuthorityError(f"Material scalar is not finite: {name}")
    numeric = float(value)
    if numeric < minimum or numeric > maximum:
        raise MaterialAuthorityError(f"Material scalar is out of range: {name}")
    return format(numeric, ".17g")


def _vector(value: Any, name: str) -> str:
    if not isinstance(value, list) or len(value) != 4:
        raise MaterialAuthorityError(f"Material vector must contain four numbers: {name}")
    # FLinearColor stores recipe components as float before the C++ owner
    # serializes its normalized semantic identity.
    components = [struct.unpack("f", struct.pack("f", float(component)))[0] for component in value]
    return ",".join(_number(component, name, 0.0, 1.0) for component in components)


def compute_recipe_sha256(recipe_path: Path, recipe_root: Path) -> str:
    recipe = _read_object(recipe_path)
    allowed = {
        "$schema", "schema_version", "material_id", "artifact_kind", "family",
        "archetype", "compiler_version", "parent", "scalars", "vectors",
    }
    unknown = set(recipe) - allowed
    if unknown:
        raise MaterialAuthorityError(f"Unknown ProjectMaterial recipe fields: {sorted(unknown)}")

    expected = {
        "$schema": "../../Schemas/material-recipe.schema.json",
        "schema_version": "1",
        "family": "surface_opaque",
        "archetype": "landscape_basic_v1",
        "compiler_version": "1",
    }
    for field, value in expected.items():
        if recipe.get(field) != value:
            raise MaterialAuthorityError(f"Unsupported ProjectMaterial {field}: {recipe.get(field)!r}")

    material_id = recipe.get("material_id")
    kind = recipe.get("artifact_kind")
    if not isinstance(material_id, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", material_id):
        raise MaterialAuthorityError("ProjectMaterial material_id is not a safe identifier")
    if kind == "parent":
        if not material_id.startswith("M_") or material_id.startswith("MI_") or "parent" in recipe:
            raise MaterialAuthorityError("ProjectMaterial parent identity is invalid")
        parent = ""
    elif kind == "instance":
        parent = recipe.get("parent")
        if not material_id.startswith("MI_") or not isinstance(parent, str) or not parent:
            raise MaterialAuthorityError("ProjectMaterial instance identity is invalid")
    else:
        raise MaterialAuthorityError("ProjectMaterial artifact_kind is unsupported")

    try:
        relative = recipe_path.resolve().relative_to(recipe_root.resolve())
    except ValueError as error:
        raise MaterialAuthorityError(f"ProjectMaterial recipe escapes its owner root: {recipe_path}") from error
    if relative.suffixes[-2:] != [".material", ".json"] or len(relative.parts) < 2:
        raise MaterialAuthorityError("ProjectMaterial recipe must live in a named family folder")
    folder = PurePosixPath(*relative.parts[:-1]).as_posix()

    scalars = recipe.get("scalars")
    vectors = recipe.get("vectors")
    if not isinstance(scalars, dict) or set(scalars) != {"Roughness", "SlopeContrast"}:
        raise MaterialAuthorityError("ProjectMaterial scalar set is incomplete or unsupported")
    if not isinstance(vectors, dict) or set(vectors) != {"LowSlopeColor", "SteepSlopeColor"}:
        raise MaterialAuthorityError("ProjectMaterial vector set is incomplete or unsupported")

    normalized = (
        f"schema=1|id={material_id}|kind={kind}|family=surface_opaque|"
        f"archetype=landscape_basic_v1|compiler=1|parent={parent}|folder={folder}"
    )
    for name in sorted(scalars):
        limits = (0.0, 1.0) if name == "Roughness" else (0.01, 16.0)
        normalized += f"|s:{name}={_number(scalars[name], name, *limits)}"
    for name in sorted(vectors):
        normalized += f"|v:{name}={_vector(vectors[name], name)}"
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()


def verify_material_authority(recipe_root: Path, manifest_path: Path) -> dict[str, str]:
    manifest = _read_object(manifest_path)
    if manifest.get("schema_version") != "1" or not isinstance(manifest.get("records"), list):
        raise MaterialAuthorityError("ProjectMaterial manifest schema is unsupported")
    accepted: dict[str, str] = {}
    for record in manifest["records"]:
        if not isinstance(record, dict) or not isinstance(record.get("recipe_path"), str):
            raise MaterialAuthorityError("ProjectMaterial manifest record is malformed")
        relative = PurePosixPath(record["recipe_path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise MaterialAuthorityError("ProjectMaterial manifest recipe path is unsafe")
        path = recipe_root.joinpath(*relative.parts)
        actual = compute_recipe_sha256(path, recipe_root)
        expected = record.get("recipe_sha256")
        if actual != expected:
            raise MaterialAuthorityError(f"ProjectMaterial recipe authority is stale: {relative.as_posix()}")
        if relative.as_posix() in accepted:
            raise MaterialAuthorityError(f"Duplicate ProjectMaterial recipe: {relative.as_posix()}")
        accepted[relative.as_posix()] = actual

    actual_paths = {
        path.relative_to(recipe_root).as_posix()
        for path in recipe_root.rglob("*.material.json")
    }
    if actual_paths != set(accepted):
        raise MaterialAuthorityError("ProjectMaterial recipe authority is incomplete")
    return accepted
