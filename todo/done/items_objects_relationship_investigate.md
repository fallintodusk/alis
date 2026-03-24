According to a document from **January 10, 2026**, you already have the right architecture (composition) - you’re just missing the **data-driven glue** between JSON and “Actor = Mesh + Capability Components”.

## Fulfilled resume - what’s true in your current system

### You already locked the key rule

* `ProjectObject` = **ACTORS ONLY** (placeable world templates) that compose components.
* `ProjectObjectCapabilities` = reusable capability components (lock, health, power, etc.).
* `ProjectMotionSystem` = reusable motion components (spring rotator / slider).
* `ProjectItems` = item templates generated from JSON, with stable `ItemId` as primary asset id (rename-safe).
* Designer flow for pickups is already “instance container actor + template definition” and **SOT = ItemDefinition.WorldVisual**.

### Dependency reality (use it, don’t fight it)

`ProjectInventory` (Features) depends on both `ProjectObject` and `ProjectItems` - it is the correct bridge layer.

So: keep Resources clean, put “spawn policy” and “interaction bridging” in `ProjectInventory`.

---

## Your question: “why can’t an object definition have an item component?”

It can - but don’t encode it as “parent pointers in inventory”.

### Hard rule

* **Inventory never stores “parent object”** (actor id, template id, world ownership). That will rot under streaming, destruction, replication, and save/load.
* Inventory stores only:

  * `ItemId`, `Quantity`, plus intrinsic per-instance meta (durability, attachments, cosmetic variant).

World representation is derived from templates, not referenced backwards.

---

## The cleanest next step (minimal change, fits your docs)

### Step 1 - Add one shared runtime component in `ProjectCore`

Make a tiny, engine-agnostic “this actor contains an item stack” component:

* `UProjectItemStackComponent`

  * `FName ItemId`
  * `int32 Quantity`
  * (optional) `bool bConsumeOnPickup`

Why `ProjectCore`:

* both `ProjectItems` and `ProjectObject` already sit above `ProjectCore`, so no circular deps.

### Step 2 - Teach `ProjectInventory` to pick up anything with that component

Your interaction handler already coordinates lock + motion on doors, etc.
Add one more check:

* if target actor has `UProjectItemStackComponent`:

  * resolve `UItemDefinition` by `ItemId`
  * add to inventory
  * destroy / clear actor (depending on flag)

This instantly enables “composite pickup object” without merging plugins.

### Step 3 - Allow items to optionally drop as object templates (for rich visuals)

Right now item JSON supports simple world visuals (`staticMesh`, `skeletalMesh`, etc.).

Add one optional field:

```json
"world": {
  "staticMesh": "...",
  "objectTemplateId": "Obj_Bag_Canvas_01"
}
```

Drop rule in `ProjectInventory`:

* if `objectTemplateId` set - spawn that **ProjectObject template actor** (or later ObjectDefinition-driven spawn)
* attach `UProjectItemStackComponent` with `ItemId` and `Quantity`
* else - spawn the existing lightweight `AProjectPickupItemActor`

This preserves your “pickup actor is lightweight” principle while allowing upgrades when needed.

### Step 4 - (Optional) Later: JSON-driven object composition

You do **not** need to jump straight to “capabilities array -> dynamic AddComponent” yet.

Incremental path:

1. Start with JSON “ObjectDefinition” that selects a base C++ template actor class (door, chest, bag) and sets component properties.
2. Only if needed later, add a registry “GameplayTag -> ComponentClass” inside `ProjectObject` (it already depends on both capabilities and motion plugins).

---

## Answer to your instruction about “ask first”

No questions needed - the dependency flows and your existing pickup workflow already dictate the clean solution.

If you want, paste your current pickup component class name (the one that owns `ItemId` in details panel) and I'll tell you whether to:

* replace it with `UProjectItemStackComponent`, or
* keep it and just wrap it (adapter) to avoid touching existing designer levels.

---

## Folder Hierarchy Strategy (avoid "two hierarchies drift")

### Problem

Don't want:
* Art hierarchy (meshes/materials/textures) AND
* Separate SOT JSON hierarchy that must mirror it AND
* Generated defs hierarchy that must mirror both

That's where drift happens.

**Solution:** Pick ONE canonical hierarchy and make everything else derived.

---

### Contract Rules

#### Rule A - Folder path is the only category

No `categoryPath` string in JSON. Category changes = moving folders, not editing fields.

#### Rule B - Object templates: JSON next to art (single hierarchy)

Put the SOT JSON **next to the art** (1 folder = 1 object bundle):

```
Plugins/Resources/ProjectObject/Content/Template/Fenestration/Door_Wood_01/
  SM_Door_Wood_01.uasset
  MI_Door_Wood_01.uasset
  T_Door_Wood_01_BaseColor.uasset
  Door_Wood_01.alis.json          <- SOT
  DA_Door_Wood_01.uasset          <- generated definition
```

**Benefits:**
* Rename/move category folder once - everything follows
* ProjectPlacementEditor folder browsing keeps working
* No duplicate "category" field to sync

#### Rule C - Keep ProjectItems contract as-is

Items stay "data first" with category folders under `Data/Items/` (consumable/equipment/etc.).
That's fine because item categories are gameplay taxonomy, not art bundles.

---

### Generator Behavior (key change)

Object generator follows same pattern as items:

1. **Scan** for `*.alis.json` inside:
   * `ProjectObject/Content/**` (co-located SOT)
   * optionally `ProjectObject/Data/**` for data-only defs

2. **Output** generated `UProjectObjectDefinition` into same directory as JSON

3. **Write metadata** on generated asset:
   * `bGenerated`
   * `GeneratorVersion`
   * `SourceJsonPath`
   * `SourceJsonHash`

4. **Cleanup**: If JSON moved/renamed, old generated asset = orphan by `SourceJsonPath`, gets deleted

**Result:** category rename = move folder = regen + cleanup. No manual edits.

---

### Two Hierarchies Exception (narrow and coherent)

* `Data/` = data-taxonomy SOT (items, balance, tables)
* `Content/<ObjectFolder>/.../*.alis.json` = asset-bundle SOT (object templates tied to that folder)

This is intentional, not drift.

---

## Asset Reference Strategy (Simple - No Catalogs)

**Verdict:** Keep it dead simple. JSON stores either a **short local name** or a **full UE soft path**. Add an **Editor-only JSON reference fixer** that auto-updates JSON on asset rename/move, plus a **commandlet validator** for CI.

### JSON Value Rules

| Pattern | Meaning | Example |
|---------|---------|---------|
| Contains `/` | Full soft object path | `"/ProjectUI/Icons/T_Door.T_Door"` |
| No `/` | Local asset (same folder as JSON) | `"SM_Door"` |

```json
{
  "mesh": "SM_Door",                           // local in same folder as JSON
  "icon": "/ProjectUI/Icons/T_Door.T_Door"     // full path (cross-plugin)
}
```

### Resolver (Generator - Minimal C++)

```cpp
static FSoftObjectPath ResolveAssetRef(const FString& Ref, const FString& JsonFilePath)
{
    // Full path - use as-is
    if (Ref.Contains(TEXT("/")) || Ref.StartsWith(TEXT("/")))
    {
        return FSoftObjectPath(Ref);
    }

    // Local: same folder as JSON
    const FString MountFolder = DeriveMountFolderFromJsonPath(JsonFilePath);
    const FString AssetName = Ref;
    return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *MountFolder, *AssetName, *AssetName));
}
```

---

### Editor Plugin: Auto-Update JSON on Asset Move/Rename

**Separate TODO:** See [editor_json_asset_ref_fixer.md](editor_json_asset_ref_fixer.md) for full implementation plan.

**Summary:** Editor plugin listens to `OnAssetRenamed()`, auto-updates JSON files, CI commandlet validates refs.

---

### Minimal Policy

| Asset Location | JSON Reference Style |
|----------------|---------------------|
| Same folder as JSON | Short name: `"SM_Door"` |
| Cross-plugin | Full path: `"/OtherPlugin/.../SM_X.SM_X"` |

**Never** store parent object refs in inventory - breaks saves under streaming/replication.

**Always** run validator in CI - cheap insurance.

---

### Why This Works

* Most renames/moves happen in Editor - caught exactly
* JSON auto-updated at source - no drift
* No UUIDs or catalogs needed
* CI catches git mv or "Fix Up Redirectors" breakage
