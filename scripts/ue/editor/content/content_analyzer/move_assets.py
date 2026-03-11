# move_assets.py
import unreal
import datetime

def ue():
    print("🚀 Starting Content Browser Analysis...")
    
    # Получаем выбранные ассеты
    selected_assets = unreal.EditorUtilityLibrary.get_selected_assets()
    
    print(f"📁 Selected {len(selected_assets)} asset(s)")
    
    if len(selected_assets) == 0:
        print("ℹ️  Please select some assets in Content Browser first!")
        return
    
    # Анализируем каждый ассет
    for i, asset in enumerate(selected_assets):
        asset_name = asset.get_name()
        asset_class = asset.get_class().get_name()
        asset_path = asset.get_path_name()
        
        print(f"\n--- Asset #{i+1} ---")
        print(f"Name: {asset_name}")
        print(f"Class: {asset_class}") 
        print(f"Path: {asset_path}")
        
        # Получаем дополнительную информацию
        try:
            if asset_class == 'Texture2D':
                size_x = asset.get_editor_property('size_x')
                size_y = asset.get_editor_property('size_y')
                print(f"Texture Size: {size_x}x{size_y}")
            elif asset_class == 'StaticMesh':
                bounds = asset.get_bounds()
                print(f"Bounds: {bounds}")
        except Exception as e:
            print(f"Additional info: {str(e)}")
    
    print(f"\n✅ Analysis completed at {datetime.datetime.now()}")

# Новый метод для получения акторов (без deprecated)
def get_level_info():
    editor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = editor_subsystem.get_all_level_actors()
    print(f"🎭 Total actors in level: {len(all_actors)}")
    return all_actors

# Запускаем полный анализ
if __name__ == "__main__":
    analyze_content_browser()
    get_level_info()