# Interaction Prompt Widget

HUD prompt showing `[E] Open` / `[E] Close` when focusing interactable parts.

## Current State

- Part-level highlighting works (only meshes with capabilities highlight)
- `HasCapabilityForComponent()` exists but returns bool, not capability
- No focus change delegate
- No prompt widget

## Design Decisions

- **Capability-focused** - Store focused capability, not just component
- **State-aware labels** - "Open" vs "Close" changes while focused
- **Fallback** - Never empty, default "Interact"
- **No ViewModel** - Direct delegate binding

## Implementation Steps

### 1. Add `GetInteractionLabel()` to Interface

File: `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractableTarget.h`

```cpp
// Add to IInteractableComponentTargetInterface
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
FText GetInteractionLabel() const;
```

**Label rules (never empty):**

| Capability | Label |
|------------|-------|
| SpringRotatorComponent | "Open" / "Close" (state-based) |
| SpringSliderComponent | "Open" / "Close" (state-based) |
| LockableComponent (locked) | "Unlock" |
| PickupComponent | "Pickup" |
| **Fallback** | "Interact" |

Interface default returns `NSLOCTEXT("Interaction", "Interact", "Interact")`

### 2. Change `HasCapabilityForComponent` -> `GetCapabilityForComponent`

File: `Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Public/InteractionComponent.h`

```cpp
// Returns capability targeting the mesh, or nullptr
UActorComponent* GetCapabilityForComponent(AActor* Actor, UPrimitiveComponent* Mesh) const;
```

### 3. Add Focus State and Delegate

File: `Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Public/InteractionComponent.h`

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInteractionFocusChanged,
    AActor*, Actor,
    UPrimitiveComponent*, Component,
    FText, Label);  // By value for Blueprint

UPROPERTY(BlueprintAssignable, Category = "Interaction")
FOnInteractionFocusChanged OnFocusChanged;

private:
    TWeakObjectPtr<UActorComponent> FocusedCapability;
    FText FocusedLabel;
```

### 4. Update SetFocusedActor - Track Capability and Label

```cpp
// Default fallback label
static const FText DefaultInteractionLabel = NSLOCTEXT("Interaction", "Interact", "Interact");

void UInteractionComponent::SetFocusedActor(AActor* NewFocus, UPrimitiveComponent* HitComponent)
{
    // Resolve capability (returns nullptr if not interactable)
    UActorComponent* NewCapability = GetCapabilityForComponent(NewFocus, HitComponent);
    if (!NewCapability)
    {
        NewFocus = nullptr;
        HitComponent = nullptr;
    }

    // Query label from capability (with fallback - never empty)
    FText NewLabel = DefaultInteractionLabel;
    if (NewCapability && NewCapability->Implements<UInteractableComponentTargetInterface>())
    {
        FText CapLabel = IInteractableComponentTargetInterface::Execute_GetInteractionLabel(NewCapability);
        if (!CapLabel.IsEmpty())
        {
            NewLabel = CapLabel;
        }
    }

    // Check if anything changed (actor + component + capability)
    const bool bSameFocus =
        (FocusedActor.Get() == NewFocus) &&
        (FocusedComponent.Get() == HitComponent) &&
        (FocusedCapability.Get() == NewCapability);

    if (bSameFocus)
    {
        // Still same focus - check if label changed (Open -> Close)
        if (!NewLabel.EqualTo(FocusedLabel))
        {
            FocusedLabel = NewLabel;
            OnFocusChanged.Broadcast(FocusedActor.Get(), FocusedComponent.Get(), FocusedLabel);
        }
        return;
    }

    // Focus changed - unhighlight old, highlight new
    UPrimitiveComponent* OldComponent = FocusedComponent.Get();
    if (OldComponent)
        SetComponentCustomDepth(OldComponent, false);

    FocusedActor = NewFocus;
    FocusedComponent = HitComponent;
    FocusedCapability = NewCapability;
    FocusedLabel = NewLabel;

    if (HitComponent)
        SetComponentCustomDepth(HitComponent, true);

    OnFocusChanged.Broadcast(FocusedActor.Get(), FocusedComponent.Get(), FocusedLabel);
}
```

### 5. Create Simple Prompt Widget

File: `Plugins/UI/ProjectHUD/Source/ProjectHUD/Public/Widgets/W_InteractionPrompt.h`

```cpp
UCLASS()
class UW_InteractionPrompt : public UUserWidget
{
    UPROPERTY(meta = (BindWidget))
    UTextBlock* PromptText;

    UFUNCTION()
    void HandleFocusChanged(AActor* Actor, UPrimitiveComponent* Component, FText Label);

    void BindToInteractionComponent(UInteractionComponent* Comp);

protected:
    virtual void NativeDestruct() override;

private:
    TWeakObjectPtr<UInteractionComponent> BoundComponent;
};
```

**Binding (safe - prevent double bind):**
```cpp
void UW_InteractionPrompt::BindToInteractionComponent(UInteractionComponent* Comp)
{
    if (!Comp) return;

    // Unbind old if exists
    if (BoundComponent.IsValid())
    {
        BoundComponent->OnFocusChanged.RemoveDynamic(this, &UW_InteractionPrompt::HandleFocusChanged);
    }

    BoundComponent = Comp;
    Comp->OnFocusChanged.AddDynamic(this, &UW_InteractionPrompt::HandleFocusChanged);
}

void UW_InteractionPrompt::NativeDestruct()
{
    if (BoundComponent.IsValid())
    {
        BoundComponent->OnFocusChanged.RemoveDynamic(this, &UW_InteractionPrompt::HandleFocusChanged);
    }
    Super::NativeDestruct();
}
```

**Display logic:**
- `Component == nullptr` -> Hide widget
- Else -> Show `[E] {Label}` (label never empty due to fallback)

### 6. Add to HUD Layout

- Add prompt widget to W_HUDLayout (screen center)
- Bind on pawn possession, unbind on unpossess

## File Changes Summary

| File | Change |
|------|--------|
| IInteractableTarget.h | Add `GetInteractionLabel()` |
| SpringRotatorComponent.h/cpp | Implement label (Open/Close) |
| SpringSliderComponent.h/cpp | Implement label (Open/Close) |
| InteractionComponent.h | Add delegate, FocusedCapability, FocusedLabel |
| InteractionComponent.cpp | GetCapabilityForComponent, label query, broadcast |
| W_InteractionPrompt.h/cpp | New widget |
| W_HUDLayout.h/cpp | Add prompt slot |

## Out of Scope (Later)

- Key rebinding display (hardcode `[E]`)
- Localization (use NSLOCTEXT)
- Hold interactions
- Priority rules (multiple capabilities)
