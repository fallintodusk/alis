// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UW_InventoryPanel;
class UInventoryViewModel;
class UProjectGridCell;
class UTextBlock;
class UUniformGridPanel;
class UHorizontalBox;
class UVerticalBox;
class UWidgetTree;
class UProjectUIThemeData;

/**
 * Handles all grid and UI element construction for the inventory panel.
 * Single responsibility: build and populate widget hierarchies.
 */
class PROJECTINVENTORYUI_API FInventoryPanelGridBuilder
{
public:
	FInventoryPanelGridBuilder() = default;

	/** Initialize with required context */
	void Initialize(UW_InventoryPanel* InOwner, UWidgetTree* InWidgetTree);

	/** Update theme reference for styling */
	void SetTheme(UProjectUIThemeData* InTheme) { Theme = InTheme; }

	/** Set cell size for grid layout */
	void SetCellSize(float InCellSize) { CellSize = InCellSize; }

	/**
	 * Build container tabs for primary or secondary grid
	 */
	void BuildContainerTabs(
		UHorizontalBox* TabsHost,
		const TArray<FText>& Labels,
		TArray<TObjectPtr<UProjectGridCell>>& OutTabCells,
		bool bIsSecondary);

	/**
	 * Build a grid panel with cells
	 * @return The created UniformGridPanel (caller takes ownership)
	 */
	UUniformGridPanel* BuildGrid(
		int32 GridWidth,
		int32 GridHeight,
		TArray<TObjectPtr<UTextBlock>>& OutCellWidgets,
		TArray<TObjectPtr<UProjectGridCell>>& OutCellBorders,
		bool bIsSecondary);

	/**
	 * Build equipment slot cells in body-position layout using nested boxes
	 */
	void BuildEquipSlots(
		UVerticalBox* SlotsHost,
		UInventoryViewModel* ViewModel,
		TArray<TObjectPtr<UProjectGridCell>>& OutSlotCells);

	/**
	 * Update text in grid cells (uses cached fonts from Initialize)
	 */
	void UpdateGridTexts(
		const TArray<FText>& CellTexts,
		TArray<TObjectPtr<UTextBlock>>& CellWidgets);

private:
	UW_InventoryPanel* Owner = nullptr;
	UWidgetTree* WidgetTree = nullptr;
	UProjectUIThemeData* Theme = nullptr;
	float CellSize = 64.f;
	void EnsureFontsResolved();

	FSlateFontInfo CachedIconFont;
	FSlateFontInfo CachedTextFont;
	bool bFontsResolved = false;
};
