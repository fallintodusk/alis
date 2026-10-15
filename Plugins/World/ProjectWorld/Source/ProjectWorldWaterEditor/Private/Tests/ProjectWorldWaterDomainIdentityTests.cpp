// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldWaterLayerProducer.h"
#include "ProjectWorldWaterMeshBuilder.h"
#include "ProjectWorldWaterSourceContext.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldWaterDomainIdentityTest
{
    bool Emit(const FProjectWorldCanonicalBundle& Bundle, FMeshDescription& Out, FString& Error)
    {
        const auto Bounds=ProjectWorldWaterSourceContext::PreparationBounds(Bundle).FindChecked(TEXT("water/lake"));
        FProjectWorldPreparedWaterSurface Prepared;
        const auto& Feature=Bundle.Features.FindChecked(TEXT("water/lake"));
        FProjectWorldWaterMeshBuildResult Result;
        if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Feature,Bounds,Prepared,Error) ||
            !ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Bundle.Cells[0],Feature,Prepared,.25,Result,Error)) return false;
        Out=MoveTemp(Result.MeshDescription); return true;
    }
    TArray<FVector3f> Points(const FMeshDescription& Mesh)
    {
        TArray<FVector3f> Out;
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        for (const auto Id : Mesh.Triangles().GetElementIDs())
            for (const auto Instance : Mesh.GetTriangleVertexInstances(Id))
                Out.Add(Positions[Mesh.GetVertexInstanceVertex(Instance)]);
        return Out;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterPreparationDomainIdentityTest,
    "Project.World.Water.PreparationDomainIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterPreparationDomainIdentityTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterDomainIdentityTest;
    FProjectWorldCanonicalBundle Bundle;
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    for (int32 X=0; X<3; ++X)
    {
        FProjectWorldCanonicalCell Cell;
        Cell.CellId=FString::Printf(TEXT("domain:x%d:y0"),X); Cell.CellX=X;
        Cell.Bounds=FVector4d(X*100,0,(X+1)*100,100);
        if (X==0) Cell.OwnedFeatureIds={TEXT("water/lake")};
        if (X==1) Cell.ReferencedFeatureIds={TEXT("water/lake")};
        Bundle.Cells.Add(Cell);
    }
    FProjectWorldCanonicalFeature Feature;
    Feature.FeatureId=TEXT("water/lake"); Feature.FeatureClass=TEXT("water");
    Feature.WaterSurface.bValid=true; Feature.WaterSurface.Geometry=TEXT("polygon");
    Feature.WaterSurface.Behavior=TEXT("standing"); Feature.WaterSurface.LevelMeters=12.5;
    Feature.GeometryPolygons.Add({{FVector2D(-30,20),FVector2D(280,20),FVector2D(280,90),FVector2D(-30,90)}, {}});
    Bundle.Features.Add(Feature.FeatureId,Feature);
    FProjectWorldRealizationProfile Profile;
    FProjectWorldRealizationLayer Layer;
    const FProjectWorldAuthoredOverlaySet Overlays;
    auto Hash=[&](const FProjectWorldCanonicalBundle& B)
    {
        FProjectWorldLayerUnitInputs Units; FString Error;
        TestTrue(TEXT("Native producer hashes its canonical unit inputs"),
            FProjectWorldWaterLayerProducer::Get().HashUnitInputs({B,Profile,Layer,Overlays},Units,Error));
        return Units.HashByUnit.FindRef(B.Cells[0].CellId);
    };
    FString Error; FMeshDescription Before,After;
    if (!TestTrue(TEXT("Native baseline source preparation emits geometry"),Emit(Bundle,Before,Error))) {AddError(Error); return false;}
    const FString BeforeHash=Hash(Bundle);
    Bundle.Cells[2].ReferencedFeatureIds.Add(Feature.FeatureId);
    if (!TestTrue(TEXT("Changed consumed preparation context emits geometry"),Emit(Bundle,After,Error))) {AddError(Error); return false;}
    if (!TestTrue(TEXT("Context changes real emitted float triangle positions/order"),Points(Before)!=Points(After))) return false;
    TestNotEqual(TEXT("Output-changing consumed domain must change the original cell identity"),BeforeHash,Hash(Bundle));
    const FString WithContext=Hash(Bundle);
    Bundle.Cells[2].ReferencedFeatureIds.Add(TEXT("unconsumed/feature"));
    TestEqual(TEXT("Unconsumed references leave the domain identity unchanged"),WithContext,Hash(Bundle));
    auto Remote=Feature; Remote.FeatureId=TEXT("water/remote");
    Bundle.Features.Add(Remote.FeatureId,Remote);
    Bundle.Cells[2].ReferencedFeatureIds.Add(Remote.FeatureId);
    TestEqual(TEXT("A different valid water source does not dirty an unconsuming cell"),WithContext,Hash(Bundle));
    Bundle.Cells[1].ReferencedFeatureIds.Reset();
    TestEqual(TEXT("Interior membership changes preserving consumed domain do not dirty this cell"),WithContext,Hash(Bundle));
    return true;
}
#endif
