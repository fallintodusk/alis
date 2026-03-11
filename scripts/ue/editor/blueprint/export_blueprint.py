# Blueprint to JSON Exporter - Based on diagnostics
# Uses dir(cdo) + smart filtering

import unreal
import json
import os
from datetime import datetime

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 80)
print("Blueprint → JSON Export")
print("=" * 80)

try:
    # Load Blueprint
    print("\nLoading Blueprint...")
    asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
    blueprint = unreal.Blueprint.cast(asset)
    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    print(f"Loaded: {blueprint.get_name()}")
    print(f"   Class: {gen_class.get_name()}")

    # Extract properties
    print("\nExtracting properties...")

    all_attrs = dir(cdo)
    print(f"   Total attributes in dir(cdo): {len(all_attrs)}")

    # Filter only properties (exclude methods and private)
    property_candidates = []
    for attr_name in all_attrs:
        # Skip private
        if attr_name.startswith('_'):
            continue

        # Skip known methods (check if callable)
        try:
            attr = getattr(cdo, attr_name)
            if callable(attr):
                continue
        except:
            pass

        property_candidates.append(attr_name)

    print(f"   Property candidates (non-methods): {len(property_candidates)}")

    # Try to read each property
    props = {}
    success_count = 0
    error_count = 0

    for prop_name in property_candidates:
        try:
            value = cdo.get_editor_property(prop_name)

            # Serialize
            if isinstance(value, (int, float, bool, str)) or value is None:
                serialized = value
            elif isinstance(value, unreal.Name):
                serialized = str(value)
            elif isinstance(value, unreal.Guid):
                serialized = str(value)
            elif hasattr(value, "x") and hasattr(value, "y"):
                # Vector-like
                serialized = {"x": float(value.x), "y": float(value.y)}
                if hasattr(value, "z"):
                    serialized["z"] = float(value.z)
                if hasattr(value, "w"):
                    serialized["w"] = float(value.w)
            elif isinstance(value, unreal.Object):
                # Asset reference
                try:
                    path = value.get_path_name() if value else None
                    serialized = {"_type": "AssetReference", "path": path} if path else None
                except:
                    serialized = str(value)
            elif isinstance(value, (list, tuple)) and len(value) == 0:
                # Empty arrays
                serialized = []
            elif isinstance(value, (list, tuple)):
                # Non-empty arrays (limit to 10)
                serialized = []
                for item in value[:10]:
                    try:
                        if isinstance(item, (int, float, bool, str)) or item is None:
                            serialized.append(item)
                        else:
                            serialized.append(str(item))
                    except:
                        serialized.append("...")
                if len(value) > 10:
                    serialized.append(f"... and {len(value) - 10} more items")
            else:
                # Fallback
                serialized = str(value)

            props[prop_name] = {
                "value": serialized,
                "type": type(value).__name__
            }
            success_count += 1

        except Exception as e:
            # Property is protected or not accessible
            error_count += 1
            continue

    print(f"   Success: {success_count}")
    print(f"   Protected/errors: {error_count}")

    # Save JSON
    print("\nSaving JSON...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_path = os.path.join(OUTPUT_DIR, f"blueprint_{timestamp}.json")

    result = {
        "asset_path": TEST_ASSET,
        "asset_type": "Blueprint",
        "class_name": gen_class.get_name(),
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "export_method": "dir(cdo) + smart filtering",
        "properties": props
    }

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"Saved: {json_path}")
    print(f"   Size: {os.path.getsize(json_path)} bytes")

    # Preview
    print("\nFirst 20 properties:")
    print("-" * 80)
    for i, (name, data) in enumerate(list(props.items())[:20], 1):
        val_str = str(data['value'])
        if len(val_str) > 50:
            val_str = val_str[:47] + "..."
        type_str = data.get('type', 'Unknown')
        print(f"  {i:2}. {name:30} = {val_str:50} [{type_str}]")

    if len(props) > 20:
        print(f"\n  ... and {len(props) - 20} more properties")

    # Group by type
    print("\nProperties by type:")
    type_counts = {}
    for prop_data in props.values():
        t = prop_data.get('type', 'Unknown')
        type_counts[t] = type_counts.get(t, 0) + 1

    for prop_type, count in sorted(type_counts.items(), key=lambda x: -x[1])[:10]:
        print(f"   {prop_type:20} : {count}")

    print("\n" + "=" * 80)
    print("EXPORT COMPLETE!")
    print("=" * 80)
    print(f"\nJSON file: {json_path}")
    print(f"Total properties: {len(props)}")
    print("\nNow send the JSON file to Claude for editing!")

except Exception as e:
    print(f"\nERROR: {e}")
    import traceback
    traceback.print_exc()
