"""
Blueprint → JSON Exporter
Exports Blueprint CDO properties to JSON for AI editing
"""
import unreal
import json
import os
from datetime import datetime

# ------------------------------------------------
# Configuration
# ------------------------------------------------

OUTPUT_DIR = os.path.join(
    unreal.SystemLibrary.get_project_directory(),
    "Saved",
    "AI_Snapshots"
)

# ------------------------------------------------
# Serialization Helpers
# ------------------------------------------------

def ensure_dir(path):
    """Create directory if it doesn't exist"""
    if not os.path.exists(path):
        os.makedirs(path)

def serialize_value(value):
    """Convert UE types to JSON-serializable format"""
    if isinstance(value, (int, float, bool, str)) or value is None:
        return value

    # FVector / FRotator / FColor / FLinearColor
    if hasattr(value, "x") and hasattr(value, "y"):
        data = {"x": value.x, "y": value.y}
        if hasattr(value, "z"):
            data["z"] = value.z
        if hasattr(value, "w"):
            data["w"] = value.w
        return data

    # Asset references
    if isinstance(value, unreal.Object):
        try:
            return {
                "_type": "AssetReference",
                "path": unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(value)
            }
        except Exception:
            return {"_type": "Object", "value": str(value)}

    # Arrays
    if isinstance(value, (list, tuple)):
        return [serialize_value(v) for v in value]

    # Fallback to string
    return str(value)

# ------------------------------------------------
# Blueprint CDO Export
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

def export_blueprint_to_json(asset_path):
    """Export Blueprint CDO properties to JSON"""
    cdo, generated_class = get_cdo_from_blueprint(asset_path)

    data = {
        "asset_path": asset_path,
        "asset_type": "Blueprint",
        "class_name": generated_class.get_name(),
        "parent_class": generated_class.get_super_class().get_name() if generated_class.get_super_class() else None,
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "properties": {}
    }

    # Export all editor-visible properties
    # FIXED: Call get_editor_property_names() on CDO instance, not class
    try:
        prop_names = dir(cdo)  # Fallback if get_editor_property_names not available
        if hasattr(cdo, 'get_editor_property_names'):
            prop_names = cdo.get_editor_property_names()
    except Exception as e:
        unreal.log_warning(f"Failed to get property names: {e}")
        prop_names = []

    for prop_name in prop_names:
        # Skip private/internal properties
        if prop_name.startswith('_'):
            continue

        try:
            value = cdo.get_editor_property(prop_name)
            data["properties"][prop_name] = {
                "value": serialize_value(value),
                "editable": True
            }
        except Exception as e:
            # Property might not be accessible via editor API
            continue

    return data

def save_json(data, custom_path=None):
    """Save exported data to JSON file"""
    ensure_dir(OUTPUT_DIR)

    if custom_path:
        path = custom_path
    else:
        name = data["asset_path"].split("/")[-1].split(".")[0]
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        path = os.path.join(OUTPUT_DIR, f"{name}_{timestamp}.json")

    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

    unreal.log(f"✅ Blueprint exported to JSON: {path}")
    return path

# ------------------------------------------------
# CLI Interface
# ------------------------------------------------

def export_selected_blueprints():
    """Export currently selected Blueprints in Content Browser"""
    selected_assets = unreal.EditorUtilityLibrary.get_selected_assets()

    if not selected_assets:
        unreal.log_warning("No assets selected in Content Browser")
        return

    exported = []
    for asset in selected_assets:
        if isinstance(asset, unreal.Blueprint):
            asset_path = unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(asset)
            try:
                data = export_blueprint_to_json(asset_path)
                json_path = save_json(data)
                exported.append(json_path)
            except Exception as e:
                unreal.log_error(f"Failed to export {asset_path}: {e}")

    if exported:
        unreal.log(f"✅ Exported {len(exported)} Blueprint(s)")
    else:
        unreal.log_warning("No Blueprints were exported")

# ------------------------------------------------
# Example Usage
# ------------------------------------------------

if __name__ == "__main__":
    # Example: Export specific Blueprint
    EXAMPLE_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"

    try:
        snapshot = export_blueprint_to_json(EXAMPLE_ASSET)
        save_json(snapshot)
    except Exception as e:
        unreal.log_error(f"Export failed: {e}")
