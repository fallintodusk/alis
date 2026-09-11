#!/usr/bin/env python3
"""Plan and validate the required public developer package dependency closure."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any


REQUIRED_OWNERS = {"ProjectWorldData", "ProjectExperienceData", "ProjectMaterial"}
PUBLIC_ROOTS = {
    "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
    "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
}


class AuditError(RuntimeError):
    pass


def read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise AuditError(f"Expected JSON object: {path}")
    return value


def read_unreal_descriptor(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8-sig")
    text = re.sub(r",(?=\s*[}\]])", "", text)
    value = json.loads(text)
    if not isinstance(value, dict):
        raise AuditError(f"Expected Unreal descriptor object: {path}")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_identity(repo_root: Path) -> tuple[str | None, str | None]:
    values = []
    for revision in ("HEAD", "HEAD^{tree}"):
        result = subprocess.run(
            ["git", "-c", "core.fsmonitor=false", "-C", str(repo_root), "rev-parse", revision],
            capture_output=True,
            text=True,
            check=False,
        )
        values.append(result.stdout.strip() if result.returncode == 0 else None)
    return values[0], values[1]


def load_composer(repo_root: Path):
    path = repo_root / "scripts" / "git" / "mirror" / "compose_developer_payload.py"
    spec = importlib.util.spec_from_file_location("alis_developer_payload_composer", path)
    if spec is None or spec.loader is None:
        raise AuditError(f"Could not load payload composer: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def package_from_artifact(path: str) -> str | None:
    normalized = path.replace("\\", "/")
    match = re.fullmatch(r"Plugins/[^/]+/([^/]+)/Content/(.+)\.(uasset|umap)", normalized, re.IGNORECASE)
    if not match:
        return None
    return f"/{match.group(1)}/{match.group(2)}"


def build_seed_plan(repo_root: Path) -> dict[str, Any]:
    composer = load_composer(repo_root)
    entries = {}
    composer.collect_manifest_authority(repo_root, "ProjectWorldData", entries)
    public_authorities = composer.collect_public_asset_authority(repo_root, entries)
    authority_by_owner = {item["owner"]: item for item in public_authorities}
    world_root = repo_root / "Plugins" / "World" / "ProjectWorldData"
    active = read_json(world_root / "Data" / "Manifests" / "active_set.json")
    required_world_artifacts = {}
    required_scope_prefixes = (
        "layer_kazan_territory_",
        "map_territory_l_projectworldkazanterritory",
        "layer_manhattan_showcase_",
        "map_showcase_manhattan_l_projectworldmanhattanshowcase",
    )
    for scope in active.get("scopes", []):
        if not str(scope.get("scope_id", "")).startswith(required_scope_prefixes):
            continue
        manifest = read_json(world_root / "Data" / "Manifests" / str(scope["manifest_path"]))
        for item in manifest.get("artifacts", []):
            required_world_artifacts[str(item["path"])] = str(scope["scope_id"])
    seeds = []
    for entry in entries.values():
        if entry.owner not in REQUIRED_OWNERS:
            continue
        if entry.owner == "ProjectWorldData" and entry.path not in required_world_artifacts:
            continue
        package_name = package_from_artifact(entry.path)
        if not package_name:
            continue
        authority = authority_by_owner.get(entry.owner, {})
        seeds.append(
            {
                "package_name": package_name,
                "owner": entry.owner,
                "artifact_path": entry.path,
                "artifact_sha256": entry.sha256,
                "authority_path": authority.get(
                    "manifest_path",
                    "Plugins/World/ProjectWorldData/Data/Manifests/active_set.json",
                ),
                "distribution_class": authority.get(
                    "distribution_class", "owner_manifest_with_canonical_attribution"
                ),
                "provenance": "owner_manifest_selected",
                "scope_id": required_world_artifacts.get(entry.path, entry.owner),
            }
        )
    seeds.sort(key=lambda item: item["package_name"])
    packages = {item["package_name"] for item in seeds}
    missing_roots = sorted(PUBLIC_ROOTS - packages)
    if missing_roots:
        raise AuditError(f"Required public roots are absent from accepted authority: {missing_roots}")
    if any("hlod" in item["package_name"].lower() for item in seeds):
        raise AuditError("HLOD package entered the public dependency seed set")
    return {"schema_version": 1, "public_roots": sorted(PUBLIC_ROOTS), "seeds": seeds}


def project_mounts_and_modules(repo_root: Path) -> tuple[set[str], set[str]]:
    mounts = {"Game"}
    modules = set()
    descriptors = [repo_root / "Alis.uproject", *repo_root.glob("Plugins/*/*/*.uplugin")]
    for path in descriptors:
        descriptor = read_json(path)
        if path.suffix == ".uplugin":
            mounts.add(path.stem)
        for module in descriptor.get("Modules", []):
            name = str(module.get("Name", ""))
            if name:
                modules.add(name)
    return mounts, modules


def engine_mounts_and_modules(engine_root: Path) -> tuple[set[str], set[str]]:
    mounts = {"Engine"}
    modules = set()
    for path in (engine_root / "Engine" / "Plugins").rglob("*.uplugin"):
        descriptor = read_unreal_descriptor(path)
        mounts.add(path.stem)
        for module in descriptor.get("Modules", []):
            name = str(module.get("Name", ""))
            if name:
                modules.add(name)
    for path in (engine_root / "Engine" / "Source").rglob("*.Build.cs"):
        modules.add(path.name.removesuffix(".Build.cs"))
    return mounts, modules


def classify_dependency(
    package_name: str,
    selected: dict[str, dict[str, Any]],
    project_mounts: set[str],
    project_modules: set[str],
    engine_mounts: set[str] | None = None,
    engine_modules: set[str] | None = None,
) -> dict[str, str]:
    engine_mounts = engine_mounts or {"Engine"}
    engine_modules = engine_modules or set()
    if package_name in selected:
        seed = selected[package_name]
        return {
            "owner": seed["owner"],
            "authority": seed["authority_path"],
            "provenance": seed["provenance"],
            "distribution_class": seed["distribution_class"],
            "disposition": "approved_payload",
            "reason": "Selected by the owning accepted manifest.",
        }
    if package_name.startswith("/Engine/"):
        return {
            "owner": "UnrealEngine",
            "authority": "local_launcher_engine",
            "provenance": "engine_installed",
            "distribution_class": "not_redistributed",
            "disposition": "local_engine",
            "reason": "Provided by the developer's launcher-installed Unreal Engine.",
        }
    if package_name.startswith("/Script/"):
        module = package_name.split("/", 2)[2]
        project_owned = module in project_modules
        if not project_owned and engine_modules and module not in engine_modules:
            return {
                "owner": module,
                "authority": "none",
                "provenance": "unclassified",
                "distribution_class": "rejected",
                "disposition": "rejected_unknown_script_module",
                "reason": "Required script module is neither public project source nor configured Unreal Engine source.",
            }
        return {
            "owner": module if project_owned else "UnrealEngine",
            "authority": "public_source" if project_owned else "local_launcher_engine",
            "provenance": "project_source" if project_owned else "engine_installed",
            "distribution_class": "MPL-2.0" if project_owned else "not_redistributed",
            "disposition": "public_source" if project_owned else "local_engine",
            "reason": "Compiled from public source." if project_owned else "Provided by Unreal Engine.",
        }
    parts = PurePosixPath(package_name.lstrip("/")).parts
    mount = parts[0] if parts else ""
    if mount in project_mounts:
        return {
            "owner": mount,
            "authority": "none",
            "provenance": "unclassified",
            "distribution_class": "rejected",
            "disposition": "rejected_required_project_content",
            "reason": "Required project content is not selected by an approved payload authority.",
        }
    if mount in engine_mounts:
        return {
            "owner": "UnrealEngine",
            "authority": "local_launcher_engine",
            "provenance": "engine_plugin_installed",
            "distribution_class": "not_redistributed",
            "disposition": "local_engine",
            "reason": "Resolved from the configured Unreal Engine, not the project checkout.",
        }
    return {
        "owner": mount or "unknown",
        "authority": "none",
        "provenance": "unclassified",
        "distribution_class": "rejected",
        "disposition": "rejected_unknown_mount",
        "reason": "Required package mount is neither public project source nor the configured Unreal Engine.",
    }


def validate_inventory(
    repo_root: Path,
    seed_path: Path,
    inventory_path: Path,
    engine_root: Path | None = None,
) -> dict[str, Any]:
    seed_document = read_json(seed_path)
    inventory = read_json(inventory_path)
    selected = {item["package_name"]: item for item in seed_document.get("seeds", [])}
    expected_roots = set(selected)
    actual_roots = set(inventory.get("roots", []))
    if actual_roots != expected_roots:
        raise AuditError("Unreal dependency roots do not match the accepted seed set")
    missing = set(inventory.get("missing_packages", []))
    packages = set(inventory.get("packages", []))
    if not expected_roots.issubset(packages):
        raise AuditError("Unreal dependency inventory omitted accepted seed packages")
    project_mounts, project_modules = project_mounts_and_modules(repo_root)
    engine_mounts, engine_modules = (
        engine_mounts_and_modules(engine_root) if engine_root else ({"Engine"}, set())
    )
    dependencies = []
    issues = []
    for package_name in sorted({edge["to"] for edge in inventory.get("edges", [])} | expected_roots):
        if "hlod" in package_name.lower():
            issues.append({"code": "hlod_forbidden", "package_name": package_name})
        classification = classify_dependency(
            package_name,
            selected,
            project_mounts,
            project_modules,
            engine_mounts,
            engine_modules,
        )
        if package_name in missing:
            classification = {
                **classification,
                "disposition": "rejected_missing_package",
                "reason": "Unreal Asset Registry could not resolve the required package.",
            }
        if classification["disposition"].startswith("rejected_"):
            issues.append(
                {
                    "code": classification["disposition"],
                    "package_name": package_name,
                    "reason": classification["reason"],
                }
            )
        dependencies.append({"package_name": package_name, **classification})
    source_revision, source_tree = source_identity(repo_root)
    return {
        "schema_version": 1,
        "status": "accepted" if not issues else "rejected",
        "source_revision": source_revision,
        "source_tree": source_tree,
        "public_roots": seed_document["public_roots"],
        "seed_sha256": sha256_file(seed_path),
        "inventory_sha256": sha256_file(inventory_path),
        "selected_package_count": len(selected),
        "closure_package_count": len(packages),
        "dependency_edge_count": len(inventory.get("edges", [])),
        "hlod_package_count": sum(issue["code"] == "hlod_forbidden" for issue in issues),
        "issues": issues,
        "dependencies": dependencies,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan_parser = subparsers.add_parser("plan")
    plan_parser.add_argument("--repo-root", type=Path, required=True)
    plan_parser.add_argument("--output", type=Path, required=True)
    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("--repo-root", type=Path, required=True)
    validate_parser.add_argument("--seeds", type=Path, required=True)
    validate_parser.add_argument("--inventory", type=Path, required=True)
    validate_parser.add_argument("--output", type=Path, required=True)
    validate_parser.add_argument("--engine-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "plan":
            result = build_seed_plan(args.repo_root.resolve())
        else:
            result = validate_inventory(
                args.repo_root.resolve(),
                args.seeds.resolve(),
                args.inventory.resolve(),
                args.engine_root.resolve(),
            )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (AuditError, OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    if args.command == "validate" and result.get("status") != "accepted":
        print(f"[FAIL] Developer dependency validate rejected: {args.output}", file=sys.stderr)
        return 1
    print(f"[OK] Developer dependency {args.command}: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
