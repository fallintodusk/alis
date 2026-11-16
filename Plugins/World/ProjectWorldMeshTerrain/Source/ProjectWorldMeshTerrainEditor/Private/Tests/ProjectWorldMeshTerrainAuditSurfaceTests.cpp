// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainAuditSurfaces.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionStaticMeshDescriptor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldTerrainAuditPrimarySurfaces,
    "Project.World.Realization.MeshTerrain.AuditPrimarySurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldTerrainAuditPrimarySurfaces::RunTest(const FString&)
{
    using namespace UE::MeshPartition;
    using namespace ProjectWorldMeshTerrainAuditSurfaces;
    AddInfo(TEXT("[Fast][Integration][World]"));
    const auto Init=UWorld::InitializationValues().CreateWorldPartition(false).EnableWorldPartitionStreaming(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("AuditPrimarySurfaces"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("The audit fixture owns a disposable editor world."),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);World->RemoveFromRoot();};
    auto* Section=World->SpawnActor<ACompiledSection>();
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!TestNotNull(TEXT("The actual native section exists."),Section) ||
        !TestNotNull(TEXT("The fixture uses an existing native plane asset."),Plane)) return false;
    FStaticMeshDescriptor Descriptor;Descriptor.CollisionProfileName=TEXT("NoCollision");
    Section->AddStaticMesh(Plane,Descriptor);
    TInlineComponentArray<UStaticMeshComponent*> All(Section);
    int32 Bearing=0,MainPass=0;
    for (auto* Component : All) if (Component->GetStaticMesh()==Plane)
    {++Bearing;MainPass+=Component->bRenderInMainPass;}
    if (!TestEqual(TEXT("Native AddStaticMesh registers primary and auxiliary mesh-bearing components."),Bearing,2) ||
        !TestEqual(TEXT("The native primary inventory selects one component."),Section->GetMeshComponents().Num(),1)) return false;
    TestEqual(TEXT("The auxiliary copy does not render in the main pass."),MainPass,1);

    FSurfaceProbe Source;
    if (!TestTrue(TEXT("The original primary mesh supplies the expected source field."),
        BuildSurfaceProbe(Section,Section->GetMeshComponents()[0],Source,true))) return false;
    FProjectWorldCanonicalBundle Bundle;auto& Cell=Bundle.Cells.AddDefaulted_GetRef();
    FBox2D Bounds(ForceInit);auto& Terrain=Cell.Terrain;
    auto& Quad=Terrain.RefinedQuads.AddDefaulted_GetRef();Quad.Coordinate=FIntPoint(0,0);
    for (const auto& P : Source.Vertices)
    {
        const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,P);
        Quad.Vertices.Add(Canonical);Bounds+=FVector2D(Canonical);
    }
    Quad.Triangles=Source.Triangles;
    Cell.Bounds=FVector4(Bounds.Min.X,Bounds.Min.Y,Bounds.Max.X,Bounds.Max.Y);
    Terrain.Bounds=Cell.Bounds;Terrain.SamplesX=Terrain.SamplesY=2;
    Terrain.SampleSpacing=Bounds.GetSize();Terrain.HeightsMeters={0,0,0,0};
    const auto Collect=[&]()
    {
        TArray<FSurfaceProbe> Probes;
        for (auto* Component : VisualComponents(*Section))
        {
            FSurfaceProbe Probe;
            if (BuildSurfaceProbe(Section,Component,Probe,true)) Probes.Add(MoveTemp(Probe));
        }
        return Probes;
    };
    const auto One=Collect();
    FProjectWorldMeshTerrainSurfaceFidelity Nanite,Fallback;TArray<FString> Failures;
    const bool Accepted=CompareRefinements(Bundle,One,One,1.e-4,Nanite,Fallback,Failures);
    AddInfo(FString::Join(Failures,TEXT("; ")));
    if (!TestTrue(TEXT("The actual audit must accept one primary plane despite its native RVT copy."),Accepted) ||
        !TestEqual(TEXT("The audit observes exactly one primary surface."),One.Num(),1)) return false;
    TestEqual(TEXT("Both representations cover the original source triangles."),Nanite.ExpectedTriangles,Fallback.ExpectedTriangles);
    TestTrue(TEXT("The source plane has nonempty measured refinement coverage."),Nanite.ExpectedTriangles>0);

    Section->AddStaticMesh(Plane,Descriptor);
    const auto Duplicate=Collect();
    TestEqual(TEXT("Two genuine primary instances sharing one mesh remain distinct."),Duplicate.Num(),2);
    Failures.Reset();Nanite={};Fallback={};
    TestFalse(TEXT("Actual duplicate primary geometry still fails the unchanged overlap gate."),
        CompareRefinements(Bundle,Duplicate,One,1.e-4,Nanite,Fallback,Failures));
    TestTrue(TEXT("The duplicate reaches the actual coverage refusal."),
        Failures.Num()>0 && Failures[0].Contains(TEXT("Overlapping compiled faces")));
    TestTrue(TEXT("A failed Nanite field cannot suppress independent fallback measurement."),Fallback.ExpectedTriangles>0);
    TestEqual(TEXT("The unchanged fallback remains correct after the Nanite refusal."),Fallback.MismatchedTriangles,0);
    Failures.Reset();Nanite={};Fallback={};
    TestFalse(TEXT("A duplicate fallback also refuses after a good Nanite field."),
        CompareRefinements(Bundle,One,Duplicate,1.e-4,Nanite,Fallback,Failures));
    TestEqual(TEXT("The good Nanite field remains independently measured."),Nanite.MismatchedTriangles,0);
    TestTrue(TEXT("The bad fallback is measured and rejected."),Fallback.MismatchedTriangles>0);
    return !HasAnyErrors();
}
#endif
