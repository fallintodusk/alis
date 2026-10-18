// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterSourceApproximation.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldWaterLayerProducer.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterExactLevelTest,
    "Project.World.Water.ExactCanonicalLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterExactLevelTest::RunTest(const FString&)
{
    FProjectWorldCanonicalBundle Bundle;
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    FProjectWorldCanonicalCell Cell;
    Cell.CellId=TEXT("exact:x0:y0"); Cell.Bounds=FVector4d(0,0,100,100);
    FProjectWorldCanonicalFeature Exact;
    Exact.FeatureId=TEXT("water/contained"); Exact.FeatureClass=TEXT("water");
    Exact.WaterSurface.bValid=true; Exact.WaterSurface.Behavior=TEXT("standing");
    Exact.WaterSurface.Geometry=TEXT("polygon"); Exact.WaterSurface.LevelMeters=13.2;
    Exact.WaterSurface.FunctionId=TEXT("contained_ground_quantile"); Exact.WaterSurface.FunctionVersion=1;
    Exact.WaterSurface.ContainmentSpillMeters=13.38;
    Exact.GeometryPolygons.Add({{FVector2D(10,10),FVector2D(30,10),FVector2D(30,30),FVector2D(10,30)}, {}});
    auto Legacy=Exact;
    Legacy.FeatureId=TEXT("water/ordinary"); Legacy.WaterSurface.FunctionId=TEXT("standing_polygon_quantile");
    Bundle.Features.Add(Exact.FeatureId,Exact); Bundle.Features.Add(Legacy.FeatureId,Legacy);
    Cell.OwnedFeatureIds={Exact.FeatureId,Legacy.FeatureId}; Bundle.Cells.Add(Cell);
    FString Error;
    TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
    FMeshDescription Separate;
    TArray<const FProjectWorldCanonicalFeature*> Sources;
    FVector Origin;
    for (const auto& Id : Cell.OwnedFeatureIds)
    {
        const auto& Feature=Bundle.Features[Id];
        auto& Surface=Prepared.Add(Id);
        FProjectWorldWaterMeshBuildResult Result;
        if (!TestTrue(TEXT("Native polygon preparation and emission succeed"),
            ProjectWorldWaterMeshBuilder::PrepareSurface(Feature,Cell.Bounds,Surface,Error) &&
            ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Feature,Surface,.25,Result,Error)))
        {AddError(Error);return false;}
        Origin=Result.ActorOrigin;
        const auto Positions=FStaticMeshConstAttributes(Result.MeshDescription).GetVertexPositions();
        const float Expected=FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
            FVector(0,0,Id==Exact.FeatureId ? 13.2 : 13.45))-Origin).Z;
        for (const auto Vertex : Result.MeshDescription.Vertices().GetElementIDs())
            TestEqual(TEXT("Stored float height uses exact canonical level only for the contained model"),Positions[Vertex].Z,Expected);
        for (int32 I=0;I<Result.TriangleCount;++I) Sources.Add(&Feature);
        if (Separate.Triangles().Num()==0) Separate=MoveTemp(Result.MeshDescription);
        else FStaticMeshOperations::AppendMeshDescription(Result.MeshDescription,Separate,FStaticMeshOperations::FAppendSettings());
    }
    bool Changed=false; FString Hash;
    TestTrue(TEXT("Different effective planes do not require source approximation"),
        ProjectWorldWaterSourceApproximation::Refine(Separate,Sources,Bundle,Cell,Origin,.25,Changed,Hash,Error) && !Changed);
    FMeshDescription Combined; FString Semantic; int32 Count=0;
    if (!TestTrue(TEXT("Cell composition retains different effective planes"),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Cell.OwnedFeatureIds,.25,Combined,Origin,Semantic,Count,Error)))
    {AddError(Error);return false;}
    TSet<float> Levels;
    const auto Positions=FStaticMeshConstAttributes(Combined).GetVertexPositions();
    for (const auto Vertex : Combined.Vertices().GetElementIDs()) Levels.Add(Positions[Vertex].Z);
    TestEqual(TEXT("Same raw level with different application modes remains two final planes"),Levels.Num(),2);
    const FProjectWorldRealizationProfile Profile; const FProjectWorldRealizationLayer Layer;
    const FProjectWorldAuthoredOverlaySet Overlays;
    const auto InputHash=[&]()
    {
        FProjectWorldLayerUnitInputs Units;
        TestTrue(TEXT("Water producer hashes consumed model"),FProjectWorldWaterLayerProducer::Get().HashUnitInputs(
            {Bundle,Profile,Layer,Overlays},Units,Error));
        return Units.HashByUnit.FindRef(Cell.CellId);
    };
    const FString Before=InputHash();
    Bundle.Features[Exact.FeatureId].WaterSurface.ContainmentSpillMeters=13.4;
    TestNotEqual(TEXT("Consumed spill constraint changes unit identity"),Before,InputHash());
    Bundle.Features[Exact.FeatureId].WaterSurface.ContainmentSpillMeters=13.38;
    Bundle.Features[Exact.FeatureId].WaterSurface.FunctionId=Legacy.WaterSurface.FunctionId;
    TestNotEqual(TEXT("Consumed exact-height model changes the unit identity"),Before,InputHash());
    auto& NearSpill=Bundle.Features[Exact.FeatureId];
    NearSpill.WaterSurface.FunctionId=Exact.WaterSurface.FunctionId;
    NearSpill.WaterSurface.LevelMeters=13.2000008;
    NearSpill.WaterSurface.ContainmentSpillMeters=13.2000009;
    // The valid raw13.2000008 level rounds to13.200001220703125m in stored cm.
    FProjectWorldWaterMeshBuildResult Refused;
    TestFalse(TEXT("Float storage crossing the contained spill must refuse"),
        ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,NearSpill,Prepared[Exact.FeatureId],.25,Refused,Error));
    TestEqual(TEXT("Stored-spill refusal leaves no partial mesh"),Refused.MeshDescription.Triangles().Num(),0);
    auto Safe=NearSpill; Safe.FeatureId=TEXT("water/a-safe"); Safe.WaterSurface.ContainmentSpillMeters=13.3;
    auto Tight=NearSpill; Tight.FeatureId=TEXT("water/z-tight");
    FProjectWorldCanonicalBundle GroupBundle=Bundle;
    GroupBundle.Features.Reset(); GroupBundle.Features.Add(Safe.FeatureId,Safe); GroupBundle.Features.Add(Tight.FeatureId,Tight);
    TMap<FString,FProjectWorldPreparedWaterSurface> GroupPrepared;
    GroupPrepared.Add(Safe.FeatureId,Prepared[Exact.FeatureId]); GroupPrepared.Add(Tight.FeatureId,Prepared[Exact.FeatureId]);
    TestFalse(TEXT("A nonfirst tight spill bound cannot be lost by equal-plane grouping"),
        ProjectWorldWaterCellMesh::Build(GroupBundle,Cell,GroupPrepared,{Safe.FeatureId,Tight.FeatureId},.25,
            Combined,Origin,Semantic,Count,Error));
    TestEqual(TEXT("Group preflight refuses before any geometry is emitted"),Combined.Triangles().Num(),0);
    Bundle.HeightOriginMeters=13.2;
    TestTrue(TEXT("The same raw level is safe in a frame whose stored float stays below spill"),
        ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,NearSpill,Prepared[Exact.FeatureId],.25,Refused,Error));
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldWaterExactLevelTest,
    "Project.World.Water.ExactCanonicalLevel","[Fast][Unit][World]")
#endif
