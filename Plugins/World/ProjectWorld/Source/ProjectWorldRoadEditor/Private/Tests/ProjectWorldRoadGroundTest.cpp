// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "ProjectWorldVerifySurfaceFixture.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadGroundTest,
    "Project.World.Realization.Road.GroundFinalSurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadGroundTest::RunTest(const FString&)
{
    FString Input=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_GROUND_SCENE_FIXTURE"));
    const bool bRealScene=!Input.IsEmpty();
    if (!bRealScene)
    {
        const FString Root=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
            TEXT("tmp/world/road_ground_tests"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        const FString Script=FPaths::Combine(FPaths::ProjectDir(),TEXT("tools/World/CanonicalCompilation/tests/ground_consumer_fixture.py"));
        int32 Code=-1;FString Out,Error;
        if (!TestTrue(TEXT("Public Ground constructor creates its own disposable fixture."),
            FPlatformProcess::ExecProcess(TEXT("python.exe"),*FString::Printf(TEXT("-B \"%s\" \"%s\""),*Script,*Root),
                &Code,&Out,&Error) && Code==0)) { AddError(Out+Error);return false; }
        Input=FPaths::Combine(Root,TEXT("road-ground-fixture.json"));
    }
    FString Json,Error;TSharedPtr<FJsonObject> Fixture;
    if (!TestTrue(TEXT("Explicit test-only typed scene parses."),FFileHelper::LoadFileToString(Json,*Input) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Fixture))) return false;
    const FString OutputPath=FPaths::ConvertRelativePathToFull(Fixture->GetStringField(TEXT("output_path")));
    if (!TestTrue(TEXT("Proof output is a new file under repository tmp."),
        FPaths::IsUnderDirectory(OutputPath,FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp")))) &&
        !FPaths::FileExists(OutputPath))) return false;
    FProjectWorldCanonicalBundle Bundle;
    FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("The scene starts from authenticated canonical fixture/base input."),
        FProjectWorldCanonicalLoader::Load(Fixture->GetStringField(TEXT("base_result")),Bundle,Validation)))
    { AddError(Validation.Message);return false; }
    if (!TestTrue(TEXT("Explicit typed Ground overrides only its selected cells."),
        ProjectWorldVerifyTestFlow::ApplyTypedTerrainFixture(Bundle,*Fixture,Error))) { AddError(Error);return false; }
    const FString RoadId=Fixture->GetStringField(TEXT("road_id"));
    if (!bRealScene)
    {
        FProjectWorldCanonicalFeature Road;Road.FeatureId=RoadId;Road.FeatureClass=TEXT("road");Road.GeometryType=TEXT("LineString");
        Road.RoadClass=Fixture->GetStringField(TEXT("road_class"));Road.WidthMeters=Fixture->GetNumberField(TEXT("road_width_m"));
        for (const auto& V:Fixture->GetArrayField(TEXT("road_points")))
        { const auto XY=V->AsArray();Road.GeometryPoints.Add(FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber())); }
        Road.GeometryParts={Road.GeometryPoints};
        const auto AxisPoint=[&](double X)
        {
            const FVector2D A=Road.GeometryPoints[0],B=Road.GeometryPoints.Last();
            return FVector2D(X,FMath::Lerp(A.Y,B.Y,(X-A.X)/(B.X-A.X)));
        };
        for (const auto& Cell:Bundle.Cells)
        {
            auto& Fragment=Road.Representations.AddDefaulted_GetRef();Fragment.CellId=Cell.CellId;
            Fragment.Kind=TEXT("road_fragment");
            Fragment.Parts={{AxisPoint(FMath::Max(Cell.Bounds.X,10.0)),AxisPoint(FMath::Min(Cell.Bounds.Z,50.0))}};
        }
        Road.OwnerCellId=Bundle.Cells[0].CellId;Bundle.Features.Add(RoadId,MoveTemp(Road));
    }
    if (!TestTrue(TEXT("The requested actual Road is present."),Bundle.Features.Contains(RoadId))) return false;
    for (auto& Cell:Bundle.Cells)
    { Cell.OwnedFeatureIds.Reset();Cell.ReferencedFeatureIds.Reset(); }
    for (const auto& Key:Fixture->GetArrayField(TEXT("selected_cells")))
    {
        auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Key->AsString();});
        if (!TestNotNull(TEXT("Road cell exists."),Cell)) return false;
        Cell->OwnedFeatureIds={RoadId};
    }
    FProjectWorldRealizationLayer Layer;Layer.LayerId=TEXT("roads");Layer.GeneratorId=TEXT("project_road_mesh");Layer.GeneratorVersion=1;Layer.DependencyHaloCells=1;
    Layer.NormalizedSettings=TEXT("{\"selected_classes\":[\"footway\"],\"surface_offset_m\":0.15,\"maximum_segment_length_m\":7.5,")
        TEXT("\"nanite\":true,\"collision\":\"complex_as_simple\",\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
    TSharedPtr<FJsonObject> Settings;
    if (!TestTrue(TEXT("Fixture Road settings parse."),FJsonSerializer::Deserialize(
        TJsonReaderFactory<>::Create(Layer.NormalizedSettings),Settings))) return false;
    TArray<TSharedPtr<FJsonValue>> SelectedClasses;
    SelectedClasses.Add(MakeShared<FJsonValueString>(Bundle.Features[RoadId].RoadClass));
    Settings->SetArrayField(TEXT("selected_classes"),SelectedClasses);
    Layer.NormalizedSettings.Reset();
    if (!TestTrue(TEXT("Fixture selects the actual Road class."),FJsonSerializer::Serialize(
        Settings.ToSharedRef(),TJsonWriterFactory<>::Create(&Layer.NormalizedSettings)))) return false;
    Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
    TArray<TSharedPtr<FJsonValue>> Vertices,Faces;double MaximumError=0;int32 BuiltCells=0,OutsideOwnerVertices=0;
    for (auto& Cell:Bundle.Cells)
    {
        if (Cell.OwnedFeatureIds.IsEmpty()) continue;
        FMeshDescription Mesh;FVector Origin;
        if (!TestTrue(TEXT("The production Road builder consumes the typed Ground scene without package writes."),
            ProjectWorldRoadRealization::BuildDescription(Bundle,Cell,Layer,Mesh,Origin,Error))) { AddError(Error);return false; }
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();TMap<FVertexID,int32> Indices;
        for (const auto V:Mesh.Vertices().GetElementIDs())
        {
            const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(Positions[V])+Origin);
            TArray<FProjectWorldSurfaceTriangle> Support;
            const FBox2D Query(FVector2D(P)-FVector2D(.001,.001),FVector2D(P)+FVector2D(.001,.001));
            if (P.X<Cell.Bounds.X || P.X>Cell.Bounds.Z || P.Y<Cell.Bounds.Y || P.Y>Cell.Bounds.W) ++OutsideOwnerVertices;
            // Ribbon width can cross the cell which owns its clipped source axis.
            for (const auto& Neighbor:Bundle.Cells)
            {
                if (!Query.Intersect(FBox2D(FVector2D(Neighbor.Bounds.X,Neighbor.Bounds.Y),
                    FVector2D(Neighbor.Bounds.Z,Neighbor.Bounds.W)))) continue;
                TArray<FProjectWorldSurfaceTriangle> Incident;
                if (!ProjectWorldTerrainSurface::Triangles(Neighbor,Query,Incident,Error)) { AddError(Error);return false; }
                Support.Append(Incident);
            }
            TArray<double> Heights;
            for (const auto& Triangle:Support)
            {
                bool bInside=true;
                for (int32 I=0;I<3;++I)
                {
                    const auto& A=Triangle.Points[I];const auto& B=Triangle.Points[(I+1)%3];
                    if ((B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X)<-1e-8) bInside=false;
                }
                double Height=0;
                if (bInside && ProjectWorldTerrainSurface::Height(Triangle,FVector2D(P),Height,Error)) Heights.Add(Height);
            }
            if (!TestTrue(TEXT("Every stored Road vertex has actual affine terrain support."),!Heights.IsEmpty()))
            {
                AddError(FString::Printf(TEXT("GroundRoadSupport cell=%s point=(%.15g,%.15g,%.15g) bounds=(%.15g,%.15g,%.15g,%.15g) candidates=%d"),
                    *Cell.CellId,P.X,P.Y,P.Z,Cell.Bounds.X,Cell.Bounds.Y,Cell.Bounds.Z,Cell.Bounds.W,Support.Num()));
                return false;
            }
            const double Height=Heights[0];
            for (double Other:Heights) if (!TestTrue(TEXT("The diagnostic never crosses an unresolved terrain seam."),FMath::Abs(Other-Height)<.001)) return false;
            MaximumError=FMath::Max(MaximumError,FMath::Abs(P.Z-Height-.15));
            TArray<TSharedPtr<FJsonValue>> XYZ;for (int32 I=0;I<3;++I) XYZ.Add(MakeShared<FJsonValueNumber>(P[I]));
            Indices.Add(V,Vertices.Num());Vertices.Add(MakeShared<FJsonValueArray>(XYZ));
        }
        for (const auto T:Mesh.Triangles().GetElementIDs())
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            for (int32 I=0;I<3;++I) Row.Add(MakeShared<FJsonValueNumber>(Indices[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(T,I))]));
            Faces.Add(MakeShared<FJsonValueArray>(Row));
        }
        ++BuiltCells;
    }
    TestTrue(TEXT("Actual producer returns nonempty Road."),!Faces.IsEmpty());
    if (!bRealScene) TestTrue(TEXT("The oblique ribbon exercises actual neighboring-cell support."),OutsideOwnerVertices>0);
    TestTrue(TEXT("Stored float Road remains on the selected final field plus its owned presentation offset."),MaximumError<.001);
    auto Broken=Bundle;
    for (auto& Cell:Broken.Cells) for (auto& Quad:Cell.Terrain.RefinedQuads) Quad.Triangles.Reset();
    for (const auto& Cell:Broken.Cells)
    {
        if (Cell.OwnedFeatureIds.IsEmpty()) continue;
        FMeshDescription Refused;FVector Origin;
        TestFalse(TEXT("Missing final Ground coverage refuses instead of silently draping onto the old DSM."),
            ProjectWorldRoadRealization::BuildDescription(Broken,Cell,Layer,Refused,Origin,Error));
        TestEqual(TEXT("Refusal leaves no partial Road."),Refused.Triangles().Num(),0);
    }
    auto Output=MakeShared<FJsonObject>();Output->SetStringField(TEXT("scope"),TEXT("Typed Ground/native Road source description only; no loader admission for replacement, saved collision or package proof"));
    Output->SetArrayField(TEXT("vertices"),Vertices);Output->SetArrayField(TEXT("triangles"),Faces);Output->SetNumberField(TEXT("maximum_final_height_error_m"),MaximumError);
    Output->SetNumberField(TEXT("built_cells"),BuiltCells);Output->SetStringField(TEXT("road_id"),RoadId);
    FString Result;FJsonSerializer::Serialize(Output,TJsonWriterFactory<>::Create(&Result));
    TestTrue(TEXT("Native source proof stays in its declared disposable output."),FFileHelper::SaveStringToFile(Result,*OutputPath));
    AddInfo(FString::Printf(TEXT("GroundRoadProof cells=%d vertices=%d triangles=%d outside_owner_vertices=%d maximum_error_m=%.12g"),
        BuiltCells,Vertices.Num(),Faces.Num(),OutsideOwnerVertices,MaximumError));
    return true;
}
#endif
