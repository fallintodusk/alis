# ProjectVitalsUI

Vitals HUD and expanded panel UI for survival mechanics.

## Purpose

Displays player vitals (Condition, Stamina, Calories, Hydration, Fatigue) and status effects (Bleeding, Poisoned, Radiation) using the MVVM pattern.

**What ProjectVitalsUI provides:** VitalsViewModel, W_VitalsHUD (mini bar), W_VitalsPanel (expanded view), JSON layouts.

**What ProjectVitalsUI does NOT do:** Calculate vitals, manage ASC, handle metabolism. That belongs to ProjectVitals.

**SOT:** This plugin for vitals UI; [ProjectVitals](../../Gameplay/ProjectVitals/README.md) for vitals logic.

## Framework Consolidation Rules (Critical)

- Keep vitals plugin code domain-specific (attribute mapping, vitals presentation semantics).
- Reuse ProjectUI primitives for widget binding, popup/tooltip lifecycle, and shared interaction mechanics.
- Do not introduce local generic helpers when the logic is reusable by other UI plugins.
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

## Architecture

### MVVM Pattern

```
ASC (GAS attributes) -> VitalsViewModel -> Widget (W_VitalsHUD, W_VitalsPanel)
                                        -> JSON Layout (visual structure)
                                        -> Theme (colors/fonts)
```

### Components

| Component | Purpose |
|-----------|---------|
| VitalsViewModel | Reads ASC attributes, exposes ViewModel properties, broadcasts changes |
| W_VitalsHUD | Mini HUD bar (Condition only) + warning icons, click to expand |
| W_VitalsPanel | Expanded panel with all vitals, values, status effects |

### Warning Flags (Font-Safe)

Uses boolean flags, not combined strings, for reliable glyph rendering.
Icons use game-icons.ttf PUA codepoints (see `Plugins/UI/ProjectUI/Content/Slate/Fonts/game-icons.css`):

- `bWarnAnyLow` - Any vital in Low state (hazard-sign 0xF718)
- `bWarnAnyCritical` - Any vital in Critical/Empty state (skull-crossed-bones 0xFC9B)
- `bIsBleeding` - Active bleeding (bleeding-wound 0xF1CA)
- `bIsPoisoned` - Active poison (biohazard 0xF1AC)
- `RadiationLevel > 0` - Radiation exposure (radioactive 0xFB46)

## Usage

### Spawning the UI (ProjectUI)

Definitions live in `Config/UI/ui_definitions.json`. ProjectUI loads them and owns creation.

```cpp
// In your player controller or game mode:
UProjectUILayerHostSubsystem* LayerHost =
    GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>();

if (LayerHost)
{
    LayerHost->InitializeForPlayer(PlayerController);
}
```

Notes:
- VitalsHUD is preloaded as a HUD slot widget (HUD.Slot.VitalsMini).
- VitalsPanel is on-demand in UI.Layer.Menu.
- Both use vm_creation=Global so they share one VitalsViewModel instance.

### Refreshing Data

VitalsViewModel does NOT auto-subscribe to ASC attribute changes (to avoid coupling). Call `RefreshFromASC()` when needed:

```cpp
// Option 1: Timer-based refresh (simple, 0.5s interval)
GetWorld()->GetTimerManager().SetTimer(RefreshTimer, [VitalsVM]()
{
    VitalsVM->RefreshFromASC();
}, 0.5f, true);

// Option 2: Subscribe to ASC attribute changes (more efficient)
ASC->GetGameplayAttributeValueChangeDelegate(UHealthAttributeSet::GetConditionAttribute())
    .AddUObject(this, &MyClass::OnConditionChanged);

void MyClass::OnConditionChanged(const FOnAttributeChangeData& Data)
{
    VitalsVM->RefreshFromASC();
}
```

### Hotkey Toggle

```cpp
// In your input handling (EnhancedInput or legacy):
void MyPlayerController::HandleVitalsHotkey()
{
    VitalsVM->TogglePanel();
}
```

## Color Mapping

Uses existing theme semantic colors (no new theme entries needed):

| Vital State | Theme Color | Visual |
|-------------|-------------|--------|
| OK (>70%) | Success | Green |
| Low (40-70%) | Warning | Orange |
| Critical (20-40%) | Error | Red |
| Empty (<=20%) | Error | Red |

Fatigue (inverted):
| Fatigue State | Theme Color |
|---------------|-------------|
| Rested (<30%) | Success |
| Tired (30-60%) | Warning |
| Exhausted (60-85%) | Error |
| Critical (>=85%) | Error |

## File Structure

```
ProjectVitalsUI/
  README.md                         # This file
  ProjectVitalsUI.uplugin
  Source/ProjectVitalsUI/
    Public/
      ProjectVitalsUIModule.h
      MVVM/
        VitalsViewModel.h           # ViewModel with per-vital properties
      Widgets/
        W_VitalsHUD.h               # Mini HUD bar
        W_VitalsPanel.h             # Expanded panel (two-column layout)
    Private/
      ProjectVitalsUIModule.cpp
      MVVM/
        VitalsViewModel.cpp
      Widgets/
        W_VitalsHUD.cpp
        W_VitalsPanel.cpp
  Data/
    VitalsHUD.json                  # HUD layout (hot-reloadable)
    VitalsPanel.json                # Panel layout (hot-reloadable)
    ui_definitions.json             # ProjectUI definitions
```

## Dependencies

- ProjectUI: MVVM base, JSON layout loader, theme system
- ProjectGAS: AttributeSets (HealthAttributeSet, StaminaAttributeSet, etc.)
- ProjectVitals: EVitalState, EFatigueState enums
- GameplayAbilities: UAbilitySystemComponent

## Hot Reload (Editor)

Edit JSON layouts and call `ReloadLayout()` on the widget to see changes without restarting:

```cpp
// In console or via debug command:
VitalsHUD->ReloadLayout();
VitalsPanel->ReloadLayout();
```

## References

- [ProjectUI README](../ProjectUI/README.md) - MVVM, layout, theme system
- [ProjectVitals README](../../Gameplay/ProjectVitals/README.md) - Vitals logic
- [ProjectVitals Design Vision](../../Gameplay/ProjectVitals/docs/design_vision.md) - Design rationale
- [ProjectGAS README](../../Gameplay/ProjectGAS/README.md) - AttributeSets
