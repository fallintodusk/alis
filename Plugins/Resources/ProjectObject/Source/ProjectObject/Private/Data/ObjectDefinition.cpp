// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Data/ObjectDefinition.h"
#include "ProjectObjectModule.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "Engine/AssetManager.h"

void FStorageSection::BuildSeedLootEntries(TArray<FLootEntryView>& OutEntries) const
{
	OutEntries.Reserve(OutEntries.Num() + SeedEntries.Num());
	for (const FStorageSeedEntry& SeedEntry : SeedEntries)
	{
		if (SeedEntry.IsValid())
		{
			OutEntries.Add(SeedEntry.ToLootEntryView());
		}
	}
}

FItemDataView UObjectDefinition::GetItemDataView_Implementation() const
{
	FItemDataView View;

	if (const FItemSection* ItemData = GetItemSection())
	{
		View.bIsValid = true;
		View.DisplayName = ItemData->DisplayName;
		View.Description = ItemData->Description;
		View.IconCode = ItemData->IconCode;
		View.Tags = ItemData->Tags;
		View.Weight = ItemData->Weight;
		View.Volume = ItemData->Volume;
		View.GridSize = ItemData->GridSize;
		View.MaxStack = ItemData->MaxStack;
		View.UnitsPerDepthUnit = ItemData->UnitsPerDepthUnit;
		View.bCanBeDropped = ItemData->bCanBeDropped;
		View.bConsumeOnUse = ItemData->bConsumeOnUse;
		View.Magnitudes = ItemData->Magnitudes;
		View.EquipSlotTag = ItemData->EquipSlotTag;
		View.EquipAbilitySetPath = ItemData->EquipAbilitySet;
		View.ContainerGrants = ItemData->ContainerGrants;
		View.bIsConsumable = ItemData->IsConsumable();
		View.bIsEquipment = ItemData->IsEquipment();
	}

	return View;
}

FPrimaryAssetId UObjectDefinition::GetPrimaryAssetId() const
{
	if (ObjectId.IsNone())
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), ObjectId);
}

#if WITH_EDITORONLY_DATA
void UObjectDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	TArray<FTopLevelAssetPath> ReferencedAssets;

	// Spawn class
	if (!SpawnClass.IsNull())
	{
		ReferencedAssets.Add(SpawnClass.ToSoftObjectPath().GetAssetPath());
	}

	// Meshes: assets, materials, anim classes, bindings
	for (const FObjectMeshEntry& Mesh : Meshes)
	{
		if (!Mesh.Asset.IsNull())
		{
			ReferencedAssets.Add(Mesh.Asset.ToSoftObjectPath().GetAssetPath());
		}

		for (const TSoftObjectPtr<UMaterialInterface>& Mat : Mesh.Materials)
		{
			if (!Mat.IsNull())
			{
				ReferencedAssets.Add(Mat.ToSoftObjectPath().GetAssetPath());
			}
		}

		if (!Mesh.AnimClass.IsNull())
		{
			ReferencedAssets.Add(Mesh.AnimClass.ToSoftObjectPath().GetAssetPath());
		}

		if (!Mesh.BindingAsset.IsNull())
		{
			ReferencedAssets.Add(Mesh.BindingAsset.ToSoftObjectPath().GetAssetPath());
		}
	}

	// Item section: abilities, effects, ability set
	if (const FItemSection* ItemData = GetItemSection())
	{
		for (const FSoftClassPath& Ability : ItemData->GrantedAbilities)
		{
			if (Ability.IsValid())
			{
				ReferencedAssets.Add(Ability.GetAssetPath());
			}
		}

		for (const FSoftObjectPath& Effect : ItemData->GrantedEffects)
		{
			if (Effect.IsValid())
			{
				ReferencedAssets.Add(Effect.GetAssetPath());
			}
		}

		if (ItemData->EquipAbilitySet.IsValid())
		{
			ReferencedAssets.Add(ItemData->EquipAbilitySet.GetAssetPath());
		}
	}

	// Customization section: mutable source
	if (const FCustomizationSection* CustomData = GetSection<FCustomizationSection>(ObjectSectionIds::Customization))
	{
		if (!CustomData->MutableSource.IsNull())
		{
			ReferencedAssets.Add(CustomData->MutableSource.ToSoftObjectPath().GetAssetPath());
		}
	}

	if (ReferencedAssets.Num() > 0)
	{
		AssetBundleData.SetBundleAssets(FName(TEXT("Default")), MoveTemp(ReferencedAssets));
	}
}
#endif

void UObjectDefinition::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// -------------------------------------------------------------------------
	// Layer 1: Capability tags (world interactions)
	// -------------------------------------------------------------------------
	for (const FObjectCapabilityEntry& Cap : Capabilities)
	{
		const FString TagName = FString::Printf(TEXT("ALIS.Cap.%s"), *Cap.Type.ToString());
		Context.AddTag(FAssetRegistryTag(*TagName, TEXT("true"), FAssetRegistryTag::TT_Alphabetical));
	}

	// -------------------------------------------------------------------------
	// Sections: Export tags per section type
	// -------------------------------------------------------------------------

	// Export which sections exist
	for (const auto& SectionPair : Sections)
	{
		const FString TagName = FString::Printf(TEXT("ALIS.Section.%s"), *SectionPair.Key.ToString());
		Context.AddTag(FAssetRegistryTag(*TagName, TEXT("true"), FAssetRegistryTag::TT_Alphabetical));
	}

	// Item section specific tags
	if (const FItemSection* ItemData = GetItemSection())
	{
		// Display name for tooltips (without loading asset)
		if (!ItemData->DisplayName.IsEmpty())
		{
			Context.AddTag(FAssetRegistryTag(TEXT("DisplayName"), ItemData->DisplayName.ToString(), FAssetRegistryTag::TT_Alphabetical));
		}

		// Weight for tooltips
		if (ItemData->Weight > 0.0f)
		{
			Context.AddTag(FAssetRegistryTag(TEXT("Weight"), FString::SanitizeFloat(ItemData->Weight), FAssetRegistryTag::TT_Numerical));
		}

		// Export item tags for filtering (uses existing GameplayTag patterns)
		// Item.Type.Consumable, Item.Type.Equipment, Item.Type.Quest, etc.
		for (const FGameplayTag& Tag : ItemData->Tags)
		{
			// Export as ALIS.ItemTag.<full.tag.path> = "true"
			const FString TagName = FString::Printf(TEXT("ALIS.ItemTag.%s"), *Tag.ToString());
			Context.AddTag(FAssetRegistryTag(*TagName, TEXT("true"), FAssetRegistryTag::TT_Alphabetical));
		}
	}
}
