# ProjectUI MVVM

Model-View-ViewModel pattern for keeping UI logic in C++ while UMG stays presentation-only.

## Scope and Paths
- Plugin: `Plugins/UI/ProjectUI`
- Base class: `Source/ProjectUI/Public/MVVM/ProjectViewModel.h`
- User widget base with binding hooks: `Source/ProjectUI/Public/Widgets/ProjectUserWidget.h`
- Property macros: `Source/ProjectUI/Public/MVVM/ViewModelPropertyMacros.h`

## Flow
1) Screen controller creates a ViewModel (`NewObject<UProject...ViewModel>`), calls `Initialize(GameInstance)`, then `OnViewAttached()`.
2) ViewModel subscribes to subsystems/models and exposes bindable properties via macros.
3) UMG widget binds to ViewModel getters and listens for property change notifications.
4) On teardown, the View calls `OnViewDetached()`, then the controller calls `Shutdown()` to remove delegates.

## Property Macros (quick view)
- `VIEWMODEL_PROPERTY(Type, Name)` -> read/write with change notifications.
- `VIEWMODEL_PROPERTY_READONLY(Type, Name)` -> public getter, private setter for ViewModel-only writes.
- `VIEWMODEL_PROPERTY_ARRAY(Type, Name)` -> array helpers (`Add/Remove/Clear`) with notifications.

Example:
```cpp
// In ViewModel
VIEWMODEL_PROPERTY_READONLY(float, LoadingProgress)

void UProjectLoadingViewModel::HandleProgress(float Value, FText Status)
{
    SetLoadingProgress(Value);
    SetStatusMessage(Status);
}
```

## Lifecycle Checklist
- Initialize: cache subsystems, subscribe to events, seed properties.
- OnViewAttached: start timers or polls that require the view to exist.
- OnViewDetached: pause UI-driven timers, but keep data alive if needed.
- Shutdown: remove all delegates, clear subsystem refs.

## Patterns to Keep
- One-way data flow: Model -> ViewModel -> View; Views trigger commands back on the ViewModel (e.g. `RequestCancel()`).
- No UI types inside ViewModels (no `UButton` pointers).
- Prefer semantic command methods over directly mutating properties from Views.

## Game Entity Isolation (CRITICAL)

UI is a black box that displays data and sends commands. It does NOT know about game entities.

### Layer Access Rules

| Layer | Can Access Game Entities? | Notes |
|-------|--------------------------|-------|
| **Widgets** | NO | Only read from ViewModel, send commands to ViewModel |
| **ViewModels** | Receive via Initialize() | Resolve data sources from context, expose to widgets |
| **Subsystems/Factory** | YES | Infrastructure layer, bridges game and UI |

### Widget Rules

Widgets (classes inheriting `UProjectUserWidget`) must NEVER:
- Call `GetOwningPlayer()` for game logic (infrastructure calls are OK)
- Call `GetOwningPlayerPawn()` - convention, not compile-time enforced (see note below)
- Access `APawn`, `ACharacter`, `APlayerController` directly
- Access game components (ASC, Inventory, etc.)
- Include `GameFramework/` headers

If a widget needs game data, add a property to its ViewModel.

**Note on GetOwningPlayerPawn():** This method is NOT virtual in UUserWidget, so it cannot
be overridden or blocked at compile-time. The convention to not call it is enforced via
code review. If you see widget code calling `GetOwningPlayerPawn()`, flag it in review.

### ViewModel Rules

ViewModels CAN:
- Receive context in `Initialize(UObject* Context)` (Pawn, PC, Widget)
- Resolve data sources from context (ASC, InventoryComponent, etc.)
- Subscribe to game services via interfaces

ViewModels should NOT:
- Store direct references to widgets
- Know about UMG types
- Call widget methods

### Data Flow Example

```
Game World                    UI Layer
---------------------------------------------
Pawn/ASC/Components  ->  ViewModel  ->  Widget
     ^                       |
     +---- Commands ---------+

Widget sees:
  - ViewModel.HealthPercent (not ASC)
  - ViewModel.ItemName (not InventoryComponent)
  - ViewModel.RequestUse() (not Pawn->UseItem())
```

### Enforcement

`UProjectUserWidget` enforces these rules:
- `GetOwningPlayerPawn()` - Returns nullptr, asserts in dev builds
- `GetOwningPlayer()` - Works (needed for UMG), logs at verbose level

## ViewModel as Bridge Layer (Architecture Decision)

**Question:** "Why does ViewModel know about APawn/ASC? Shouldn't UI be isolated from game entities?"

**Answer:** ViewModel IS the bridge layer. This is by design.

### Layer Responsibilities

```
┌─────────────────────────────────────────────────────────────────┐
│  Widget Layer (Presentation)                                    │
│  - Reads ViewModel properties (HealthPercent, bHasFocus)        │
│  - Sends commands (RequestUseItem, RequestInteract)             │
│  - ZERO knowledge of Pawn, ASC, Components                      │
│  - GetOwningPlayerPawn() is BLOCKED                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  ViewModel Layer (Bridge)                                       │
│  - Receives context via Initialize(UObject*)                    │
│  - Resolves game entities: Pawn, ASC, InventoryComponent        │
│  - Subscribes to game services with game-layer identifiers      │
│  - Translates game state → UI-friendly properties               │
│  - THIS LAYER IS ALLOWED TO KNOW ABOUT GAME ENTITIES            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  Game Layer (Services, Components, Actors)                      │
│  - IInteractionService, IInventoryService                       │
│  - APawn, ACharacter, UAbilitySystemComponent                   │
│  - Game logic, not UI                                           │
└─────────────────────────────────────────────────────────────────┘
```

### Established Patterns (All Cache Game Entities)

| ViewModel | Cached Game Entity | Purpose |
|-----------|-------------------|---------|
| VitalsViewModel | `TWeakObjectPtr<UAbilitySystemComponent>` | Read health/stamina attributes |
| InventoryViewModel | `TScriptInterface<IInventoryReadOnly>` | Read inventory state |
| InteractionPromptViewModel | `TWeakObjectPtr<APawn>` | Subscribe to per-pawn focus events |

### Service Subscription Pattern

When a service provides per-player events, ViewModel subscribes using game-layer identifier:

```cpp
// In ViewModel::Initialize(Context)
APawn* LocalPawn = ResolveFromContext(Context);  // ViewModel resolves this
Service->OnFocusChangedForPawn(LocalPawn).AddUObject(this, &ThisClass::HandleFocusChanged);

// Handler receives filtered events - no game entity in signature
void HandleFocusChanged(UPrimitiveComponent* Comp, FText Label)
{
    UpdatebHasFocus(Comp != nullptr);  // Pure UI state
    UpdateFocusLabel(Label);
}
```

**Why not filter in Widget?** Widget cannot access Pawn to compare.
**Why not filter in HUD?** HUD would become god-object knowing all ViewModels' internals.
**Why ViewModel?** It's the designated bridge layer - this is its job.

### Anti-Pattern: HUD Push (Don't Do This)

```cpp
// BAD - HUD becomes tightly coupled to all ViewModels
void UHUDComponent::HandleFocusChanged(APawn* Instigator, ...)
{
    if (Instigator == GetLocalPawn())
        InteractionPromptVM->SetFocus(Component, Label);  // HUD knows VM internals
}
```

This breaks the pattern where ViewModel owns its own subscription lifecycle.

### Summary

- **Widget:** Pure presentation, no game knowledge
- **ViewModel:** Bridge layer, ALLOWED to cache game refs and subscribe to services
- **Service:** Provides filtering APIs so ViewModel handlers stay clean

This is not a leak - it's the architecture working as designed.

## Example ViewModels
- Map list: `ProjectMenuUI/Public/ViewModels/ProjectMapListViewModel.h`
- Loading screen: `ProjectMenuUI/Public/ViewModels/ProjectLoadingViewModel.h`
- Menu navigation: `ProjectMenuUI/Public/ViewModels/ProjectMenuViewModel.h`

## Best Practices
- Subscribe in `Initialize`, unsubscribe in `Shutdown`.
- Use `GET_MEMBER_NAME_CHECKED` when comparing property names in View change handlers.
- Keep ViewModels testable: no reliance on UMG widgets, only subsystems/data.
- Document each property with intent (e.g. selection state, async flags).

## References
- Layout system that builds the widgets bound to ViewModels: `docs/ui_layout.md`
- Theme system used by the widgets: `docs/ui_theme.md`
- Global router page: `../../../docs/ui.md`
