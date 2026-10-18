// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Verify/ProjectWorldBuildingSurfaceReference.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingSourceComponentPhysicsReferenceTest,
	"Project.World.SavedProjection.BuildingComponentPhysicsReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldBuildingSourceComponentPhysicsReferenceTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("An isolated physics world exists."), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The native static-mesh control exists."), Cube)) return false;
	TArray<UStaticMeshComponent*> Components;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FVector(Index * 400.0, 0, 0), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("A transient source cell actor exists."), Actor)) return false;
		Actor->Tags.Add(TEXT("ProjectWorld.BuildingMassing.v2"));
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Cube);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Components.Add(Component);
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldBuildingSourceComponentControl), true);
	TestTrue(TEXT("The actual component geometry ray hits the cube."), Components[0]->LineTraceComponent(
		Hit, FVector(0, 0, 150), FVector(0, 0, -150), Query));
	TestFalse(TEXT("Native component geometry tracing uses an overlap filter, independently of Pawn blocking."), Hit.bBlockingHit);
	TestTrue(TEXT("The physical intersection belongs to the queried component."), Hit.GetComponent() == Components[0]);
	TestEqual(TEXT("The component separately blocks Pawn."), Components[0]->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("tmp/world/building_source_reference_tests"), FGuid::NewGuid().ToString() + TEXT(".json")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Path); };
	FString Error;
	const bool bWritten = ProjectWorldBuildingSurfaceReference::Write(*World,
		TEXT("/ProjectWorldTestData/Generated/Fixtures/L_ComponentPhysics"), FString::ChrN(64, TEXT('a')), Path, Error);
	TestTrue(TEXT("The source exporter accepts actual geometry hits with Pawn blocking configured."), bWritten);
	if (!bWritten) { AddError(Error); return false; }
	FString Json;
	TSharedPtr<FJsonObject> Reference;
	TestTrue(TEXT("The source reference is readable."), FFileHelper::LoadFileToString(Json, *Path) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Reference) && Reference.IsValid());
	if (Reference) TestEqual(TEXT("The exported fixture has six measured wall/roof rays."),
		Reference->GetArrayField(TEXT("samples")).Num(), 6);
	Components[0]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	TestFalse(TEXT("Geometry visibility cannot replace the required Pawn-block configuration."),
		ProjectWorldBuildingSurfaceReference::Write(*World,
			TEXT("/ProjectWorldTestData/Generated/Fixtures/L_ComponentPhysics"), FString::ChrN(64, TEXT('a')), Path, Error));
	Components[0]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Components[0]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Disabled collision refuses instead of authenticating the prior reference."),
		ProjectWorldBuildingSurfaceReference::Write(*World,
			TEXT("/ProjectWorldTestData/Generated/Fixtures/L_ComponentPhysics"), FString::ChrN(64, TEXT('a')), Path, Error));
	return true;
}

#endif
