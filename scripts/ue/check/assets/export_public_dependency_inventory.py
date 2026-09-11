"""Export package dependency evidence for a declared public developer seed set."""

import json
import os
from pathlib import Path

import unreal


def required_environment_path(name):
    value = os.environ.get(name, "").strip()
    if not value:
        raise RuntimeError(f"Missing required environment variable: {name}")
    return Path(value)


def main():
    seed_path = required_environment_path("ALIS_PUBLIC_DEPENDENCY_SEEDS")
    output_path = required_environment_path("ALIS_PUBLIC_DEPENDENCY_OUTPUT")
    seed_document = json.loads(seed_path.read_text(encoding="utf-8-sig"))
    roots = sorted({str(item["package_name"]) for item in seed_document["seeds"]})

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    project_root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    artifact_files = [
        str(project_root / Path(str(item["artifact_path"])))
        for item in seed_document["seeds"]
    ]
    registry.scan_files_synchronous(artifact_files, True)
    dependency_options = {
        "hard_package": unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=False,
            include_hard_package_references=True,
            include_searchable_names=False,
            include_soft_management_references=False,
            include_hard_management_references=False,
        ),
        "soft_package": unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True,
            include_hard_package_references=False,
            include_searchable_names=False,
            include_soft_management_references=False,
            include_hard_management_references=False,
        ),
        "hard_management": unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=False,
            include_hard_package_references=False,
            include_searchable_names=False,
            include_soft_management_references=False,
            include_hard_management_references=True,
        ),
        "soft_management": unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=False,
            include_hard_package_references=False,
            include_searchable_names=False,
            include_soft_management_references=True,
            include_hard_management_references=False,
        ),
    }
    queue = list(roots)
    seen = set()
    edges = []
    missing = []
    assets = []
    while queue:
        package = queue.pop(0)
        if package in seen:
            continue
        seen.add(package)
        package_assets = list(registry.get_assets_by_package_name(package) or [])
        if not package_assets:
            missing.append(package)
        else:
            assets.append(
                {
                    "package_name": package,
                    "asset_classes": sorted(
                        {str(asset.asset_class_path.asset_name) for asset in package_assets}
                    ),
                }
            )
        dependencies = set()
        for dependency_kind, options in dependency_options.items():
            for dependency in sorted(
                {str(value) for value in (registry.get_dependencies(package, options) or [])}
            ):
                edges.append({"from": package, "to": dependency, "kind": dependency_kind})
                dependencies.add(dependency)
        for dependency in dependencies:
            if dependency not in seen and not dependency.startswith(("/Engine/", "/Script/")):
                queue.append(dependency)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "roots": roots,
                "packages": sorted(seen),
                "assets": sorted(assets, key=lambda item: item["package_name"]),
                "edges": edges,
                "missing_packages": sorted(missing),
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    unreal.log(
        f"[PublicDependencyInventory] roots={len(roots)} packages={len(seen)} "
        f"edges={len(edges)} missing={len(missing)} output={output_path}"
    )


main()
