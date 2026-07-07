# Find Variables - Shows ALL attributes for Blueprint variable search

import unreal

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"

print("=" * 80)
print("[SEARCH] FIND BLUEPRINT VARIABLES")
print("=" * 80)

asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
blueprint = unreal.Blueprint.cast(asset)
gen_class = blueprint.generated_class()
cdo = unreal.get_default_object(gen_class)

print(f"\nLoaded: {blueprint.get_name()}")

# Get ALL attributes
all_attrs = dir(cdo)
print(f"\n[STATS] Total attributes: {len(all_attrs)}")

# Search for our variables (case-insensitive partial match)
search_terms = ['sky', 'intensity', 'current', 'target', 'newvar', 'char', 'alis']

print("\n[SEARCH] Searching attributes that contain our variable names:")
print("-" * 80)

found_matches = {}
for search in search_terms:
    matches = [a for a in all_attrs if search.lower() in a.lower()]
    if matches:
        found_matches[search] = matches
        print(f"\n'{search}':")
        for match in matches:
            # Try to get value
            try:
                attr = getattr(cdo, match)
                if callable(attr):
                    type_str = "method"
                else:
                    type_str = f"{type(attr).__name__}"
                    # Try to get value
                    try:
                        val = cdo.get_editor_property(match)
                        type_str += f" = {val}"
                    except:
                        pass
                print(f"  - {match:40} -> {type_str}")
            except Exception as e:
                print(f"  - {match:40} -> [error: {e}]")

if not found_matches:
    print("\n[FAIL] No attributes similar to our variables were found!")
    print("\n[TIP] Showing ALL non-method attributes:")
    print("-" * 80)

    non_methods = []
    for attr_name in all_attrs:
        if attr_name.startswith('_'):
            continue
        try:
            attr = getattr(cdo, attr_name)
            if not callable(attr):
                non_methods.append((attr_name, type(attr).__name__))
        except:
            pass

    print(f"\nTotal non-methods: {len(non_methods)}")
    print("\nFirst 50:")
    for i, (name, type_name) in enumerate(non_methods[:50], 1):
        try:
            val = cdo.get_editor_property(name)
            val_str = str(val)
            if len(val_str) > 40:
                val_str = val_str[:37] + "..."
            print(f"  {i:2}. {name:35} [{type_name:15}] = {val_str}")
        except:
            print(f"  {i:2}. {name:35} [{type_name:15}]")

print("\n" + "=" * 80)
print("[TIP] If your variables were not found, then:")
print("   1. Blueprint variables are not accessible through the Python API")
print("   2. Or they have completely different names")
print("   3. Or a different approach is needed, such as reading the .uasset directly")
print("=" * 80)
