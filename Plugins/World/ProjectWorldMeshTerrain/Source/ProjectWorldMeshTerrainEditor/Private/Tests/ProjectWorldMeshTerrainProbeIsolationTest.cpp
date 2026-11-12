#include "ProjectWorldMeshTerrainCollisionFixture.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPhysicsProbeIsolationTest,
    "Project.World.Realization.MeshTerrain.PhysicsProbePackageIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldPhysicsProbeIsolationTest::RunTest(const FString&)
{
    const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Mount=TEXT("/ProjectWorldPhysicsProbe")+Id+TEXT("/");
    const FString Directory=FPaths::ProjectDir()/TEXT("tmp/world/physics_probe")/Id/TEXT("Content/");
    IFileManager::Get().MakeDirectory(*Directory,true);
    FPackageName::RegisterMountPoint(Mount,Directory);
    auto* Package=CreatePackage(*(Mount+TEXT("ProbeMap")));
    const auto Init=UWorld::InitializationValues().CreateWorldPartition(true);
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("ProbeMap"),Package,true,ERHIFeatureLevel::Num,&Init);
    ON_SCOPE_EXIT
    {
        if (World) {World->DestroyWorld(false);World->RemoveFromRoot();}
        FPackageName::UnRegisterMountPoint(Mount,Directory);
    };
    if (!TestNotNull(TEXT("The probe regression uses a native external-actor world."),World)) return false;
    World->PersistentLevel->SetUseExternalActors(true);
    if (!TestTrue(TEXT("The native level would externalize an ordinary calibration actor."),
        World->GetWorldPartition()!=nullptr && World->PersistentLevel->ShouldCreateNewExternalActors())) return false;
    FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;Spawn.bCreateActorPackage=false;
    auto* Owner=World->SpawnActor<AActor>(Spawn);
    if (!TestNotNull(TEXT("The declared control collider has its own transient owner."),Owner)) return false;
    auto* Collider=NewObject<UBoxComponent>(Owner);Owner->SetRootComponent(Collider);
    Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision);Collider->RegisterComponent();
    ProjectWorldTerrainCollisionFixture::FInventory Inventory;Inventory.World=World;Inventory.Components={Collider};
    TSet<FString> Before;
    for (auto* External:World->GetPackage()->GetExternalPackages()) Before.Add(External->GetName());
    if (!Inventory.Calibrate(*this,Collider,false)) return false;
    TSet<FString> After;
    for (auto* External:World->GetPackage()->GetExternalPackages()) After.Add(External->GetName());
    TestEqual(TEXT("A destroyed foreign-owner calibration must not leave an unsaved external actor package."),After.Num(),Before.Num());
    for (const auto& Name:After) TestTrue(TEXT("Calibration preserves the exact original package census."),Before.Contains(Name));
    return !HasAnyErrors();
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldPhysicsProbeIsolationTest,
    "Project.World.Realization.MeshTerrain.PhysicsProbePackageIsolation","[Fast][Unit][World]")
#endif
