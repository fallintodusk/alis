// Copyright ALIS. All Rights Reserved.

#include "Customization/LootEntryViewCustomization.h"
#include "ProjectPlacementEditor.h"
#include "Data/ObjectDefinition.h"
#include "Types/LootEntryTypes.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "LootEntryViewCustomization"

namespace
{
	constexpr int32 ThumbnailSize = 24;
	const FString ItemTypeTagPrefix = TEXT("ALIS.ItemTag.Item.Type.");
	const FString UncategorizedName = TEXT("Other");
}

TSharedRef<IPropertyTypeCustomization> FLootEntryViewCustomization::MakeInstance()
{
	return MakeShareable(new FLootEntryViewCustomization);
}

void FLootEntryViewCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	// Just show the struct name in header
	HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FLootEntryViewCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Get child property handles
	ObjectIdPropertyHandle = InStructPropertyHandle->GetChildHandle(TEXT("ObjectId"));
	QuantityPropertyHandle = InStructPropertyHandle->GetChildHandle(TEXT("Quantity"));

	if (!ObjectIdPropertyHandle.IsValid() || !QuantityPropertyHandle.IsValid())
	{
		return;
	}

	// Create thumbnail pool
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(64));

	// Build grouped list of valid pickup items
	RefreshPickupItemList();

	// ObjectId - custom combo box with thumbnails and categories
	// Note: SComboBox doesn't expose OnIsSelectableOrNavigable from SListView,
	// so category headers are filtered in OnEntrySelected instead
	TSharedRef<SComboBox<TSharedPtr<FLootPickerEntry>>> ComboBox =
		SNew(SComboBox<TSharedPtr<FLootPickerEntry>>)
		.OptionsSource(&PickerEntries)
		.OnGenerateWidget(this, &FLootEntryViewCustomization::GeneratePickerRow)
		.OnSelectionChanged(this, &FLootEntryViewCustomization::OnEntrySelected)
		.InitiallySelectedItem(FindCurrentEntry())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, 0)
			[
				SNew(STextBlock)
				.Text(this, &FLootEntryViewCustomization::GetCurrentSelectionText)
			]
		];

	ComboBoxWidget = ComboBox;

	StructBuilder.AddCustomRow(LOCTEXT("ObjectId_Label", "Object"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ObjectId_Label", "Object"))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		.MaxDesiredWidth(250.0f)
		[
			ComboBox
		];

	// Quantity - default numeric editor
	StructBuilder.AddProperty(QuantityPropertyHandle.ToSharedRef());
}

FString FLootEntryViewCustomization::GetItemTypeRole(const FAssetData& AssetData) const
{
	// Look for ALIS.ItemTag.Item.Type.* tags
	// For hierarchical tags like Item.Type.Key.Apartment.Luxury, extract only first segment -> "Key"
	FString FoundRole;

	AssetData.EnumerateTags([&](TPair<FName, FAssetTagValueRef> TagPair)
	{
		const FString TagStr = TagPair.Key.ToString();
		if (TagStr.StartsWith(ItemTypeTagPrefix))
		{
			// Extract first segment: "ALIS.ItemTag.Item.Type.Key.Apartment.Luxury" -> "Key"
			FString Rest = TagStr.RightChop(ItemTypeTagPrefix.Len());
			int32 DotIndex = INDEX_NONE;
			if (Rest.FindChar(TEXT('.'), DotIndex))
			{
				FoundRole = Rest.Left(DotIndex);
			}
			else
			{
				FoundRole = Rest;
			}
		}
	});

	return FoundRole.IsEmpty() ? UncategorizedName : FoundRole;
}

void FLootEntryViewCustomization::RefreshPickupItemList()
{
	PickerEntries.Empty();

	// Add "None" option first
	PickerEntries.Add(FLootPickerEntry::MakeNone());

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Query ObjectDefinitions with Pickup capability
	FARFilter Filter;
	Filter.ClassPaths.Add(UObjectDefinition::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/ProjectObject")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> ObjectAssets;
	AssetRegistry.GetAssets(Filter, ObjectAssets);

	// Group by Item.Type role
	TMap<FString, TArray<FAssetData>> GroupedAssets;

	for (const FAssetData& Asset : ObjectAssets)
	{
		// Check for ALIS.Cap.Pickup tag
		FString HasPickup;
		Asset.GetTagValue(TEXT("ALIS.Cap.Pickup"), HasPickup);
		if (HasPickup == TEXT("true"))
		{
			FString Role = GetItemTypeRole(Asset);
			GroupedAssets.FindOrAdd(Role).Add(Asset);
		}
	}

	// Sort categories alphabetically
	TArray<FString> Categories;
	GroupedAssets.GetKeys(Categories);
	Categories.Sort();

	// Build final list with category headers
	for (const FString& Category : Categories)
	{
		// Add category header
		PickerEntries.Add(FLootPickerEntry::MakeCategory(Category));

		// Sort assets within category
		TArray<FAssetData>& Assets = GroupedAssets[Category];
		Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.AssetName.LexicalLess(B.AssetName);
		});

		// Add assets
		for (const FAssetData& Asset : Assets)
		{
			PickerEntries.Add(FLootPickerEntry::MakeAsset(Asset));
		}
	}

	UE_LOG(LogProjectPlacementEditor, Verbose,
		TEXT("LootEntryViewCustomization: Found %d pickup items in %d categories"),
		PickerEntries.Num() - 1 - Categories.Num(), // -1 for None, -Categories for headers
		Categories.Num());
}

TSharedRef<SWidget> FLootEntryViewCustomization::GeneratePickerRow(TSharedPtr<FLootPickerEntry> Entry)
{
	if (!Entry.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Category header - bold, no thumbnail
	if (Entry->bIsCategory)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Entry->CategoryName))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.2f))); // Yellow-ish
	}

	// "None" option
	if (!Entry->AssetData.IsValid())
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("None", "None"));
	}

	// Asset entry with thumbnail
	TSharedPtr<FAssetThumbnail> Thumbnail = MakeShareable(
		new FAssetThumbnail(*Entry->AssetData, ThumbnailSize, ThumbnailSize, ThumbnailPool));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(16, 0, 4, 0) // Indent under category
		[
			SNew(SBox)
			.WidthOverride(ThumbnailSize)
			.HeightOverride(ThumbnailSize)
			[
				Thumbnail->MakeThumbnailWidget()
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromName(Entry->AssetData->AssetName))
		];
}

void FLootEntryViewCustomization::OnEntrySelected(TSharedPtr<FLootPickerEntry> Entry, ESelectInfo::Type SelectInfo)
{
	if (!ObjectIdPropertyHandle.IsValid() || !Entry.IsValid() || Entry->bIsCategory)
	{
		return;
	}

	if (!Entry->AssetData.IsValid())
	{
		// Set to empty/invalid (None)
		ObjectIdPropertyHandle->SetValueFromFormattedString(TEXT(""));
	}
	else
	{
		// Build FPrimaryAssetId string: "ObjectDefinition:AssetName"
		FString AssetIdString = FString::Printf(TEXT("ObjectDefinition:%s"), *Entry->AssetData->AssetName.ToString());
		ObjectIdPropertyHandle->SetValueFromFormattedString(AssetIdString);
	}
}

FText FLootEntryViewCustomization::GetCurrentSelectionText() const
{
	if (!ObjectIdPropertyHandle.IsValid() || !ObjectIdPropertyHandle->IsValidHandle())
	{
		return LOCTEXT("None", "None");
	}

	FString ValueString;
	if (ObjectIdPropertyHandle->GetValueAsFormattedString(ValueString) == FPropertyAccess::Success)
	{
		// Parse "ObjectDefinition:AssetName" format
		FString Type, Name;
		if (ValueString.Split(TEXT(":"), &Type, &Name) && !Name.IsEmpty())
		{
			return FText::FromString(Name);
		}
	}

	return LOCTEXT("None", "None");
}

TSharedPtr<FLootPickerEntry> FLootEntryViewCustomization::FindCurrentEntry() const
{
	if (!ObjectIdPropertyHandle.IsValid() || !ObjectIdPropertyHandle->IsValidHandle())
	{
		// Return None entry
		return PickerEntries.Num() > 0 ? PickerEntries[0] : nullptr;
	}

	FString ValueString;
	if (ObjectIdPropertyHandle->GetValueAsFormattedString(ValueString) == FPropertyAccess::Success)
	{
		// Parse "ObjectDefinition:AssetName" format
		FString Type, Name;
		if (ValueString.Split(TEXT(":"), &Type, &Name) && !Name.IsEmpty())
		{
			FName AssetName(*Name);
			for (const TSharedPtr<FLootPickerEntry>& Entry : PickerEntries)
			{
				if (Entry.IsValid() && !Entry->bIsCategory && Entry->AssetData.IsValid())
				{
					if (Entry->AssetData->AssetName == AssetName)
					{
						return Entry;
					}
				}
			}
		}
	}

	// Return None entry
	return PickerEntries.Num() > 0 ? PickerEntries[0] : nullptr;
}

#undef LOCTEXT_NAMESPACE
