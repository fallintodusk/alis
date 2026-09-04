// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDefinitionDescriptor.h"

#include "Engine/World.h"
#include "Experience/ProjectExperienceDefinition.h"
#include "Types/ProjectLoadRequest.h"

namespace
{
	/** Load-request option key owned by ProjectSinglePlay's traversal policy. */
	const TCHAR* const TraversalOptionKey = TEXT("Traversal");
}

bool UProjectExperienceDefinitionDescriptor::InitializeFromDefinition(
	const UProjectExperienceDefinition& Definition)
{
	if (!Definition.IsValidDefinition())
	{
		return false;
	}

	ExperienceName = Definition.ExperienceName;
	LoadAssets.Map = Definition.Map;
	TraversalMode = Definition.TraversalMode;
	AssetScanDirectory = Definition.AssetScanDirectory;

	LoadAssets.CriticalAssets.Reset();
	for (const TSoftObjectPtr<UObject>& Asset : Definition.CriticalAssets)
	{
		if (!Asset.IsNull())
		{
			LoadAssets.CriticalAssets.Add(Asset.ToSoftObjectPath());
		}
	}

	LoadAssets.WarmupAssets.Reset();
	for (const TSoftObjectPtr<UObject>& Asset : Definition.WarmupAssets)
	{
		if (!Asset.IsNull())
		{
			LoadAssets.WarmupAssets.Add(Asset.ToSoftObjectPath());
		}
	}

	return true;
}

void UProjectExperienceDefinitionDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);

	if (!TraversalMode.IsEmpty())
	{
		OutRequest.CustomOptions.Add(TraversalOptionKey, TraversalMode);
	}
}

void UProjectExperienceDefinitionDescriptor::GetAssetScanSpecs(
	TArray<FExperienceAssetScanSpec>& OutSpecs) const
{
	if (AssetScanDirectory.IsEmpty())
	{
		return;
	}

	FExperienceAssetScanSpec Spec;
	Spec.PrimaryAssetType = TEXT("Map");
	Spec.BaseClass = UWorld::StaticClass();
	Spec.Directories.Add(AssetScanDirectory);
	Spec.bHasBlueprintClasses = false;
	Spec.bIsEditorOnly = false;
	Spec.bForceSynchronousScan = true;
	Spec.bRequireNonEmpty = true;
	OutSpecs.Add(MoveTemp(Spec));
}
