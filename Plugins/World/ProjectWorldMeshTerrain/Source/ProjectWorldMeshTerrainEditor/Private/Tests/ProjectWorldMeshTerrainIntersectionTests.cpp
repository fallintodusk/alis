#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMeshTerrainSplitIntersectionTest,
    "Project.World.Realization.MeshTerrain.SplitIntersection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldMeshTerrainSplitIntersectionTest::RunTest(const FString&)
{
    AddInfo(TEXT("[Fast][Integration][World]"));
    // Captured canonical face and its actual stored native-grid neighborhood.
    const FProjectWorldSurfaceTriangle Expected={{FVector(583369.7936440681,4508122.206355932,3.223747425914036),FVector(583370.01,4508122.22,3.2256186656238777),FVector(583369.8473707708,4508122.229295895,3.2241654770120256)}};
    const TArray<FProjectWorldSurfaceTriangle> Actual={
        {{FVector(583367.0560205078,4508120.846084595,3.1963262939453125),FVector(583369.8090240479,4508127.5200000005,3.2232861328125),FVector(583366.7583483887,4508130.2720281985,2.906211242675781)}},
        {{FVector(583369.8078857422,4508127.512555666,3.2232873535156252),FVector(583367.0560205078,4508120.846084595,3.1963262939453125),FVector(583368.8620025635,4508125.103448791,3.214327697753906)}},
        {{FVector(583367.0560205078,4508120.846084595,3.1963262939453125),FVector(583369.8078857422,4508127.512555666,3.2232873535156252),FVector(583368.8909765625,4508125.292879944,3.2142999267578127)}},
        {{FVector(583369.8090240479,4508127.5200000005,3.2232861328125),FVector(583367.0560205078,4508120.846084595,3.1963262939453125),FVector(583368.8909765625,4508125.292879944,3.2142999267578127)}},
        {{FVector(583369.0819824219,4508121.047061767,3.2171331787109376),FVector(583370.0800000001,4508121.11,3.2275323486328125),FVector(583369.7936437989,4508122.206356201,3.2237475585937503)}},
        {{FVector(583369.1020629883,4508122.897937012,3.2160128784179687),FVector(583369.7384680177,4508122.261531983,3.223130187988281),FVector(583370.01,4508122.22,3.225618591308594)}},
        {{FVector(583369.1020629883,4508122.897937012,3.2160128784179687),FVector(583370.01,4508122.22,3.225618591308594),FVector(583369.2706274415,4508124.0,3.214867858886719)}},
        {{FVector(583369.2706274415,4508124.0,3.214867858886719),FVector(583370.01,4508122.22,3.225618591308594),FVector(583371.2938873292,4508124.0,3.232671203613281)}},
        {{FVector(583369.7384680177,4508122.261531983,3.223130187988281),FVector(583369.7936437989,4508122.206356201,3.2237475585937503),FVector(583369.8473706055,4508122.229295654,3.224165344238281)}},
        {{FVector(583369.7936437989,4508122.206356201,3.2237475585937503),FVector(583370.01,4508122.22,3.225618591308594),FVector(583369.8473706055,4508122.229295654,3.224165344238281)}},
        {{FVector(583370.01,4508122.22,3.225618591308594),FVector(583369.7384680177,4508122.261531983,3.223130187988281),FVector(583369.8473706055,4508122.229295654,3.224165344238281)}},
        {{FVector(583369.7936437989,4508122.206356201,3.2237475585937503),FVector(583370.0254809571,4508121.974519043,3.22634033203125),FVector(583370.01,4508122.22,3.225618591308594)}},
        {{FVector(583369.7936437989,4508122.206356201,3.2237475585937503),FVector(583370.0800000001,4508121.11,3.2275323486328125),FVector(583370.0254809571,4508121.974519043,3.22634033203125)}},
        {{FVector(583370.01,4508122.22,3.225618591308594),FVector(583370.0254809571,4508121.974519043,3.22634033203125),FVector(583371.0148016358,4508122.175358887,3.234566650390625)}},
        {{FVector(583370.01,4508122.22,3.225618591308594),FVector(583371.0148016358,4508122.175358887,3.234566650390625),FVector(583371.2938873292,4508124.0,3.232671203613281)}},
    };
    FProjectWorldCanonicalBundle Bundle; auto& Cell=Bundle.Cells.AddDefaulted_GetRef();
    Cell.Bounds=FVector4(583369.7936440681,4508122.206355932,583370.01,4508122.229295895);
    auto& Terrain=Cell.Terrain; Terrain.Bounds=Cell.Bounds; Terrain.SamplesX=2; Terrain.SamplesY=2;
    Terrain.SampleSpacing=FVector2D(Cell.Bounds.Z-Cell.Bounds.X,Cell.Bounds.W-Cell.Bounds.Y);
    Terrain.HeightsMeters={0,0,0,0};
    auto& Quad=Terrain.RefinedQuads.AddDefaulted_GetRef(); Quad.Coordinate=FIntPoint(0,0);
    for (const auto& P : Expected.Points) Quad.Vertices.Add(P);
    Quad.Triangles.Add(FIntVector(0,1,2));
    FProjectWorldMeshTerrainSurfaceFidelity Fidelity; FString Error;
    TestTrue(FString::Printf(TEXT("The captured native split preserves the complete canonical face: %s"),*Error),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Actual,1.e-4,Fidelity,Error));
    TestEqual(TEXT("The complete captured field has no rejected source face."),Fidelity.MismatchedTriangles,0);
    auto ReversedBundle=Bundle; ReversedBundle.Cells[0].Terrain.RefinedQuads[0].Triangles[0]=FIntVector(0,2,1);
    auto Reversed=Actual; for (auto& Face : Reversed) Swap(Face.Points[1],Face.Points[2]);
    TestTrue(TEXT("The same comparison preserves a complete field with both windings reversed."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(ReversedBundle,Reversed,1.e-4,Fidelity,Error));
    TestFalse(TEXT("Reversing only stored face indices cannot preserve canonical orientation."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Reversed,1.e-4,Fidelity,Error));
    auto Missing=Actual; Missing.RemoveAt(9);
    TestFalse(TEXT("A missing real covering triangle cannot pass as disjoint."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Missing,1.e-4,Fidelity,Error));
    auto Raised=Actual; for (auto& Face : Raised) for (auto& P : Face.Points) P.Z+=.001;
    TestFalse(TEXT("The unchanged height limit rejects a real shifted field."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Raised,1.e-4,Fidelity,Error));
    auto Duplicate=Actual; Duplicate.Add(Actual[9]);
    TestFalse(TEXT("A duplicate covering face still fails the positive-area overlap gate."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Duplicate,1.e-4,Fidelity,Error));
    auto Overlap=Actual; const auto& Face=Actual[9];
    Overlap.Add({{Face.Points[0],(Face.Points[0]+Face.Points[1])*.5,(Face.Points[0]+Face.Points[1]+Face.Points[2])/3.}});
    TestFalse(TEXT("A distinct additional covering face cannot pass as a duplicate-free field."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Overlap,1.e-4,Fidelity,Error));
    TestTrue(TEXT("The distinct overlap fails the actual positive-overlap guard."),Error.Contains(TEXT("Overlapping compiled faces")));
    FProjectWorldCanonicalBundle Holed; auto& Square=Holed.Cells.AddDefaulted_GetRef();
    Square.Bounds=FVector4(0,0,4,4); auto& Grid=Square.Terrain; Grid.Bounds=Square.Bounds;
    Grid.SamplesX=Grid.SamplesY=2; Grid.SampleSpacing=FVector2D(4,4); Grid.HeightsMeters={3,3,3,3};
    const FVector A(0,0,3),B(4,0,3),C(4,4,3),D(0,4,3),E(1,1,3),F(3,1,3),G(3,3,3),H(1,3,3);
    auto& Whole=Grid.RefinedQuads.AddDefaulted_GetRef(); Whole.Coordinate=FIntPoint(0,0);
    Whole.Vertices={A,B,C,D}; Whole.Triangles={FIntVector(0,1,2),FIntVector(0,2,3)};
    TArray<FProjectWorldSurfaceTriangle> Ring={{{A,B,F}},{{A,F,E}},{{B,C,G}},{{B,G,F}},{{C,D,H}},{{C,H,G}},{{D,A,E}},{{D,E,H}}};
    TestFalse(TEXT("An interior hole refuses despite preserving every original outer boundary edge."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Holed,Ring,1.e-4,Fidelity,Error));
    Ring.Add({{E,F,G}}); Ring.Add({{E,G,H}});
    TestTrue(TEXT("Filling exactly the missing interior restores complete field coverage."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Holed,Ring,1.e-4,Fidelity,Error));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldCoastalIntersectionTest,
    "Project.World.Realization.MeshTerrain.CoastalIntersection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldCoastalIntersectionTest::RunTest(const FString&)
{
    AddInfo(TEXT("[Fast][Integration][World]"));
    // A captured canonical face and all four positively covering stored faces.
    const FProjectWorldSurfaceTriangle Expected={{FVector(583363.8477020704,4508291.419971019,0.03939318633056246),FVector(583392,4508292,3.2672172559549644),FVector(583370.1790337877,4508292.45,1.1454943026619542)}};
    const TArray<FProjectWorldSurfaceTriangle> Actual={
        {{FVector(583363.41,4508292.45,0.0),FVector(583363.8199865723,4508291.14602539,0.036898820400238036),FVector(583363.8477026367,4508291.419970703,0.03939318656921387)}},
        {{FVector(583363.41,4508292.45,0.0),FVector(583363.8477026367,4508291.419970703,0.03939318656921387),FVector(583364.0032861328,4508292.45,0.05339573860168457)}},
        {{FVector(583363.8477026367,4508291.419970703,0.03939318656921387),FVector(583370.1790338135,4508292.45,1.1454943084716798),FVector(583364.0032861328,4508292.45,0.05339573860168457)}},
        {{FVector(583363.8477026367,4508291.419970703,0.03939318656921387),FVector(583392.0,4508292.0,3.2672174072265623),FVector(583370.1790338135,4508292.45,1.1454943084716798)}},
    };
    FProjectWorldCanonicalBundle Bundle; auto& Cell=Bundle.Cells.AddDefaulted_GetRef();
    Cell.Bounds=FVector4d(583363.8477020704,4508291.419971019,583392,4508292.45);
    auto& Terrain=Cell.Terrain; Terrain.Bounds=Cell.Bounds;
    Terrain.SamplesX=Terrain.SamplesY=2;
    Terrain.SampleSpacing=FVector2D(Cell.Bounds.Z-Cell.Bounds.X,Cell.Bounds.W-Cell.Bounds.Y);
    Terrain.HeightsMeters={0,0,0,0};
    auto& Quad=Terrain.RefinedQuads.AddDefaulted_GetRef(); Quad.Coordinate=FIntPoint(0,0);
    for (const auto& P : Expected.Points) Quad.Vertices.Add(P);
    Quad.Triangles.Add(FIntVector(0,1,2));
    FProjectWorldMeshTerrainSurfaceFidelity Fidelity; FString Error;
    const bool Covered=ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Actual,1.e-4,Fidelity,Error);
    TestTrue(FString::Printf(TEXT("Complete captured native coverage is preserved: %s"),*Error),Covered);
    TestEqual(TEXT("The complete coastal face has no coverage mismatch."),Fidelity.MismatchedTriangles,0);
    auto Missing=Actual; Missing.RemoveAt(3);
    TestFalse(TEXT("Removing the actual principal covering face still refuses."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Missing,1.e-4,Fidelity,Error));
    auto Raised=Actual; for (auto& Face : Raised) for (auto& P : Face.Points) P.Z+=.001;
    TestFalse(TEXT("The unchanged precision bound rejects a one millimetre departure."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Raised,1.e-4,Fidelity,Error));
    auto Duplicate=Actual; Duplicate.Add(Actual[3]);
    TestFalse(TEXT("An extra actual covering face still refuses overlap."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Duplicate,1.e-4,Fidelity,Error));
    TestTrue(TEXT("Duplicate coverage reaches the actual overlap guard."),Error.Contains(TEXT("Overlapping compiled faces")));
    auto Reversed=Actual; for (auto& Face : Reversed) Swap(Face.Points[1],Face.Points[2]);
    TestFalse(TEXT("Reversed stored orientation cannot authenticate the canonical field."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Reversed,1.e-4,Fidelity,Error));
    return !HasAnyErrors();
}
#endif
