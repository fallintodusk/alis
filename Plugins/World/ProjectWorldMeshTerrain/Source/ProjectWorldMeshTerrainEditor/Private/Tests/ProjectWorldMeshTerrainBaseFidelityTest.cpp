#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMeshTerrainBaseFidelityTest,
    "Project.World.Realization.MeshTerrain.UnrefinedSurfaceFidelity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldMeshTerrainBaseFidelityTest::RunTest(const FString&)
{
    FProjectWorldCanonicalBundle Bundle;auto& Cell=Bundle.Cells.AddDefaulted_GetRef();
    // Retained Kazan bank quad: ordinary grid Terrain, not a manufactured refinement.
    Cell.CellId=TEXT("captured_bank");Cell.Bounds=FVector4(380990,6185670,381020,6185700);
    auto& Grid=Cell.Terrain;Grid.Bounds=Cell.Bounds;Grid.SamplesX=2;Grid.SamplesY=2;
    Grid.SampleSpacing=FVector2D(30,30);Grid.HeightsMeters={58,59.9,50,50};
    TArray<FProjectWorldSurfaceTriangle> Original;FString Error;
    if (!TestTrue(TEXT("The retained unrefined source declares two complete physical faces."),
        ProjectWorldTerrainSurface::Triangles(Cell,FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W)),Original,Error)
        && Original.Num()==2)) return false;
    FProjectWorldMeshTerrainSurfaceFidelity Result;
    TestTrue(TEXT("The complete original grid field matches."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Original,1.e-4,Result,Error));
    TestEqual(TEXT("Base Terrain comparison must consume every source face."),Result.ExpectedTriangles,2);
    auto Missing=Original;Missing.RemoveAt(0);
    TestFalse(TEXT("Deleting a base Terrain face refuses."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Missing,1.e-4,Result,Error));
    auto Raised=Original;Raised[0].Points[0].Z+=.01;
    TestFalse(TEXT("Changing a base face interior refuses."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Raised,1.e-4,Result,Error));
    auto Duplicate=Original;Duplicate.Add(Original[0]);
    TestFalse(TEXT("Overlapping base Terrain faces refuse."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Duplicate,1.e-4,Result,Error));
    auto Outside=Original;auto Stray=Original[0];
    for (auto& Point:Stray.Points) Point.X+=90;
    Outside.Add(Stray);
    TestFalse(TEXT("Extra Terrain outside the complete selected field refuses."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Outside,1.e-4,Result,Error));
    auto TinyOutside=Original;auto Tiny=Stray;
    Tiny.Points[1]=Tiny.Points[0]+FVector(1.e-5,0,0);Tiny.Points[2]=Tiny.Points[0]+FVector(0,1.e-5,0);
    TinyOutside.Add(Tiny);
    TestFalse(TEXT("A tiny wholly unrepresented face cannot pass an absolute area tolerance."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,TinyOutside,1.e-4,Result,Error));
    auto Degenerate=Original;Stray.Points[2]=Stray.Points[1];Degenerate.Add(Stray);
    TestFalse(TEXT("A degenerate face cannot evade the complete-field census."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Degenerate,1.e-4,Result,Error));
    TestFalse(TEXT("Missing actual base Terrain refuses."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,{},1.e-4,Result,Error));
    TestFalse(TEXT("An empty selected source cannot prove field fidelity."),ProjectWorldMeshTerrainProducer::CompareCanonicalSurface({},Original,1.e-4,Result,Error));
    TestEqual(TEXT("The ordinary source was never relabeled as refined."),Cell.Terrain.RefinedQuads.Num(),0);
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldMeshTerrainBaseFidelityTest,
    "Project.World.Realization.MeshTerrain.UnrefinedSurfaceFidelity","[Fast][Unit][World]")
#endif
