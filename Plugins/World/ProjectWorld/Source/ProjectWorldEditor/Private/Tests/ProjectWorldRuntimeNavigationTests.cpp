// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeNavigation.h"

#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Tests/ProjectWorldBasinNavigation.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Character.h"
#include "ProjectWorldVerify.h"
#include "ProjectWorldRuntimePartitionPolicy.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldTerritoryLoadDefaultsTest,
    "Project.World.Realization.Runtime.TerritoryLoadDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerritoryLoadDefaultsTest::RunTest(const FString& Parameters)
{
    auto* Defaults = GetMutableDefault<ARecastNavMesh>();
    TGuardValue<float> CapturedDefault(Defaults->TileSizeUU, 1000.f);
    FProjectWorldVerifyWorld Fixture(TEXT("TerritoryLoadDefaults"));
    FString Error;
    if (!Fixture.IsReady(Error)) { AddError(Error); return false; }
    {
        TGuardValue<float> SavedDefault(Defaults->TileSizeUU, 4096.f);
        FProjectWorldRuntimePartitionSettings Settings;
        Settings.PartitionCount = 1; Settings.CellSizeCentimeters = 12800;
        Settings.LoadingRangeCentimeters = 25600; Settings.bIs2D = true;
        bool Changed = false;
        UWorld* World = Fixture.GetWorld();
        if (!ProjectWorldRuntimePartitionPolicy::Apply(World, Settings, Changed, Error)) { AddError(Error); return false; }
        auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
        Bounds->SetIsSpatiallyLoaded(false);
        ARecastNavMesh* Data = nullptr;
        if (!ProjectWorldRuntimeNavigation::ConfigureTerritoryDomain(World, Bounds,
            FBox(FVector(-10000, -10000, -2000), FVector(10000, 10000, 2000)), Data, Error) ||
            !Fixture.SaveOwned(Error)) { AddError(Error); return false; }
    }
    UWorld* Loaded = ProjectWorldRuntimeNavigation::WithTerritoryLoadDefaults([&]() -> UWorld*
    {
        TestEqual(TEXT("The accepted minimum exists before the actual load."), Defaults->TileSizeUU, 4096.f);
        if (!Fixture.Reload(Error)) { AddError(Error); return nullptr; }
        UWorld* World = Fixture.GetWorld();
        int32 Count = 0;
        for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
        {
            ++Count;
            TestTrue(TEXT("The loaded actor inherits accepted tiles before reconciliation."), It->TileSizeUU >= 4096.f);
        }
        TestEqual(TEXT("The loaded map has one native default."), Count, 1);
        return World;
    }, Error);
    TestNotNull(TEXT("The bounded retained map loads under the policy."), Loaded);
    TestEqual(TEXT("Loading restores the process class default."), Defaults->TileSizeUU, 1000.f);
    TestNull(TEXT("A failed load returns failure."), ProjectWorldRuntimeNavigation::WithTerritoryLoadDefaults(
        [&]() -> UWorld* { TestEqual(TEXT("Failure runs within the same scope."), Defaults->TileSizeUU, 4096.f); return nullptr; }, Error));
    TestEqual(TEXT("A failed load restores class defaults too."), Defaults->TileSizeUU, 1000.f);
    {
        TGuardValue<float> Larger(Defaults->TileSizeUU, 5000.f);
        ProjectWorldRuntimeNavigation::WithTerritoryLoadDefaults([&]() -> UWorld*
            { TestEqual(TEXT("A legal larger native default is retained."), Defaults->TileSizeUU, 5000.f); return Loaded; }, Error);
        TestEqual(TEXT("Larger defaults survive scope exit."), Defaults->TileSizeUU, 5000.f);
    }
    bool Called = false;
    {
        TGuardValue<float> Invalid(Defaults->TileSizeUU, std::numeric_limits<float>::quiet_NaN());
        TestNull(TEXT("Unknown defaults refuse before loading."), ProjectWorldRuntimeNavigation::WithTerritoryLoadDefaults(
            [&]() -> UWorld* { Called = true; return nullptr; }, Error));
        TestTrue(TEXT("Refusal does not replace the invalid input."), FMath::IsNaN(Defaults->TileSizeUU));
    }
    TestFalse(TEXT("An invalid default never reaches the loader."), Called);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectWorldExternalNavigationReconciliationTest,
    "Project.World.Realization.Runtime.ExternalNavigationReconciliation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldExternalNavigationReconciliationTest::RunTest(const FString& Parameters)
{
    FProjectWorldVerifyWorld Fixture(TEXT("ExternalNavigationReconciliation"));
    FString Error;
    if (!Fixture.IsReady(Error)) { AddError(Error); return false; }
    UWorld* World = Fixture.GetWorld();
    FProjectWorldRuntimePartitionSettings Settings;
    Settings.PartitionCount = 1; Settings.CellSizeCentimeters = 12800;
    Settings.LoadingRangeCentimeters = 25600; Settings.bIs2D = true;
    bool Changed = false;
    if (!ProjectWorldRuntimePartitionPolicy::Apply(World, Settings, Changed, Error)) { AddError(Error); return false; }
    auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    ARecastNavMesh* Recast = nullptr;
    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
    if (!TestNotNull(TEXT("The retained navigation case has a real bounds volume."), Bounds)) return false;
    Bounds->SetIsSpatiallyLoaded(false);
    if (!ProjectWorldRuntimeNavigation::ConfigureTerritoryDomain(World, Bounds,
        FBox(FVector(-5000, -5000, -2000), FVector(5000, 5000, 2000)), Recast, Error)) { AddError(Error); return false; }
    const FNavDataConfig Agent = Recast->GetConfig();
    Recast->SetPackageExternal(true);
    Recast->TileSizeUU = 1000.f;
    Recast->MarkPackageDirty();
    if (!TestTrue(TEXT("The retained input is one externally packaged stock default."),
        Recast->IsPackageExternal() && Recast->GetClass() == ARecastNavMesh::StaticClass() &&
        Recast->GetLevel() == World->PersistentLevel && Recast->IsRegistered())) return false;
    const FString ExternalFile = FPackageName::LongPackageNameToFilename(
        Recast->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
    if (!Fixture.SaveOwned(Error)) { AddError(Error); return false; }
    if (!TestTrue(TEXT("The captured external actor exists on disk."), IFileManager::Get().FileExists(*ExternalFile))) return false;
    if (!TestTrue(TEXT("Reconciliation admits the exact stock external default after full preflight."),
        ProjectWorldRuntimeNavigation::ReconcileTerritoryData(World, Error))) { AddError(Error); return false; }
    TestFalse(TEXT("The same actor becomes internal through its existing navigation owner."), Recast->IsPackageExternal());
    TestTrue(TEXT("Registration, supported agent and territory tile policy remain valid."),
        Recast->IsRegistered() && Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) == Recast &&
        Recast->GetConfig().Name == Agent.Name && Recast->GetConfig().AgentRadius == Agent.AgentRadius &&
        Recast->GetConfig().AgentHeight == Agent.AgentHeight && Recast->TileSizeUU >= 4096.f);
    if (!Fixture.SaveOwned(Error)) { AddError(Error); return false; }
    TestFalse(TEXT("Saving internal ownership retires its former external package."), IFileManager::Get().FileExists(*ExternalFile));
    if (!Fixture.Reload(Error)) { AddError(Error); return false; }
    World = Fixture.GetWorld();
    Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    int32 Count = 0;
    for (TActorIterator<ARecastNavMesh> It(World); It; ++It) { Recast = *It; ++Count; }
    if (!TestEqual(TEXT("Saved reload has one navigation authority."), Count, 1)) return false;
    TestFalse(TEXT("Saved reload retains internal packaging."), Recast->IsPackageExternal());
    const auto Before = Fixture.PackageDigests();
    if (!ProjectWorldRuntimeNavigation::ReconcileTerritoryData(World, Error) || !Fixture.SaveOwned(Error)) { AddError(Error); return false; }
    TestTrue(TEXT("An unchanged reconciliation and save preserve package bytes."), Before.OrderIndependentCompareEqual(Fixture.PackageDigests()));

    Recast->SetPackageExternal(true);
    FNavDataConfig Wrong = Recast->GetConfig(); Wrong.AgentHeight += 1;
    Recast->SetConfig(Wrong);
    TestFalse(TEXT("An external registered actor with inconsistent bookkeeping refuses."),
        ProjectWorldRuntimeNavigation::ReconcileTerritoryData(World, Error));
    TestTrue(TEXT("Refusal preserves external packaging and the inconsistent value."),
        Recast->IsPackageExternal() && Recast->GetConfig().AgentHeight == Wrong.AgentHeight);
    Recast->SetConfig(Agent);
    ARecastNavMesh* Competing = World->SpawnActor<ARecastNavMesh>();
    if (!TestNotNull(TEXT("The ownership control creates a competing native actor."), Competing)) return false;
    ON_SCOPE_EXIT { Navigation->UnregisterNavData(Competing); Competing->CleanUpAndMarkPendingKill(); };
    TestFalse(TEXT("A competing default refuses before external normalization."),
        ProjectWorldRuntimeNavigation::ReconcileTerritoryData(World, Error));
    TestTrue(TEXT("Conflicting ownership leaves the accepted actor external."), Recast->IsPackageExternal());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeNavigationDataOwnershipTest,
	"Project.World.Realization.Runtime.NavigationDataOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeNavigationDataOwnershipTest::RunTest(const FString& Parameters)
{
	FString Error;

	UWorld* MigratedWorld = GEditor->NewMap(false);
	UNavigationSystemV1* MigratedNavigation = Cast<UNavigationSystemV1>(
		MigratedWorld->GetNavigationSystem());
	TestNotNull(TEXT("Migration world has NavigationSystemV1."), MigratedNavigation);
	if (MigratedNavigation == nullptr)
	{
		return false;
	}
	ARecastNavMesh* OwnedRecast = nullptr;
	TestTrue(
		TEXT("The runtime owner resolves map-owned navigation data."),
		ProjectWorldRuntimeNavigation::EnsureInternalData(
			MigratedWorld, MigratedNavigation, OwnedRecast, Error));
	TestNotNull(TEXT("Owned navigation data exists."), OwnedRecast);
	if (OwnedRecast == nullptr)
	{
		return false;
	}
	TestFalse(TEXT("Navigation data stays inside the map package."), OwnedRecast->IsPackageExternal());

	ARecastNavMesh* ReusedRecast = nullptr;
	TestTrue(
		TEXT("An unchanged Apply reuses map-owned navigation data."),
		ProjectWorldRuntimeNavigation::EnsureInternalData(
			MigratedWorld, MigratedNavigation, ReusedRecast, Error));
	TestEqual(TEXT("Unchanged Apply keeps the same actor."), ReusedRecast, OwnedRecast);
	const FNavDataConfig Original = OwnedRecast->GetConfig();
	FNavDataConfig Stale = Original; Stale.AgentHeight += 10;
	OwnedRecast->SetConfig(Stale);
	ON_SCOPE_EXIT { OwnedRecast->SetConfig(Original); };
	ReusedRecast = OwnedRecast;
	TestTrue(TEXT("A preexisting mismatched agent refuses without replacing or editing it."),
		!ProjectWorldRuntimeNavigation::EnsureInternalData(MigratedWorld, MigratedNavigation, ReusedRecast, Error) &&
		!ReusedRecast && OwnedRecast->GetConfig().AgentHeight == Stale.AgentHeight);
	OwnedRecast->SetConfig(Original);
	ARecastNavMesh* Unexpected = MigratedWorld->SpawnActor<ARecastNavMesh>();
	if (!TestNotNull(TEXT("The refusal control contains actual unexpected native data."), Unexpected)) return false;
	ON_SCOPE_EXIT { MigratedNavigation->UnregisterNavData(Unexpected); Unexpected->CleanUpAndMarkPendingKill(); };
	TestTrue(TEXT("An unexpected native data census refuses without replacing the accepted actor."),
		!ProjectWorldRuntimeNavigation::EnsureInternalData(MigratedWorld, MigratedNavigation, ReusedRecast, Error) &&
		!ReusedRecast && OwnedRecast->GetConfig().AgentHeight == Original.AgentHeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectWorldRuntimeNavigationAgentConfigurationTest,
    "Project.World.Realization.Runtime.NavigationAgentConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeNavigationAgentConfigurationTest::RunTest(const FString& Parameters)
{
    UWorld* World=GEditor->NewMap(false);auto* Character=World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("Agent configuration is checked against an actual native standing capsule."),Character)) return false;
    ON_SCOPE_EXIT {Character->Destroy();};
    FJsonObject Receipt;FString Error;
    if (!ProjectWorldBasinNavigation::CheckAgent(*World,*Character,Receipt,Error)) {AddError(Error);return false;}
    auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    auto* Data=Navigation ? Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
    if (!TestNotNull(TEXT("Configured resolution is observed on the actual native data."),Data)) return false;
    TestEqual(TEXT("The project's intended five-centimeter default cells reach the current native consumer."),Data->GetCellSize(ENavigationDataResolution::Default),5.f);
    TestTrue(TEXT("Configured native high/default/low resolution tiers remain ordered."),
        Data->GetCellSize(ENavigationDataResolution::High)<=Data->GetCellSize(ENavigationDataResolution::Default) &&
        Data->GetCellSize(ENavigationDataResolution::Default)<=Data->GetCellSize(ENavigationDataResolution::Low));
    auto* Capsule=Character->GetCapsuleComponent();const float Previous=Capsule->GetUnscaledCapsuleHalfHeight();
    ON_SCOPE_EXIT {Capsule->SetCapsuleHalfHeight(Previous);};
    Capsule->SetCapsuleHalfHeight(Receipt.GetNumberField(TEXT("navigation_data_height_cm"))*.5+10);
    TestTrue(TEXT("The same native data refuses an actual capsule taller than its built clearance."),
        !ProjectWorldBasinNavigation::CheckAgent(*World,*Character,Receipt,Error) &&
        Error==TEXT("Native selected navigation data does not conservatively contain the actual Hero capsule."));
    Capsule->SetCapsuleHalfHeight(Previous);
    TestTrue(TEXT("Restoring the capsule restores the same agent-selection contract."),
        ProjectWorldBasinNavigation::CheckAgent(*World,*Character,Receipt,Error));
    AddInfo(FString::Printf(TEXT("NativeNavigationAgent radius_cm=%.17g height_cm=%.17g data_radius_cm=%.17g data_height_cm=%.17g; standing capsule and taller-capsule refusal/restoration"),
        Receipt.GetNumberField(TEXT("navigation_agent_radius_cm")),Receipt.GetNumberField(TEXT("navigation_agent_height_cm")),
        Receipt.GetNumberField(TEXT("navigation_data_radius_cm")),Receipt.GetNumberField(TEXT("navigation_data_height_cm"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProjectWorldRuntimeNavigationMapProjectionTest,
    "Project.World.Realization.Runtime.NavigationMapProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeNavigationMapProjectionTest::RunTest(const FString& Parameters)
{
    FProjectWorldVerifyWorld Fixture(TEXT("MapNavigationAgent"));FString Error;
    if (!Fixture.IsReady(Error)) {AddError(Error);return false;}
    UWorld* World=Fixture.GetWorld();
    FProjectWorldRuntimePartitionSettings Settings;Settings.PartitionCount=1;Settings.CellSizeCentimeters=12800;
    Settings.LoadingRangeCentimeters=25600;Settings.bIs2D=true;bool Changed=false;
    if (!ProjectWorldRuntimePartitionPolicy::Apply(World,Settings,Changed,Error)) {AddError(Error);return false;}
    auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);ARecastNavMesh* Recast=nullptr;
    if (!ProjectWorldRuntimeNavigation::EnsureInternalData(World,Navigation,Recast,Error)) {AddError(Error);return false;}
    int32 Version=0;FProjectWorldProjectFn Project;
    if (!TestTrue(TEXT("The native map projection is registered."),ProjectWorldVerify::FindProjection(TEXT("map"),1,Version,Project))) return false;
    TestEqual(TEXT("Map comparison version includes actual navigation agent and runtime mode."),Version,2);
    FProjectWorldVerifyScope Scope;Scope.World=World;Scope.ArtifactRoot=Fixture.GetRoot();
    auto Digest=[&](FString& Hash)
    {
        FProjectWorldProjection Result;
        if (!Project(Scope,Result,Error) || !Result.IsValid(Error)) {AddError(Error);return false;}
        Hash=Result.Sha256();return true;
    };
    FString Baseline;if (!Digest(Baseline)) return false;
    const FNavDataConfig Original=Recast->GetConfig();
    ON_SCOPE_EXIT {Recast->SetConfig(Original);};
    for (int32 Field=0;Field<3;++Field)
    {
        FNavDataConfig Candidate=Original;
        if (Field==0) Candidate.Name=TEXT("ChangedAgent");
        if (Field==1) Candidate.AgentRadius+=1;
        if (Field==2) Candidate.AgentHeight+=1;
        Recast->SetConfig(Candidate);FString Current;
        if (!Digest(Current)) return false;
        TestNotEqual(*FString::Printf(TEXT("Actual native agent field %d changes the map projection."),Field),Current,Baseline);
        Recast->SetConfig(Original);
        if (!Digest(Current)) return false;
        TestEqual(TEXT("Restoring the actual agent restores the exact projection."),Current,Baseline);
    }
    auto* Mode=FindFProperty<FEnumProperty>(Recast->GetClass(),TEXT("RuntimeGeneration"));
    if (!TestNotNull(TEXT("The actual native runtime-generation property is observable."),Mode)) return false;
    void* Value=Mode->ContainerPtrToValuePtr<void>(Recast);const uint64 Previous=Mode->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Value);
    ON_SCOPE_EXIT {Mode->GetUnderlyingProperty()->SetIntPropertyValue(Value,Previous);};
    Mode->GetUnderlyingProperty()->SetIntPropertyValue(Value,Previous==uint64(ERuntimeGenerationType::Static) ? uint64(ERuntimeGenerationType::Dynamic) : uint64(ERuntimeGenerationType::Static));
    FString Current;if (!Digest(Current)) return false;
    TestNotEqual(TEXT("Actual native generation mode changes the map projection."),Current,Baseline);
    Mode->GetUnderlyingProperty()->SetIntPropertyValue(Value,Previous);
    if (!Digest(Current)) return false;
    TestEqual(TEXT("Restoring native generation mode restores the exact projection."),Current,Baseline);
    for (const auto Resolution : {ENavigationDataResolution::Low,ENavigationDataResolution::Default,ENavigationDataResolution::High})
    {
        const float Size=Recast->GetCellSize(Resolution),Height=Recast->GetCellHeight(Resolution),Step=Recast->GetAgentMaxStepHeight(Resolution);
        ON_SCOPE_EXIT {Recast->SetCellSize(Resolution,Size);Recast->SetCellHeight(Resolution,Height);Recast->SetAgentMaxStepHeight(Resolution,Step);};
        for (int32 Field=0;Field<3;++Field)
        {
            if (Field==0) Recast->SetCellSize(Resolution,Size+1);
            if (Field==1) Recast->SetCellHeight(Resolution,Height+1);
            if (Field==2) Recast->SetAgentMaxStepHeight(Resolution,Step+1);
            if (!Digest(Current)) return false;
            TestNotEqual(*FString::Printf(TEXT("Actual native resolution %d field %d changes the map projection."),int32(Resolution),Field),Current,Baseline);
            Recast->SetCellSize(Resolution,Size);Recast->SetCellHeight(Resolution,Height);Recast->SetAgentMaxStepHeight(Resolution,Step);
            if (!Digest(Current)) return false;
            TestEqual(TEXT("Restoring actual resolution restores the exact projection."),Current,Baseline);
        }
    }
    for (float* Property : {&Recast->MaxSimplificationError,&Recast->SimplificationElevationRatio,&Recast->AgentMaxSlope})
    {
        const float PreviousValue=*Property;ON_SCOPE_EXIT {*Property=PreviousValue;};*Property=PreviousValue+1;
        if (!Digest(Current)) return false;
        TestNotEqual(TEXT("Actual native approximation settings change the map projection."),Current,Baseline);
        *Property=PreviousValue;if (!Digest(Current)) return false;
        TestEqual(TEXT("Restoring native approximation restores the exact projection."),Current,Baseline);
    }
    return !HasAnyErrors();
}

#endif
