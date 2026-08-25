// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Types/ProjectLoadRequest.h"

#if WITH_DEV_AUTOMATION_TESTS

PROJECTLOADING_API FLoadRequest BuildResolvedLoadRequest_ForTests(
	const UProjectExperienceDescriptorBase& Descriptor);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_KazanProductRouteProjection,
	"ProjectLoading.DescriptorResolution.Kazan.ProductRouteProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_KazanProductRouteProjection::RunTest(const FString& Parameters)
{
	const UProjectExperienceDescriptorBase* Descriptor =
		UProjectExperienceRegistry::Get()->FindDescriptor(TEXT("KazanTerritory"));
	TestNotNull(TEXT("The game composition root registers the Kazan territory experience."), Descriptor);
	if (Descriptor == nullptr)
	{
		return false;
	}

	const FLoadRequest Request = BuildResolvedLoadRequest_ForTests(*Descriptor);
	const FString MapPath(TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory.L_ProjectWorldKazanTerritory"));
	TestEqual(TEXT("Kazan resolves through the existing descriptor path."), Request.ExperienceName, FName(TEXT("KazanTerritory")));
	TestEqual(TEXT("Kazan resolves the accepted territory map."), Request.MapSoftPath.ToString(), MapPath);

	TArray<FExperienceAssetScanSpec> ScanSpecs;
	Descriptor->GetAssetScanSpecs(ScanSpecs);
	const bool bScansTerritory = ScanSpecs.ContainsByPredicate([](const FExperienceAssetScanSpec& Spec)
	{
		return Spec.PrimaryAssetType == TEXT("Map") &&
			Spec.Directories.Contains(TEXT("/ProjectWorldData/Generated/Territory"));
	});
	TestTrue(TEXT("The experience scans the territory map mount."), bScansTerritory);

	FString MenuJson;
	const FString MenuPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("UI/ProjectMenuMain/Data/MainMenu.json"));
	TestTrue(TEXT("The product menu JSON is readable."), FFileHelper::LoadFileToString(MenuJson, *MenuPath));
	TestTrue(TEXT("The product menu exposes the Kazan action."), MenuJson.Contains(TEXT("\"LoadKazanTerritory\"")));
	FString MenuSource;
	const FString MenuSourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("UI/ProjectMenuMain/Source/ProjectMenuMain/Private/Widgets/W_MainMenu.cpp"));
	TestTrue(TEXT("The product menu source is readable."), FFileHelper::LoadFileToString(MenuSource, *MenuSourcePath));
	TestTrue(TEXT("The Kazan action binds through the existing menu composer."),
		MenuSource.Contains(TEXT("RequestStartGame(TEXT(\"KazanTerritory\"), TEXT(\"SinglePlayer\"))")));

	FString DefaultGame;
	const FString DefaultGamePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
	TestTrue(TEXT("The production packaging config is readable."), FFileHelper::LoadFileToString(DefaultGame, *DefaultGamePath));
	TestTrue(TEXT("The accepted territory map is explicitly cooked."), DefaultGame.Contains(
		TEXT("+MapsToCook=(FilePath=\"/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory\")")));
	return true;
}

#endif
