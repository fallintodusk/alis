// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioInventoryInput.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "MVVM/InventoryViewModel.h"
#include "ProjectGameplayTags.h"
#include "Scenario/SinglePlayScenarioSlateInput.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryCellDropTarget.h"
#include "Widgets/W_ItemContextMenu.h"

namespace
{
	bool IsUsableWidget(const UWidget& Widget, const APlayerController& Controller)
	{
		return Widget.GetOwningPlayer() == &Controller && Widget.IsVisible() &&
			Widget.GetCachedGeometry().GetLocalSize().GetMin() > 1.0f;
	}

	bool RouteClick(UWidget& Widget, const FKey& Button, int32& InOutPointerEventCount)
	{
		const FVector2D Position = Widget.GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5));
		const bool bRouted = FSinglePlayScenarioSlateInput::RouteClick(Widget, Position, Button);
		InOutPointerEventCount += bRouted ? 2 : 0;
		return bRouted;
	}
}

bool FSinglePlayScenarioInventoryInput::TickUseItem(
	FName ItemName,
	UInventoryViewModel& ViewModel,
	APlayerController& Controller,
	bool& bContextOpened,
	bool& bUseRequested,
	int32& InOutPointerEventCount,
	FString& OutError)
{
	if (bUseRequested) { return true; }
	const FInventoryEntryView* Entry = ViewModel.GetCachedEntriesForDiagnostics().FindByPredicate(
		[ItemName](const FInventoryEntryView& Candidate)
		{
			return Candidate.ItemId.PrimaryAssetName == ItemName;
		});
	if (Entry == nullptr)
	{
		OutError = FString::Printf(TEXT("Inventory item '%s' is unavailable for Use input."), *ItemName.ToString());
		return false;
	}

	if (!bContextOpened)
	{
		const int32 Cell = Entry->GridPos.Y * UInventoryViewModel::HandGridSize + Entry->GridPos.X;
		for (TObjectIterator<UW_InventoryCellDropTarget> It; It; ++It)
		{
			if (IsUsableWidget(**It, Controller) &&
				It->GetSurfaceTag() == ProjectTags::Item_Container_LeftHand && It->GetCellIndex() == Cell &&
				It->GetHostedCell() != nullptr &&
				RouteClick(*It->GetHostedCell(), EKeys::RightMouseButton, InOutPointerEventCount))
			{
				bContextOpened = true;
				return true;
			}
		}
		OutError = TEXT("The real left-hand item context input target is unavailable.");
		return false;
	}

	for (TObjectIterator<UW_ItemContextMenu> It; It; ++It)
	{
		UButton* UseAction = It->GetUseActionButton();
		if (IsUsableWidget(**It, Controller) && It->IsMenuVisible() && UseAction != nullptr &&
			UseAction->GetIsEnabled() && UseAction->GetCachedGeometry().GetLocalSize().GetMin() > 1.0f &&
			RouteClick(*UseAction, EKeys::LeftMouseButton, InOutPointerEventCount))
		{
			bUseRequested = true;
			return true;
		}
	}
	return true;
}
