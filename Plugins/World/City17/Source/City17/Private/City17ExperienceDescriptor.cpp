#include "City17ExperienceDescriptor.h"

#include "Types/ProjectLoadRequest.h"

UCity17ExperienceDescriptor::UCity17ExperienceDescriptor()
{
	ExperienceName = TEXT("City17");

	// City17 main map (World Partition)
	LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/City17/Maps/City17_Persistent_WP.City17_Persistent_WP")));

	// CriticalAssets auto-discovered from map via InitialExperienceLoader::DiscoverMapDependencies()
	// Uses AssetRegistry to find StaticMesh, SkeletalMesh, Texture, Material dependencies
}

void UCity17ExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);
	OutRequest.ExperienceName = ExperienceName;
}

void UCity17ExperienceDescriptor::GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const
{
	// Scan for City17 maps - this registers them with AssetManager at runtime
	// Required because plugin Config/DefaultGame.ini may not merge reliably
	if (LoadAssets.Map.ToSoftObjectPath().IsNull())
	{
		return;
	}

	const FSoftObjectPath MapPath = LoadAssets.Map.ToSoftObjectPath();
	const FString LongPackageName = MapPath.GetLongPackageName(); // "/City17/Maps/City17_Persistent_WP"

	TArray<FString> Segments;
	LongPackageName.ParseIntoArray(Segments, TEXT("/"), true);

	if (Segments.Num() >= 2)
	{
		FExperienceAssetScanSpec Spec;
		Spec.PrimaryAssetType = TEXT("Map");
		Spec.BaseClass = UWorld::StaticClass();
		Spec.Directories.Add(FString::Printf(TEXT("/%s/%s"), *Segments[0], *Segments[1])); // "/City17/Maps"
		Spec.bHasBlueprintClasses = false;
		Spec.bIsEditorOnly = false;
		Spec.bForceSynchronousScan = true;
		OutSpecs.Add(Spec);
	}
}
