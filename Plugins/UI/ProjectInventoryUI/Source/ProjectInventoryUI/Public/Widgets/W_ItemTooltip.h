// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "W_ItemTooltip.generated.h"

class UTextBlock;
class UProgressBar;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class UWidget;

/**
 * Item Tooltip Widget - displays detailed item information.
 *
 * Shows:
 * - Item name and description
 * - Weight/Volume
 * - Durability bar (if < 100%)
 * - Ammo count (if applicable)
 * - Modifiers/Attachments (if any)
 *
 * Layout structure (ItemTooltipLayout.json):
 * - Border (background)
 *   - VerticalBox
 *     - TextBlock "ItemName"
 *     - TextBlock "ItemDescription"
 *     - HorizontalBox "StatsRow"
 *       - TextBlock "WeightText"
 *       - TextBlock "VolumeText"
 *     - HorizontalBox "DurabilityRow"
 *       - TextBlock "DurabilityLabel"
 *       - ProgressBar "DurabilityBar"
 *     - TextBlock "AmmoText"
 *     - TextBlock "ModifiersText"
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_ItemTooltip : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_ItemTooltip(const FObjectInitializer& ObjectInitializer);

	static FText BuildMetaText(const FInventoryEntryView& EntryView);

	/**
	 * Update tooltip content from an inventory entry view.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void SetItemData(const FInventoryEntryView& EntryView);

	/**
	 * Clear tooltip content.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tooltip")
	void Clear();

protected:
	virtual void NativeConstruct() override;

private:
	// Child widgets (found by name)
	TWeakObjectPtr<UWidget> HeaderIconBox;
	TWeakObjectPtr<UTextBlock> ItemIcon;
	TWeakObjectPtr<UTextBlock> ItemName;
	TWeakObjectPtr<UTextBlock> ItemMetaText;
	TWeakObjectPtr<UTextBlock> ItemDescription;
	TWeakObjectPtr<UTextBlock> WeightText;
	TWeakObjectPtr<UTextBlock> VolumeText;
	TWeakObjectPtr<UTextBlock> DurabilityLabel;
	TWeakObjectPtr<UProgressBar> DurabilityBar;
	TWeakObjectPtr<UTextBlock> AmmoText;
	TWeakObjectPtr<UHorizontalBox> EffectsList;
	TWeakObjectPtr<UTextBlock> EffectsTextTemplate;
	TWeakObjectPtr<UTextBlock> ModifiersText;
	TWeakObjectPtr<UWidget> DurabilityRow;
	TWeakObjectPtr<UWidget> AmmoRow;
	TWeakObjectPtr<UWidget> EffectsRow;
	TWeakObjectPtr<UWidget> ModifiersRow;

	/** Helper to set text visibility based on content. */
	void SetTextWithVisibility(UTextBlock* TextBlock, const FText& Text, UWidget* RowWidget = nullptr);

	/** Format durability as percentage. */
	static float CalculateDurabilityPercent(int32 Durability, int32 MaxDurability);

	/** Get durability bar color based on percentage. */
	static FLinearColor GetDurabilityColor(float Percent);

};
