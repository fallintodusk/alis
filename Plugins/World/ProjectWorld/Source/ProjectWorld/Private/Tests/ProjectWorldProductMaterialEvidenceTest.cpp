// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductMaterialEvidence.h"
#include "ProjectWorldTerrainRuntimeRole.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldProductMaterialComponentsTest,
    "Project.World.Presentation.ProductMaterialComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductMaterialComponentsTest::RunTest(const FString&)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("Disposable runtime world exists."), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Parent = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Current generated material exists."), Parent) || !Cube) return false;
    TArray<FMaterialParameterInfo> Parameters; TArray<FGuid> Ids;
    Parent->GetAllTextureParameterInfo(Parameters, Ids);
    if (!TestTrue(TEXT("Current material exposes its texture contract."), Parameters.Num() > 0)) return false;
    auto* Target = NewObject<UTextureRenderTarget2D>();
    Target->InitAutoFormat(4,4); Target->UpdateResourceImmediate(true); FlushRenderingCommands();
    auto* Material = UMaterialInstanceDynamic::Create(Parent, World);
    Material->SetTextureParameterValue(Parameters[0].Name, Target);
    UStaticMeshComponent* Terrain = nullptr;
    for (int32 Index=0; Index<3; ++Index)
    {
        AActor* Actor = Index == 0 ? World->SpawnActor<AActor>() : World->SpawnActor<AStaticMeshActor>();
        auto* Component = Index == 0 ? NewObject<UStaticMeshComponent>(Actor) : CastChecked<AStaticMeshActor>(Actor)->GetStaticMeshComponent();
        Actor->Tags.Add(Index == 0 ? ProjectWorldTerrainRuntimeRole::RoleTag() :
            FName(Index == 1 ? TEXT("ProjectWorld.Road.v1") : TEXT("ProjectWorld.BuildingMassing.v2")));
        if (Index == 0) { Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component); Terrain=Component; }
        Component->SetStaticMesh(Cube); Component->SetMaterial(0,Material); Component->RegisterComponent();
    }
    FJsonObject Receipt; FString Error;
    TestTrue(TEXT("Generic compiled-section owner and static-mesh actors share live outputs."),
        ProjectWorldProductMaterialEvidence::ReadBindings(*World, Receipt, Error));
    Terrain->SetVisibility(false);
    FJsonObject Hidden;
    TestFalse(TEXT("Hidden terrain cannot satisfy visible consumer evidence."),
        ProjectWorldProductMaterialEvidence::ReadBindings(*World, Hidden, Error));
    Terrain->SetVisibility(true);
    Material->SetTextureParameterValue(Parameters[0].Name, NewObject<UTextureRenderTarget2D>());
    FJsonObject Unready;
    TestFalse(TEXT("Uninitialized runtime targets cannot satisfy readiness."),
        ProjectWorldProductMaterialEvidence::ReadBindings(*World, Unready, Error));
    return true;
}
#endif
