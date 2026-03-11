// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "AssetRegistry/AssetData.h"

class IPropertyHandle;
class FAssetThumbnailPool;

/**
 * Entry in the loot picker - either a category header or an asset item.
 */
struct FLootPickerEntry
{
	/** True if this is a category header (not selectable). */
	bool bIsCategory = false;

	/** Category name (if bIsCategory). */
	FString CategoryName;

	/** Asset data (if !bIsCategory). */
	TSharedPtr<FAssetData> AssetData;

	/** Create a category header entry. */
	static TSharedPtr<FLootPickerEntry> MakeCategory(const FString& Name)
	{
		TSharedPtr<FLootPickerEntry> Entry = MakeShareable(new FLootPickerEntry);
		Entry->bIsCategory = true;
		Entry->CategoryName = Name;
		return Entry;
	}

	/** Create an asset entry. */
	static TSharedPtr<FLootPickerEntry> MakeAsset(const FAssetData& Asset)
	{
		TSharedPtr<FLootPickerEntry> Entry = MakeShareable(new FLootPickerEntry);
		Entry->bIsCategory = false;
		Entry->AssetData = MakeShareable(new FAssetData(Asset));
		return Entry;
	}

	/** Create a None entry. */
	static TSharedPtr<FLootPickerEntry> MakeNone()
	{
		TSharedPtr<FLootPickerEntry> Entry = MakeShareable(new FLootPickerEntry);
		Entry->bIsCategory = false;
		Entry->AssetData = nullptr;
		return Entry;
	}
};

/**
 * Property customization for FLootEntryView struct.
 * Filters ObjectId picker to only show ObjectDefinitions with Pickup capability.
 * Groups items by Item.Type (Consumable, Equipment, etc.).
 * Shows thumbnails in dropdown.
 */
class FLootEntryViewCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	/** Build list of valid ObjectIds grouped by Item.Type. */
	void RefreshPickupItemList();

	/** Extract Item.Type role from asset (e.g., "Consumable", "Equipment"). */
	FString GetItemTypeRole(const FAssetData& AssetData) const;

	/** Generate combo row widget. */
	TSharedRef<SWidget> GeneratePickerRow(TSharedPtr<FLootPickerEntry> Entry);

	/** Called when combo selection changes (ignores category headers). */
	void OnEntrySelected(TSharedPtr<FLootPickerEntry> Entry, ESelectInfo::Type SelectInfo);

	/** Get current selection text. */
	FText GetCurrentSelectionText() const;

	/** Find entry matching current property value. */
	TSharedPtr<FLootPickerEntry> FindCurrentEntry() const;

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> ObjectIdPropertyHandle;
	TSharedPtr<IPropertyHandle> QuantityPropertyHandle;

	/** Cached list of picker entries (categories + assets). */
	TArray<TSharedPtr<FLootPickerEntry>> PickerEntries;

	/** Thumbnail pool for rendering. */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Combo box widget reference. */
	TSharedPtr<SWidget> ComboBoxWidget;
};
