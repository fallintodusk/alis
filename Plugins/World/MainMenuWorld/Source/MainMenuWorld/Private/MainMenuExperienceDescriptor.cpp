#include "MainMenuExperienceDescriptor.h"

#include "Types/ProjectLoadRequest.h"

UMainMenuExperienceDescriptor::UMainMenuExperienceDescriptor()
{
	// Keep experience name aligned with the plugin name to avoid confusion in config/entry selection.
	ExperienceName = TEXT("MainMenuWorld");

	// Map + supporting assets (soft refs)
	LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")));

	LoadAssets.WarmupAssets.Add(FSoftObjectPath(TEXT("/MainMenuWorld/Maps/MainMenu_Streaming.MainMenu_Streaming")));

	// Optional cinematic or background content can be added here
	// NOTE: Menu UI lives in ProjectMenuMain; add those soft refs once paths are finalized there.
}

void UMainMenuExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);

	OutRequest.ExperienceName = ExperienceName;
}

void UMainMenuExperienceDescriptor::GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const
{
	if (LoadAssets.Map.ToSoftObjectPath().IsNull())
	{
		return;
	}

	const FSoftObjectPath MapPath = LoadAssets.Map.ToSoftObjectPath();
	const FString LongPackageName = MapPath.GetLongPackageName(); // "/MainMenuWorld/Maps/MainMenu_Persistent"

	TArray<FString> Segments;
	LongPackageName.ParseIntoArray(Segments, TEXT("/"), /*CullEmpty*/ true);

	if (Segments.Num() >= 2)
	{
		FExperienceAssetScanSpec Spec;
		Spec.PrimaryAssetType = TEXT("Map");
		Spec.BaseClass = UWorld::StaticClass();
		Spec.Directories.Add(FString::Printf(TEXT("/%s/%s"), *Segments[0], *Segments[1])); // "/MainMenuWorld/Maps"
		Spec.bHasBlueprintClasses = false;
		Spec.bIsEditorOnly = false;
		Spec.bForceSynchronousScan = true;
		OutSpecs.Add(Spec);
	}
}
