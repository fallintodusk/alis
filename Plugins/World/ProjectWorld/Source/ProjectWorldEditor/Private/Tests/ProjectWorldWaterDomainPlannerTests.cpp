// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldLayerInventory.h"
#include "ProjectWorldRealizationProfile.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterDomainPlannerTest,
    "Project.World.Realization.Layers.WaterPreparationDomain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterDomainPlannerTest::RunTest(const FString&)
{
    FProjectWorldRealizationProfile Profile;
    FString Error,Code;
    if (!ProjectWorldRealizationProfile::Load(FPaths::ProjectPluginsDir()/
        TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_territory_twin.realization.json"),
        Profile,Code,Error)) {AddError(Error); return false;}
    FProjectWorldCanonicalBundle Bundle;
    Bundle.GridId=TEXT("grid_24b9032e5f87005d"); Bundle.ProfileId=Profile.CanonicalProfileId;
    Bundle.WorldDataPluginName=TEXT("ProjectWorldTestData");
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    for (int32 X=0; X<3; ++X)
    {
        FProjectWorldCanonicalCell Cell;
        Cell.CellId=FString::Printf(TEXT("domain:x%d:y0"),X); Cell.CellX=X;
        Cell.Bounds=FVector4d(X*63,0,(X+1)*63,63);
        Cell.Terrain.Bounds=Cell.Bounds; Cell.Terrain.SamplesX=Cell.Terrain.SamplesY=2;
        Cell.Terrain.SampleSpacing=FVector2D(63,63); Cell.Terrain.HeightsMeters={20,21,22,23};
        Cell.Terrain.ArtifactHash=FString::ChrN(64,TEXT('a')+X);
        Cell.FeatureArtifactHash=FString::ChrN(64,TEXT('d')+X);
        if (X==0) Cell.OwnedFeatureIds={TEXT("water/lake")};
        if (X==1) Cell.ReferencedFeatureIds={TEXT("water/lake")};
        Bundle.Cells.Add(Cell);
    }
    FProjectWorldCanonicalFeature Feature;
    Feature.FeatureId=TEXT("water/lake"); Feature.FeatureClass=TEXT("water");
    Feature.WaterSurface.bValid=true; Feature.WaterSurface.Geometry=TEXT("polygon");
    Feature.WaterSurface.Behavior=TEXT("standing"); Feature.WaterSurface.LevelMeters=12.5;
    Feature.GeometryPolygons.Add({{FVector2D(-30,20),FVector2D(180,20),FVector2D(180,50),FVector2D(-30,50)}, {}});
    Bundle.Features.Add(Feature.FeatureId,Feature);
    const FProjectWorldAuthoredOverlaySet Overlays;
    FProjectWorldRealizationResult Baseline,Result;
    if (!TestTrue(TEXT("Actual planner builds baseline inventories"),ProjectWorldLayerInventory::Build(
        Bundle,Profile,Overlays,true,nullptr,Baseline,Error))) {AddError(Error); return false;}
    FProjectWorldLayerDirtyInput Base;
    Base.RealizationProfileId=Profile.ProfileId;
    for (const auto& Layer : Baseline.LayerInventories)
    {
        Base.ProducerFingerprints.Add(Layer.LayerId,FString::ChrN(64,TEXT('f')));
        auto& Entry=Base.BaseLayers.Add(Layer.LayerId);
        Entry.NormalizedLayerContractHash=Layer.NormalizedLayerContractHash;
        for (const auto& Input : Layer.CanonicalInputs) Entry.CanonicalInputs.Add(Input.UnitId,Input.Hash);
    }
    Bundle.Cells[2].ReferencedFeatureIds.Add(Feature.FeatureId);
    if (!TestTrue(TEXT("Planner compares changed context under identical shared contracts/fingerprints"),
        ProjectWorldLayerInventory::Build(Bundle,Profile,Overlays,false,&Base,Result,Error))) {AddError(Error); return false;}
    const auto* Water=Result.LayerInventories.FindByPredicate([](const auto& L){return L.LayerId==TEXT("water");});
    const auto* Terrain=Result.LayerInventories.FindByPredicate([](const auto& L){return L.LayerId==TEXT("terrain");});
    if (!TestNotNull(TEXT("Water inventory exists"),Water) || !TestNotNull(TEXT("Terrain inventory exists"),Terrain)) return false;
    TestTrue(TEXT("Actual planner selects original cell whose consumed preparation changed"),Water->FinalDirtyUnits.Contains(Bundle.Cells[0].CellId));
    TestFalse(TEXT("Bounded context never requires a water wildcard"),Water->FinalDirtyUnits.Contains(TEXT("*")));
    TestEqual(TEXT("Context-only water input changes leave terrain clean"),Terrain->FinalDirtyUnits.Num(),0);
    return true;
}
#endif
