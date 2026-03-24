# Fix Font Glyph Warnings (U+F12A) - RESOLVED

## Symptom

When picking up a backpack and opening inventory, Slate logs:
```
LogSlate: Warning: Could not find Glyph Index 0 with codepoint U+f12a,
  getting last resort font data DroidSansFallback.ttf
```
Icons render correctly despite warnings. Only happens when inventory has items with icon codepoints.

## Root Cause (FOUND)

**UE_LOG outputs raw PUA characters -> Editor Output Log widget renders them with Roboto -> warning.**

The warning was NEVER from our UI widgets. It came from the editor's Output Log:

1. `InventoryViewModel.cpp:470` logged: `"BuildHandCellTexts: Hands entry '\uF12A' -> Left hand"`
2. `W_InventoryPanel.cpp:413,420` logged (Verbose): `"LHand[0]: \uF12A"`
3. The editor's Output Log widget (SMultiLineEditableText) rendered these log lines
4. Output Log uses Roboto/DroidSansFallback (no PUA glyphs) -> Slate warned

**Why pickup triggers it:** No backpack = no item with IconCode in hand slot = no PUA in log.
Equip slots don't log icon codes -> no warning from them.

### Engine source confirmation

Warning emitted at `SlateFontRenderer.cpp:380` in `FSlateFontRenderer::GetFontFaceForCodepoint()`.
No widget context available at that depth (7 call levels below STextBlock).
The warning fires for ANY text rendering path, including the Output Log.

## Fix Applied

Replace raw PUA chars with hex in all UE_LOG calls:

**InventoryViewModel.cpp:470** (Log level - always fires):
```cpp
// Before: UE_LOG(..., *Label, ...)  where Label = "\uF12A"
// After:  UE_LOG(..., *SafeLabel, ...)  where SafeLabel = "U+F12A"
const FString SafeLabel = (!Label.IsEmpty() && Label[0] >= 0xE000)
    ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Label[0]))
    : Label;
```

**W_InventoryPanel.cpp:413,420** (Verbose level):
```cpp
const FString Safe = (!Str.IsEmpty() && Str[0] >= 0xE000)
    ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0])) : Str;
```

## What Was Tried (6 failed attempts - all targeted wrong source)

| # | What | Result | Why it failed |
|---|------|--------|---------------|
| 1 | Remove static font variable | Warning persists | Wrong source |
| 2 | Cache fonts as members | Warning persists | Wrong source (also timing bug) |
| 3 | SetFont before SetText in TextUpdater | Warning persists | Wrong source (good practice though) |
| 4 | Diagnostic logging | Confirmed timing bug | Timing bug was real but secondary |
| 5 | EnsureFontsResolved() lazy init | Warning persists | Wrong source |
| 6 | Early return if fonts not resolved | Warning persists | Proved UpdateGridTexts is NOT the source |

All 6 fixes assumed a UI widget was rendering U+F12A with wrong font.
The actual source was UE_LOG passing PUA chars to the Output Log widget.

## Key Lesson

When Slate warns about a missing glyph, the rendering widget might NOT be your code.
The editor's Output Log, tooltip, and other built-in widgets also render text.
If your UE_LOG contains non-ASCII/PUA characters, the Output Log triggers the warning.

---

## Investigation Methodology (for future similar bugs)

### What worked: Comprehensive text flow mapping + engine source search

1. **Map ALL text paths** - found 11 distinct code paths that could render text
2. **Search engine source** for warning message -> found exact location in SlateFontRenderer.cpp:380
3. **Realized** warning has no widget context (7 levels deep) -> can't identify widget from warning
4. **Hypothesized** Output Log as source -> searched for UE_LOG with PUA content -> found 3 culprits

### What would have worked faster: Segmentation

Disabling ALL inventory UI (#7 in segmentation matrix) would have shown warning STILL fires
(because UE_LOG runs in ViewModel BEFORE any widget exists) -> immediate pivot to non-widget source.

### For future: Debug toolkit

**A. PUA-safe logging helper:**
```cpp
// Add to ProjectCore or ProjectUI utilities
static FString SafeLogStr(const FString& Str)
{
    if (!Str.IsEmpty() && Str[0] >= 0xE000)
        return FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0]));
    return Str;
}
```

**B. Engine-level font trace (if needed for deeper issues):**
- CVar `UI.Debug.FontTrace` -> log every glyph lookup with font + codepoint
- Engine patch at `SlateFontRenderer.cpp:380` -> add callstack capture on warning
- Reference: @[docs/testing/agent_ue_inspection.md] for autonomous test infrastructure

**C. Segmentation CVars (if needed for widget-level isolation):**

| CVar | What it disables |
|------|-----------------|
| `UI.Debug.DisableGridTexts` | Grid cell text updates |
| `UI.Debug.DisableEquipIcons` | Equip slot icon text |
| `UI.Debug.DisableItemIcon` | Selection panel ItemIcon |
| `UI.Debug.DisableTooltip` | Item tooltip text |
| `UI.Debug.DisableContextMenu` | Context menu text |
