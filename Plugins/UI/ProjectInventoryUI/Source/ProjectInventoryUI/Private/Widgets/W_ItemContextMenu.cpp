// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_ItemContextMenu.h"
#include "MVVM/InventoryViewModel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Presentation/ProjectUIWidgetBinder.h"

DEFINE_LOG_CATEGORY_STATIC(LogItemContextMenu, Log, All);

UW_ItemContextMenu::UW_ItemContextMenu(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectInventoryUI"), TEXT("ItemContextMenu.json"));
    SetIsFocusable(true);
}

void UW_ItemContextMenu::NativeConstruct()
{
    Super::NativeConstruct();

    if (RootWidget)
    {
        FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
        ItemNameText = Binder.FindRequired<UTextBlock>(TEXT("ItemNameText"));
        CloseButton = Binder.FindRequired<UButton>(TEXT("CloseButton"));
        UseAction = Binder.FindRequired<UButton>(TEXT("UseAction"));
        EquipAction = Binder.FindRequired<UButton>(TEXT("EquipAction"));
        DropAction = Binder.FindRequired<UButton>(TEXT("DropAction"));
        SplitAction = Binder.FindRequired<UButton>(TEXT("SplitAction"));
        Binder.LogMissingRequired(TEXT("UW_ItemContextMenu::NativeConstruct"));

        // Bind button clicks
        if (CloseButton.IsValid())
        {
            CloseButton->OnClicked.AddDynamic(this, &UW_ItemContextMenu::HandleCloseClicked);
        }
        if (UseAction.IsValid())
        {
            UseAction->OnClicked.AddDynamic(this, &UW_ItemContextMenu::HandleUseClicked);

            if (UHorizontalBox* HBox = Cast<UHorizontalBox>(UseAction->GetChildAt(0)))
            {
                for (int32 i = HBox->GetChildrenCount() - 1; i >= 0; --i)
                {
                    if (UTextBlock* TB = Cast<UTextBlock>(HBox->GetChildAt(i)))
                    {
                        UseActionText = TB;
                        break;
                    }
                }
            }
        }
        if (EquipAction.IsValid())
        {
            EquipAction->OnClicked.AddDynamic(this, &UW_ItemContextMenu::HandleEquipClicked);

            // Find the text label inside the button (layout: Button -> HBox -> [Icon, Text])
            if (UHorizontalBox* HBox = Cast<UHorizontalBox>(EquipAction->GetChildAt(0)))
            {
                for (int32 i = HBox->GetChildrenCount() - 1; i >= 0; --i)
                {
                    if (UTextBlock* TB = Cast<UTextBlock>(HBox->GetChildAt(i)))
                    {
                        EquipActionText = TB;
                        break;
                    }
                }
            }
        }
        if (DropAction.IsValid())
        {
            DropAction->OnClicked.AddDynamic(this, &UW_ItemContextMenu::HandleDropClicked);
        }
        if (SplitAction.IsValid())
        {
            SplitAction->OnClicked.AddDynamic(this, &UW_ItemContextMenu::HandleSplitClicked);
        }
    }

    // Start hidden
    SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogItemContextMenu, Verbose, TEXT("ItemContextMenu constructed"));
}

void UW_ItemContextMenu::ShowForItem(const FInventoryEntryView& EntryView, const TArray<FProjectUIActionDescriptor>& ActionDescriptors, const FVector2D& AbsolutePosition)
{
    CurrentInstanceId = EntryView.InstanceId;

    // Set item name
    if (ItemNameText.IsValid())
    {
        FText NameText = EntryView.DisplayName;
        if (EntryView.Quantity > 1)
        {
            NameText = FText::Format(NSLOCTEXT("ContextMenu", "ItemNameWithQty", "{0} x{1}"),
                EntryView.DisplayName, FText::AsNumber(EntryView.Quantity));
        }
        ItemNameText->SetText(NameText);
    }

    // Update action visibility based on item properties
    UpdateActionVisibility(ActionDescriptors);

    // Position the menu in parent canvas local space
    PositionAtAbsoluteLocation(AbsolutePosition);

    // Show and focus
    SetVisibility(ESlateVisibility::Visible);
    // Avoid forcing OS text-input focus transitions (ctfmon/InputService path).
    // Mouse interaction works without explicit keyboard focus here.

    UE_LOG(LogItemContextMenu, Display, TEXT("Showing context menu for item %d at abs (%.0f, %.0f)"),
        CurrentInstanceId, AbsolutePosition.X, AbsolutePosition.Y);
}

void UW_ItemContextMenu::Hide()
{
    SetVisibility(ESlateVisibility::Collapsed);
    CurrentInstanceId = INDEX_NONE;
    OnMenuClosed.Broadcast();

    UE_LOG(LogItemContextMenu, Verbose, TEXT("Context menu hidden"));
}

bool UW_ItemContextMenu::IsMenuVisible() const
{
    return GetVisibility() == ESlateVisibility::Visible;
}

FReply UW_ItemContextMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // ESC closes menu
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        Hide();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UW_ItemContextMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Click outside the menu closes it
    // Note: This is called for clicks ON the menu, so we handle it in parent
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UW_ItemContextMenu::HandleCloseClicked()
{
    Hide();
}

void UW_ItemContextMenu::HandleUseClicked()
{
    if (CurrentInstanceId != INDEX_NONE)
    {
        OnUseAction.Broadcast(CurrentInstanceId);
    }
    Hide();
}

void UW_ItemContextMenu::HandleEquipClicked()
{
    if (CurrentInstanceId != INDEX_NONE)
    {
        OnEquipAction.Broadcast(CurrentInstanceId);
    }
    Hide();
}

void UW_ItemContextMenu::HandleDropClicked()
{
    if (CurrentInstanceId != INDEX_NONE)
    {
        OnDropAction.Broadcast(CurrentInstanceId);
    }
    Hide();
}

void UW_ItemContextMenu::HandleSplitClicked()
{
    if (CurrentInstanceId != INDEX_NONE)
    {
        OnSplitAction.Broadcast(CurrentInstanceId);
    }
    Hide();
}

void UW_ItemContextMenu::UpdateActionVisibility(const TArray<FProjectUIActionDescriptor>& ActionDescriptors)
{
    auto ApplyActionState = [&ActionDescriptors](UButton* Button, FName ActionId)
    {
        if (!Button)
        {
            return;
        }

        const FProjectUIActionDescriptor* Action = UInventoryViewModel::FindActionDescriptor(ActionDescriptors, ActionId);
        const bool bVisible = Action && Action->bVisible;
        const bool bEnabled = Action && Action->bVisible && Action->bEnabled;
        Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(bEnabled);
    };

    if (DropAction.IsValid())
    {
        ApplyActionState(DropAction.Get(), UInventoryViewModel::GetActionIdDrop());
    }

    if (UseAction.IsValid())
    {
        ApplyActionState(UseAction.Get(), UInventoryViewModel::GetActionIdUse());

        if (UseActionText.IsValid())
        {
            const FProjectUIActionDescriptor* UseDesc = UInventoryViewModel::FindActionDescriptor(
                ActionDescriptors, UInventoryViewModel::GetActionIdUse());
            if (UseDesc)
            {
                UseActionText->SetText(UseDesc->Label);
            }
        }
    }

    if (EquipAction.IsValid())
    {
        ApplyActionState(EquipAction.Get(), UInventoryViewModel::GetActionIdEquip());

        // Update Equip/Unequip label text from descriptor
        if (EquipActionText.IsValid())
        {
            const FProjectUIActionDescriptor* EquipDesc = UInventoryViewModel::FindActionDescriptor(
                ActionDescriptors, UInventoryViewModel::GetActionIdEquip());
            if (EquipDesc)
            {
                EquipActionText->SetText(EquipDesc->Label);
            }
        }
    }

    if (SplitAction.IsValid())
    {
        ApplyActionState(SplitAction.Get(), UInventoryViewModel::GetActionIdSplit());
    }
}

void UW_ItemContextMenu::PositionAtAbsoluteLocation(const FVector2D& AbsolutePosition)
{
    // Convert absolute (screen) coords to parent canvas local coords.
    // CanvasSlot::SetPosition is relative to the parent canvas, NOT the viewport.
    UWidget* ParentWidget = GetParent();
    if (!ParentWidget)
    {
        return;
    }

    const FGeometry ParentGeom = ParentWidget->GetCachedGeometry();
    const FVector2D CanvasSize = ParentGeom.GetLocalSize();
    const FVector2D CursorLocal = ParentGeom.AbsoluteToLocal(AbsolutePosition);

    const FVector2D MenuSize(180.f, 160.f);

    // Offset slightly from cursor
    FVector2D FinalPos = CursorLocal;
    FinalPos.X += 10.f;
    FinalPos.Y += 10.f;

    // Flip to other side if overflowing canvas edge
    if (FinalPos.X + MenuSize.X > CanvasSize.X)
    {
        FinalPos.X = CursorLocal.X - MenuSize.X - 10.f;
    }
    if (FinalPos.Y + MenuSize.Y > CanvasSize.Y)
    {
        FinalPos.Y = CursorLocal.Y - MenuSize.Y - 10.f;
    }

    // Clamp within canvas bounds
    const float MaxX = FMath::Max(0.f, CanvasSize.X - MenuSize.X);
    const float MaxY = FMath::Max(0.f, CanvasSize.Y - MenuSize.Y);
    FinalPos.X = FMath::Clamp(FinalPos.X, 0.f, MaxX);
    FinalPos.Y = FMath::Clamp(FinalPos.Y, 0.f, MaxY);

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        CanvasSlot->SetPosition(FinalPos);
        CanvasSlot->SetAutoSize(true);
    }
    else
    {
        SetRenderTranslation(FinalPos);
    }
}
