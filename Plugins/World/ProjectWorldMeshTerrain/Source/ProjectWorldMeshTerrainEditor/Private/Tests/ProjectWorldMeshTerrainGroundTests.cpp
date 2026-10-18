#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldTerrainSurface.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldGroundTypedQueriesTest,
    "Project.World.Realization.MeshTerrain.GroundTypedQueries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGroundTypedQueriesTest::RunTest(const FString&)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
        TEXT("tmp/world/ground_typed_tests"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const FString Script = FPaths::Combine(FPaths::ProjectDir(),
        TEXT("tools/World/CanonicalCompilation/tests/ground_query_fixture.py"));
    int32 Code = -1; FString Out, Error;
    if (!TestTrue(TEXT("The existing pure canonical constructor creates a bounded typed fixture."),
        FPlatformProcess::ExecProcess(TEXT("python.exe"),
            *FString::Printf(TEXT("-B \"%s\" \"%s\""), *Script, *Root), &Code, &Out, &Error) && Code == 0))
    { AddError(Out + Error); return false; }
    FString Json; TSharedPtr<FJsonObject> Fixture;
    if (!TestTrue(TEXT("Test payload parses; this is deliberately not authority admission."),
        FFileHelper::LoadFileToString(Json, *FPaths::Combine(Root, TEXT("ground-typed-fixture.json"))) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Fixture))) return false;
    FProjectWorldCanonicalBundle Bundle;
    Bundle.CanonicalCrs = TEXT("EPSG:32618"); Bundle.CoordinateTransform = TEXT("fixture_affine");
    Bundle.CellQuads = FIntPoint(1,1); Bundle.SampleSpacingMeters = FVector2D(30,30);
    Bundle.CoordinateQuantizationMeters = 0.01; Bundle.HeightQuantizationMeters = 0.1;
    for (const auto& Value : Fixture->GetArrayField(TEXT("cells")))
    {
        const auto Object = Value->AsObject();
        auto& Cell = Bundle.Cells.AddDefaulted_GetRef();
        Cell.CellId = Object->GetStringField(TEXT("cell_id"));
        const auto Bounds = Object->GetArrayField(TEXT("bounds"));
        Cell.Bounds = FVector4d(Bounds[0]->AsNumber(),Bounds[1]->AsNumber(),Bounds[2]->AsNumber(),Bounds[3]->AsNumber());
        auto& Terrain = Cell.Terrain; Terrain.Bounds = Cell.Bounds;
        Terrain.SamplesX = 2; Terrain.SamplesY = 2; Terrain.SampleSpacing = FVector2D(30,30);
        Terrain.SurfaceRoles = {TEXT("ground"),TEXT("hydro_transition")};
        for (const auto& Z : Object->GetArrayField(TEXT("base_heights"))) Terrain.HeightsMeters.Add(Z->AsNumber());
        for (int32 Role=0; Role<2; ++Role)
            for (const auto& W : Object->GetArrayField(TEXT("base_weights")))
                Terrain.SurfaceWeights.FindOrAdd(Terrain.SurfaceRoles[Role]).Add(W->AsArray()[Role]->AsNumber());
        for (const auto& Q : Object->GetArrayField(TEXT("quads")))
        {
            const auto ObjectQuad = Q->AsObject(); auto& Quad = Terrain.RefinedQuads.AddDefaulted_GetRef();
            const auto Coordinate = ObjectQuad->GetArrayField(TEXT("coordinate"));
            Quad.Coordinate = FIntPoint(Coordinate[0]->AsNumber(),Coordinate[1]->AsNumber());
            for (const auto& V : ObjectQuad->GetArrayField(TEXT("vertices")))
            {
                const auto P = V->AsArray();
                Quad.Vertices.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()));
                Quad.Weights.Add(FVector2f(P[3]->AsNumber(),P[4]->AsNumber()));
            }
            for (const auto& F : ObjectQuad->GetArrayField(TEXT("triangles")))
            {
                const auto I = F->AsArray();
                Quad.Triangles.Add(FIntVector(I[0]->AsNumber(),I[1]->AsNumber(),I[2]->AsNumber()));
            }
        }
    }
    if (!TestEqual(TEXT("Two selected cells exercise a shared final boundary."),Bundle.Cells.Num(),2)) return false;
    int32 Queries=0, Faces=0; double MaximumError=0;
    for (const auto& Cell : Bundle.Cells)
    {
        for (const auto& V : Fixture->GetArrayField(TEXT("source_queries")))
        {
            const auto P=V->AsArray(); const double X=P[0]->AsNumber(), Y=P[1]->AsNumber();
            if (X<Cell.Bounds.X || X>Cell.Bounds.Z) continue;
            double Actual=0;
            if (!TestTrue(TEXT("Final query covers an independently specified source grid node."),
                ProjectWorldGeneratedGeometry::SampleTerrain(Cell,X,Y,Actual,Error))) { AddError(Error); return false; }
            MaximumError=FMath::Max(MaximumError,FMath::Abs(Actual-P[2]->AsNumber())); ++Queries;
        }
        const auto& Quad=Cell.Terrain.RefinedQuads[0];
        for (const auto& Face : Quad.Triangles)
        {
            const FVector Center=(Quad.Vertices[Face.X]+Quad.Vertices[Face.Y]+Quad.Vertices[Face.Z])/3;
            double Height=0;
            if (!ProjectWorldGeneratedGeometry::SampleTerrain(Cell,Center.X,Center.Y,Height,Error))
            { AddError(Error); return false; }
            MaximumError=FMath::Max(MaximumError,FMath::Abs(Height-Center.Z)); ++Faces;
        }
        TArray<FProjectWorldSurfaceTriangle> Triangles;
        TestTrue(TEXT("Road/placement triangle consumers see the selected Ground faces."),
            ProjectWorldTerrainSurface::Triangles(Cell,FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),
                FVector2D(Cell.Bounds.Z,Cell.Bounds.W)),Triangles,Error));
        TestEqual(TEXT("Triangle consumers retain complete source topology."),Triangles.Num(),Quad.Triangles.Num());
        UE::Geometry::FDynamicMesh3 Mesh;
        if (!TestTrue(TEXT("Mesh Terrain realizes the same typed Ground surface."),
            ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Cell,Mesh,Error))) { AddError(Error); return false; }
        TestTrue(TEXT("Mesh Terrain source fidelity includes selected heights, topology and weights."),
            ProjectWorldMeshTerrainProducer::MatchesCellMesh(Bundle,Cell,Mesh,Error));
        auto HasSpecifiedSource = [&Bundle,&Quad](const UE::Geometry::FDynamicMesh3& Actual)
        {
            if (Actual.VertexCount()!=Quad.Vertices.Num() || Actual.TriangleCount()!=Quad.Triangles.Num() || Actual.Attributes()==nullptr ||
                Actual.Attributes()->NumWeightLayers()!=2) return false;
            TSet<int32> Seen; TMap<int32,int32> SourceIndices;
            for (int32 Id : Actual.VertexIndicesItr())
            {
                const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Actual.GetVertex(Id));
                const int32 Index=Quad.Vertices.IndexOfByPredicate([&P](const FVector& V){return V.Equals(P,1e-8);});
                if (Index==INDEX_NONE || Seen.Contains(Index)) return false;
                Seen.Add(Index); SourceIndices.Add(Id,Index);
                for (int32 Role=0; Role<2; ++Role)
                {
                    float Weight=0; Actual.Attributes()->GetWeightLayer(Role)->GetValue(Id,&Weight);
                    if (!FMath::IsNearlyEqual(Weight,Quad.Weights[Index][Role],1e-6f)) return false;
                }
            }
            auto OrientedFace = [](int32 A,int32 B,int32 C)
            {
                if (B<A && B<C) return FIntVector(B,C,A);
                if (C<A && C<B) return FIntVector(C,A,B);
                return FIntVector(A,B,C);
            };
            TMap<FIntVector,int32> ExpectedFaces;
            for (const auto& Face : Quad.Triangles)
                ++ExpectedFaces.FindOrAdd(OrientedFace(Face.X,Face.Y,Face.Z));
            for (int32 Id : Actual.TriangleIndicesItr())
            {
                const auto Face=Actual.GetTriangle(Id);
                int32* Remaining=ExpectedFaces.Find(OrientedFace(SourceIndices.FindChecked(Face.A),
                    SourceIndices.FindChecked(Face.B),SourceIndices.FindChecked(Face.C)));
                if (Remaining==nullptr || *Remaining==0) return false;
                --*Remaining;
            }
            for (const auto& Pair : ExpectedFaces) if (Pair.Value!=0) return false;
            return Seen.Num()==Quad.Vertices.Num();
        };
        TestTrue(TEXT("Actual dynamic-mesh vertices, weights and oriented faces independently match the specifying payload."),
            HasSpecifiedSource(Mesh));
        auto WrongWeight=Mesh; const float InvalidWeight=-0.25f;
        WrongWeight.Attributes()->GetWeightLayer(0)->SetValue(*WrongWeight.VertexIndicesItr().begin(),&InvalidWeight);
        TestFalse(TEXT("A corrupted realized semantic weight fails the independent source check."),
            HasSpecifiedSource(WrongWeight));
        auto Reversed=Mesh;
        Reversed.ReverseTriOrientation(*Reversed.TriangleIndicesItr().begin());
        TestFalse(TEXT("A reversed realized face fails independent oriented source topology."),
            HasSpecifiedSource(Reversed));
        auto Stale=Cell; Stale.Terrain.RefinedQuads.Reset(); UE::Geometry::FDynamicMesh3 Old;
        TestTrue(TEXT("Old grid is independently realizable as a negative control."),
            ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Stale,Old,Error));
        TestFalse(TEXT("The unchanged original grid cannot certify the new Ground surface."),
            ProjectWorldMeshTerrainProducer::MatchesCellMesh(Bundle,Cell,Old,Error));
        auto Missing=Cell; Missing.Terrain.RefinedQuads[0].Triangles.Reset(); double Height=0;
        TestFalse(TEXT("Missing Ground coverage refuses rather than falling back to the DSM grid."),
            ProjectWorldGeneratedGeometry::SampleTerrain(Missing,Cell.Bounds.X+15,15,Height,Error));
        TestTrue(TEXT("Immutable queries retain the original base ground."),
            ProjectWorldGeneratedGeometry::SampleTerrain(Cell,Cell.Bounds.X+15,15,Height,Error,EProjectWorldTerrainQuery::ImmutableGrid));
        TestEqual(TEXT("The independent old-grid height remains 12 m."),Height,12.0);
    }
    TestTrue(TEXT("Source nodes and final triangle interiors agree within double precision."),MaximumError<1e-8);
    AddInfo(FString::Printf(TEXT("GroundTypedProof nodes=%d centroids=%d maximum_height_error_m=%.12g fixture=%s; no loader/render/collision claim"),
        Queries,Faces,MaximumError,*Root));
    return true;
}
#endif
