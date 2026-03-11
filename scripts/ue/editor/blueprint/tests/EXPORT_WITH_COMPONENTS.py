# Export With Components - Actor + Components Export

import unreal
import json
import os
from datetime import datetime

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 80)
print("🔧 EXPORT WITH COMPONENTS")
print("=" * 80)

def serialize_value(value):
    """Serialize UE types to JSON"""
    if isinstance(value, (int, float, bool, str)) or value is None:
        return value
    elif isinstance(value, unreal.Name):
        return str(value)
    elif isinstance(value, unreal.Guid):
        return str(value)
    elif hasattr(value, "r") and hasattr(value, "g") and hasattr(value, "b"):
        # Color (LinearColor or Color)
        result = {"r": float(value.r), "g": float(value.g), "b": float(value.b)}
        if hasattr(value, "a"):
            result["a"] = float(value.a)
        return result
    elif hasattr(value, "x") and hasattr(value, "y"):
        # Vector-like
        result = {"x": float(value.x), "y": float(value.y)}
        if hasattr(value, "z"):
            result["z"] = float(value.z)
        if hasattr(value, "w"):
            result["w"] = float(value.w)
        return result
    elif isinstance(value, unreal.Object):
        try:
            path = value.get_path_name() if value else None
            return {"_type": "AssetReference", "path": path} if path else None
        except:
            return str(value)
    elif isinstance(value, (list, tuple)) and len(value) == 0:
        return []
    elif isinstance(value, (list, tuple)):
        return [str(v) for v in value[:10]]
    else:
        return str(value)

try:
    # Load Blueprint
    print("\n📦 Загрузка...")
    asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
    blueprint = unreal.Blueprint.cast(asset)
    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    print(f"Loaded: {blueprint.get_name()}")

    result = {
        "asset_path": TEST_ASSET,
        "asset_type": "Blueprint",
        "class_name": gen_class.get_name(),
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "actor_properties": {},
        "components": {}
    }

    # === 1. ACTOR PROPERTIES ===
    print("\n🔍 Step 1: Actor properties...")

    all_attrs = [a for a in dir(cdo) if not a.startswith('_')]
    filtered = []
    for attr_name in all_attrs:
        try:
            attr = getattr(cdo, attr_name)
            if not callable(attr):
                filtered.append(attr_name)
        except:
            pass

    for prop_name in filtered:
        try:
            value = cdo.get_editor_property(prop_name)
            result["actor_properties"][prop_name] = {
                "value": serialize_value(value),
                "type": type(value).__name__
            }
        except:
            continue

    print(f"   Found {len(result['actor_properties'])} actor properties")

    # === 2. COMPONENTS ===
    print("\n🔍 Step 2: Components...")

    # Get all components
    try:
        # Try to get components
        if hasattr(cdo, 'get_components_by_class'):
            all_components = cdo.get_components_by_class(unreal.ActorComponent)
        elif hasattr(cdo, 'get_components'):
            all_components = cdo.get_components()
        else:
            # Fallback: try RootComponent
            root = getattr(cdo, 'RootComponent', None) if hasattr(cdo, 'RootComponent') else None
            all_components = [root] if root else []

        print(f"   Found {len(all_components)} components")

        for comp in all_components:
            if comp is None:
                continue

            comp_name = comp.get_name()
            comp_class = comp.get_class().get_name()

            print(f"\n   📦 Component: {comp_name} ({comp_class})")

            comp_props = {}

            # Get component properties
            comp_attrs = [a for a in dir(comp) if not a.startswith('_')]

            # Filter non-methods
            comp_filtered = []
            for attr_name in comp_attrs:
                try:
                    attr = getattr(comp, attr_name)
                    if not callable(attr):
                        comp_filtered.append(attr_name)
                except:
                    pass

            # Export component properties
            success_count = 0
            for prop_name in comp_filtered:
                try:
                    value = comp.get_editor_property(prop_name)
                    comp_props[prop_name] = {
                        "value": serialize_value(value),
                        "type": type(value).__name__
                    }
                    success_count += 1
                except:
                    continue

            print(f"      ✅ {success_count} properties")

            # Save component
            result["components"][comp_name] = {
                "class": comp_class,
                "properties": comp_props
            }

            # Show important properties for lights
            if "Light" in comp_class:
                print(f"      💡 Light component detected!")
                light_props = ['intensity', 'light_color', 'temperature', 'use_temperature']
                for lp in light_props:
                    if lp in comp_props:
                        val = comp_props[lp]['value']
                        print(f"         • {lp}: {val}")

    except Exception as e:
        print(f"   Error getting components: {e}")
        import traceback
        traceback.print_exc()

    # === SAVE JSON ===
    print("\n💾 Сохранение...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_path = os.path.join(OUTPUT_DIR, f"with_components_{timestamp}.json")

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"✅ Сохранено: {json_path}")
    print(f"   Размер: {os.path.getsize(json_path)} bytes")

    # === SUMMARY ===
    print("\n" + "=" * 80)
    print("EXPORT COMPLETE!")
    print("=" * 80)
    print(f"\n📊 Summary:")
    print(f"   Actor properties: {len(result['actor_properties'])}")
    print(f"   Components: {len(result['components'])}")

    if result['components']:
        print(f"\n📦 Components:")
        for comp_name, comp_data in result['components'].items():
            prop_count = len(comp_data['properties'])
            print(f"   • {comp_name} ({comp_data['class']}): {prop_count} properties")

    print(f"\n📁 JSON: {json_path}")

except Exception as e:
    print(f"\n❌ ERROR: {e}")
    import traceback
    traceback.print_exc()
