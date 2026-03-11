"""
JSON to Blueprint Importer
Applies AI-edited JSON properties back to Blueprint CDO
"""
import unreal
import json
import os

# ------------------------------------------------
# Configuration
# ------------------------------------------------

INPUT_DIR = os.path.join(
    unreal.SystemLibrary.get_project_directory(),
    "Saved",
    "AI_Snapshots"
)

# ------------------------------------------------
# Deserialization Helpers
# ------------------------------------------------

def deserialize_value(value_data, property_type=None):
    """Convert JSON data back to UE types"""
    if isinstance(value_data, dict):
        # Asset reference
        if "_type" in value_data and value_data["_type"] == "AssetReference":
            try:
                return unreal.load_object(None, value_data["path"])
            except Exception as e:
                unreal.log_warning(f"Failed to load asset '{value_data['path']}': {e}")
                return None

        # Vector-like structures
        if "x" in value_data and "y" in value_data:
            if "w" in value_data:
                return unreal.Vector4(
                    value_data["x"],
                    value_data["y"],
                    value_data.get("z", 0),
                    value_data["w"]
                )
            elif "z" in value_data:
                return unreal.Vector(
                    value_data["x"],
                    value_data["y"],
                    value_data["z"]
                )
            else:
                return unreal.Vector2D(value_data["x"], value_data["y"])

        # Generic object - return as is
        return value_data

    # Arrays
    if isinstance(value_data, list):
        return [deserialize_value(v) for v in value_data]

    # Primitives
    return value_data

# ------------------------------------------------
# Blueprint CDO Import
# ------------------------------------------------

def get_cdo_from_blueprint(asset_path):
    """Load Blueprint and return its CDO + GeneratedClass"""
    asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if not asset_data.is_valid():
        raise RuntimeError(f"Asset not found: {asset_path}")

    generated_class_path = asset_data.get_tag_value("GeneratedClass")
    if not generated_class_path:
        raise RuntimeError(f"GeneratedClass tag not found for: {asset_path}")

    generated_class = unreal.load_object(None, generated_class_path)
    if not generated_class:
        raise RuntimeError(f"Failed to load GeneratedClass: {generated_class_path}")

    return generated_class.get_default_object(), generated_class

def import_json_to_blueprint(json_path, dry_run=False):
    """Apply JSON properties to Blueprint CDO

    Args:
        json_path: Path to JSON file
        dry_run: If True, only validate without applying changes

    Returns:
        dict with import results
    """
    # Load JSON
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    asset_path = data.get("asset_path")
    if not asset_path:
        raise RuntimeError("JSON missing 'asset_path' field")

    # Load Blueprint
    cdo, generated_class = get_cdo_from_blueprint(asset_path)

    results = {
        "asset_path": asset_path,
        "total_properties": len(data.get("properties", {})),
        "applied": 0,
        "skipped": 0,
        "errors": []
    }

    # Apply properties
    for prop_name, prop_data in data.get("properties", {}).items():
        if not prop_data.get("editable", True):
            results["skipped"] += 1
            continue

        try:
            value = deserialize_value(prop_data["value"])

            if dry_run:
                # Only validate property exists
                current_value = cdo.get_editor_property(prop_name)
                unreal.log(f"[DRY RUN] Would set '{prop_name}' = {value}")
            else:
                # Actually set the property
                cdo.set_editor_property(prop_name, value)
                unreal.log(f"[OK] Set '{prop_name}' = {value}")

            results["applied"] += 1

        except Exception as e:
            error_msg = f"Failed to set property '{prop_name}': {e}"
            unreal.log_error(error_msg)
            results["errors"].append(error_msg)
            continue

    # Mark Blueprint as modified (if not dry run)
    if not dry_run and results["applied"] > 0:
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        if blueprint:
            unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
            unreal.log(f"[OK] Saved Blueprint: {asset_path}")

    return results

# ------------------------------------------------
# CLI Interface
# ------------------------------------------------

def import_from_file(json_filename, dry_run=False):
    """Import JSON file by filename (searches in INPUT_DIR)"""
    json_path = os.path.join(INPUT_DIR, json_filename)

    if not os.path.exists(json_path):
        unreal.log_error(f"JSON file not found: {json_path}")
        return None

    try:
        results = import_json_to_blueprint(json_path, dry_run=dry_run)

        # Print summary
        unreal.log("=" * 60)
        unreal.log(f"Import Summary: {results['asset_path']}")
        unreal.log(f"  Total properties: {results['total_properties']}")
        unreal.log(f"  [OK] Applied: {results['applied']}")
        unreal.log(f"  [SKIP] Skipped: {results['skipped']}")
        unreal.log(f"  [ERR] Errors: {len(results['errors'])}")

        if results['errors']:
            unreal.log("\nErrors:")
            for error in results['errors']:
                unreal.log(f"  • {error}")

        unreal.log("=" * 60)

        return results

    except Exception as e:
        unreal.log_error(f"Import failed: {e}")
        return None

def list_available_snapshots():
    """List all JSON snapshots in INPUT_DIR"""
    if not os.path.exists(INPUT_DIR):
        unreal.log_warning(f"Snapshots directory not found: {INPUT_DIR}")
        return []

    snapshots = [f for f in os.listdir(INPUT_DIR) if f.endswith(".json")]

    if not snapshots:
        unreal.log("No JSON snapshots found")
        return []

    unreal.log(f"Available snapshots ({len(snapshots)}):")
    for i, snapshot in enumerate(snapshots, 1):
        unreal.log(f"  {i}. {snapshot}")

    return snapshots

# ------------------------------------------------
# Example Usage
# ------------------------------------------------

if __name__ == "__main__":
    # List available snapshots
    list_available_snapshots()

    # Example: Import specific JSON (dry run first)
    # import_from_file("BP_SunSky_Child_20250101_120000.json", dry_run=True)
    # import_from_file("BP_SunSky_Child_20250101_120000.json", dry_run=False)
