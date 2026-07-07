# DIAGNOSTIC - Shows available methods and properties of the Blueprint
# Use this to understand the Blueprint structure in the Python API

import unreal

TEST_ASSET = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"

print("=" * 80)
print("[SEARCH] DIAGNOSTIC: Blueprint Structure Analysis")
print("=" * 80)

try:
    # Load Blueprint
    print("\n[PACKAGE] Loading Blueprint...")
    asset = unreal.EditorAssetLibrary.load_asset(TEST_ASSET)
    blueprint = unreal.Blueprint.cast(asset)
    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    print(f"Loaded: {blueprint.get_name()}")
    print(f"   Type: {type(gen_class).__name__}")

    # === 1. INSPECT GENERATED CLASS ===
    print("\n" + "=" * 80)
    print("[1] BlueprintGeneratedClass Methods & Attributes")
    print("=" * 80)

    class_methods = [m for m in dir(gen_class) if not m.startswith('_')]
    print(f"\nTotal methods/attributes: {len(class_methods)}")
    print("\nFirst 30:")
    for i, method in enumerate(class_methods[:30], 1):
        try:
            attr = getattr(gen_class, method)
            attr_type = type(attr).__name__
            print(f"  {i:2}. {method:30} -> {attr_type}")
        except:
            print(f"  {i:2}. {method:30} -> [error accessing]")

    # === 2. INSPECT CDO ===
    print("\n" + "=" * 80)
    print("[2] CDO (Class Default Object) Methods & Attributes")
    print("=" * 80)

    cdo_methods = [m for m in dir(cdo) if not m.startswith('_')]
    print(f"\nTotal methods/attributes: {len(cdo_methods)}")
    print("\nFirst 30:")
    for i, method in enumerate(cdo_methods[:30], 1):
        try:
            attr = getattr(cdo, method)
            attr_type = type(attr).__name__
            print(f"  {i:2}. {method:30} -> {attr_type}")
        except:
            print(f"  {i:2}. {method:30} -> [error accessing]")

    # === 3. TRY TO GET PROPERTIES ===
    print("\n" + "=" * 80)
    print("[3] Attempt to Extract Properties")
    print("=" * 80)

    # Method A: Check if gen_class has properties() iterator
    print("\n- Method A: gen_class.properties()")
    if hasattr(gen_class, 'properties'):
        try:
            props = list(gen_class.properties())
            print(f"   Found {len(props)} properties via gen_class.properties()")
            if props:
                print("\n   First 5 properties:")
                for i, prop in enumerate(props[:5], 1):
                    print(f"     {i}. {prop.get_name()} ({type(prop).__name__})")
        except Exception as e:
            print(f"   [FAIL] Error: {e}")
    else:
        print("   [FAIL] gen_class.properties() does not exist")

    # Method B: Check for get_properties
    print("\n- Method B: Various property methods")
    property_methods = [
        'get_properties',
        'get_fields',
        'get_uproperty',
        'get_all_properties',
        'fields'
    ]
    for method_name in property_methods:
        if hasattr(gen_class, method_name):
            print(f"   [OK] {method_name} exists")
            try:
                method = getattr(gen_class, method_name)
                if callable(method):
                    result = method()
                    print(f"      -> Returns: {type(result).__name__}, count: {len(list(result)) if hasattr(result, '__iter__') else 'N/A'}")
            except Exception as e:
                print(f"      -> Error calling: {e}")
        else:
            print(f"   [FAIL] {method_name} does not exist")

    # Method C: Try CDO methods
    print("\n- Method C: CDO property methods")
    cdo_property_methods = [
        'get_editor_property_names',
        'get_editable_properties',
        'get_properties'
    ]
    for method_name in cdo_property_methods:
        if hasattr(cdo, method_name):
            print(f"   [OK] cdo.{method_name} exists")
            try:
                method = getattr(cdo, method_name)
                if callable(method):
                    result = method()
                    if hasattr(result, '__iter__'):
                        result_list = list(result)
                        print(f"      -> Returns {len(result_list)} items")
                        if result_list:
                            print(f"      -> First item: {result_list[0]}")
                    else:
                        print(f"      -> Returns: {type(result).__name__}")
            except Exception as e:
                print(f"      -> Error: {e}")
        else:
            print(f"   [FAIL] cdo.{method_name} does not exist")

    # === 4. TRY PARENT CLASS ===
    print("\n" + "=" * 80)
    print("[4] Parent Class Detection")
    print("=" * 80)

    parent_methods = [
        'get_super_class',
        'get_super_struct',
        'get_parent_class',
        'super_class',
        'super_struct'
    ]
    for method_name in parent_methods:
        if hasattr(gen_class, method_name):
            print(f"   [OK] {method_name} exists")
            try:
                attr = getattr(gen_class, method_name)
                if callable(attr):
                    result = attr()
                    print(f"      -> Returns: {result.get_name() if hasattr(result, 'get_name') else result}")
                else:
                    print(f"      -> Attribute: {attr.get_name() if hasattr(attr, 'get_name') else attr}")
            except Exception as e:
                print(f"      -> Error: {e}")
        else:
            print(f"   [FAIL] {method_name} does not exist")

    # === 5. MANUAL PROPERTY EXTRACTION TEST ===
    print("\n" + "=" * 80)
    print("[5] Manual Property Extraction Test")
    print("=" * 80)

    print("\n- Trying to get properties manually...")
    test_props = [
        'RootComponent',
        'bHidden',
        'Tags',
        'ActorLabel'
    ]

    for prop_name in test_props:
        try:
            value = cdo.get_editor_property(prop_name)
            print(f"   [OK] {prop_name}: {value} ({type(value).__name__})")
        except Exception as e:
            print(f"   [FAIL] {prop_name}: {e}")

    print("\n" + "=" * 80)
    print("[OK] DIAGNOSTIC COMPLETE")
    print("=" * 80)
    print("\n[TIP] Analyze the output above and send it to Claude")
    print("   This will help find the correct way to export properties")

except Exception as e:
    print(f"\n[FAIL] FATAL ERROR: {e}")
    import traceback
    traceback.print_exc()
