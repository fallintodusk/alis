# Export All Variables - Including Private Blueprint Variables
# Uses SimpleConstructionScript для поиска переменных

import unreal
import json
import os
from datetime import datetime

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 80)
print("🔧 EXPORT ALL VARIABLES (including Private)")
print("=" * 80)

try:
    # Load Blueprint
    print("\n📦 Loading...")
    asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
    blueprint = unreal.Blueprint.cast(asset)
    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    print(f"Loaded: {blueprint.get_name()}")
    print(f"   Class: {gen_class.get_name()}")

    # === METHOD 1: Standard properties (from previous script) ===
    print("\n🔍 Method 1: Standard Actor properties...")

    all_attrs = dir(cdo)
    property_candidates = [a for a in all_attrs if not a.startswith('_')]

    # Filter out methods
    filtered = []
    for attr_name in property_candidates:
        try:
            attr = getattr(cdo, attr_name)
            if not callable(attr):
                filtered.append(attr_name)
        except:
            pass

    props_standard = {}
    for prop_name in filtered:
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
                serialized = {"x": float(value.x), "y": float(value.y)}
                if hasattr(value, "z"):
                    serialized["z"] = float(value.z)
                if hasattr(value, "w"):
                    serialized["w"] = float(value.w)
            elif isinstance(value, unreal.Object):
                try:
                    path = value.get_path_name() if value else None
                    serialized = {"_type": "AssetReference", "path": path} if path else None
                except:
                    serialized = str(value)
            elif isinstance(value, (list, tuple)) and len(value) == 0:
                serialized = []
            elif isinstance(value, (list, tuple)):
                serialized = [str(v) for v in value[:10]]
                if len(value) > 10:
                    serialized.append(f"... and {len(value) - 10} more")
            else:
                serialized = str(value)

            props_standard[prop_name] = {
                "value": serialized,
                "type": type(value).__name__,
                "source": "CDO standard"
            }
        except:
            continue

    print(f"   Found {len(props_standard)} standard properties")

    # === METHOD 2: Try to access known Blueprint variables directly ===
    print("\n🔍 Method 2: Attempting direct access to Blueprint variables...")

    known_bp_vars = [
        'BP_Alis_PCharacter',
        'CurrentSkyIntensity',
        'TargetSkyIntensity',
        'NewVar'
    ]

    props_bp_direct = {}
    for var_name in known_bp_vars:
        try:
            # Try getattr first
            value = getattr(cdo, var_name, None)
            if value is not None:
                if isinstance(value, (int, float, bool, str)):
                    serialized = value
                elif isinstance(value, unreal.Object):
                    try:
                        serialized = {"_type": "AssetReference", "path": value.get_path_name()}
                    except:
                        serialized = str(value)
                else:
                    serialized = str(value)

                props_bp_direct[var_name] = {
                    "value": serialized,
                    "type": type(value).__name__,
                    "source": "Blueprint variable (direct)"
                }
                print(f"   ✅ {var_name}: {serialized}")
        except Exception as e:
            print(f"   ❌ {var_name}: {e}")

            # Try get_editor_property as fallback
            try:
                value = cdo.get_editor_property(var_name)
                if isinstance(value, (int, float, bool, str)) or value is None:
                    serialized = value
                else:
                    serialized = str(value)

                props_bp_direct[var_name] = {
                    "value": serialized,
                    "type": type(value).__name__,
                    "source": "Blueprint variable (editor_property)"
                }
                print(f"   ✅ {var_name} (via editor_property): {serialized}")
            except Exception as e2:
                print(f"   ❌ {var_name} (editor_property failed too): {e2}")

    print(f"\n   Found {len(props_bp_direct)} Blueprint variables")

    # === MERGE ALL PROPERTIES ===
    all_props = {}
    all_props.update(props_standard)
    all_props.update(props_bp_direct)  # Blueprint vars override standard

    print(f"\n📊 Total unique properties: {len(all_props)}")

    # === SAVE JSON ===
    print("\n💾 Saving...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_path = os.path.join(OUTPUT_DIR, f"all_variables_{timestamp}.json")

    result = {
        "asset_path": TEST_ASSET,
        "asset_type": "Blueprint",
        "class_name": gen_class.get_name(),
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "export_method": "Combined (standard + Blueprint variables)",
        "properties": all_props
    }

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"✅ Сохранено: {json_path}")
    print(f"   Size: {os.path.getsize(json_path)} bytes")

    # === PREVIEW ===
    print("\n👁️ Blueprint Variables:")
    print("-" * 80)
    if props_bp_direct:
        for name, data in props_bp_direct.items():
            val_str = str(data['value'])
            if len(val_str) > 50:
                val_str = val_str[:47] + "..."
            print(f"  • {name:30} = {val_str}")
    else:
        print("  (none found - they might be protected)")

    print("\n👁️ Standard Properties (first 10):")
    print("-" * 80)
    for i, (name, data) in enumerate(list(props_standard.items())[:10], 1):
        val_str = str(data['value'])
        if len(val_str) > 50:
            val_str = val_str[:47] + "..."
        print(f"  {i:2}. {name:30} = {val_str}")

    print("\n" + "=" * 80)
    print("EXPORT COMPLETE!")
    print("=" * 80)
    print(f"\n📁 JSON: {json_path}")
    print(f"📊 Total: {len(all_props)} properties")
    print(f"   - Standard: {len(props_standard)}")
    print(f"   - Blueprint variables: {len(props_bp_direct)}")

    if len(props_bp_direct) == 0:
        print("\n⚠️ Blueprint variables not accessible!")
        print("   Они могут быть Private. Попробуйте:")
        print("   1. Сделать их Instance Editable в Blueprint Editor")
        print("   2. Или использовать их текущие значения в игре")

except Exception as e:
    print(f"\n❌ ERROR: {e}")
    import traceback
    traceback.print_exc()
