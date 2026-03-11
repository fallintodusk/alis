# Layer Contract: Object Data Architecture

One definition system. Capabilities for world interactions. Sections for other systems.

## Core Definitions

- **ObjectDefinition** is the only definition type.
- **Capabilities** = world interactions only.
- **Sections** = data for other systems.
- **Item** = ObjectDefinition with `sections.item`.
- **World pickup item** = Item + `Pickup` capability.

## Core Principle

```text
ObjectDefinition (JSON SOT)
  |
  +-- meshes[]           = visual representation
  |
  +-- capabilities[]     = world interactions only
  |     - Pickup         (PickupTime, InitialQuantity)
  |     - Hinged         (OpenAngle, Speed, TargetMesh)
  |     - Lockable       (LockTag, DefaultLocked)
  |     - Sliding        (OpenOffset, Speed, TargetMesh)
  |
  +-- sections{}         = data for other systems (optional)
        - item           (identity + rules + behavior refs)
        - loot           (future)
        - quest          (future)
```

## What Makes Something an "Item"?

An **item** is an ObjectDefinition that has `sections.item`.

A **world pickup item** is an item that also has `Pickup` capability (can be picked up from world).

**Items without Pickup capability:** Vendor rewards, quest rewards, crafting outputs - they exist in inventory but may never exist as world actors.

A **door** is an ObjectDefinition that:
- Has `Hinged` + `Lockable` capabilities
- No `item` section

**There is no separate ItemDefinition asset type.**

---

## Capabilities (World Interactions Only)

**Location:** [ProjectObjectCapabilities](../../../Gameplay/ProjectObjectCapabilities/) module

**Rule:** A capability answers: "What can the player DO to this object in the world?"

| Capability | Properties | Purpose |
|------------|------------|---------|
| `Pickup` | PickupTime, InitialQuantity | Can be picked up |
| `Hinged` | OpenAngle, Speed, TargetMesh | Rotates on hinge |
| `Lockable` | LockTag, DefaultLocked | Can be locked/unlocked |
| `Sliding` | OpenOffset, Speed, TargetMesh | Slides open/closed |

**Hard contract:** Capability properties MUST be world-interaction fields.

**Violations:**
- DisplayName in Pickup (that's for UI, not world)
- Weight in Pickup (that's for inventory rules, not world)
- Effects in Pickup (that's for GAS, not world)

---

## Sections (Data for Other Systems)

**Location:** `TMap<FName, FInstancedStruct> Sections` in [ObjectDefinition](../Source/ProjectObject/Public/Data/ObjectDefinition.h)

**Rule:** A section answers: "What data do other systems need about this object?"

### Item Section

**Purpose:** Data for inventory, UI, and GAS systems. Contains identity, rules, and behavior refs.

| Field | Type | Purpose |
|-------|------|---------|
| DisplayName | FText | Identity - UI display |
| Description | FText | Identity - tooltip |
| Icon | TSoftObjectPtr | Identity - inventory icon |
| Weight | float | Rules - encumbrance |
| Volume | float | Rules - container capacity |
| MaxStack | int32 | Rules - stacking |
| GridSize | FIntPoint | Rules - grid inventory |
| Tags | FGameplayTagContainer | Identity - categorization |
| bCanBeDropped | bool | Rules |
| bCanBeTraded | bool | Rules |
| bIsQuestItem | bool | Rules |
| Magnitudes | TMap<FGameplayTag, float> | Consumable - SetByCaller values |
| bConsumeOnUse | bool | Consumable - remove on use |
| GrantedAbilities | TArray<FSoftClassPath> | Equipment - C++ ability classes |
| GrantedEffects | TArray<FSoftObjectPath> | Equipment - GameplayEffect assets |
| EquipSlotTag | FGameplayTag | Equipment - slot tag |
| EquipAbilitySet | FSoftObjectPath | Equipment - generated AbilitySet (filled by generator) |

**Hard contract:** Section fields MUST NOT be world-interaction fields.

**The object doesn't "know" inventory tech.** It just contains optional data. Only inventory/GAS code interprets the item section.

---

## GAS Boundary: Compile-Time vs Runtime

```text
COMPILE-TIME (no GAS dependency)
================================
ProjectCore (tags, base types)
    |
    v
ProjectObject (ObjectDefinition, sections)
  - stores FSoftObjectPath, TMap<FGameplayTag, float>
  - NO #include of GameplayEffect.h
    |
    v
ProjectObjectCapabilities (Pickup, Hinged, Lockable)
  - world interactions only
  - NO GAS references

================================
RUNTIME (GAS dependency)
================================
ProjectInventory / ProjectGAS
  - reads item section
  - loads FSoftObjectPath --> UGameplayEffect*
  - applies effects with SetByCaller magnitudes
```

---

## One Editor Panel

**Kill separate Items/Objects panels.** One "Definitions" browser with AssetRegistry tag filters:

| Filter | AssetRegistry Tag |
|--------|-------------------|
| All Objects | (no filter) |
| Pickups | `ALIS.Cap.Pickup = "true"` |
| Items | `ALIS.Section.Item = "true"` OR `ALIS.ItemTag.*` |
| Doors | `ALIS.Cap.Hinged` OR `ALIS.Cap.Lockable` |
| Consumables | `ALIS.ItemTag.Item.Type.Consumable` |
| Equipment | `ALIS.ItemTag.Item.Type.Equipment` |

**No asset loading for filtering.** All filters read AssetRegistry tags only.

---

## JSON Structure

**Canonical form** (matches runtime `TMap<FName, FInstancedStruct> Sections`):

```json
{
  "id": "WaterBottle",
  "meshes": [
    {
      "id": "body",
      "asset": "/ProjectObject/HumanMade/Consumables/Drink/WaterBottle/SM_WaterBottle"
    }
  ],

  "capabilities": [
    {
      "type": "Pickup",
      "scope": ["actor"],
      "properties": {
        "InitialQuantity": "1",
        "PickupTime": "0.5"
      }
    }
  ],

  "sections": {
    "item": {
      "displayName": "Water Bottle",
      "description": "A bottle of water.",
      "icon": "/Game/UI/Icons/Items/T_WaterBottle",
      "weight": 0.5,
      "volume": 0.1,
      "maxStack": 5,
      "gridSize": "1,1",
      "tags": "Item.Type.Consumable,Item.Survival.Hydration",
      "canBeDropped": true,
      "canBeTraded": true,
      "isQuestItem": false,

      "magnitudes": {
        "SetByCaller.Hydration": -30.0
      },
      "consumeOnUse": true
    }
  }
}
```

**Schema vs authoring:**
- `object.schema.json` validates **canonical form only** (`sections.item`)
- Generator accepts **sugar** (top-level `"item": {...}`) and maps it to `sections.item`

---

## GAS Integration

### Consumables (magnitudes)

JSON specifies SetByCaller magnitudes. Runtime applies via `GE_GenericInstant`:

```json
"magnitudes": { "SetByCaller.Hydration": -30.0 },
"consumeOnUse": true
```

Runtime: `UProjectGASLibrary::ApplyMagnitudes(ASC, Magnitudes)`

### Equipment (abilities/effects)

JSON specifies C++ ability classes and effect assets. Generator emits AbilitySet:

```json
"grantedAbilities": ["/Script/ProjectCombat.GA_MeleeAttack"],
"grantedEffects": ["/Script/ProjectGAS.GE_MeleeStats"],
"equipSlotTag": "Item.EquipmentSlot.MainHand"
```

Runtime: `AbilitySet->GiveToAbilitySystem(ASC, &Handles)`

---

## Inventory Flow

Inventory stores **instances** (lightweight):
```cpp
struct FInventoryEntry
{
    FPrimaryAssetId ObjectId;  // "ObjectDefinition:WaterBottle"
    int32 Quantity;
    // Instance-specific data (durability, ammo, etc.)
};
```

Definition data (DisplayName, Weight, Effects) lives in `ObjectDefinition.Sections["Item"]`.

**Pickup flow:**
1. Player interacts with object that has Pickup capability
2. Pickup capability broadcasts `OnPickupAttempted`
3. Inventory system creates `FInventoryEntry` with ObjectId + Quantity
4. World actor destroyed

**Drop flow:**
1. Inventory system calls `ProjectObjectSpawn::SpawnFromDefinition(ObjectId)`
2. World actor spawned with Pickup capability
3. `FInventoryEntry` removed

---

## Deferred: Profile Layer

**Status:** Not implemented. Deferred until reuse appears.

**When to implement:** When N items share exact same behavior (effects + magnitudes). Currently each item has unique magnitudes (35 analyzed, 0 sharing).

---

## See Also

- [ProjectObjectCapabilities README](../../../Gameplay/ProjectObjectCapabilities/README.md) - Capability components
- [merge_pickups_into_objects.md](../../../../todo/current/merge_pickups_into_objects.md) - Implementation plan
