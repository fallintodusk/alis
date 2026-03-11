# Export SCS Components - via SimpleConstructionScript

import unreal
import json
import os
from datetime import datetime

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 80)
print("🔧 EXPORT SCS COMPONENTS")
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
        result = {"r": float(value.r), "g": float(value.g), "b": float(value.b)}
        if hasattr(value, "a"):
            result["a"] = float(value.a)
        return result
    elif hasattr(value, "x") and hasattr(value, "y"):
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

    print(f"Loaded: {blueprint.get_name()}")

    result = {
        "asset_path": TEST_ASSET,
        "asset_type": "Blueprint",
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "components": {}
    }

    # === ACCESS SCS ===
    print("\n🔍 Accessing SimpleConstructionScript...")

    # Try to get SCS
    scs = None
    if hasattr(blueprint, 'simple_construction_script'):
        scs = blueprint.simple_construction_script
        print(f"   Found SCS via blueprint.simple_construction_script")
    elif hasattr(blueprint, 'get_simple_construction_script'):
        scs = blueprint.get_simple_construction_script()
        print(f"   Found SCS via get_simple_construction_script()")
    elif hasattr(blueprint, 'SimpleConstructionScript'):
        scs = blueprint.SimpleConstructionScript
        print(f"   Found SCS via SimpleConstructionScript property")

    if scs is None:
        print(f"   SCS not found!")
        print(f"\n   Available Blueprint attributes:")
        bp_attrs = [a for a in dir(blueprint) if not a.startswith('_')]
        for i, attr in enumerate(bp_attrs[:30], 1):
            print(f"     {i:2}. {attr}")
    else:
        print(f"   ✅ SCS type: {type(scs).__name__}")

        # Get SCS nodes
        if hasattr(scs, 'get_all_nodes'):
            nodes = scs.get_all_nodes()
            print(f"   Found {len(nodes)} SCS nodes")

            for node in nodes:
                node_name = node.get_variable_name() if hasattr(node, 'get_variable_name') else node.get_name()
                print(f"\n   📦 Node: {node_name}")

                # Get component template
                template = None
                if hasattr(node, 'component_template'):
                    template = node.component_template
                elif hasattr(node, 'get_component_template'):
                    template = node.get_component_template()
                elif hasattr(node, 'ComponentTemplate'):
                    template = node.ComponentTemplate

                if template is None:
                    print(f"      No template found")
                    continue

                comp_class = template.get_class().get_name()
                print(f"      Class: {comp_class}")

                # Export component properties
                comp_props = {}
                comp_attrs = [a for a in dir(template) if not a.startswith('_')]

                # Filter non-methods
                filtered = []
                for attr_name in comp_attrs:
                    try:
                        attr = getattr(template, attr_name)
                        if not callable(attr):
                            filtered.append(attr_name)
                    except:
                        pass

                success_count = 0
                for prop_name in filtered:
                    try:
                        value = template.get_editor_property(prop_name)
                        comp_props[prop_name] = {
                            "value": serialize_value(value),
                            "type": type(value).__name__
                        }
                        success_count += 1
                    except:
                        continue

                print(f"      ✅ {success_count} properties")

                # Save component
                result["components"][str(node_name)] = {
                    "class": comp_class,
                    "properties": comp_props
                }

                # Show light properties
                if "Light" in comp_class:
                    print(f"      💡 Light component!")
                    light_props = ['intensity', 'light_color', 'temperature', 'use_temperature']
                    for lp in light_props:
                        if lp in comp_props:
                            print(f"         • {lp}: {comp_props[lp]['value']}")

        else:
            print(f"   ❌ SCS has no get_all_nodes() method")
            print(f"   SCS attributes: {[a for a in dir(scs) if not a.startswith('_')][:20]}")

    # === SAVE JSON ===
    print("\n💾 Сохранение...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_path = os.path.join(OUTPUT_DIR, f"scs_components_{timestamp}.json")

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"✅ Сохранено: {json_path}")
    print(f"   Размер: {os.path.getsize(json_path)} bytes")

    # === SUMMARY ===
    print("\n" + "=" * 80)
    print("EXPORT COMPLETE!")
    print("=" * 80)
    print(f"\n📊 Summary:")
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
