# COPY-PASTE TEST - Copy everything below this line
# ===============================================

import unreal
import json
import os
from datetime import datetime

# === CONFIGURATION ===
TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 60)
print("[TOOL] Blueprint -> JSON Test")
print("=" * 60)

# === EXPORT ===
print("\n[PACKAGE] Step 1: Loading Blueprint...")
try:
    asset_data = unreal.EditorAssetLibrary.find_asset_data(TEST_ASSET)
    if not asset_data.is_valid():
        print(f"[FAIL] ERROR: Blueprint not found: {TEST_ASSET}")
        print("[NOTE] Change TEST_ASSET to the path of your Blueprint")
    else:
        gen_class = unreal.load_object(None, asset_data.get_tag_value("GeneratedClass"))
        cdo = gen_class.get_default_object()
        print(f"[OK] Loaded: {gen_class.get_name()}")

        # === COLLECTING PROPERTIES ===
        print("\n[SEARCH] Step 2: Exporting properties...")

        # FIXED: Use the correct method for getting properties
        props = {}

        # Attempt 1: get_editor_property_names (if available)
        prop_names = []
        if hasattr(cdo, 'get_editor_property_names'):
            try:
                prop_names = list(cdo.get_editor_property_names())
                print(f"   Method: get_editor_property_names() -> {len(prop_names)} properties")
            except:
                pass

        # Attempt 2: dir() + filtering
        if not prop_names:
            all_attrs = [a for a in dir(cdo) if not a.startswith('_')]
            prop_names = all_attrs
            print(f"   Method: dir() -> {len(prop_names)} candidates")

        # Export properties
        for prop_name in prop_names:
            try:
                value = cdo.get_editor_property(prop_name)

                # Serialize different types
                if isinstance(value, (int, float, bool, str)) or value is None:
                    serialized = value
                elif hasattr(value, "x") and hasattr(value, "y"):
                    # Vector-like types
                    serialized = {"x": value.x, "y": value.y}
                    if hasattr(value, "z"):
                        serialized["z"] = value.z
                    if hasattr(value, "w"):
                        serialized["w"] = value.w
                elif isinstance(value, unreal.Object):
                    # Asset references
                    try:
                        serialized = {
                            "_type": "AssetReference",
                            "path": unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(value)
                        }
                    except:
                        serialized = str(value)
                elif isinstance(value, (list, tuple)):
                    # Arrays (simple values only)
                    serialized = [str(v) for v in value]
                else:
                    # Fallback
                    serialized = str(value)

                props[prop_name] = {
                    "value": serialized,
                    "type": type(value).__name__
                }

            except Exception:
                # Property not accessible via editor API
                continue

        print(f"[OK] Exported: {len(props)} properties")

        # === SAVING ===
        print("\n[SAVE] Step 3: Saving JSON...")
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        json_path = os.path.join(OUTPUT_DIR, f"test_export_{timestamp}.json")

        with open(json_path, "w", encoding="utf-8") as f:
            json.dump({
                "asset_path": TEST_ASSET,
                "class_name": gen_class.get_name(),
                "exported_at": datetime.utcnow().isoformat() + "Z",
                "properties": props
            }, f, indent=2, ensure_ascii=False)

        print(f"[OK] Saved: {json_path}")

        # === PREVIEW ===
        print("\n[PREVIEW] First 10 properties:")
        print("-" * 60)
        for i, (k, v) in enumerate(list(props.items())[:10]):
            val_str = str(v['value'])
            if len(val_str) > 40:
                val_str = val_str[:37] + "..."
            print(f"  {k}: {val_str}")

        if len(props) > 10:
            print(f"  ... and {len(props) - 10} properties")

        print("\n" + "=" * 60)
        print("[OK] TEST COMPLETED SUCCESSFULLY!")
        print("=" * 60)
        print(f"\n[DIR] JSON file: {json_path}")
        print(f"[STATS] Properties exported: {len(props)}")
        print("\n[TIP] Next step: open the JSON file and send it to Claude for editing")

except Exception as e:
    print(f"\n[FAIL] ERROR: {e}")
    print("\n[TOOL] Possible solutions:")
    print("1. Check that the Blueprint path is correct")
    print("2. Make sure the Blueprint is compiled")
    print("3. Check the Output Log for errors")
