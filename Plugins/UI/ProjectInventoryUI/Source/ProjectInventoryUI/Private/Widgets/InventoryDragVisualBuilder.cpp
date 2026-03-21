// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryDragVisualBuilder.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"

namespace
{
FText BuildDisplayNameText(const FInventoryEntryView& Entry)
{
    FText NameText = Entry.DisplayName.IsEmpty()
        ? FText::FromString(Entry.ItemId.ToString())
        : Entry.DisplayName;
    if (Entry.Quantity > 1)
    {
        NameText = FText::Format(
            NSLOCTEXT("Inventory", "DragVisualItemNameWithQty", "{0} x{1}"),
            NameText,
            FText::AsNumber(Entry.Quantity));
    }

    return NameText;
}
}

UWidget* FInventoryDragVisualBuilder::Build(
    UObject* Outer,
    const FInventoryEntryView& Entry,
    int32 DragQuantity,
    UProjectUIThemeData* Theme)
{
    if (!Outer)
    {
        return nullptr;
    }

    FInventoryEntryView DragEntry = Entry;
    DragEntry.Quantity = DragQuantity;

    UBorder* DragCard = NewObject<UBorder>(Outer, TEXT("DragCard"));
    if (!DragCard)
    {
        return nullptr;
    }

    DragCard->SetPadding(FMargin(8.f, 6.f));
    DragCard->SetBrushColor(Theme ? Theme->Colors.Surface : FLinearColor(0.12f, 0.12f, 0.12f, 0.95f));
    DragCard->SetContentColorAndOpacity(FLinearColor::White);
    DragCard->SetRenderOpacity(0.96f);
    DragCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    DragCard->SetRenderScale(FVector2D(0.96f, 0.96f));
    DragCard->SetVisibility(ESlateVisibility::HitTestInvisible);

    UHorizontalBox* ContentRow = NewObject<UHorizontalBox>(DragCard, TEXT("DragContentRow"));
    if (!ContentRow)
    {
        return DragCard;
    }

    USizeBox* IconBox = NewObject<USizeBox>(ContentRow, TEXT("DragIconBox"));
    if (IconBox)
    {
        IconBox->SetWidthOverride(42.f);
        IconBox->SetHeightOverride(42.f);

        UTextBlock* Icon = NewObject<UTextBlock>(IconBox, TEXT("DragIconText"));
        if (Icon)
        {
            Icon->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("GameIcon"), Theme));
            Icon->SetColorAndOpacity(FSlateColor(Theme ? Theme->Colors.TextPrimary : FLinearColor::White));
            Icon->SetJustification(ETextJustify::Center);
            Icon->SetText(FText::FromString(Entry.IconCode));
            Icon->SetVisibility(Entry.IconCode.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
            IconBox->SetContent(Icon);
        }

        if (UHorizontalBoxSlot* IconSlot = ContentRow->AddChildToHorizontalBox(IconBox))
        {
            IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
            IconSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    UTextBlock* Label = NewObject<UTextBlock>(ContentRow, TEXT("DragLabelText"));
    if (!Label)
    {
        DragCard->SetContent(ContentRow);
        return DragCard;
    }

    Label->SetText(BuildDisplayNameText(DragEntry));
    Label->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), Theme));
    Label->SetColorAndOpacity(FSlateColor(Theme ? Theme->Colors.TextPrimary : FLinearColor::White));
    Label->SetVisibility(Entry.IconCode.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
    if (UHorizontalBoxSlot* LabelSlot = ContentRow->AddChildToHorizontalBox(Label))
    {
        LabelSlot->SetVerticalAlignment(VAlign_Center);
    }

    DragCard->SetContent(ContentRow);
    return DragCard;
}
