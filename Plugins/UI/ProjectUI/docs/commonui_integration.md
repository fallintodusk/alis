# CommonUI Integration Guide

## Overview

CommonUI is Epic's plugin for gamepad-friendly, cross-platform UI. ProjectUI builds on top of it with JSON-driven layouts, MVVM, and theming.

## Prerequisites (Required Setup)

Before using CommonUI features, ensure these project settings:

### 1. Game Viewport Client

Set `Game Viewport Client Class` to `CommonGameViewportClient`:
- Project Settings > Engine > General Settings > Default Classes > Game Viewport Client Class
- Or in `DefaultEngine.ini`:
```ini
[/Script/Engine.Engine]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
```

### 2. Enhanced Input Support

Enable Enhanced Input under Common Input settings:
- Project Settings > Game > Common Input Settings > Enable Enhanced Input Support = true

### 3. Module Dependencies

For C++ usage, add to your `.Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "CommonUI",
    "CommonInput",
    "EnhancedInput"
});
```

### 4. Input Action Data Assets

Create `UInputAction` assets for common UI actions (Confirm, Back, Tab Next, Tab Prev) and register them with CommonUI input settings.

## What CommonUI Provides

| Feature | Class | Use Case |
|---------|-------|----------|
| Input routing + activation | `UCommonActivatableWidget` | `GetDesiredInputConfig()` returns `TOptional<FUIInputConfig>` |
| Action bar prompts | `UCommonBoundActionBar` | Shows controller hints like "[A] Confirm [B] Back" |
| Tabs with gamepad nav | `UCommonTabListWidgetBase` | Tab list + linked switcher with Next/Prev actions |
| Buttons with input binding | `UCommonButtonBase` | Integrates with `CommonActionWidget` for platform icons |
| Text widgets | `UCommonTextBlock`, `UCommonRichTextBlock` | Styled text with CommonUI style assets |
| Button styles | `UCommonButtonStyle` | Data-driven button appearance |
| Text styles | `UCommonTextStyle` | Data-driven text appearance |

## What ProjectUI Provides (Unique Value)

| Feature | Class | Notes |
|---------|-------|-------|
| JSON-driven layouts | `ProjectWidgetLayoutLoader` | Data-driven widget trees |
| MVVM pattern | `ProjectViewModel` | Reactive data binding |
| Theme system | `ProjectUIThemeData`, `ProjectUIThemeManager` | Centralized visual styling |
| Definition registry | `ProjectUIRegistrySubsystem` | Widget definition loading/lookup |
| Factory + VM injection | `ProjectUIFactorySubsystem` | Widget creation with ViewModel |
| Layer hosting | `ProjectUILayerHostSubsystem` | HUD/layer management |

## Loading Screen Plugin

**Important:** The loading screen is a separate plugin, not part of CommonUI.

| Plugin | Purpose |
|--------|---------|
| `CommonLoadingScreen` | Epic's first-party loading screen plugin (used by Lyra, Parrot) |

Setup pattern from Parrot sample:
1. Preload async in `GameInstance`
2. Show in "preload" phase
3. Remove in "postload" phase

Only consider this if you have actual loading issues (stutter, teardown on map travel).

## Integration Recommendations

### High ROI: Action Bar

Add `UCommonBoundActionBar` to your root HUD layer for controller hints:

```cpp
// In your HUD widget
UPROPERTY(meta = (BindWidget))
UCommonBoundActionBar* ActionBar;
```

**Note:** ActionBar updates on Tick. If your pause stops player ticking, you may need "tick while paused" for the owning player/actor.

### Medium ROI: Tabs

Use `UCommonTabListWidgetBase` if:
- You're fighting gamepad navigation bugs
- You want standardized Next/Prev tab input actions

### Optional: Button Migration

Only migrate to `UCommonButtonBase` if you need:
- Per-button input icons (like "[E] Use" embedded in button)
- Console-first UI

**ProjectUI's `ProjectButton` recommendation:**
- Keep `ProjectButton` for visual styling with JSON layouts
- Add action bar at HUD level for gamepad hints (separate concern)

### Do NOT Replace

Keep ProjectUI's unique pillars:
- JSON layout system
- MVVM bindings
- Theme layer

CommonUI styles (`UCommonButtonStyle`, `UCommonTextStyle`) complement but don't replace a fully data-driven theme pipeline.

## References

- [UCommonActivatableWidget::GetDesiredInputConfig](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget/GetDesiredInputConfig)
- [Using the Common Bound Action Bar](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-common-bound-action-bar-in-unreal-engine)
- [UCommonTabListWidgetBase](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonTabListWidgetBase)
- [UCommonButtonBase](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonButtonBase)
- [UCommonTextBlock](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonTextBlock)
- [Parrot UI Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/user-interface-for-parrot-in-unreal-engine)
