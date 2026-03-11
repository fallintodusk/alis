# Common UI Pitfalls

Lessons learned from debugging UE5 UMG widget issues.

---

## 1. HorizontalBox/VerticalBox Children Ignore Explicit Size

**Symptom:** Widget in HorizontalBox has `size: {x: 250, y: 24}` in JSON but renders as 20x32.

**Root cause:** BoxSlots (HorizontalBoxSlot/VerticalBoxSlot) use `SizeRule` (Auto/Fill), NOT pixel sizes. The "size" property only works for CanvasPanelSlot.

**Solution:** Wrap children with explicit size in `SizeBox`:
```cpp
// Layout loader wraps child in SizeBox when:
// 1. Parent is HorizontalBox or VerticalBox
// 2. Child JSON has explicit "size" property
USizeBox* SizeBox = CreateWidget<USizeBox>();
SizeBox->SetWidthOverride(Size.X);
SizeBox->SetHeightOverride(Size.Y);
SizeBox->AddChild(ChildWidget);
ParentBox->AddChild(SizeBox);  // Add SizeBox, not child directly
```

**Log to look for:**
```
SizeBox wrap: ConditionBar -> ConditionBar_SizeBox (250x24) in BarContainer
```

**File:** [ProjectWidgetLayoutLoader.cpp](../Source/ProjectUI/Private/Layout/ProjectWidgetLayoutLoader.cpp) - ProcessChildren()

---

## 2. OverlaySlot Alignment Not Applied (UE5 Race Condition)

**Symptom:** Set `SetHorizontalAlignment(HAlign_Fill)` but widget stays at Left/Top (0,0).

**Root cause:** `UOverlay::OnSlotAdded()` calls `BuildSlot()` IMMEDIATELY when parent Slate widget exists. BuildSlot creates Slate slot with default alignment BEFORE your Set*Alignment call executes.

**Timeline:**
```
1. Overlay->AddChild(Widget)
2. OnSlotAdded() -> BuildSlot() with HAlign=Left, VAlign=Top (defaults)
3. Your code: SetHorizontalAlignment(HAlign_Fill)  // Too late! Slate slot already built
```

**Solution:** Call `SynchronizeProperties()` after setting alignment:
```cpp
UOverlaySlot* Slot = Overlay->AddChildToOverlay(Widget);
Slot->SetHorizontalAlignment(HAlign_Fill);
Slot->SetVerticalAlignment(VAlign_Fill);
Slot->SynchronizeProperties();  // CRITICAL: Re-syncs UPROPERTY values to Slate slot
```

**Log to look for:**
```
AttachWidgetToSlot: Overlay VitalsMiniSlot <- W_VitalsHUD_0 (Fill alignment + SynchronizeProperties)
```

**Files:**
- [ProjectUIFactorySubsystem.cpp](../Source/ProjectUI/Private/Subsystems/ProjectUIFactorySubsystem.cpp) - AttachWidgetToSlot()
- [ProjectUserWidget.cpp](../Source/ProjectUI/Private/Widgets/ProjectUserWidget.cpp) - LoadLayoutFromConfig()

---

## 3. Widget Has 0x0 Size (PENDING vs ZERO!)

**Symptom:** Widget tree dump shows `[0x0]` size.

**Diagnosis:**
- `[PENDING]` - Geometry not computed yet (normal during construction)
- `[ZERO!]` - Geometry computed but widget has no size (BUG)

**Common causes for ZERO!:**
- Parent slot has no size (CanvasPanelSlot with size 0,0 and AutoSize=false)
- OverlaySlot not set to Fill alignment
- Widget's DesiredSize is zero (no content)

**Debug command:**
```
Project.UI.DumpWidgetTree
```

---

## 4. CanvasPanelSlot Size vs AutoSize

**Symptom:** Widget in CanvasPanel has unexpected size.

**Rule:**
- `anchor: "Fill"` -> Slot size becomes left/top/right/bottom offsets (0,0 = full stretch)
- `size: {x:N, y:N}` -> Explicit size, AutoSize=false
- `autoSize: true` -> Size to content, explicit size ignored

**Conflict resolution:** If both `size` and `autoSize: true` specified, size wins (AutoSize forced false).

**File:** [LayoutSlotConfig.cpp](../Source/ProjectUI/Private/Layout/LayoutSlotConfig.cpp)

---

## 5. SetIgnoreMoveInput Uses a Counter (CRITICAL!)

**Symptom:** After closing a UI panel, character cannot move even though input mode is set to Game.

**Root cause:** `APlayerController::SetIgnoreMoveInput(true/false)` uses an internal **counter**, not a boolean flag:
- `SetIgnoreMoveInput(true)` **increments** the counter
- `SetIgnoreMoveInput(false)` **decrements** the counter
- Movement is ignored while counter > 0

If `ShowDefinition` is called multiple times (e.g., bootstrap + delegate), the counter increments multiple times. A single `HideDefinition` only decrements once, leaving the counter > 0.

**Example bug scenario:**
```
First I press (opening):
  1. Bootstrap ShowDefinition → SetIgnoreMoveInput(true)  [counter: 0→1]
  2. Delegate ShowDefinition  → SetIgnoreMoveInput(true)  [counter: 1→2]

Second I press (closing):
  1. HideDefinition → SetIgnoreMoveInput(false)           [counter: 2→1]
  // Counter still > 0, movement still blocked!
```

**Solution:** Use `ResetIgnoreMoveInput()` instead of `SetIgnoreMoveInput(false)`:
```cpp
// WRONG - only decrements counter by 1
PlayerController->SetIgnoreMoveInput(false);

// CORRECT - resets counter to 0
PlayerController->ResetIgnoreMoveInput();
```

**Same applies to:** `SetIgnoreLookInput` / `ResetIgnoreLookInput`

**File:** [ProjectUILayerHostSubsystem.cpp](../Source/ProjectUI/Private/Subsystems/ProjectUILayerHostSubsystem.cpp) - ApplyInputForActiveLayers()

**Debug tip:** Add logging to track the counter:
```cpp
UE_LOG(LogTemp, Log, TEXT("IgnoreMoveInput=%d, IgnoreLookInput=%d"),
    PlayerController->IsMoveInputIgnored(), PlayerController->IsLookInputIgnored());
```

---

## 6. Font Not Rendering (AAAAA or Missing Glyphs)

**Symptom:** Text shows as "AAAAA" pattern, boxes, or missing characters in dynamically created TextBlocks.

**Root cause:** `FSlateFontInfo` from theme (`CurrentTheme->Typography.BodySmall`) may only have `Size` set but no valid font family. The default constructor doesn't load any font.

**Wrong approach:**
```cpp
// Theme Typography has Size but no font family loaded
SlotText->SetFont(CurrentTheme->Typography.BodySmall);  // May have invalid font!
```

**Correct approach:**
```cpp
// Use the layout loader's font resolver which loads Inter/Roboto fonts
SlotText->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), CurrentTheme));
```

**File:** [LayoutThemeResolver.cpp](../Source/ProjectUI/Private/Layout/LayoutThemeResolver.cpp) - GetFont()

**Available font names:** HeadingLarge, HeadingMedium, HeadingSmall, BodyLarge, BodyMedium, BodySmall, Button, Label, Icon

---

## 7. UE_LOG with PUA/Icon Characters Triggers Slate Glyph Warnings (CRITICAL!)

**Symptom:** `LogSlate: Warning: Could not find Glyph Index 0 with codepoint U+XXXX, getting last resort font data DroidSansFallback.ttf` - appears intermittently, hard to trace.

**Root cause:** The editor's **Output Log widget** renders ALL log text with Roboto/DroidSansFallback. If your `UE_LOG` contains raw Private Use Area characters (U+E000-U+F8FF, used by icon fonts like game-icons.ttf), the Output Log tries to render them and fails.

**Why it's hard to find:**
- The Slate warning at `SlateFontRenderer.cpp:380` has NO widget context (7 call levels deep)
- You assume the warning comes from YOUR TextBlock widgets
- All widget-level font fixes have no effect
- The warning only fires when code paths that LOG icon chars are hit

**Wrong:**
```cpp
// IconCode = "\uF12A" - raw PUA char goes into Output Log -> Slate warns
UE_LOG(LogMyUI, Log, TEXT("Item icon: %s"), *Entry.IconCode);
```

**Correct:**
```cpp
// Log hex representation - safe for Output Log
const FString SafeIcon = (!Entry.IconCode.IsEmpty() && Entry.IconCode[0] >= 0xE000)
    ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Entry.IconCode[0]))
    : Entry.IconCode;
UE_LOG(LogMyUI, Log, TEXT("Item icon: %s"), *SafeIcon);
```

**Rule:** Never pass raw PUA/icon codepoint strings to UE_LOG. Always log hex (`U+XXXX`) instead.

**Applies to:** Any code using icon fonts with PUA codepoints (game-icons.ttf, FontAwesome, Material Icons, etc.)

**Case study:** [ProjectInventoryUI common_pitfalls.md](../../ProjectInventoryUI/docs/common_pitfalls.md#ue_log-with-pua-icon-chars-triggers-slate-glyph-warnings) - took 6 failed widget-level fixes before identifying UE_LOG as the source

---

## Quick Debug Checklist

1. Run `Project.UI.DumpWidgetTree` - Check for ZERO!, PENDING, SlotSize=0!, Align flags
2. Check log for `SizeBox wrap:` messages (HorizontalBox children)
3. Check log for `SynchronizeProperties` messages (Overlay alignment)
4. Use Widget Reflector (`Ctrl+Shift+W`) for interactive inspection
5. Filter log by `LogProjectUILayerHost` - Trace input mode changes and active layers
6. Check `IsMoveInputIgnored()` / `IsLookInputIgnored()` after UI operations
