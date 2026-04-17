# Inventory UI Bugs — Packaged Build

**Status:** Resolved
**Priority:** Major
**Date:** 2026-04-17
**Related:** [Test Report](16.04.26_test_report.md) | [Release Plan](00_release_1.0.0.md)

---

## Problems

### B1 — Raw Definition Names Instead of Display Names

Item tooltips and labels show `ObjectDefinition:Crowbar`, `ObjectDefinition:CigarettePacket` etc. instead of human-readable names like "Crowbar", "Cigarette Packet".

**Likely cause:** InventoryUI ViewModel uses the `PrimaryAssetId` string instead of the item's `DisplayName` field from the ObjectDefinition.

### B2 — Icons Disappear After Drag or Re-open

When dragging items to backpack slots, the item icon disappears. Also when re-opening the inventory panel, previously placed item icons are gone.

**Likely cause:** Texture/icon soft references not resolved in packaged build, or icon widget not refreshed after drag-drop completes.

### B3 — Large Overlay Text on Screen

Definition names (`ObjectDefinition:Crowbar`, `ObjectDefinition:KeyDuniyaApartment`) rendered as large text overlapping the inventory UI.

**Likely cause:** Debug text rendering or tooltip positioned at screen-space instead of widget-relative coordinates.

---

## Root Cause

AssetManager didn't scan `ObjectDefinition` assets in standalone/debug game mode. When inventory queried item data, `GetPrimaryAssetObject()` returned null → empty DisplayName and IconCode → UI fell back to raw ID text at 64pt font.

## Fix

One change in [ObjectDefinitionCache.cpp:8-36](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp#L8-L36):

Added `ScanPathsForPrimaryAssets("ObjectDefinition", "/ProjectObject")` on first item lookup — forces AssetManager to discover all item assets. Then `TryLoad()` loads them synchronously into memory.

## Key Files

| File | Purpose |
|------|---------|
| [ObjectDefinitionCache.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp) | **Fixed** — added scan + TryLoad fallback |
| [ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) | GetItemDataView calls cache |
| [DefaultGame.ini:52](../../Config/DefaultGame.ini#L52) | `+DirectoriesToAlwaysCook=(Path="/ProjectObject")` — ensures assets are cooked |
