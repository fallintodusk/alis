// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryPanelGridBuilder.h"
#include "Widgets/W_InventoryPanel.h"
#include "Widgets/ProjectGridCell.h"
#include "MVVM/InventoryViewModel.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "ProjectGameplayTags.h"
#include "Fonts/CompositeFont.h" // for FCompositeFont in EnsureFontsResolved

namespace
{
	// Body-position layout: maps slot tag to (col, row) in equipment grid
	// Layout: 5 rows x 3 columns representing body silhouette
	//   Row0:          [HEAD]
	//   Row1: [L.HAND] [CHEST]  [R.HAND]
	//   Row2:          [BACK]
	//   Row3: [L.LEG]           [R.LEG]
	//   Row4: [L.FOOT]          [R.FOOT]
	FIntPoint GetEquipSlotGridPos(const FGameplayTag& SlotTag)
	{
		static const TMap<FGameplayTag, FIntPoint> SlotPositions = {
			{ProjectTags::Item_EquipmentSlot_Head,      FIntPoint(1, 0)},
			{ProjectTags::Item_EquipmentSlot_MainHand,  FIntPoint(0, 1)},
			{ProjectTags::Item_EquipmentSlot_Chest,     FIntPoint(1, 1)},
			{ProjectTags::Item_EquipmentSlot_OffHand,   FIntPoint(2, 1)},
			{ProjectTags::Item_EquipmentSlot_Back,      FIntPoint(1, 2)},
			{ProjectTags::Item_EquipmentSlot_Legs,      FIntPoint(0, 3)},
			{ProjectTags::Item_EquipmentSlot_Feet,      FIntPoint(0, 4)},
		};
		const FIntPoint* Found = SlotPositions.Find(SlotTag);
		return Found ? *Found : FIntPoint(-1, -1);
	}

	// Slots that are mirrored left/right in the body silhouette
	bool IsSymmetricSlot(const FGameplayTag& SlotTag)
	{
		return SlotTag == ProjectTags::Item_EquipmentSlot_Legs
			|| SlotTag == ProjectTags::Item_EquipmentSlot_Feet;
	}

	// Game Icons (game-icons.net) codepoints for equipment slots.
	// CSS uses \ffXXX (5 hex), actual BMP codepoint is \uFXXX (drop leading f).
	// Font cmap: U+F000-U+FFFE (BMP PUA), 4099 glyphs, no supplementary plane.
	// Lookup: Plugins/UI/ProjectUI/Content/Slate/Fonts/game-icons.css
	// Docs:   Plugins/UI/ProjectUI/Content/Slate/Fonts/README.txt
	// NOTE: "knapsack" (U+F831 GID 2096) has wrong glyph outline - font build error.
	// Using "backpack" (U+F12A GID 299) instead. Verified via cmap inspection 2026-02-10.
	FString GetEquipSlotIcon(const FGameplayTag& SlotTag)
	{
		static const TMap<FGameplayTag, FString> SlotIcons = {
			{ProjectTags::Item_EquipmentSlot_Head,      TEXT("\uF155")},    // barbute
			{ProjectTags::Item_EquipmentSlot_Chest,     TEXT("\uF234")},    // breastplate
			{ProjectTags::Item_EquipmentSlot_Back,      TEXT("\uF12A")},    // backpack (not knapsack - glyph bug)
			{ProjectTags::Item_EquipmentSlot_Legs,      TEXT("\uF0F4")},    // armored-pants
			{ProjectTags::Item_EquipmentSlot_Feet,      TEXT("\uF200")},    // boots
			{ProjectTags::Item_EquipmentSlot_MainHand,  TEXT("\uF23C")},    // broadsword
			{ProjectTags::Item_EquipmentSlot_OffHand,   TEXT("\uFC57")},    // shield
		};
		const FString* Found = SlotIcons.Find(SlotTag);
		return Found ? *Found : TEXT("\uF62F");  // footprint as fallback
	}
}

void FInventoryPanelGridBuilder::Initialize(UW_InventoryPanel* InOwner, UWidgetTree* InWidgetTree)
{
	Owner = InOwner;
	WidgetTree = InWidgetTree;
	EnsureFontsResolved();
}

void FInventoryPanelGridBuilder::EnsureFontsResolved()
{
	if (bFontsResolved)
	{
		return;
	}

	CachedIconFont = UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("GameIcon"), nullptr);
	CachedTextFont = UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), nullptr);

	// Verify the icon font resolved to game-icons.ttf (not LastResort fallback)
	const FCompositeFont* IconComposite = CachedIconFont.GetCompositeFont();
	if (IconComposite && IconComposite->DefaultTypeface.Fonts.Num() > 0)
	{
		const FString& FontFile = IconComposite->DefaultTypeface.Fonts[0].Font.GetFontFilename();
		if (FontFile.Contains(TEXT("game-icons")))
		{
			bFontsResolved = true;
		}
	}
}

void FInventoryPanelGridBuilder::BuildContainerTabs(
	UHorizontalBox* TabsHost,
	const TArray<FText>& Labels,
	TArray<TObjectPtr<UProjectGridCell>>& OutTabCells,
	bool bIsSecondary)
{
	if (!TabsHost || !Owner)
	{
		return;
	}

	TabsHost->ClearChildren();
	OutTabCells.Reset();

	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		UProjectGridCell* TabCell = WidgetTree
			? WidgetTree->ConstructWidget<UProjectGridCell>(UProjectGridCell::StaticClass())
			: NewObject<UProjectGridCell>(Owner);

		// Style tab cell with theme colors and padding
		const FLinearColor TabBgColor = Theme ? Theme->Colors.Surface : FLinearColor(0.12f, 0.12f, 0.12f, 0.9f);
		TabCell->SetBrushColor(TabBgColor);
		TabCell->SetPadding(FMargin(12.f, 6.f));  // Horizontal, Vertical padding

		UTextBlock* Text = WidgetTree
			? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
			: NewObject<UTextBlock>(Owner);

		Text->SetText(Labels[Index]);
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(false);
		Text->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), Theme));
		Text->SetColorAndOpacity(FSlateColor(Theme ? Theme->Colors.TextPrimary : FLinearColor::White));
		TabCell->SetContent(Text);
		TabCell->SetCellIndex(Index);
		TabCell->SetSecondaryGrid(bIsSecondary);

		UHorizontalBoxSlot* TabSlot = TabsHost->AddChildToHorizontalBox(TabCell);
		TabSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));  // Right margin between tabs
		OutTabCells.Add(TabCell);
	}
}

UUniformGridPanel* FInventoryPanelGridBuilder::BuildGrid(
	int32 GridWidth,
	int32 GridHeight,
	TArray<TObjectPtr<UTextBlock>>& OutCellWidgets,
	TArray<TObjectPtr<UProjectGridCell>>& OutCellBorders,
	bool bIsSecondary)
{
	if (!Owner || GridWidth <= 0 || GridHeight <= 0)
	{
		return nullptr;
	}

	UUniformGridPanel* GridPanel = WidgetTree
		? WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass())
		: NewObject<UUniformGridPanel>(Owner);

	// Set slot padding to create visible grid lines between cells
	const float GridLineWidth = 1.f;
	GridPanel->SetSlotPadding(FMargin(GridLineWidth));

	OutCellWidgets.Reset();
	OutCellBorders.Reset();

	const float CellPadding = 4.f;

	for (int32 Row = 0; Row < GridHeight; ++Row)
	{
		for (int32 Col = 0; Col < GridWidth; ++Col)
		{
			const int32 CellIndex = Row * GridWidth + Col;

			USizeBox* SizeBox = WidgetTree
				? WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass())
				: NewObject<USizeBox>(Owner);
			SizeBox->SetWidthOverride(CellSize);
			SizeBox->SetHeightOverride(CellSize);

			UProjectGridCell* CellBorder = WidgetTree
				? WidgetTree->ConstructWidget<UProjectGridCell>(UProjectGridCell::StaticClass())
				: NewObject<UProjectGridCell>(Owner);

			// Use Surface color for cell background, Border color shows through gaps as grid lines
			const FLinearColor CellColor = Theme ? Theme->Colors.Surface : FLinearColor(0.12f, 0.12f, 0.12f, 0.9f);
			CellBorder->SetBrushColor(CellColor);
			CellBorder->SetPadding(FMargin(CellPadding));
			CellBorder->SetCellIndex(CellIndex);
			CellBorder->SetSecondaryGrid(bIsSecondary);
			CellBorder->SetGridMouseDownHandler(UProjectGridCell::FOnGridCellMouseDown::CreateUObject(Owner, &UW_InventoryPanel::HandleCellMouseDown));

			UTextBlock* CellText = WidgetTree
				? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
				: NewObject<UTextBlock>(Owner);
			CellText->SetJustification(ETextJustify::Center);
			CellText->SetText(FText::GetEmpty());

			const FLinearColor TextColor = Theme ? Theme->Colors.TextSecondary : FLinearColor::White;
			CellText->SetColorAndOpacity(FSlateColor(TextColor));

			CellBorder->SetContent(CellText);
			CellBorder->SetHorizontalAlignment(HAlign_Center);
			CellBorder->SetVerticalAlignment(VAlign_Center);
			SizeBox->SetContent(CellBorder);

			UUniformGridSlot* GridSlot = GridPanel->AddChildToUniformGrid(SizeBox, Row, Col);
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);

			OutCellWidgets.Add(CellText);
			OutCellBorders.Add(CellBorder);
		}
	}

	return GridPanel;
}

void FInventoryPanelGridBuilder::BuildEquipSlots(
	UVerticalBox* SlotsHost,
	UInventoryViewModel* ViewModel,
	TArray<TObjectPtr<UProjectGridCell>>& OutSlotCells)
{
	if (!SlotsHost || !ViewModel || !Owner)
	{
		return;
	}

	SlotsHost->ClearChildren();
	OutSlotCells.Reset();

	const int32 SlotCount = ViewModel->GetEquipSlotCount();
	if (SlotCount <= 0)
	{
		return;
	}

	constexpr int32 NumRows = 5;
	constexpr int32 NumCols = 3;
	const float SlotSize = CellSize;
	const float GridLineWidth = 1.f;
	const float CellPadding = 4.f;
	const float RowSpacing = 4.f;
	const float ColSpacing = 4.f;

	// Map grid positions to slot indices (-1 = empty/spacer)
	TArray<TArray<int32>> GridSlotIndices;
	GridSlotIndices.SetNum(NumRows);
	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		GridSlotIndices[Row].Init(-1, NumCols);
	}

	// Fill grid with slot indices based on body positions
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const FGameplayTag SlotTag = ViewModel->GetEquipSlotTag(Index);
		const FIntPoint GridPos = GetEquipSlotGridPos(SlotTag);
		if (GridPos.X >= 0 && GridPos.X < NumCols && GridPos.Y >= 0 && GridPos.Y < NumRows)
		{
			GridSlotIndices[GridPos.Y][GridPos.X] = Index;

			// Mirror symmetric slots (legs, feet) to right column
			if (IsSymmetricSlot(SlotTag))
			{
				const int32 MirrorCol = NumCols - 1 - GridPos.X;
				if (MirrorCol != GridPos.X)
				{
					GridSlotIndices[GridPos.Y][MirrorCol] = Index;
				}
			}
		}
	}

	// Theme colors (same as hand/storage grid cells)
	const FLinearColor BaseColor = Theme ? Theme->Colors.Surface : FLinearColor(0.12f, 0.12f, 0.12f, 0.9f);
	const FLinearColor BorderColor = Theme ? Theme->Colors.Border : FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
	const FLinearColor HighlightColor = Theme ? Theme->Colors.Primary : FLinearColor(0.2f, 0.6f, 1.f, 0.9f);
	const FLinearColor TextColor = Theme ? Theme->Colors.TextSecondary : FLinearColor::White;

	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		UHorizontalBox* RowBox = WidgetTree
			? WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass())
			: NewObject<UHorizontalBox>(Owner);

		for (int32 Col = 0; Col < NumCols; ++Col)
		{
			const int32 SlotIndex = GridSlotIndices[Row][Col];

			USizeBox* CellSizeBox = WidgetTree
				? WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass())
				: NewObject<USizeBox>(Owner);
			CellSizeBox->SetWidthOverride(SlotSize);
			CellSizeBox->SetHeightOverride(SlotSize);

			if (SlotIndex >= 0)
			{
				const FGameplayTag SlotTag = ViewModel->GetEquipSlotTag(SlotIndex);
				const bool bOccupied = ViewModel->GetEquipSlotInstanceId(SlotIndex) != UInventoryViewModel::EmptyCellInstanceId;
				const FLinearColor SlotColor = bOccupied
					? FMath::Lerp(BaseColor, HighlightColor, 0.35f)
					: BaseColor;

				// Outer border (creates visible cell outline like hand grid)
				UBorder* OuterBorder = WidgetTree
					? WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass())
					: NewObject<UBorder>(Owner);
				OuterBorder->SetBrushColor(BorderColor);
				OuterBorder->SetPadding(FMargin(GridLineWidth));

				// Inner cell with slot background
				UProjectGridCell* SlotCell = WidgetTree
					? WidgetTree->ConstructWidget<UProjectGridCell>(UProjectGridCell::StaticClass())
					: NewObject<UProjectGridCell>(Owner);
				SlotCell->SetBrushColor(SlotColor);
				SlotCell->SetPadding(FMargin(CellPadding));
				SlotCell->SetCellIndex(SlotIndex);

				UTextBlock* SlotText = WidgetTree
					? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
					: NewObject<UTextBlock>(Owner);
				SlotText->SetJustification(ETextJustify::Center);
				SlotText->SetAutoWrapText(false);
				SlotText->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("GameIcon"), Theme));
				// Show item icon (blue-tinted) when occupied, silhouette when empty
				const FString ItemIcon = bOccupied ? ViewModel->GetEquipSlotItemIconCode(SlotIndex) : FString();
				if (!ItemIcon.IsEmpty())
				{
					SlotText->SetText(FText::FromString(ItemIcon));
					SlotText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f, 1.0f)));
				}
				else
				{
					SlotText->SetText(FText::FromString(GetEquipSlotIcon(SlotTag)));
					SlotText->SetColorAndOpacity(FSlateColor(TextColor));
				}

				SlotCell->SetContent(SlotText);
				OuterBorder->SetContent(SlotCell);
				CellSizeBox->SetContent(OuterBorder);
				OutSlotCells.Add(SlotCell);
			}

			UHorizontalBoxSlot* ColSlot = RowBox->AddChildToHorizontalBox(CellSizeBox);
			ColSlot->SetHorizontalAlignment(HAlign_Center);
			ColSlot->SetVerticalAlignment(VAlign_Center);
			if (Col < NumCols - 1)
			{
				ColSlot->SetPadding(FMargin(0.f, 0.f, ColSpacing, 0.f));
			}
		}

		UVerticalBoxSlot* RowSlot = SlotsHost->AddChildToVerticalBox(RowBox);
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		if (Row < NumRows - 1)
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, RowSpacing));
		}
	}
}

void FInventoryPanelGridBuilder::UpdateGridTexts(
	const TArray<FText>& CellTexts,
	TArray<TObjectPtr<UTextBlock>>& CellWidgets)
{
	// Lazy-resolve fonts (can be called before NativeConstruct/Initialize)
	EnsureFontsResolved();
	if (!bFontsResolved)
	{
		// FontsDir not ready yet (module not started). Skip - will be called again after Initialize.
		return;
	}

	// Icon font sized to ~80% of cell content area (cell - 2*(padding + gridline))
	const float ContentArea = CellSize - 10.f;

	FSlateFontInfo IconFont = CachedIconFont;
	IconFont.Size = FMath::Clamp(FMath::RoundToInt(ContentArea * 0.8f), 12, 128);

	FSlateFontInfo TextFont = CachedTextFont;
	TextFont.Size = FMath::Clamp(FMath::RoundToInt(ContentArea * 0.55f), 10, 64);

	const int32 Count = FMath::Min(CellWidgets.Num(), CellTexts.Num());

	static const FSlateColor IconColor(FLinearColor(0.4f, 0.7f, 1.0f, 1.0f));
	static const FSlateColor TextColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	static const FSlateColor EmptyColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (CellWidgets[Index])
		{
			const FString Str = CellTexts[Index].ToString();
			const bool bIsIcon = !Str.IsEmpty() && Str[0] >= 0xF000;

			CellWidgets[Index]->SetFont(bIsIcon ? IconFont : TextFont);
			CellWidgets[Index]->SetText(CellTexts[Index]);
			CellWidgets[Index]->SetColorAndOpacity(bIsIcon ? IconColor : (Str.IsEmpty() ? EmptyColor : TextColor));
		}
	}

	for (int32 Index = Count; Index < CellWidgets.Num(); ++Index)
	{
		if (CellWidgets[Index])
		{
			CellWidgets[Index]->SetText(FText::GetEmpty());
			CellWidgets[Index]->SetColorAndOpacity(EmptyColor);
		}
	}
}
