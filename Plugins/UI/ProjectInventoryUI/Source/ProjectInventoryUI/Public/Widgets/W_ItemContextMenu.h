// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "W_ItemContextMenu.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContextMenuClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContextMenuAction, int32, InstanceId);

	/**
	 * Item Context Menu - floating popup for item actions.
 *
 * Shows on click/right-click on inventory item.
 * Actions visibility/enabled state comes from ViewModel action descriptors.
 *
 * Layout structure (ItemContextMenu.json):
 * - Border (ContextMenuRoot)
 *   - VerticalBox (MainColumn)
 *     - Border (HeaderRow)
 *       - HorizontalBox (HeaderContent)
 *         - TextBlock (ItemNameText)
 *         - Button (CloseButton)
 *     - Border (Divider)
 *     - VerticalBox (ActionsColumn)
 *       - Button (UseAction)
 *       - Button (EquipAction)
 *       - Button (DropAction)
 *       - Button (SplitAction)
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_ItemContextMenu : public UProjectUserWidget
{
    GENERATED_BODY()

public:
    UW_ItemContextMenu(const FObjectInitializer& ObjectInitializer);

    /** Show context menu for given item at absolute screen position. */
    UFUNCTION(BlueprintCallable, Category = "ContextMenu")
    void ShowForItem(const FInventoryEntryView& EntryView, const TArray<FProjectUIActionDescriptor>& ActionDescriptors, const FVector2D& AbsolutePosition);

    /** Hide and clear context menu. */
    UFUNCTION(BlueprintCallable, Category = "ContextMenu")
    void Hide();

    /** Check if menu is currently visible. */
    UFUNCTION(BlueprintPure, Category = "ContextMenu")
    bool IsMenuVisible() const;

    // Action delegates - bind to handle item actions
    UPROPERTY(BlueprintAssignable, Category = "ContextMenu")
    FOnContextMenuAction OnUseAction;

    UPROPERTY(BlueprintAssignable, Category = "ContextMenu")
    FOnContextMenuAction OnEquipAction;

    UPROPERTY(BlueprintAssignable, Category = "ContextMenu")
    FOnContextMenuAction OnDropAction;

    UPROPERTY(BlueprintAssignable, Category = "ContextMenu")
    FOnContextMenuAction OnSplitAction;

    UPROPERTY(BlueprintAssignable, Category = "ContextMenu")
    FOnContextMenuClosed OnMenuClosed;

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    // Button click handlers
    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleUseClicked();
    UFUNCTION() void HandleEquipClicked();
    UFUNCTION() void HandleDropClicked();
    UFUNCTION() void HandleSplitClicked();

    // Apply action visibility/enabled state from unified descriptor model.
    void UpdateActionVisibility(const TArray<FProjectUIActionDescriptor>& ActionDescriptors);

    // Position menu in parent canvas local space, avoiding edges.
    void PositionAtAbsoluteLocation(const FVector2D& AbsolutePosition);

    // Child widgets
    TWeakObjectPtr<UTextBlock> ItemNameText;
    TWeakObjectPtr<UButton> CloseButton;
    TWeakObjectPtr<UButton> UseAction;
    TWeakObjectPtr<UButton> EquipAction;
    TWeakObjectPtr<UTextBlock> EquipActionText;
    TWeakObjectPtr<UButton> DropAction;
    TWeakObjectPtr<UButton> SplitAction;

    // Current item being shown
    int32 CurrentInstanceId = INDEX_NONE;
};
