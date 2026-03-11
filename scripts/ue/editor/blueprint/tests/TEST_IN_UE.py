"""
🧪 COPY-PASTE TEST SCRIPT
Скопируйте весь этот файл и вставьте в UE Python Console
"""

import unreal
import json
import os
from datetime import datetime

print("=" * 60)
print("🔧 Blueprint → JSON Converter Test")
print("=" * 60)

# ===== CONFIGURATION =====
# Замените на ваш Blueprint:
TEST_ASSET_PATH = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"

OUTPUT_DIR = os.path.join(
    unreal.SystemLibrary.get_project_directory(),
    "Saved",
    "AI_Snapshots"
)

# ===== STEP 1: LOAD BLUEPRINT =====
print("\n📦 Step 1: Loading Blueprint...")

try:
    asset_data = unreal.EditorAssetLibrary.find_asset_data(TEST_ASSET_PATH)
    if not asset_data.is_valid():
        print(f"❌ ERROR: Asset not found: {TEST_ASSET_PATH}")
        print("👉 Откройте Content Browser и скопируйте правильный путь")
        raise SystemExit

    generated_class_path = asset_data.get_tag_value("GeneratedClass")
    if not generated_class_path:
        print(f"❌ ERROR: Not a Blueprint asset")
        raise SystemExit

    generated_class = unreal.load_object(None, generated_class_path)
    cdo = generated_class.get_default_object()

    print(f"Loaded: {generated_class.get_name()}")
    print(f"   Parent: {generated_class.get_super_class().get_name()}")

except Exception as e:
    print(f"❌ FAILED: {e}")
    raise SystemExit

# ===== STEP 2: EXTRACT PROPERTIES =====
print("\n🔍 Step 2: Extracting properties...")

properties = {}
exported_count = 0

# Try multiple methods to get property names
property_names = []

# Method A: get_editor_property_names (preferred)
try:
    if hasattr(cdo, 'get_editor_property_names'):
        property_names = list(cdo.get_editor_property_names())
        print(f"   Method: get_editor_property_names() → {len(property_names)} properties")
except Exception as e:
    print(f"   ⚠️ get_editor_property_names() failed: {e}")

# Method B: Fallback to dir() filtering
if not property_names:
    all_attrs = dir(cdo)
    property_names = [
        attr for attr in all_attrs
        if not attr.startswith('_')
        and not callable(getattr(cdo, attr, None))
    ]
    print(f"   Method: dir() fallback → {len(property_names)} candidates")

# Extract property values
for prop_name in property_names:
    try:
        value = cdo.get_editor_property(prop_name)

        # Serialize
        if isinstance(value, (int, float, bool, str)) or value is None:
            serialized = value
        elif hasattr(value, "x") and hasattr(value, "y"):
            serialized = {"x": value.x, "y": value.y}
            if hasattr(value, "z"):
                serialized["z"] = value.z
            if hasattr(value, "w"):
                serialized["w"] = value.w
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

        properties[prop_name] = {
            "value": serialized,
            "type": type(value).__name__
        }
        exported_count += 1

    except:
        continue

print(f"✅ Exported {exported_count} properties")

# ===== STEP 3: BUILD JSON =====
print("\n📝 Step 3: Building JSON...")

result = {
    "asset_path": TEST_ASSET_PATH,
    "asset_type": "Blueprint",
    "class_name": generated_class.get_name(),
    "parent_class": generated_class.get_super_class().get_name(),
    "exported_at": datetime.utcnow().isoformat() + "Z",
    "properties": properties
}

print(f"✅ JSON structure created")
print(f"   Total size: {len(json.dumps(result))} bytes")

# ===== STEP 4: SAVE TO FILE =====
print("\n💾 Step 4: Saving to file...")

os.makedirs(OUTPUT_DIR, exist_ok=True)

name = TEST_ASSET_PATH.split("/")[-1].split(".")[0]
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
json_path = os.path.join(OUTPUT_DIR, f"{name}_{timestamp}.json")

with open(json_path, "w", encoding="utf-8") as f:
    json.dump(result, f, indent=2, ensure_ascii=False)

print(f"✅ Saved to: {json_path}")

# ===== STEP 5: PREVIEW =====
print("\n👁️ Preview (first 30 properties):")
print("-" * 60)

preview_count = 0
for prop_name, prop_data in properties.items():
    if preview_count >= 30:
        print(f"... and {len(properties) - 30} more properties")
        break

    value_preview = str(prop_data["value"])
    if len(value_preview) > 50:
        value_preview = value_preview[:47] + "..."

    print(f"  {prop_name}: {value_preview}")
    preview_count += 1

# ===== FINAL SUMMARY =====
print("\n" + "=" * 60)
print("✅ TEST COMPLETED SUCCESSFULLY!")
print("=" * 60)
print(f"📁 JSON file location:")
print(f"   {json_path}")
print(f"\n📊 Statistics:")
print(f"   Blueprint: {generated_class.get_name()}")
print(f"   Properties exported: {exported_count}")
print(f"   File size: {os.path.getsize(json_path)} bytes")
print("\n🎯 Next steps:")
print("   1. Откройте JSON файл в редакторе")
print("   2. Отправьте Claude для редактирования")
print("   3. Используйте json_to_bp.py для импорта изменений")
print("=" * 60)

# ===== QUICK COMMANDS =====
print("\n📋 Quick commands for next steps:")
print("\n# List all snapshots:")
print(f'import os; print("\\n".join(os.listdir(r"{OUTPUT_DIR}")))')

print("\n# Import this JSON back (dry run):")
print(f'from json_to_bp import import_from_file')
print(f'import_from_file("{os.path.basename(json_path)}", dry_run=True)')

print("\n# Import this JSON back (real):")
print(f'import_from_file("{os.path.basename(json_path)}", dry_run=False)')
