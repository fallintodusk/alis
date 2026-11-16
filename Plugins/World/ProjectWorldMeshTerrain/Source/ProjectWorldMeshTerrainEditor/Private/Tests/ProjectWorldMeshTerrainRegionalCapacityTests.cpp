// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainRegionalScene.h"
#include "ProjectWorldMeshTerrainRegionalSupport.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalCapacityTest
{
    using FValues=TArray<TSharedPtr<FJsonValue>>;
    TSharedPtr<FJsonObject> Row(const TArray<FProjectWorldSurfaceTriangle>& Faces)
    {
        FValues Vertices,Triangles;
        for (const auto& F:Faces)
        {
            FValues Indices;
            for (const auto& P:F.Points)
            {
                Indices.Add(MakeShared<FJsonValueNumber>(Vertices.Num()));
                Vertices.Add(MakeShared<FJsonValueArray>(FValues{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
            }
            Triangles.Add(MakeShared<FJsonValueArray>(Indices));
        }
        auto O=MakeShared<FJsonObject>();O->SetArrayField(TEXT("world_vertices_cm"),Vertices);
        O->SetArrayField(TEXT("vertices_canonical"),Vertices);O->SetArrayField(TEXT("triangles"),Triangles);return O;
    }
    bool Fixture(ProjectWorldRegionalSceneTest::FScene& Scene,TArray<FProjectWorldSurfaceTriangle>& Ground,FString& Error)
    {
        for (int32 X=0;X<2;++X)
        {
            FProjectWorldCanonicalCell Cell;Cell.CellId=FString::FromInt(X);Cell.Bounds=FVector4d(X*930,0,(X+1)*930,930);
            auto& T=Cell.Terrain;T.Bounds=Cell.Bounds;T.SampleSpacing=FVector2D(30,30);T.SamplesX=32;T.SamplesY=32;T.HeightsMeters.Init(0,1024);
            if (X==0)
            {
                FProjectWorldCanonicalTerrainQuad Q;Q.Coordinate=FIntPoint(0,0);
                for (int32 J=0;J<=16;++J) for (int32 I=0;I<=16;++I) Q.Vertices.Add(FVector(I*30./16,930-J*30./16,0));
                for (int32 J=0;J<16;++J) for (int32 I=0;I<16;++I)
                {
                    const int32 A=J*17+I;Q.Triangles.Add(FIntVector(A,A+17,A+1));Q.Triangles.Add(FIntVector(A+1,A+17,A+18));
                }
                T.RefinedQuads.Add(MoveTemp(Q));
            }
            TArray<FProjectWorldSurfaceTriangle> Faces;
            if (!ProjectWorldTerrainSurface::Triangles(Cell,FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W)),Faces,Error)) return false;
            for (auto& Face:Faces) for (auto& P:Face.Points) P=FProjectWorldCanonicalLoader::CanonicalToUnreal(Scene.Bundle,P);
            Ground.Append(Faces);Scene.Bundle.Cells.Add(MoveTemp(Cell));
        }
        auto GroundRow=Row(Ground);GroundRow->SetStringField(TEXT("kind"),TEXT("gpu_fallback"));
        auto Variant=MakeShared<FJsonObject>();Variant->SetStringField(TEXT("variant"),TEXT("fallback"));
        Variant->SetArrayField(TEXT("components"),{MakeShared<FJsonValueObject>(GroundRow)});
        Scene.Expected=MakeShared<FJsonObject>();Scene.Expected->SetArrayField(TEXT("variants"),{MakeShared<FJsonValueObject>(Variant)});
        TArray<FProjectWorldSurfaceTriangle> Water{{{FVector(0,0,-1),FVector(1860,0,-1),FVector(0,930,-1)}},
            {{FVector(1860,0,-1),FVector(1860,930,-1),FVector(0,930,-1)}}};
        auto Waters=MakeShared<FJsonObject>();Waters->SetArrayField(TEXT("actors"),{MakeShared<FJsonValueObject>(Row(Water))});
        Scene.Expected->SetObjectField(TEXT("regional_water_before"),Waters);return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRegionalGroundCapacityTest,"Project.World.Realization.MeshTerrain.RegionalGroundCapacity",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectWorldRegionalGroundCapacityTest::RunTest(const FString&)
{
    using namespace ProjectWorldRegionalCapacityTest;ProjectWorldRegionalSceneTest::FScene Scene(*this);
    TArray<FProjectWorldSurfaceTriangle> Ground;FString Error;
    if (!TestTrue(TEXT("Two complete cells and one legal 512-face refined quad construct."),Fixture(Scene,Ground,Error))) return false;
    TestEqual(TEXT("The complete Ground census exceeds the separate structural cap."),Ground.Num(),4354);
    ProjectWorldRegionalSupportTest::FSupport Support;
    if (!TestTrue(FString::Printf(TEXT("The regional Ground reader accepts its authenticated complete census: %s"),*Error),Support.Read(Scene,Error)))
    {AddError(Error);return false;}
    ProjectWorldRegionalSupportTest::FSample Sample;
    TestTrue(TEXT("Sampling uses the same Ground capacity and actual field."),Support.Sample(
        FProjectWorldCanonicalLoader::CanonicalToUnreal(Scene.Bundle,FVector(15,915,0)),Sample,Error));
    TestEqual(TEXT("Actual dry Ground remains the sampled support."),Sample.Height,0.);
    TestFalse(TEXT("A large Ground field cannot widen the structural validation policy."),ProjectWorldConstructedSurface::ValidateField(Ground,Error));
    double Height=0;
    TestTrue(TEXT("The existing structural sampler also keeps its strict capacity."),ProjectWorldConstructedSurface::TrySample(Ground,FVector2D::ZeroVector,Height,Error)==ProjectWorldConstructedSurface::ESampleResult::Invalid);
    auto Component=Scene.Expected->GetArrayField(TEXT("variants"))[0]->AsObject()->GetArrayField(TEXT("components"))[0]->AsObject();
    auto Faces=Component->GetArrayField(TEXT("triangles"));Faces.Pop();Component->SetArrayField(TEXT("triangles"),Faces);
    TestFalse(TEXT("A missing saved Ground face cannot pass the authenticated census."),Support.Read(Scene,Error));
    return !HasAnyErrors();
}
#endif
