# System Limitations: Blueprint → JSON

## What Works ✅

### Actor Properties (28 properties)

The system **successfully** exports base Actor properties:

| Category | Examples | Use Cases |
|----------|----------|-----------|
| **Gameplay** | `can_be_damaged`, `custom_time_dilation` | Invincibility, time slowdown |
| **Networking** | `net_priority`, `net_update_frequency` | Network optimization |
| **Transform** | `relative_location`, `relative_rotation` | Position, rotation |
| **Lifecycle** | `initial_life_span`, `auto_destroy_when_finished` | Actor lifetime |
| **Visibility** | `hidden`, `enable_auto_lod_generation` | Visibility, LOD |

**Example JSON:**
```json
{
  "asset_path": "/Game/Project/BP_SunSky_Child.BP_SunSky_Child",
  "properties": {
    "custom_time_dilation": {
      "value": 1.0,
      "type": "float"
    },
    "can_be_damaged": {
      "value": true,
      "type": "bool"
    },
    "relative_location": {
      "value": {"x": 0.0, "y": 0.0, "z": 100.0},
      "type": "Vector"
    }
  }
}
```

---

## What Doesn't Work ❌

### Blueprint Variables (custom variables)

**Problem:** UE Python API **doesn't provide access** to Blueprint-specific variables.

**Examples of inaccessible variables:**
- `CurrentSkyIntensity` (Float)
- `TargetSkyIntensity` (Float)
- `NewVar` (Boolean)
- `BP_Alis_PCharacter` (Object Reference)

**Reason:**
- Blueprint variables are stored in `BlueprintGeneratedClass`, not in CDO Actor properties
- Python API `dir(cdo)` doesn't return Blueprint variables
- Methods like `get_editor_property_names()` and `properties()` don't exist in UE 5.5 Python API

**Tested approaches (all failed):**
1. ❌ `cdo.get_editor_property_names()` - method doesn't exist
2. ❌ `gen_class.properties()` - method doesn't exist
3. ❌ `getattr(cdo, 'CurrentSkyIntensity')` - attribute not found
4. ❌ `cdo.get_editor_property('CurrentSkyIntensity')` - property protected
5. ❌ Instance Editable + Compile + Restart - doesn't help

---

## Alternative Solutions

### Option 1: Use Actor Properties (RECOMMENDED)

**Advantages:**
- ✅ Works out of the box
- ✅ Supports import/export
- ✅ Claude can edit JSON

**How to use:**
```json
{
  "custom_time_dilation": {"value": 2.0},  // 2x time slowdown
  "can_be_damaged": {"value": false},       // Invincibility
  "net_priority": {"value": 5.0}            // High network priority
}
```

**Use Cases:**
- Gameplay balancing (damage, speed)
- Network replication setup
- Actor lifecycle management
- Debugging (invincibility, immortality)

---

### Option 2: Move Data to Components

Instead of Blueprint variables, use **Actor Components**:

**Steps:**
1. Create C++ `UActorComponent`:
   ```cpp
   UCLASS()
   class USkyComponent : public UActorComponent {
       UPROPERTY(EditAnywhere, BlueprintReadWrite)
       float CurrentSkyIntensity = 1.0f;

       UPROPERTY(EditAnywhere, BlueprintReadWrite)
       float TargetSkyIntensity = 1.0f;
   };
   ```

2. Add Component to Blueprint
3. Components accessible via Python:
   ```python
   components = cdo.get_components_by_class(unreal.SkyComponent)
   for comp in components:
       value = comp.get_editor_property('CurrentSkyIntensity')
   ```

**Advantages:**
- ✅ Accessible via Python API
- ✅ Modularity (components are reusable)
- ✅ Better architecture

**Disadvantages:**
- ❌ Requires C++ code
- ❌ Project recompilation needed

---

### Option 3: UAssetAPI (complex, external tool)

Use [UAssetAPI](https://github.com/atenfyr/UAssetAPI) to read `.uasset` files directly.

**Advantages:**
- ✅ Full access to Blueprint data
- ✅ Works outside UE Editor

**Disadvantages:**
- ❌ Complex setup
- ❌ Binary format deserialization required
- ❌ Fragile (depends on UE version)

---

### Option 4: C++ Commandlet (complex)

Create C++ Commandlet with reflection:

```cpp
for (TFieldIterator<FProperty> It(GeneratedClass); It; ++It) {
    FProperty* Property = *It;
    // Access to private Blueprint variables
}
```

**Advantages:**
- ✅ Full control via C++ API
- ✅ Access to all variables

**Disadvantages:**
- ❌ Requires C++ knowledge
- ❌ Complex setup

---

## Recommendations

### For Current Project

**Use what works:**
1. ✅ Export Actor properties (28 properties)
2. ✅ Edit them via JSON + Claude
3. ✅ Import back via `import_blueprint.py`

**Example applications:**
- **Balancing:** change `custom_time_dilation` for slowdown
- **Debug:** set `can_be_damaged = false` for testing
- **Optimization:** configure `net_priority` for network actors

### For Future Projects

**Architectural recommendations:**
1. ✅ **Use Components instead of Blueprint variables**
   - Data in C++ `UActorComponent`
   - Logic in Blueprint
   - Access via Python API

2. ✅ **Data-Driven Design**
   - Store data in DataTables/DataAssets
   - DataAssets accessible via Python API

3. ✅ **Actor Properties for configuration**
   - Use base Actor properties
   - Inheritance via C++ classes

---

## Summary

| Task | Solution | Status |
|------|----------|--------|
| Export Actor properties | export_blueprint.py | ✅ Works |
| Import JSON → Blueprint | import_blueprint.py | ✅ Works |
| Export Blueprint variables | — | ❌ Impossible (API limitation) |
| Edit with Claude | Via Actor properties | ✅ Works |

**The system is fully functional** for working with Actor properties.

For Blueprint variables, **architectural changes** (Components) or **complex workarounds** (UAssetAPI, C++) are required.

---

## Additional Information

### Tested on:
- Unreal Engine 5.5
- Python API (built-in)
- Blueprint: BP_SunSky_Child

### Attempted solutions (all failed):
1. Instance Editable + Compile
2. Editor restart
3. Direct access via `getattr()`
4. Search in `dir()` attributes
5. Reflection via `gen_class.properties()`

### Reason for limitation:
UE Python API focuses on **Editor Automation**, not **Blueprint Internals**.

Blueprint variables are internal structures of the Blueprint system, not accessible via public Python API.
