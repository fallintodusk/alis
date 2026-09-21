"""Export Asset Registry evidence for generated definition assets."""

import json
import os
from pathlib import Path

import unreal


def tag_value(asset_data, name):
    try:
        value = asset_data.get_tag_value(name)
    except Exception:
        return None
    if value is None:
        return None
    return str(value)


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    records = []
    for asset in registry.get_all_assets(True):
        generated = tag_value(asset, "bGenerated")
        source_path = tag_value(asset, "SourceJsonPath")
        source_hash = tag_value(asset, "SourceJsonHash")
        if not source_path and generated not in {"True", "true", "1"}:
            continue
        records.append(
            {
                "asset_class": str(asset.asset_class_path.asset_name),
                "capability_cook_references": sorted(
                    path.strip()
                    for path in (tag_value(asset, "CapabilityCookReferences") or "").split(";")
                    if path.strip()
                ),
                "object_path": f"{asset.package_name}.{asset.asset_name}",
                "package_name": str(asset.package_name),
                "source_json_hash": source_hash,
                "source_json_path": source_path,
            }
        )

    records.sort(key=lambda item: item["package_name"])
    project_root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    configured_output = os.environ.get("ALIS_GENERATED_ASSET_INVENTORY_OUTPUT")
    output = (
        Path(configured_output)
        if configured_output
        else project_root / "Saved" / "Inspection" / "generated_asset_inventory.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps({"schema_version": 1, "assets": records}, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    unreal.log(f"[GeneratedAssetInventory] Exported {len(records)} assets to {output}")


main()
