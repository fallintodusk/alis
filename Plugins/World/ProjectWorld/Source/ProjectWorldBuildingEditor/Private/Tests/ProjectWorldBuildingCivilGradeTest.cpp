// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingCivilGradeTest,
    "Project.World.Realization.Building.ImmutableCivilGrade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBuildingCivilGradeTest::RunTest(const FString& Parameters)
{
    const FString Root = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_CIVIL_GRADE_FIXTURE"));
    if (!TestFalse(TEXT("An explicit disposable canonical fixture is required."), Root.IsEmpty())) return false;
    FProjectWorldCanonicalBundle Base, Candidate;
    FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("Original canonical bundle authenticates."), FProjectWorldCanonicalLoader::Load(
        FPaths::Combine(Root, TEXT("base/compile_result.json")), Base, Validation))) { AddError(Validation.Message); return false; }
    if (!TestTrue(TEXT("Refined canonical bundle authenticates."), FProjectWorldCanonicalLoader::Load(
        FPaths::Combine(Root, TEXT("candidate/refinement_result.json")), Candidate, Validation))) { AddError(Validation.Message); return false; }
    FProjectWorldAuthoredOverlaySet Overlays;
    FProjectWorldBuildingSettings Settings;
    FProjectWorldBuildingMeshBuildResult Original, Refined;
    FString Error;
    if (!TestTrue(TEXT("Original Building builds."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Base, Base.Cells[0], Overlays, Settings, Original, Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Dry anchor builds with its explicit immutable grade."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Candidate, Candidate.Cells[0], Overlays, Settings, Refined, Error))) { AddError(Error); return false; }
    TArray<FVector3f> A, B;
    for (const auto V : Original.MeshDescription.Vertices().GetElementIDs()) A.Add(FStaticMeshConstAttributes(Original.MeshDescription).GetVertexPositions()[V]);
    for (const auto V : Refined.MeshDescription.Vertices().GetElementIDs()) B.Add(FStaticMeshConstAttributes(Refined.MeshDescription).GetVertexPositions()[V]);
    TestTrue(TEXT("The existing bilinear Bounds.Center anchor and all Building vertices are retained."), !A.IsEmpty() && A == B);
    FProjectWorldRealizationLayer Layer;
    Layer.LayerId = TEXT("buildings"); Layer.GeneratorId = TEXT("project_building_massing"); Layer.GeneratorVersion = 1;
    Layer.NormalizedSettings = TEXT("{\"collision\":\"complex_as_simple\",\"conflict_policy\":\"reject_affected_fragments\",")
        TEXT("\"contained_policy\":\"associate_with_container\",\"duplicate_policy\":\"stable_feature_id\",")
        TEXT("\"maximum_height_m\":300,\"nanite\":true,\"navigation\":\"no_navigation\",")
        TEXT("\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\",\"topology_policy\":\"logical_building_classify_v2\"}");
    Layer = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
    FString Before, After;
    TestTrue(TEXT("Building grade identity hashes."), ProjectWorldBuildingRealization::HashCellInput(
        Candidate, Candidate.Cells[0], Layer, Overlays, Before, Error));
    Candidate.Cells[0].Terrain.ArtifactHash = FString::ChrN(64, 'e');
    TestTrue(TEXT("Unconsumed final wrapper does not stale the immutable anchor."), ProjectWorldBuildingRealization::HashCellInput(
        Candidate, Candidate.Cells[0], Layer, Overlays, After, Error));
    TestEqual(TEXT("Unconsumed final Terrain does not regenerate Building geometry."), Before, After);
    const auto Valid = Candidate;
    Candidate.Features[TEXT("alis:fixture:way:3")].CivilGrade.Reset();
    FProjectWorldBuildingMeshBuildResult Default;
    TestTrue(TEXT("Unreferenced Building keeps the final-surface query."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Candidate, Candidate.Cells[0], Overlays, Settings, Default, Error));
    TArray<FVector3f> DefaultVertices;
    for (const auto V : Default.MeshDescription.Vertices().GetElementIDs()) DefaultVertices.Add(FStaticMeshConstAttributes(Default.MeshDescription).GetVertexPositions()[V]);
    TestTrue(TEXT("The affine dry anchor is observably different from the accepted bilinear anchor."), A != DefaultVertices);
    Candidate = Valid;
    Candidate.Cells[0].Terrain.BaseArtifactHash = FString::ChrN(64, 'd');
    FProjectWorldBuildingMeshBuildResult Refused;
    TestFalse(TEXT("Stale base refuses Building hashing."), ProjectWorldBuildingRealization::HashCellInput(
        Candidate, Candidate.Cells[0], Layer, Overlays, After, Error));
    TestFalse(TEXT("Stale base refuses Building geometry."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Candidate, Candidate.Cells[0], Overlays, Settings, Refused, Error));
    TestEqual(TEXT("Stale grade emits no Building geometry."), Refused.MeshDescription.Triangles().Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingRefinedOwnerClampTest,
    "Project.World.Realization.Building.RefinedOwnerClampedAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBuildingRefinedOwnerClampTest::RunTest(const FString&)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
        TEXT("tmp/world/building_refined_owner_clamp"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
        TEXT("tools/World/CanonicalCompilation/tests/refinement_fixture.py")));
    int32 Code = -1; FString Output, Error;
    if (!TestTrue(TEXT("Public canonical owner creates the isolated two-cell surface."),
        FPlatformProcess::ExecProcess(TEXT("python.exe"), *FString::Printf(TEXT("-B \"%s\" \"%s\""), *Script, *Root),
            &Code, &Output, &Error) && Code == 0)) { AddError(Output + Error); return false; }
    FProjectWorldCanonicalBundle Bundle; FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("Real canonical loader authenticates the refined surface."), FProjectWorldCanonicalLoader::Load(
        FPaths::Combine(Root, TEXT("candidate/refinement_result.json")), Bundle, Validation))) { AddError(Validation.Message); return false; }
    auto& Cell = Bundle.Cells[0];
    FProjectWorldCanonicalFeature Feature;
    Feature.FeatureId = TEXT("fixture/building/outside-owner-center"); Feature.FeatureClass = TEXT("building");
    Feature.OwnerCellId = Cell.CellId; Feature.GeometryType = TEXT("Polygon"); Feature.HeightMeters = 6;
    FProjectWorldCanonicalPolygon Polygon;
    const double Boundary = Cell.Bounds.Z;
    Polygon.Outer = {FVector2D(Boundary-1,14), FVector2D(Boundary+3,14), FVector2D(Boundary+3,16),
        FVector2D(Boundary-1,16), FVector2D(Boundary-1,14)};
    Feature.GeometryPolygons = {Polygon};
    auto& Volume = Feature.BuildingVolumes.AddDefaulted_GetRef();
    Volume.VolumeId = TEXT("fixture/volume"); Volume.SourceFeatureId = Feature.FeatureId;
    Volume.GeometryType = Feature.GeometryType; Volume.GeometryPolygons = {Polygon}; Volume.HeightMeters = Feature.HeightMeters;
    Cell.OwnedFeatureIds = {Feature.FeatureId}; Bundle.Features.Add(Feature.FeatureId, Feature);
    const auto Bottom = [&](const FProjectWorldBuildingMeshBuildResult& Build)
    {
        double Minimum = MAX_dbl;
        const auto Positions = FStaticMeshConstAttributes(Build.MeshDescription).GetVertexPositions();
        for (const FVertexID V : Build.MeshDescription.Vertices().GetElementIDs()) Minimum = FMath::Min(Minimum, double(Positions[V].Z));
        return Minimum;
    };
    FProjectWorldBuildingMeshBuildResult Final;
    if (!TestTrue(TEXT("Cross-cell footprint builds through the actual Building owner."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Bundle, Cell, FProjectWorldAuthoredOverlaySet(), FProjectWorldBuildingSettings(), Final, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Building produces real mesh vertices."), Final.MeshDescription.Vertices().Num() > 0);
    TestEqual(TEXT("Raw center beyond the owner anchors at the refined shared edge."), Bottom(Final), 1000.0);
    FProjectWorldRealizationLayer Layer;
    Layer.LayerId = TEXT("buildings"); Layer.GeneratorId = TEXT("project_building_massing"); Layer.GeneratorVersion = 1;
    Layer.NormalizedSettings = TEXT("{\"collision\":\"complex_as_simple\",\"conflict_policy\":\"reject_affected_fragments\",")
        TEXT("\"contained_policy\":\"associate_with_container\",\"duplicate_policy\":\"stable_feature_id\",")
        TEXT("\"maximum_height_m\":300,\"nanite\":true,\"navigation\":\"no_navigation\",")
        TEXT("\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\",\"topology_policy\":\"logical_building_classify_v2\"}");
    Layer = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
    const auto Hash = [&](const FProjectWorldCanonicalBundle& Input, int32 CellIndex)
    {
        FString Value;
        TestTrue(TEXT("Building consumed inputs hash."), ProjectWorldBuildingRealization::HashCellInput(
            Input, Input.Cells[CellIndex], Layer, FProjectWorldAuthoredOverlaySet(), Value, Error));
        return Value;
    };
    Bundle.Cells[1].ReferencedFeatureIds = {Feature.FeatureId};
    Bundle.Cells[1].Terrain.RefinedQuads.Reset();
    auto GridOnly = Bundle;
    // Hold the opaque artifact key fixed to isolate the consumed query variant.
    GridOnly.Cells[0].Terrain.RefinedQuads.Reset();
    TestNotEqual(TEXT("Refined default anchor includes its changed query contract."), Hash(Bundle, 0), Hash(GridOnly, 0));
    TestNotEqual(TEXT("A cross-cell fragment keys the owner query, not its own unrefined Terrain."), Hash(Bundle, 1), Hash(GridOnly, 1));
    TestEqual(TEXT("Unrefined owner retains its pre-correction identity."), Hash(GridOnly, 0),
        FString(TEXT("c26b18cda35785085fd735341f59f8f4eb76aeee184d40727e8a26a3b91bc0a5")));
    TestEqual(TEXT("Unrefined fragment retains its pre-correction identity."), Hash(GridOnly, 1),
        FString(TEXT("e387a9284649f1055b300f33e89adfd25a6d14bd6326f68160f716f30e9d7946")));
    FProjectWorldCanonicalCivilGrade Grade; Grade.TerrainArtifacts.Add(Cell.CellId, Cell.Terrain.BaseArtifactHash);
    Bundle.Features[Feature.FeatureId].CivilGrade = MoveTemp(Grade);
    FProjectWorldBuildingMeshBuildResult Preserved;
    TestTrue(TEXT("Explicit civil grade still builds independently of the final edge."), ProjectWorldBuildingMeshBuilder::BuildCell(
        Bundle, Cell, FProjectWorldAuthoredOverlaySet(), FProjectWorldBuildingSettings(), Preserved, Error));
    TestEqual(TEXT("Explicit immutable grade retains the original Building elevation."), Bottom(Preserved), 1200.0);
    GridOnly.Features[Feature.FeatureId].CivilGrade = Bundle.Features[Feature.FeatureId].CivilGrade;
    TestEqual(TEXT("Unconsumed final query variant preserves immutable-grade identity."), Hash(Bundle, 0), Hash(GridOnly, 0));
    TestEqual(TEXT("Immutable-grade fragment identity is independent of the final owner query."), Hash(Bundle, 1), Hash(GridOnly, 1));
    TestEqual(TEXT("Immutable-grade owner retains its pre-correction identity."), Hash(Bundle, 0),
        FString(TEXT("e852ded03037d479c036a5495c0cb1cc3d60686824c7951dd32c3c6dc267ff86")));
    TestEqual(TEXT("Immutable-grade fragment retains its pre-correction identity."), Hash(Bundle, 1),
        FString(TEXT("f0f64ad177dde137cd00780fbe1e32ae74a4280c8379794975467639956bb992")));
    AddInfo(TEXT("RefinedOwnerClampFixture=") + Root);
    return true;
}
#endif
