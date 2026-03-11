"""
Quick Test Script - Blueprint to JSON Export
Paste this directly into UE Python console
"""
import unreal
import json
import os
from datetime import datetime

# ===== QUICK EXPORT FUNCTION =====

def quick_export_bp(asset_path_str):
    """One-function export for testing"""

    # Load Blueprint
    asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path_str)
    if not asset_data.is_valid():
        unreal.log_error(f"Asset not found: {asset_path_str}")
        return None

    generated_class_path = asset_data.get_tag_value("GeneratedClass")
    if not generated_class_path:
        unreal.log_error(f"Not a Blueprint: {asset_path_str}")
        return None

    generated_class = unreal.load_object(None, generated_class_path)
    cdo = generated_class.get_default_object()

    # Get properties via reflection
    result = {
        "asset_path": asset_path_str,
        "class_name": generated_class.get_name(),
        "properties": {}
    }

    # Method 1: Try get_editor_property_names (may not exist)
    property_names = []
    try:
        if hasattr(cdo, 'get_editor_property_names'):
            property_names = list(cdo.get_editor_property_names())
            unreal.log(f"✅ Found {len(property_names)} properties via get_editor_property_names()")
    except Exception as e:
        unreal.log_warning(f"get_editor_property_names() failed: {e}")

    # Method 2: Fallback to dir() filtering
    if not property_names:
        all_attrs = dir(cdo)
        property_names = [
            attr for attr in all_attrs
            if not attr.startswith('_')  # Skip private
            and not callable(getattr(cdo, attr, None))  # Skip methods
            and attr not in ['class', 'name']  # Skip meta
        ]
        unreal.log(f"⚠️ Using dir() fallback: {len(property_names)} candidates")

    # Export properties
    exported_count = 0
    for prop_name in property_names:
        try:
            value = cdo.get_editor_property(prop_name)

            # Serialize value
            if isinstance(value, (int, float, bool, str)) or value is None:
                serialized = value
            elif hasattr(value, "x") and hasattr(value, "y"):
                serialized = {
                    "x": value.x,
                    "y": value.y,
                    "z": getattr(value, "z", None),
                    "w": getattr(value, "w", None)
                }
                # Remove None values
                serialized = {k: v for k, v in serialized.items() if v is not None}
            elif isinstance(value, unreal.Object):
                try:
                    serialized = {
                        "_type": "AssetReference",
                        "path": unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(value)
                    }
                except:
                    serialized = str(value)
            else:
                serialized = str(value)

            result["properties"][prop_name] = {
                "value": serialized,
                "type": type(value).__name__
            }
            exported_count += 1

        except Exception as e:
            # Skip properties that can't be accessed
            continue

    unreal.log(f"✅ Successfully exported {exported_count} properties")

    # Save to file
    output_dir = os.path.join(
        unreal.SystemLibrary.get_project_directory(),
        "Saved",
        "AI_Snapshots"
    )
    os.makedirs(output_dir, exist_ok=True)

    name = asset_path_str.split("/")[-1].split(".")[0]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    json_path = os.path.join(output_dir, f"{name}_{timestamp}.json")

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    unreal.log(f"✅ Saved to: {json_path}")
    print(f"\n📄 Preview (first 20 lines):")
    print(json.dumps(result, indent=2, ensure_ascii=False)[:1000])

    return json_path

# ===== USAGE =====
# Paste into UE Python console:

# For selected asset in Content Browser:
# selected = unreal.EditorUtilityLibrary.get_selected_assets()
# if selected:
#     asset_path = unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(selected[0])
#     quick_export_bp(asset_path)

# For specific asset:
# quick_export_bp("/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child")
