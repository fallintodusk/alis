// License terms: see repository root LICENSE.

#include "KazanTerritoryExperienceDescriptor.h"

#include "Engine/World.h"
#include "Types/ProjectLoadRequest.h"

UKazanTerritoryExperienceDescriptor::UKazanTerritoryExperienceDescriptor()
{
	ExperienceName = TEXT("KazanTerritory");
	LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory.L_ProjectWorldKazanTerritory")));
}

void UKazanTerritoryExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);
	OutRequest.CustomOptions.Add(TEXT("Traversal"), TEXT("PreviewFlight"));
}

void UKazanTerritoryExperienceDescriptor::GetAssetScanSpecs(
	TArray<FExperienceAssetScanSpec>& OutSpecs) const
{
	FExperienceAssetScanSpec Spec;
	Spec.PrimaryAssetType = TEXT("Map");
	Spec.BaseClass = UWorld::StaticClass();
	Spec.Directories.Add(TEXT("/ProjectWorldData/Generated/Territory"));
	Spec.bHasBlueprintClasses = false;
	Spec.bIsEditorOnly = false;
	Spec.bForceSynchronousScan = true;
	Spec.bRequireNonEmpty = true;
	OutSpecs.Add(MoveTemp(Spec));
}
