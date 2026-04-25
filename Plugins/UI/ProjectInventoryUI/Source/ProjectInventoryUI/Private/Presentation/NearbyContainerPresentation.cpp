// Copyright ALIS. All Rights Reserved.

#include "Presentation/NearbyContainerPresentation.h"

#include "MVVM/InventoryViewModel.h"

namespace
{
    FText FormatWeight(float Current, float Max)
    {
        return FText::FromString(FString::Printf(TEXT("Weight: %.1f / %.1f kg"), Current, Max));
    }

    FText FormatVolume(float Current, float Max)
    {
        return FText::FromString(FString::Printf(TEXT("Volume: %.1f / %.1f L"), Current, Max));
    }
}

FText FNearbyContainerPresentation::BuildTitle(const UInventoryViewModel* VM)
{
    if (VM && VM->GetbHasNearbyContainer() && !VM->GetNearbyContainerLabel().IsEmpty())
    {
        return VM->GetNearbyContainerLabel();
    }
    return NSLOCTEXT("Inventory", "NearbyLootFallbackTitle", "Nearby Loot");
}

FText FNearbyContainerPresentation::BuildStats(const UInventoryViewModel* VM)
{
    if (!VM || !VM->GetbHasNearbyContainer())
    {
        return FText::GetEmpty();
    }

    FString StatsText = FText::Format(
        NSLOCTEXT("Inventory", "NearbyLootStats", "{0}   {1}"),
        FormatWeight(VM->GetNearbyContainerCurrentWeight(), VM->GetNearbyContainerMaxWeight()),
        FormatVolume(VM->GetNearbyContainerCurrentVolume(), VM->GetNearbyContainerMaxVolume())).ToString();

    if (VM->GetNearbyContainerCellDepthUnits() > 1)
    {
        StatsText += FString::Printf(TEXT("   Depth: %d/cell"), VM->GetNearbyContainerCellDepthUnits());
    }

    return FText::FromString(StatsText);
}

bool FNearbyContainerPresentation::ShouldShowTakeAll(const UInventoryViewModel* VM)
{
    return VM && VM->GetbHasNearbyContainer();
}

bool FNearbyContainerPresentation::IsTakeAllEnabled(const UInventoryViewModel* VM)
{
    return ShouldShowTakeAll(VM) && VM->HasNearbyEntries();
}

bool FNearbyContainerPresentation::ShouldShowHint(const UInventoryViewModel* VM)
{
    // Hint is visible when the panel is open but no session is active.
    // The panel itself chooses to render nothing when the whole widget is
    // collapsed; this helper focuses on the hint row inside the widget.
    return VM && VM->GetbPanelVisible() && !VM->GetbHasNearbyContainer();
}
