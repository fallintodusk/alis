// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldVerifySurfaceFixture.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingGroundTest,
    "Project.World.Realization.Building.GroundFinalAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingGroundTest::RunTest(const FString&)
{
    FString Input=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_GROUND_SCENE_FIXTURE"));
    const bool bRealScene=!Input.IsEmpty();
    if (!bRealScene)
    {
        const FString Root=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
            TEXT("tmp/world/building_ground_tests"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        const FString Script=FPaths::Combine(FPaths::ProjectDir(),TEXT("tools/World/CanonicalCompilation/tests/ground_consumer_fixture.py"));
        int32 Code=-1;FString Out,Error;
        if (!TestTrue(TEXT("The shared Ground constructor creates its own disposable fixture."),
            FPlatformProcess::ExecProcess(TEXT("python.exe"),*FString::Printf(TEXT("-B \"%s\" \"%s\""),*Script,*Root),&Code,&Out,&Error) && Code==0))
        { AddError(Out+Error);return false; }
        Input=FPaths::Combine(Root,TEXT("road-ground-fixture.json"));
    }
    FString Json,Error;TSharedPtr<FJsonObject> Fixture;
    if (!TestTrue(TEXT("Typed source scene parses."),FFileHelper::LoadFileToString(Json,*Input) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Fixture))) return false;
    const FString OutputPath=FPaths::ConvertRelativePathToFull(Fixture->GetStringField(TEXT("building_output_path")));
    if (!TestTrue(TEXT("Proof uses a new file under repository tmp."),
        FPaths::IsUnderDirectory(OutputPath,FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp")))) &&
        !FPaths::FileExists(OutputPath))) return false;
    FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("The original base authenticates before typed replacement."),
        FProjectWorldCanonicalLoader::Load(Fixture->GetStringField(TEXT("base_result")),Bundle,Validation)))
    { AddError(Validation.Message);return false; }
    FString FeatureId;
    FVector2D Anchor;double SpecifiedGround=10;
    if (bRealScene)
    {
        FeatureId=Fixture->GetStringField(TEXT("building_id"));
        const auto XY=Fixture->GetArrayField(TEXT("building_anchor_xy"));Anchor=FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber());
        SpecifiedGround=Fixture->GetNumberField(TEXT("building_ground_m"));
    }
    else
    {
        FeatureId=TEXT("fixture:ground-building");Anchor=FVector2D(22.5,15);
        FProjectWorldCanonicalFeature Feature;Feature.FeatureId=FeatureId;Feature.FeatureClass=TEXT("building");
        Feature.OwnerCellId=Bundle.Cells[0].CellId;Feature.GeometryType=TEXT("Polygon");Feature.HeightMeters=6;
        FProjectWorldCanonicalPolygon Polygon;Polygon.Outer={FVector2D(21,14),FVector2D(24,14),FVector2D(24,16),FVector2D(21,16),FVector2D(21,14)};
        Feature.GeometryPolygons={Polygon};auto& Volume=Feature.BuildingVolumes.AddDefaulted_GetRef();
        Volume.VolumeId=TEXT("fixture/ground-volume");Volume.SourceFeatureId=FeatureId;Volume.GeometryType=TEXT("Polygon");
        Volume.GeometryPolygons={Polygon};Volume.HeightMeters=6;Bundle.Features.Add(FeatureId,MoveTemp(Feature));
    }
    if (!TestTrue(TEXT("The selected Building exists."),Bundle.Features.Contains(FeatureId))) return false;
    const auto& Feature=Bundle.Features[FeatureId];
    if (!TestTrue(TEXT("This ordinary Ground discriminator cannot override an explicit civil grade."),!Feature.CivilGrade.IsSet())) return false;
    for (auto& Cell:Bundle.Cells) { Cell.OwnedFeatureIds.Reset();Cell.ReferencedFeatureIds.Reset(); }
    auto* Owner=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Feature.OwnerCellId;});
    if (!TestNotNull(TEXT("Building owner exists."),Owner)) return false;
    Owner->OwnedFeatureIds={FeatureId};FProjectWorldBuildingMeshBuildResult Before,After;
    if (!TestTrue(TEXT("Original Building source description builds without saving."),ProjectWorldBuildingMeshBuilder::BuildCell(
        Bundle,*Owner,FProjectWorldAuthoredOverlaySet(),FProjectWorldBuildingSettings(),Before,Error))) { AddError(Error);return false; }
    if (!TestTrue(TEXT("Typed Ground is applied through the same test-only projection as Road."),
        ProjectWorldVerifyTestFlow::ApplyTypedTerrainFixture(Bundle,*Fixture,Error))) { AddError(Error);return false; }
    double Height=0;
    if (!ProjectWorldGeneratedGeometry::SampleTerrain(*Owner,Anchor.X,Anchor.Y,Height,Error)) { AddError(Error);return false; }
    TestTrue(TEXT("Native final anchor agrees with the independently specified Ground face."),FMath::Abs(Height-SpecifiedGround)<1e-8);
    if (!TestTrue(TEXT("Production Building consumes final Ground."),ProjectWorldBuildingMeshBuilder::BuildCell(
        Bundle,*Owner,FProjectWorldAuthoredOverlaySet(),FProjectWorldBuildingSettings(),After,Error))) { AddError(Error);return false; }
    const auto Minimum=[&](const FProjectWorldBuildingMeshBuildResult& Build)
    {
        double Min=MAX_dbl;const auto Positions=FStaticMeshConstAttributes(Build.MeshDescription).GetVertexPositions();
        for (const auto V:Build.MeshDescription.Vertices().GetElementIDs())
            Min=FMath::Min(Min,FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(Positions[V])+Build.ActorOrigin).Z);
        return Min;
    };
    TestTrue(TEXT("The Building has actual source faces."),After.TriangleCount>0);
    double MinimumHeight=MAX_dbl;for (const auto& V:Feature.BuildingVolumes) MinimumHeight=FMath::Min(MinimumHeight,V.MinHeightMeters);
    const double AnchorError=FMath::Abs(Minimum(After)-SpecifiedGround-MinimumHeight);
    TestTrue(TEXT("Actual Building bottom uses final Ground plus its source minimum height."),AnchorError<.001);
    for (auto& Quad:Owner->Terrain.RefinedQuads) Quad.Triangles.Reset();
    FProjectWorldBuildingMeshBuildResult Refused;
    TestFalse(TEXT("Missing final anchor coverage refuses instead of falling back to DSM."),ProjectWorldBuildingMeshBuilder::BuildCell(
        Bundle,*Owner,FProjectWorldAuthoredOverlaySet(),FProjectWorldBuildingSettings(),Refused,Error));
    TestEqual(TEXT("Refusal emits no partial Building."),Refused.MeshDescription.Triangles().Num(),0);
    auto Result=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Vertices,Faces;TMap<FVertexID,int32> Map;
    const auto Positions=FStaticMeshConstAttributes(After.MeshDescription).GetVertexPositions();
    for (const auto V:After.MeshDescription.Vertices().GetElementIDs())
    {
        const auto P=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(Positions[V])+After.ActorOrigin);
        TArray<TSharedPtr<FJsonValue>> XYZ;for (int32 I=0;I<3;++I) XYZ.Add(MakeShared<FJsonValueNumber>(P[I]));
        Map.Add(V,Vertices.Num());Vertices.Add(MakeShared<FJsonValueArray>(XYZ));
    }
    for (const auto T:After.MeshDescription.Triangles().GetElementIDs())
    {
        TArray<TSharedPtr<FJsonValue>> Row;for (int32 I=0;I<3;++I)
            Row.Add(MakeShared<FJsonValueNumber>(Map[After.MeshDescription.GetVertexInstanceVertex(After.MeshDescription.GetTriangleVertexInstance(T,I))]));
        Faces.Add(MakeShared<FJsonValueArray>(Row));
    }
    Result->SetArrayField(TEXT("vertices"),Vertices);Result->SetArrayField(TEXT("triangles"),Faces);
    Result->SetStringField(TEXT("feature_id"),FeatureId);Result->SetNumberField(TEXT("ground_m"),Height);
    Result->SetNumberField(TEXT("before_bottom_m"),Minimum(Before));Result->SetNumberField(TEXT("after_bottom_m"),Minimum(After));
    Result->SetNumberField(TEXT("anchor_error_m"),AnchorError);
    Result->SetStringField(TEXT("scope"),TEXT("Native source Building and typed Ground anchor only; not persistent integration, saved collision or packaged proof"));
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    TestTrue(TEXT("Native scene proof writes only its disposable output."),FFileHelper::SaveStringToFile(Text,*OutputPath));
    AddInfo(FString::Printf(TEXT("GroundBuildingProof vertices=%d faces=%d ground_m=%.12g anchor_error_m=%.12g"),Vertices.Num(),Faces.Num(),Height,AnchorError));
    return true;
}
#endif
