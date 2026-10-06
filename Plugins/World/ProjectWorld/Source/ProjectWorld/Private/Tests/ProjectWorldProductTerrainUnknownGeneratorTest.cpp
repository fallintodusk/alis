// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductTerrainAcceptance.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldProductTerrainUnknownGeneratorTest,
	"Project.World.Realization.Neutrality.UnknownTerrainGeneratorRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldProductTerrainUnknownGeneratorTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/realization_verify/neutrality"));
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString ContractPath = FPaths::Combine(Root, TEXT("unknown_generator.json"));
	const FString RuntimeHash = FString::ChrN(64, TEXT('a'));
	const FString Payload = TEXT(R"JSON({
  "schema": "project-world-product-terrain-acceptance:v1",
  "map_package": "/ProjectWorldVerify/TerrainUnknown",
  "runtime_profile_sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "terrain_generator_id": "unknown_terrain:v1",
  "navigation": {
    "start_unreal_cm": [0, 0, 0],
    "end_unreal_cm": [100, 0, 0],
    "start_cell_id": "cell_a",
    "end_cell_id": "cell_b",
    "crossed_boundary": "a_to_b",
    "generation_radius_cm": 30000,
    "removal_radius_cm": 40000
  },
  "height_probes": [
    {"id":"center","kind":"center","stage":"center","cell_id":"cell_a","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1},
    {"id":"perimeter","kind":"perimeter","stage":"edge","cell_id":"cell_a","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1},
    {"id":"boundary","kind":"cell_boundary","stage":"edge","cell_id":"cell_b","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1},
    {"id":"high","kind":"high","stage":"center","cell_id":"cell_a","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1},
    {"id":"low","kind":"low","stage":"center","cell_id":"cell_a","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1},
    {"id":"hydro","kind":"hydro_transition","stage":"center","cell_id":"cell_a","canonical_m":[0,0,0],"unreal_cm":[0,0,0],"tolerance_cm":1}
  ]
})JSON");
	if (!TestTrue(TEXT("The contract fixture is written."), FFileHelper::SaveStringToFile(Payload, *ContractPath)))
	{
		return false;
	}
	FString ContractHash;
	if (!TestTrue(TEXT("The contract fixture is hashed."), FProjectSha256::HashFile(ContractPath, ContractHash)))
	{
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("The transient world exists."), World))
	{
		return false;
	}
	ACharacter* Character = World->SpawnActor<ACharacter>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("The character exists."), Character) ||
		!TestNotNull(TEXT("The controller exists."), Controller))
	{
		World->DestroyWorld(false);
		return false;
	}
	FString Error;
	{
		FProjectWorldProductTerrainAcceptance Acceptance;
		const bool bAccepted = Acceptance.Initialize(
			*World, *Character, *Controller, ContractPath, ContractHash,
			TEXT("/ProjectWorldVerify/TerrainUnknown"), RuntimeHash, false, Error);
		TestFalse(TEXT("An undeclared terrain generator is rejected."), bAccepted);
	}
	TestTrue(TEXT("The refusal names the undeclared generator."), Error.Contains(TEXT("unknown_terrain:v1")));
	World->DestroyWorld(false);
	return true;
}

#endif
