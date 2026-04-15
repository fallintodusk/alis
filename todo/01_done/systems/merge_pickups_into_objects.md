# Plan: Unified Object Definition System

## Summary

**One definition system.** ObjectDefinition is the only definition type. "Item" is a derived concept (has Pickup capability + item section), not a separate asset type.

- **Capabilities** = world interactions only (Pickup, Hinged, Lockable)
- **Sections** = data for other systems (item for inventory/UI/GAS)
- **One editor panel** with AssetRegistry tag filters
- **Kill ItemDefinition** and separate Items panel
- Runtime spawn shared between Editor placement and gameplay drops

## Document Fixes Applied (2026-01-21)

Fixed critical contradictions and technical mismatches:

1. **Tag format**: Removed all `ALIS.Capabilities="|...|"` references. Using only boolean `ALIS.Cap.<Name>="true"` tags.
2. **Files to Create**: Removed outdated interface-based files (CapabilityPropertyHandler, CapabilitySpawn, CapabilityTypeRegistry, ValueParse). Using existing registries + passive components.
3. **Entrypoint naming**: Standardized to `ProjectObjectSpawn::SpawnFromDefinition()` everywhere.
4. **Parser overrides**: Added explicit container/array/map parser overrides in spawn utility to support designer-friendly JSON formats (comma-separated tags, semicolon-separated arrays). Generic ImportText handles simple types.
5. **Phase ordering**: Clarified Decision #1 references Phase 2 (after UI validation gate).

**Status:** Document now internally consistent and safe to implement.

---

## Architecture Decisions (2026-01-23) - FINALIZED

### Core Mental Model

**One definition system. "Item" is a derived concept, not a separate asset type.**

```text
ObjectDefinition = single source of truth for "what exists in the world"
  |
  +-- capabilities[]     = world interactions only
  |     - Pickup         (PickupTime, InitialQuantity)
  |     - Hinged         (OpenAngle, Speed)
  |     - Lockable       (LockTag, DefaultLocked)
  |
  +-- sections{}         = data for other systems (optional)
        - item   (DisplayName, Weight, Effects, Magnitudes)
        - Loot           (future)
        - Quest          (future)
```

**Definitions:**
- **Item** = ObjectDefinition with `sections.item`
- **World pickup item** = Item + `Pickup` capability (can be picked up from world)
- **Items without Pickup:** Vendor rewards, quest rewards, crafting outputs - exist in inventory but never as world actors

**What makes something a "door":**
- Has `Hinged` + `Lockable` capabilities
- No `item` section

### Finalized Decisions

1. **One definition type** - Kill `ItemDefinition`. Everything is `ObjectDefinition`.

2. **One editor panel** - Kill separate Items panel. One "Definitions" browser with AssetRegistry tag filters:
   - "Pickups" = has `ALIS.Cap.Pickup`
   - "Items" = has `ALIS.Section.Item` (or `ItemTag.*`)
   - "Doors" = has `ALIS.Cap.Lockable` OR `ALIS.Cap.Hinged`

3. **Capabilities = world interactions only** - What the player can DO to it in the world:
   - Pickup: PickupTime, InitialQuantity
   - Hinged: OpenAngle, Speed, TargetMesh
   - Lockable: LockTag, DefaultLocked

4. **Sections = data for other systems** - Not world interactions:
   - item: DisplayName, Weight, MaxStack, Tags, Effects, Magnitudes
   - Only inventory/UI/GAS code interprets this data
   - Object doesn't "know" inventory tech - just contains optional data

5. **Hard contract:**
   - Capability fields MUST be world-interaction fields
   - Section fields MUST NOT be world-interaction fields
   - Violations = architecture smell

6. **GAS boundary: compile-time vs runtime**
   - Compile-time: ProjectObject has NO GAS dependency. Stores `FSoftObjectPath` and `TMap<FGameplayTag, float>`.
   - Runtime: Inventory/GAS modules load soft refs and apply effects.

7. **No profiles** - Analyzed 35 items, 0 shared GAS configs. PickupRole handles categorization, inline magnitudes handle GAS data. No profile abstraction needed.

---

## Implementation Phases

### Phase A: Parsing Support (enables test JSON)

Create/parse ObjectDefinition with `item` section.

| Step | File | Change | Exit Criteria |
|------|------|--------|---------------|
| A1 | [object.schema.json](../../Plugins/Resources/ProjectObject/Data/Schemas/object.schema.json) | Add `item` section definition with all fields | Schema validates new JSON |
| A2 | [ObjectDefinition.h](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) | Rename `FItemSectionData` --> `FItemSection`, change `FName UseProfileId/EquipProfileId` --> `FSoftObjectPath` + inline fields | Struct compiles |
| A3 | [DefinitionJsonParser.cpp](../../Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionJsonParser.cpp) | Parse `item` section (currently parses `item`) | Generator produces UAsset from new JSON |
| A4 | [ObjectDefinition.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Data/ObjectDefinition.cpp) | Export `ALIS.Section.Item` tag + item tags | AssetRegistry shows tags |

**Test:** Create `WaterBottle.json` with new structure --> run generator --> verify UAsset created with correct data.

---

### Phase B: Inventory Integration

Inventory reads from ObjectDefinition instead of ItemDefinition.

| Step | File | Change | Exit Criteria |
|------|------|--------|---------------|
| B1 | [ItemDefinitionCache.h](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Services/ItemDefinitionCache.h) | Rename --> `ObjectDefinitionCache`, change `UItemDefinition*` --> `UObjectDefinition*` | Compiles |
| B2 | [ItemDefinitionCache.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ItemDefinitionCache.cpp) | Update implementation | Compiles |
| B3 | [InventoryTypes.h](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Inventory/InventoryTypes.h) | `FInventoryEntry.ItemId` --> `ObjectId`, type stays `FPrimaryAssetId` but points to ObjectDefinition | Compiles |
| B4 | [ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) | Change all `Def->Data.X` --> `ObjectDef->Getitem()->X` | Compiles |
| B5 | [InventoryInteractionHandler.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp) | Find `UPickupCapabilityComponent` instead of `UProjectPickupItemComponent` | Compiles |

**Test:** Place object with Pickup + item --> pick up --> verify in inventory --> drop --> verify spawns.

---

### Phase C: UI Unification

One editor panel with filters.

| Step | File | Change | Exit Criteria |
|------|------|--------|---------------|
| C1 | [SProjectObjectBrowser.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/Widgets/SProjectObjectBrowser.cpp) | Add filter for `ALIS.Section.Item` | Filter shows only items |
| C2 | [SProjectItemBrowser.h/.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/Widgets/) | Remove or alias to Objects panel | No separate Items panel |
| C3 | [ProjectPlacementEditor module](../../Plugins/Editor/ProjectPlacementEditor/) | Remove Items tab registration | One "Definitions" tab |

**Test:** Open editor --> one panel --> filters work (Pickups, Items, Doors, etc.)

---

### Phase D: Data Migration

Convert existing item.json to new format.

| Step | File | Change | Exit Criteria |
|------|------|--------|---------------|
| D1 | All `Plugins/Resources/ProjectItems/Data/**/*.json` | Convert to ObjectDefinition with `item` section | New JSON validates |
| D2 | Move to `Plugins/Resources/ProjectObject/Content/` | Place in correct hierarchy (HumanMade/Consumables/etc.) | Files in correct location |
| D3 | Run generator | Produce new UAssets | UAssets created |
| D4 | Update any hardcoded `ItemDefinition:X` refs | Change to `ObjectDefinition:X` | No broken refs |

**Test:** All existing items work with new system.

---

### Phase E: Cleanup

Remove deprecated code.

| Step | File | Change | Exit Criteria |
|------|------|--------|---------------|
| E1 | [ProjectItems plugin](../../Plugins/Resources/ProjectItems/) | Add deprecation banner to README | Warning visible |
| E2 | Monitor for 1 release | Check logs for legacy usage | No warnings |
| E3 | Delete `ProjectItems` plugin | Remove entire plugin folder | Plugin gone |
| E4 | Delete `UItemDefinition` class | Remove from ProjectItems | Class gone |
| E5 | Delete old item.json files | Remove from ProjectItems/Data | Files gone |

**Test:** Build succeeds, no references to old system.

---

### Phase F: Additional Tags

| Step | File | Change |
|------|------|--------|
| F1 | [ProjectGameplayTags.cpp](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/ProjectGameplayTags.cpp) | Add `Item.Type.Currency` |
| F2 | Same | Add any other missing `Item.Type.*` tags |

---

## Implementation Order

```text
Phase A (Parsing)     <-- START HERE
    |
    v
Phase B (Inventory)   <-- Core functionality
    |
    v
Phase C (UI)          <-- Editor unification
    |
    v
Phase D (Migration)   <-- Convert existing data
    |
    v
Phase E (Cleanup)     <-- Remove old code
    |
    v
Phase F (Tags)        <-- Can be done anytime
```

---

## Quick Reference: File Changes

| File | Phase | Change |
|------|-------|--------|
| `object.schema.json` | A1 | Add `item` section |
| `ObjectDefinition.h` | A2 | Rename struct, add FSoftObjectPath fields |
| `DefinitionJsonParser.cpp` | A3 | Parse `item` |
| `ObjectDefinition.cpp` | A4 | Export section tags |
| `ItemDefinitionCache.h/.cpp` | B1-B2 | Rename to ObjectDefinitionCache |
| `InventoryTypes.h` | B3 | ObjectId field |
| `ProjectInventoryComponent.cpp` | B4 | Read from Getitem() |
| `InventoryInteractionHandler.cpp` | B5 | Find PickupCapabilityComponent |
| `SProjectObjectBrowser.cpp` | C1 | Add item filter |
| `SProjectItemBrowser.*` | C2 | Remove |
| `ProjectItems/Data/*.json` | D1-D2 | Migrate to new format |
| `ProjectItems/` | E1-E5 | Deprecate and delete |
| `ProjectGameplayTags.cpp` | F1-F2 | Add tags |

---

## Kill List (Phase E)

| File/System | Action |
|-------------|--------|
| `Plugins/Resources/ProjectItems/` | Delete entire plugin |
| `SProjectItemBrowser.h/.cpp` | Delete (merged into Objects panel) |
| `UItemDefinition` class | Delete (replaced by ObjectDefinition) |
| `item.json` files | Migrate then delete |
| `ItemDefinitionCache` | Renamed, old deleted |

---

## Previously Completed Work

### 1. Runtime Spawn Utility [DONE]

**Problem:** Editor factory is in editor-only module - breaks runtime drops.

**Solution:** [ObjectSpawnUtility.h/.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Spawning/ObjectSpawnUtility.h) - `ProjectObjectSpawn::SpawnFromDefinition()`

Editor factory is now thin wrapper calling the utility.

### 2. Architecture Documentation [DONE]

**Full documentation:** [Layer Contract](../../Plugins/Resources/ProjectObject/docs/layer_contract.md)

**Summary:** One definition system. Capabilities = world interactions. Sections = data for other systems.

---

### 3. Data Path Tiers (fix doc contradiction)

Define two explicit tiers:

| Tier | Path | Purpose | Shipped? |
|------|------|---------|----------|
| **SOT** | `Plugins/<Plugin>/Data/` | Generator input (editor/CI) | **NO** (default) |
| **Runtime** | `Plugins/<Plugin>/Content/Data/` | UFS for cooked builds | Yes (UAssets) |

**SOT JSON is NOT shipped by default:**

- Generator produces UAssets from JSON
- UAssets are packaged, not JSON source
- Optional: ship JSON only for modding builds (behind build flag)

### 4. Legacy Resolver (1 release compatibility)

```cpp
// In AssetManager or inventory load
if (AssetId.PrimaryAssetType == "ItemDefinition")
{
    // Map to ObjectDefinition with same name
    AssetId = FPrimaryAssetId("ObjectDefinition", AssetId.PrimaryAssetName);
    UE_LOG(LogInventory, Warning, TEXT("Legacy ItemDefinition reference: %s"), *AssetId.ToString());
}
```

### 5. AssetRegistry Tags for Fast Filtering (CRITICAL)

**Problem:** Filtering must NOT load ObjectDefinition assets - scales to thousands of assets.

**Solution:** Export boolean tags for capabilities, sections, and item tags:

**Tags exported:**

| Tag | Example Value | Purpose |
|-----|---------------|---------|
| `ALIS.Cap.<Name>` | `"true"` | Boolean per capability (Hinged, Pickup, etc.) |
| `ALIS.Section.<Name>` | `"true"` | Boolean per section (Item, Loot, etc.) |
| `ALIS.ItemTag.<Tag>` | `"true"` | Boolean per item tag (e.g., `Item.Type.Consumable`) |
| `DisplayName` | `"Water Bottle"` | Tooltip (from Item section) |
| `Weight` | `"0.5"` | Tooltip (from Item section) |

**See:** [ObjectDefinition.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Data/ObjectDefinition.cpp) for implementation.

### 6. Item.Type Tags - UI Reads Directly

**Problem:** Creating a derived `PickupRole` tag duplicates existing `Item.Type.*` information.

**Solution:** UI reads `ALIS.ItemTag.Item.Type.*` tags directly - no derived tag needed.

```cpp
// In SProjectObjectBrowser::RefreshCapabilityFilters()
// Enumerate tags, find ALIS.ItemTag.Item.Type.* prefix
// "ALIS.ItemTag.Item.Type.Consumable" -> filter button "Consumable"
```

**Benefits:**
- Zero duplication - single source of truth in JSON `tags` field
- No extra ObjectDefinition code
- UI discovers roles dynamically from content

**Filter buttons:** Auto-generated from unique `Item.Type.*` values:
- Consumable, Equipment, Currency, Quest, Key, etc.

### 7. Hard-Fail Validation in Generator

**JSON is SOT - generator must reject bad data:**

| Error | Action |
|-------|--------|
| Unknown capability type | Hard error, abort generation |
| Unknown property key | Hard error (or warning + skip) |
| Bad value format | Hard error |
| Invalid gameplay tag | Hard error |

No silent poisoning of content.

### 8. Property Application Pattern [DONE]

**Core Rule:** Components are passive data holders. Spawn utility uses validation registry + ImportText with container overrides.

**Implementation:** [ObjectSpawnUtility.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp) - `SetPropertyByName()`

**Supported types:** FText, FGameplayTag, FGameplayTagContainer, FVector, FRotator, FIntPoint, TArray\<TSoftObjectPtr\>, TMap\<FGameplayTag, float\>, bool 'b' prefix fallback.

**Validation pattern:** See [CapabilityValidationRegistry.h](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/CapabilityValidationRegistry.h) - static initializer registration.

### 9. Capability Type Registry [DONE]

Uses existing [FCapabilityRegistry](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/CapabilityRegistry.h) - CDO scan pattern proven by Hinged/Sliding/Lockable.

Components self-register via `GetPrimaryAssetId()` returning `("CapabilityComponent", "TypeName")`.

### 10. Replication Requirements

**Pattern:** `SetIsReplicatedByDefault(true)` in constructor + `GetLifetimeReplicatedProps()` for replicated properties.

[DONE] **Pickup** - Replicates `Quantity` (implemented in PickupCapabilityComponent.cpp)

⏳ **Consumable** - Will replicate `OverrideMagnitudes` (Phase 4)

**Interaction authority:** Server authoritative - never trust client for inventory modifications.

---

## Target JSON Structure

**See:** [Layer Contract - JSON Structure](../../Plugins/Resources/ProjectObject/docs/layer_contract.md#json-structure)

---

## JSON Property Format Rules

Strict formatting to avoid agent garbage:

| Type | Format | Example |
|------|--------|---------|
| `bool` | `true` or `false` | `"CanBeDropped": "true"` |
| `float/int` | Plain numeric string | `"Weight": "0.5"` |
| `FIntPoint` | `X,Y` (no parentheses) | `"GridSize": "1,1"` |
| `FVector` | `X,Y,Z` | `"Offset": "0,0,50"` |
| `GameplayTagContainer` | Comma-separated, trimmed | `"Tags": "Item.Type.Consumable,Item.Survival"` |
| `TArray<SoftPath>` | Single path OR semicolon-separated | `"Effects": "/Game/GE_A"` or `"/Game/GE_A;/Game/GE_B"` |
| `TMap<Tag,float>` | `Tag=Value` or `Tag=Value;Tag2=Value` | `"Magnitudes": "SetByCaller.Hydration=-30.0"` |

**Parser rules:**
- Trim whitespace
- Ignore empty entries
- Single value accepted (no separator needed)
- Log invalid tags (not silent accept)

---

## Implementation Phases

### Registry API Reference (Canonical Entrypoints)

**Use these exact signatures throughout implementation:**

```cpp
// Capability type registry (component class resolution)
UClass* CompClass = FCapabilityRegistry::GetCapabilityClass(FName CapabilityType);

// Validation registry (property validators)
const FCapabilityValidateFunc* Validator = FCapabilityValidationRegistry::GetValidationFunc(FName CapabilityType);
// Returns: TFunction<TArray<FCapabilityValidationResult>(const TMap<FName, FString>&)>* (nullable)
```

**Note:** Both registries use direct static functions, not `Get()` singleton pattern.

---

### ~~Phase 0: AssetRegistry Tags~~ [DONE]

**Implemented:** [ObjectDefinition.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Data/ObjectDefinition.cpp) - `GetAssetRegistryTags()`

**Docs:** [ProjectObject README - AssetRegistry Tags](../../Plugins/Resources/ProjectObject/README.md#assetregistry-tags)

---

### ~~Phase 1: UI Labels/Filtering~~ [DONE]

**Implemented:** [SProjectObjectBrowser.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/Widgets/SProjectObjectBrowser.cpp)

**Docs:** [ProjectPlacementEditor README - Capability Filtering](../../Plugins/Editor/ProjectPlacementEditor/README.md#capability-filtering)

---

### ~~Phase 2: Runtime Spawn Utility~~ [DONE]

**Files:** [ObjectSpawnUtility.h/.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Spawning/ObjectSpawnUtility.h), [ProjectObjectActorFactory.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/ProjectObjectActorFactory.cpp)

**Docs:** See code comments for property parsing formats (FText, containers, maps, bool prefix fallback).

---

### ~~Phase 3: Pickup Capability Component~~ [DONE]

**Files:** [PickupCapabilityComponent.h/.cpp](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h)

**Docs:** See [CapabilityValidationRegistry.h](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/CapabilityValidationRegistry.h) for validation registration pattern.

---

### ~~Phase 4: Clean Pickup (Remove GAS Fields)~~ [DONE]

**Goal:** Refactor PickupCapabilityComponent to follow Layer Contract - world interaction only, no GAS.

**Change:** Stripped [PickupCapabilityComponent.h](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h) to minimal:
- `InitialQuantity`, `PickupTime` (config)
- `Quantity` (runtime, replicated)
- `OnPickupAttempted` delegate

**Removed:** All item identity data (DisplayName, Weight, Tags) and GAS refs. These now live in:
- Item data --> `FItemData` struct in ObjectDefinition (see [Layer Contract](../../Plugins/Resources/ProjectObject/docs/layer_contract.md#layer-2-item-data-identityrules-only))
- GAS behavior --> UseProfile/EquipProfile in ProjectGASProfiles (future Phase 5)

**Module changes:**
- Removed GameplayAbilities, ProjectGAS deps from [Build.cs](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/ProjectObjectCapabilities.Build.cs)
- Removed GAS plugin deps from [uplugin](../../Plugins/Gameplay/ProjectObjectCapabilities/ProjectObjectCapabilities.uplugin)

**EXIT CRITERIA:** PickupCapabilityComponent has NO GAS imports, build succeeds.

---

### ~~Phase 5: GAS Profiles~~ [DROPPED]

**Dropped:** No shared GAS configs found (35 items analyzed). PickupRole + inline magnitudes sufficient. See Decision #7.

---

### Phase 6: Inventory Integration

**Moved to:** [ProjectInventory/TODO.md - Phase 4](../../Plugins/Features/ProjectInventory/TODO.md#phase-4-objectdefinition-migration)

**Summary:** Switch inventory from `ItemDefinition` to `ObjectDefinition`, use `UPickupCapabilityComponent`, read item data via `GetItemData()`.

**EXIT CRITERIA:** Pickup --> Inventory --> Drop cycle works with ObjectDefinition.

---

### Phase 7: Migration Commandlet & Placement Panel Update

**Goal:** Verify UI filtering works with real pickup objects (created via migration). Update placement panel to use Pickup sub-filters.

**Sub-tasks:**

1. **Run Migration Commandlet** (see details below)
   - Converts item.json --> object.json with Pickup/Consumable/Equipment capabilities
   - Adds PickupRole to all items (detection logic one-time use only)
   - Adds `Item.Type.Currency` tag to currency items
   - Updates loot containers to reference ObjectDefinition
   - Converts placed pickup actors to AInteractableActor with Pickup component

2. **Verify Placement Panel Filtering**
   - Open placement panel
   - Click "Pickup" filter --> Should show converted items
   - Click "Consumable" --> Should show only consumables (water, food, potions)
   - Click "Equipment" --> Should show only equipment (crowbar, tools)
   - Click "Currency" --> Should show only currency (cigarette, gold)
   - Click "Quest" --> Should show only quest items
   - Click "Key" --> Should show only keys
   - Verify NO asset loads during filtering (check logs)

3. **Update SProjectObjectBrowser.cpp** (if not already done in Phase 1)
   - Pickup sub-filter logic already implemented in Phase 1
   - Just verify it works with real data now

**Migration Commandlet Details:**

**New:** `MigrateItemsToObjectsCommandlet.cpp`

**Migrate ALL references:**

1. **item.json --> object.json** with split capabilities:
   - Read old `item.json` files
   - Create `object.json` with Pickup + optional Consumable/Equipment
   - Detect PickupRole using `UPickupCapabilityComponent::DetectPickupRoleFromLegacyItem()`:
     ```cpp
     // In migration commandlet - uses component's detection logic (single source)
     FName PickupRole = UPickupCapabilityComponent::DetectPickupRoleFromLegacyItem(
         bHasConsumableCapability,
         bHasEquipmentCapability,
         ItemTags,
         bIsQuestItem
     );
     ```
   - Add `Item.Type.Currency` tag to items detected as Currency role (if not already present)

2. **Loot containers** - `FLootEntry.ItemDefinition` --> `ObjectDefinition`
   - Find all loot container data tables
   - Update `FLootEntry.ItemId` to reference ObjectDefinition

3. **Placed pickup actors** - convert to AInteractableActor with Pickup capability
   - Find all placed pickup actors in levels
   - Replace with AInteractableActor + Pickup component
   - Preserve transform and any other properties

4. **Save games** - legacy resolver handles runtime (no migration needed)

**EXIT CRITERIA:** All items migrated, placement panel Pickup sub-filters work with real data.

---

### Phase 8: Deprecation & Cleanup

**New:** `MigrateItemsToObjectsCommandlet.cpp`

Migrate ALL references:
1. **item.json -> object.json** with split capabilities
2. **Loot containers** - migrate to ObjectDefinition + LootContainer capability (see below)
3. **Placed pickup actors** - convert to AInteractableActor with Pickup capability
4. **Save games** - legacy resolver handles runtime

#### Loot Container Architecture (New)

Loot containers follow the same pattern as pickups - become ObjectDefinition with capabilities:

| Before (deprecated) | After |
|---------------------|-------|
| `AProjectLootContainerActor` | `AInteractableActor` + `LootContainer` capability |
| `FLootEntry` with `TSoftObjectPtr<UItemDefinition>` | `FLootEntry` with `FPrimaryAssetId ObjectId` |

**JSON example:**
```json
{
  "capabilities": [
    { "type": "LootContainer", "properties": { "OpenTime": "1.0", "OneTimeUse": "true" } }
  ],
  "sections": {
    "loot": {
      "entries": [
        { "objectId": "ObjectDefinition:WaterBottle", "quantity": 2 },
        { "objectId": "ObjectDefinition:Bandage", "quantity": 1 }
      ]
    }
  }
}
```

**Code locations:**
- `ULootContainerCapabilityComponent` → `ProjectObjectCapabilities/LootContainer/`
- `FLootSection` → `ProjectObject/Data/ObjectDefinition.h` (like FItemSection)
- `FLootEntry` → `ProjectInventory/Loot/LootTypes.h` (already migrated - uses FPrimaryAssetId)

**Goal:** Mark ProjectItems as deprecated, keep for compatibility, remove after 1 release cycle.

**Immediate actions:**

1. **Add deprecation banner to ProjectItems README:**
   ```markdown
   # ⚠️ DEPRECATED - DO NOT USE

   This plugin is deprecated and will be removed in a future release.

   **Migration:** All items have been converted to ObjectDefinition with Pickup capability.
   Use the Objects placement panel with "Pickup" filter instead.

   **For developers:** This plugin is kept for save game compatibility only (legacy resolver).
   New content should use ObjectDefinition + Pickup/Consumable/Equipment capabilities.
   ```

2. **Update docs to remove ProjectItems references:**
   - `docs/data/README.md` - Remove ItemDefinition from table
   - `docs/editor/placement.md` - Remove Items section, document Pickup filter
   - `docs/architecture/flexible_path.md` - Update examples
   - All plugin READMEs - Replace ItemDefinition with ObjectDefinition examples

3. **Monitor logs for legacy warnings:**
   - Watch for `"Legacy ItemDefinition reference"` logs during gameplay
   - Track frequency - if rare (< 1% of loads), safe to remove after 1 release

**After 1 release cycle (Phase 8b - future):**

1. **Verify no legacy usage:**
   - Check logs for legacy warnings (should be zero)
   - Grep codebase for `ItemDefinition` references
   - Verify all save games migrated

2. **Remove plugin:**
   - Delete `Plugins/Resources/ProjectItems/` entirely
   - Remove `ProjectPickupItemActorFactory.h/cpp`
   - Remove legacy resolver from inventory code
   - Remove `SProjectItemBrowser.h/cpp` (if not already aliased)

**EXIT CRITERIA:**
- Deprecation banner visible in README
- Docs updated to use ObjectDefinition
- Plugin kept for compatibility (1 release minimum)

---

## Files to Create

| File | Purpose |
|------|---------|
| `ProjectObject/Public/Spawning/ObjectSpawnUtility.h` | Shared runtime spawn function (editor + runtime) |
| `ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` | Implementation (extract from factory + property import overrides) |
| `ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h` | Inventory-facing pickup data (passive holder) |
| `ProjectObjectCapabilities/Private/Pickup/PickupCapabilityComponent.cpp` | Validation + legacy detection logic |
| `ProjectObjectCapabilities/Public/Consumable/ConsumableCapabilityComponent.h` | GAS effects (passive holder) |
| `ProjectObjectCapabilities/Private/Consumable/ConsumableCapabilityComponent.cpp` | Validation logic |
| `ProjectObjectCapabilities/Public/Equipment/EquipmentCapabilityComponent.h` | Equipment slots/abilities (passive holder) |
| `ProjectObjectCapabilities/Private/Equipment/EquipmentCapabilityComponent.cpp` | Validation logic |
| `ProjectDefinitionGenerator/Private/MigrateItemsToObjectsCommandlet.cpp` | Migration tool (item.json --> object.json) |

**Note:** Uses **existing** registries (FCapabilityRegistry for type resolution, FCapabilityValidationRegistry for property validation). No new interfaces or registries needed.

## Files to Modify

| File | Change |
|------|--------|
| `ProjectObjectActorFactory.cpp` | Call `ProjectObjectSpawn::SpawnFromDefinition()` instead of inline parsing |
| `InventoryTypes.h` | `ItemId` -> `ObjectId` + legacy resolver |
| `InventoryInteractionHandler.cpp` | Find `UPickupCapabilityComponent` |
| `ProjectInventoryComponent.cpp` | Use `ProjectObjectSpawn::SpawnFromDefinition()` for drops |
| `SProjectObjectBrowser.cpp` | Add dynamic capability filter (Level 1 + Level 2) |
| `ObjectDefinition.cpp` | Add `GetAssetRegistryTags()` override (export capability tags) |

## Files to Delete (Phase 8)

| File | Reason |
|------|--------|
| `Plugins/Resources/ProjectItems/` | Plugin dissolved (after deprecation period) |
| `ProjectPickupItemActorFactory.h/cpp` | Replaced by runtime spawn |
| `SProjectItemBrowser.h/cpp` | Aliased to filtered Objects view |

---

## Documentation Updates

### Immediate Doc Fixes (Before Implementation)

| File | Change |
|------|--------|
| `Plugins/Resources/ProjectItems/README.md` | Add deprecation banner: "DEPRECATED - migration only - do not add new content" |
| `docs/data/structure.md` | Fix two-tier path definition (SOT vs Runtime) |
| `Plugins/Resources/ProjectItems/docs/designer_flow.md` | Rewrite: "Drag ObjectDefinition with Pickup capability" |

### Post-Implementation Doc Updates

| File | Change |
|------|--------|
| `docs/data/README.md:86-87` | Remove ProjectItems from table, add Pickup capability note |
| `docs/editor/placement.md` | Update to unified panel with capability filters |
| `docs/architecture/flexible_path.md` | Update ItemDefinition examples |
| `docs/architecture/plugin_rules.md:170` | Remove ProjectItems from plugin table |
| All plugin READMEs | Update ItemDefinition references |

### Docs to Delete (with plugin in Phase 8)

- `Plugins/Resources/ProjectItems/README.md`
- `Plugins/Resources/ProjectItems/docs/designer_flow.md`
- `Plugins/Resources/ProjectItems/docs/items_structure.md`

---

## Migration Scope (Complete)

| What | Migration Action |
|------|-----------------|
| item.json files | Convert to object.json with Pickup/Consumable/Equipment capabilities |
| Loot containers | Update FLootEntry to reference ObjectDefinition |
| Placed pickup actors | Convert to AInteractableActor (migration commandlet) |
| Save games | Legacy resolver at runtime (1 release) |
| UI data tables | Update any ItemId keys to ObjectId |

---

## Verification

1. **Runtime spawn test:** Call `ProjectObjectSpawn::SpawnFromDefinition()` in PIE, verify actor correct
2. **Editor placement test:** Drag object from browser, verify same result as runtime
3. **Inventory cycle:** Pickup -> inventory -> drop -> pickup again
4. **Migration test:** Run commandlet, validate all JSON valid
5. **Legacy test:** Load old save with ItemDefinition refs, verify resolves correctly
6. **Smoke test:** Boot test with migrated content

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Editor-only factory | Phase 1 creates runtime helper FIRST |
| God component | Split into Pickup/Consumable/Equipment from start |
| Save game breaks | Legacy resolver for 1 release cycle |
| Large migration | Run commandlet in CI, validate before merge |
| Doc drift | Fix contradictions BEFORE implementation |

---

## Execution Order

1. ~~**Phase 0: AssetRegistry Tags**~~ [DONE]
2. ~~**Phase 1: UI Filtering**~~ [DONE]
3. ~~**Phase 2: Runtime Spawn Utility**~~ [DONE]
4. ~~**Phase 3: Pickup Capability**~~ [DONE]
5. ~~**Phase 4: Clean Pickup (Remove GAS)**~~ [DONE] - Layer Contract enforced
6. ~~**Phase 5b: FItemSectionData struct**~~ [DONE] - Sections parsing in generator
7. ~~**Phase 5: GAS Profiles**~~ [DROPPED] - No shared configs, PickupRole + inline magnitudes sufficient
8. ~~**Phase 6: Inventory Integration**~~ [DONE] - ObjectDefinitionCache, IPickupSource, IObjectSpawnService
9. ~~**Phase 7: Migration & Verification**~~ [DONE] - JSON converted, scenes updated
10. ~~**Phase 8: Cleanup**~~ [DONE] - ProjectItems plugin deleted

---

## Critical Implementation Requirements (Must-Have)

[DONE] **All implemented in** [ObjectSpawnUtility.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp):

1. **Null-Safe Error Handling** - `SetError()` helper with null check + logging
2. **Module Loading Guard** - `EnsureCapabilityModulesLoaded()` with error caching
3. **Registry API Consistency** - Uses `FCapabilityRegistry::GetCapabilityClass()` and `FCapabilityValidationRegistry::GetValidationFunc()`
4. **Rich Error Context** - All errors include object/capability/property context

---

## Progress

| Phase | Status | Key Files |
|-------|--------|-----------|
| 0: AssetRegistry Tags | [DONE] | [ObjectDefinition.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Data/ObjectDefinition.cpp) |
| 1: UI Filtering | [DONE] | [SProjectObjectBrowser.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/Widgets/SProjectObjectBrowser.cpp) |
| 2: Runtime Spawn | [DONE] | [ObjectSpawnUtility.cpp](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp) |
| 3: Pickup Capability | [DONE] | [PickupCapabilityComponent.cpp](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/Pickup/PickupCapabilityComponent.cpp) |
| 4: Clean Pickup (No GAS) | [DONE] | [PickupCapabilityComponent.h](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h) |
| 5b: FItemSectionData | [DONE] | [ObjectDefinition.h](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h), [DefinitionJsonParser.cpp](../../Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionJsonParser.cpp) |
| 5: GAS Profiles | [DROPPED] | No shared configs - PickupRole + inline magnitudes sufficient |
| 6: Inventory Integration | [DONE] | [IObjectSpawnService](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Services/IObjectSpawnService.h), [ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) |
| 7: Migration | [DONE] | JSON converted to object.json with sections.item, scenes updated |
| 8: Cleanup | [DONE] | ProjectItems plugin deleted, SProjectItemBrowser removed |
