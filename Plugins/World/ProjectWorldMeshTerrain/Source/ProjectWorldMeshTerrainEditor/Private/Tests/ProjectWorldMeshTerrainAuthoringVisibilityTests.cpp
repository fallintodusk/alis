#include "ProjectWorldMeshTerrainPartition.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Modifiers/MeshPartitionMeshProvider.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	UPrimitiveComponent* AddSource(UWorld* World, bool bWithCell = true)
	{
		AActor* Source = World->SpawnActor<AActor>();
		Source->Tags = ProjectWorldMeshTerrainProducer::BuildBaseIdentityTags(
			bWithCell ? TEXT("fixture:x0:y0") : TEXT(""), TEXT("fixture"), TEXT("fixture"), TEXT("fixture"));
		auto* Provider = NewObject<UE::MeshPartition::UMeshProviderModifier>(Source);
		Source->SetRootComponent(Provider);
		Source->AddInstanceComponent(Provider);
		Provider->RegisterComponent();
		Source->ReregisterAllComponents();
		TInlineComponentArray<UPrimitiveComponent*> Components(Source);
		for (UPrimitiveComponent* Component : Components)
			if (Component->GetClass()->GetFName() == TEXT("DynamicMeshComponent")) return Component;
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMeshTerrainAuthoringVisibilityTest,
	"Project.World.Realization.MeshTerrain.AuthoringVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainAuthoringVisibilityTest::RunTest(const FString&)
{
	const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	for (EWorldType::Type Type : {EWorldType::Game, EWorldType::PIE, EWorldType::Editor})
	{
		UWorld* World = UWorld::CreateWorld(Type, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
		if (!TestNotNull(TEXT("Transient lifecycle fixture exists."), World)) return false;
		ON_SCOPE_EXIT { World->DestroyWorld(false); };
		auto* Partition = World->SpawnActor<AProjectWorldMeshTerrainPartition>();
		UPrimitiveComponent* Source = AddSource(World);
		if (!TestNotNull(TEXT("Native provider exposes its authoring mesh."), Source)) return false;
		TestTrue(TEXT("The native source mesh is Editor-only."), Source->IsEditorOnly());
		TestFalse(TEXT("The known-bad native default remains visible in Game."), Source->bHiddenInGame);
		UPrimitiveComponent* EmptyCell = AddSource(World, false);
		AActor* UnrelatedActor = World->SpawnActor<AActor>();
		auto* Unrelated = NewObject<UStaticMeshComponent>(UnrelatedActor);
		Unrelated->bIsEditorOnly = true;
		UnrelatedActor->SetRootComponent(Unrelated);
		UnrelatedActor->AddInstanceComponent(Unrelated);
		Unrelated->RegisterComponent();
		auto* Runtime = NewObject<UStaticMeshComponent>(Source->GetOwner());
		Source->GetOwner()->AddInstanceComponent(Runtime);
		Runtime->RegisterComponent();
		Partition->PostInitializeComponents();
		Partition->DispatchBeginPlay();
		TestEqual(TEXT("Only Game worlds suppress the existing authoring renderer."), Source->bHiddenInGame, World->IsGameWorld());
		TestTrue(TEXT("Editor visibility flag is preserved."), Source->GetVisibleFlag());
		TestFalse(TEXT("An empty cell identity is not an authenticated source."), EmptyCell->bHiddenInGame);
		TestFalse(TEXT("Unrelated Editor-only primitives remain unchanged."), Unrelated->bHiddenInGame);
		TestFalse(TEXT("Runtime primitives on source actors remain unchanged."), Runtime->bHiddenInGame);
		UPrimitiveComponent* LateSource = AddSource(World);
		if (!TestNotNull(TEXT("Native late registration source exists."), LateSource)) return false;
		TestEqual(TEXT("Registration covers streamed/late source actors."), LateSource->bHiddenInGame, World->IsGameWorld());
		Partition->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
		LateSource->SetHiddenInGame(false);
		LateSource->GetOwner()->ReregisterAllComponents();
		TestFalse(TEXT("EndPlay removes the registration handler."), LateSource->bHiddenInGame);
	}
	return true;
}
#endif
