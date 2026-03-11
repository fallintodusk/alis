# COPY-PASTE TEST - Скопируйте всё ниже этой строки
# ===============================================

import unreal
import json
import os
from datetime import datetime

# === КОНФИГУРАЦИЯ ===
TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "AI_Snapshots")

print("=" * 60)
print("🔧 Blueprint → JSON Test")
print("=" * 60)

# === ЭКСПОРТ ===
print("\n📦 Шаг 1: Загружаю Blueprint...")
try:
    asset_data = unreal.EditorAssetLibrary.find_asset_data(TEST_ASSET)
    if not asset_data.is_valid():
        print(f"❌ ОШИБКА: Blueprint не найден: {TEST_ASSET}")
        print("👉 Измените TEST_ASSET на путь к вашему Blueprint")
    else:
        gen_class = unreal.load_object(None, asset_data.get_tag_value("GeneratedClass"))
        cdo = gen_class.get_default_object()
        print(f"✅ Загружен: {gen_class.get_name()}")

        # === СБОР СВОЙСТВ ===
        print("\n🔍 Шаг 2: Экспортирую propertiesа...")

        # FIXED: Используем правильный метод получения properties
        props = {}

        # Попытка 1: get_editor_property_names (если доступен)
        prop_names = []
        if hasattr(cdo, 'get_editor_property_names'):
            try:
                prop_names = list(cdo.get_editor_property_names())
                print(f"   Метод: get_editor_property_names() → {len(prop_names)} properties")
            except:
                pass

        # Попытка 2: dir() + фильтрация
        if not prop_names:
            all_attrs = [a for a in dir(cdo) if not a.startswith('_')]
            prop_names = all_attrs
            print(f"   Метод: dir() → {len(prop_names)} кандидатов")

        # Экспорт properties
        for prop_name in prop_names:
            try:
                value = cdo.get_editor_property(prop_name)

                # Сериализация разных типов
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
                    # Arrays (только простые значения)
                    serialized = [str(v) for v in value]
                else:
                    # Fallback
                    serialized = str(value)

                props[prop_name] = {
                    "value": serialized,
                    "type": type(value).__name__
                }

            except Exception:
                # Свойство недоступно через editor API
                continue

        print(f"✅ Экспортировано: {len(props)} properties")

        # === СОХРАНЕНИЕ ===
        print("\n💾 Шаг 3: Сохраняю JSON...")
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

        print(f"✅ Сохранено: {json_path}")

        # === ПРЕВЬЮ ===
        print("\n👁️ First 10 properties:")
        print("-" * 60)
        for i, (k, v) in enumerate(list(props.items())[:10]):
            val_str = str(v['value'])
            if len(val_str) > 40:
                val_str = val_str[:37] + "..."
            print(f"  {k}: {val_str}")

        if len(props) > 10:
            print(f"  ... and {len(props) - 10} properties")

        print("\n" + "=" * 60)
        print("✅ ТЕСТ ЗАВЕРШЕН УСПЕШНО!")
        print("=" * 60)
        print(f"\n📁 JSON файл: {json_path}")
        print(f"📊 Свойств экспортировано: {len(props)}")
        print("\n💡 Следующий шаг: откройте JSON файл и отправьте Claude для редактирования")

except Exception as e:
    print(f"\n❌ ОШИБКА: {e}")
    print("\n🔧 Возможные решения:")
    print("1. Проверьте правильность пути к Blueprint")
    print("2. Убедитесь, что Blueprint скомпилирован")
    print("3. Проверьте Output Log на ошибки")
