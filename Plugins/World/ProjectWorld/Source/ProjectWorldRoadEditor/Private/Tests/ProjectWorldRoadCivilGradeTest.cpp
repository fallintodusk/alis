// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadCivilGradeTest,
    "Project.World.Realization.Road.ImmutableCivilGrade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadCivilGradeTest::RunTest(const FString& Parameters)
{
    const FString Root = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_CIVIL_GRADE_FIXTURE"));
    if (!TestFalse(TEXT("An explicit disposable canonical fixture is required."), Root.IsEmpty())) return false;
    FProjectWorldCanonicalBundle Base, Candidate;
    FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("Original canonical bundle authenticates."), FProjectWorldCanonicalLoader::Load(
        FPaths::Combine(Root, TEXT("base/compile_result.json")), Base, Validation))) { AddError(Validation.Message); return false; }
    if (!TestTrue(TEXT("Refined canonical bundle authenticates."), FProjectWorldCanonicalLoader::Load(
        FPaths::Combine(Root, TEXT("candidate/refinement_result.json")), Candidate, Validation))) { AddError(Validation.Message); return false; }
    FProjectWorldRealizationLayer Layer;
    Layer.LayerId = TEXT("roads"); Layer.GeneratorId = TEXT("project_road_mesh"); Layer.GeneratorVersion = 1;
    Layer.DependencyHaloCells = 1;
    Layer.NormalizedSettings = TEXT("{\"selected_classes\":[\"primary\"],\"surface_offset_m\":0.15,\"maximum_segment_length_m\":7.5,")
        TEXT("\"nanite\":true,\"collision\":\"complex_as_simple\",\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
    Layer = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
    FString Error;
    FMeshDescription Original, Refined;
    FVector OriginalOrigin, RefinedOrigin;
    if (!TestTrue(TEXT("Original Road builds."), ProjectWorldRoadRealization::BuildDescription(
        Base, Base.Cells[0], Layer, Original, OriginalOrigin, Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Explicit civil grade builds over the refined final surface."), ProjectWorldRoadRealization::BuildDescription(
        Candidate, Candidate.Cells[0], Layer, Refined, RefinedOrigin, Error))) { AddError(Error); return false; }
    TArray<FVector3f> A, B;
    for (const auto V : Original.Vertices().GetElementIDs()) A.Add(FStaticMeshConstAttributes(Original).GetVertexPositions()[V]);
    for (const auto V : Refined.Vertices().GetElementIDs()) B.Add(FStaticMeshConstAttributes(Refined).GetVertexPositions()[V]);
    TestEqual(TEXT("World origin is retained."), OriginalOrigin, RefinedOrigin);
    TestTrue(TEXT("Road triangle drape retains its exact original vertex sequence."), A == B);
    FString Before, After;
    TestTrue(TEXT("Road hashes the final-clearance input."), ProjectWorldRoadRealization::HashCellInput(
        Candidate, Candidate.Cells[0], Layer, Before, Error));
    Candidate.Cells[0].Terrain.ArtifactHash = FString::ChrN(64, 'e');
    TestTrue(TEXT("A different final surface with the same immutable grade remains qualified."),
        ProjectWorldRoadRealization::HashCellInput(Candidate, Candidate.Cells[0], Layer, After, Error));
    TestNotEqual(TEXT("Actually consumed final-clearance identity invalidates Road."), Before, After);
    const auto Valid = Candidate;
    auto& Feature = Candidate.Features[TEXT("alis:fixture:way:2")];
    Feature.CivilGrade.Reset();
    FMeshDescription Default;
    TestTrue(TEXT("Unreferenced Road still follows final Terrain."), ProjectWorldRoadRealization::BuildDescription(
        Candidate, Candidate.Cells[0], Layer, Default, RefinedOrigin, Error));
    TArray<FVector3f> DefaultVertices;
    for (const auto V : Default.Vertices().GetElementIDs()) DefaultVertices.Add(FStaticMeshConstAttributes(Default).GetVertexPositions()[V]);
    TestTrue(TEXT("The final-surface control actually differs from the preserved grade."), A != DefaultVertices);
    for (int32 Defect = 0; Defect < 3; ++Defect)
    {
        Candidate = Valid;
        if (Defect == 0) Candidate.Features[TEXT("alis:fixture:way:2")].CivilGrade->TerrainArtifacts.Reset();
        if (Defect == 1) Candidate.Cells[0].Terrain.BaseArtifactHash = FString::ChrN(64, 'd');
        if (Defect == 2) for (auto& Q : Candidate.Cells[0].Terrain.RefinedQuads) for (auto& V : Q.Vertices) V.Z = 100;
        FMeshDescription Refused;
        TestFalse(TEXT("Missing support, stale base or final penetration refuses the planner."),
            ProjectWorldRoadRealization::HashCellInput(Candidate, Candidate.Cells[0], Layer, After, Error));
        TestFalse(TEXT("The actual builder refuses the same bad input."), ProjectWorldRoadRealization::BuildDescription(
            Candidate, Candidate.Cells[0], Layer, Refused, RefinedOrigin, Error));
        TestEqual(TEXT("Refused input emits no partial Road."), Refused.Triangles().Num(), 0);
    }
    return true;
}
#endif
