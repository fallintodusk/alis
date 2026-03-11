# ALTERNATIVE TEST - Uses reflection для поиска всех properties
# Если COPY_PASTE_TEST.py возвращает 0 properties, используйте этот скрипт

import unreal
import json
import os
from datetime import datetime

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 60)
print("🔧 ALTERNATIVE Blueprint → JSON Test (Reflection)")
print("=" * 60)

try:
    # Load Blueprint
    print("\n📦 Загрузка Blueprint...")
    asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
    if not asset:
        print(f"❌ Не удалось загрузить: {TEST_ASSET}")
        raise SystemExit

    blueprint = unreal.Blueprint.cast(asset)
    if not blueprint:
        print(f"❌ Это не Blueprint!")
        raise SystemExit

    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    print(f"✅ Blueprint: {blueprint.get_name()}")
    print(f"   Class: {gen_class.get_name()}")
    print(f"   Parent: {gen_class.get_super_class().get_name()}")

    # Extract properties via UE Reflection
    print("\n🔍 Извлечение properties через UProperty reflection...")

    props = {}
    property_count = 0

    # Iterate through all UProperties
    for field in gen_class.properties():
        prop_name = field.get_name()
        property_count += 1

        try:
            # Get value from CDO
            value = field.get_editor_property(cdo)

            # Serialize
            if isinstance(value, (int, float, bool, str)) or value is None:
                serialized = value
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
            elif isinstance(value, (list, tuple)):
                serialized = [str(v) for v in value[:10]]  # Limit arrays to 10 items
            else:
                serialized = str(value)

            props[prop_name] = {
                "value": serialized,
                "type": field.get_class().get_name(),
                "category": field.get_meta_data("Category") or "Default"
            }

        except Exception as e:
            # Skip properties that can't be read
            continue

    print(f"✅ Найдено UProperties: {property_count}")
    print(f"✅ Экспортировано: {len(props)} properties")

    # Save JSON
    print("\n💾 Сохранение JSON...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    json_path = os.path.join(OUTPUT_DIR, f"alternative_export_{timestamp}.json")

    result = {
        "asset_path": TEST_ASSET,
        "asset_type": "Blueprint",
        "class_name": gen_class.get_name(),
        "parent_class": gen_class.get_super_class().get_name(),
        "exported_at": datetime.utcnow().isoformat() + "Z",
        "export_method": "UProperty reflection",
        "properties": props
    }

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"✅ Сохранено: {json_path}")

    # Preview
    print("\n👁️ First 15 properties:")
    print("-" * 60)
    for i, (name, data) in enumerate(list(props.items())[:15]):
        val_str = str(data['value'])
        if len(val_str) > 40:
            val_str = val_str[:37] + "..."
        category = data.get('category', 'Unknown')
        print(f"  [{category}] {name}: {val_str}")

    if len(props) > 15:
        print(f"  ... and {len(props) - 15} properties")

    print("\n" + "=" * 60)
    print("✅ ALTERNATIVE TEST ЗАВЕРШЕН!")
    print("=" * 60)
    print(f"\n📁 JSON: {json_path}")
    print(f"📊 Свойств: {len(props)}")
    print(f"📋 Файл размером: {os.path.getsize(json_path)} bytes")

    # Show property categories
    categories = {}
    for prop_data in props.values():
        cat = prop_data.get('category', 'Default')
        categories[cat] = categories.get(cat, 0) + 1

    print(f"\n📂 Категории properties:")
    for cat, count in sorted(categories.items(), key=lambda x: -x[1]):
        print(f"   {cat}: {count}")

except Exception as e:
    print(f"\n❌ ОШИБКА: {e}")
    import traceback
    print("\n📋 Stack trace:")
    traceback.print_exc()
    print("\n🔧 Возможные решения:")
    print("1. Убедитесь, что Blueprint существует и скомпилирован")
    print("2. Проверьте путь к Blueprint (должен быть /Game/...)")
    print("3. Попробуйте сначала открыть Blueprint в редакторе")
