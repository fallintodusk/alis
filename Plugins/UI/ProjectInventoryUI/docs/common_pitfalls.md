# ProjectInventoryUI - Common Pitfalls

Bugs and gotchas encountered during inventory UI development.

## Map Default Overwrites Valid Slot

**Symptom:** Wrong icon on equip slot (e.g. backpack slot shows footprint).

**Root cause:** `GetEquipSlotGridPos` default returned valid position `(1,2)` that collided with Back slot. Unmapped Accessory slot (processed after Back) overwrote it with footprint fallback.

**Fix:** Default to `(-1,-1)` for unmapped slots - bounds check filters them out.

**File:** `InventoryPanelGridBuilder.cpp` - `GetEquipSlotGridPos()`

## Hands Container Ignores IconCode

**Symptom:** Item in hand shows text name ("Backpack") instead of icon glyph.

**Root cause:** `BuildHandCellTexts` generic Hands fallback called `BuildEntryLabel()` directly, skipping `Entry.IconCode`. The `CellBuilder::Build()` path (used by LeftHand/RightHand containers) checks IconCode, but the generic Hands container path did not.

**Fix:** Check `Entry.IconCode` first, fall back to `BuildEntryLabel` only when empty.

**File:** `InventoryViewModel.cpp` - `BuildHandCellTexts()`

## Wrong Font Codepoint in JSON

**Symptom:** Item icon renders as wrong glyph (e.g. backpack shows as footprint).

**Root cause:** `game-icons.ttf` has glyph build errors. Knapsack (U+F831 GID 2096) has wrong outline data. The JSON referenced knapsack instead of backpack.

**Fix:** Verify icon name in `game-icons.css`, use correct name. Backpack = U+F12A (not knapsack U+F831).

**Reference:** `Plugins/UI/ProjectUI/Content/Slate/Fonts/README.txt` KNOWN ISSUES section.

## UE_LOG with PUA Icon Chars Triggers Slate Glyph Warnings

See [ProjectUI common_pitfalls.md #7](../../ProjectUI/docs/common_pitfalls.md#7-ue_log-with-puaicon-characters-triggers-slate-glyph-warnings-critical) for full explanation.

**Fixed in:** `InventoryViewModel.cpp` - `BuildHandCellTexts()`, `W_InventoryPanel.cpp` - `RebuildHandGrids()`

## static const TMap Not Refreshed After Hot Reload

**Symptom:** Icon codepoint change in C++ has no effect after rebuild.

**Root cause:** `static const TMap` initializes once per process. Hot reload loads new code but does not reinitialize static data in already-loaded translation units.

**Fix:** Full editor restart required after changing codepoints in static maps.

**File:** `InventoryPanelGridBuilder.cpp` - `GetEquipSlotIcon()`
