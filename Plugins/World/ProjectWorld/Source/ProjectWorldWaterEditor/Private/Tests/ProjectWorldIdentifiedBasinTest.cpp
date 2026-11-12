// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterSourceApproximation.h"
#include "ProjectWorldWaterLayerProducer.h"
#include "ProjectWorldWaterJointBoundary.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldVerify.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldIdentifiedBasinTests
{
    FString FaceKey(const FVector3f Points[3])
    {
        FString Keys[3];
        for (int32 I=0;I<3;++I) Keys[I]=FString::Printf(TEXT("%.9g,%.9g,%.9g"),Points[I].X,Points[I].Y,Points[I].Z);
        int32 Start=0; for (int32 I=1;I<3;++I) if (Keys[I]<Keys[Start]) Start=I;
        return Keys[Start]+TEXT("|")+Keys[(Start+1)%3]+TEXT("|")+Keys[(Start+2)%3];
    }
    struct FScratch
    {
        FString Root, Mount, Directory, PackageName;
        FAutomationTestBase* Test=nullptr;
        bool Unload(UPackage* Package)
        {
            UPackageTools::FUnloadPackageParams Params({Package}); Params.bResetTransBuffer=false;
            return UPackageTools::UnloadPackages(Params);
        }
        ~FScratch()
        {
            if (!PackageName.IsEmpty()) if (auto* Package=FindPackage(nullptr,*PackageName))
                if (Test) Test->TestTrue(TEXT("Disposable Water package unloads."),Unload(Package));
            if (!Mount.IsEmpty()) FPackageName::UnRegisterMountPoint(Mount,Directory);
        }
    };
    bool Read(FAutomationTestBase& Test,FScratch& Scratch,FProjectWorldCanonicalBundle& Bundle,FString& Hash)
    {
        const FString Tmp=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"));
        Scratch.Root=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_IDENTIFIED_BASIN_OUTPUT"));
        if (Scratch.Root.IsEmpty()) Scratch.Root=Tmp/TEXT("world/identified_basin")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Scratch.Root=FPaths::ConvertRelativePathToFull(Scratch.Root);
        if (!Test.TestTrue(TEXT("All basin evidence is fresh project scratch."),FPaths::IsUnderDirectory(Scratch.Root,Tmp) &&
            !IFileManager::Get().DirectoryExists(*Scratch.Root) && IFileManager::Get().MakeDirectory(*Scratch.Root,true))) return false;
        FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_IDENTIFIED_BASIN_FIXTURE"));
        if (Path.IsEmpty())
        {
            const FString Destination=Scratch.Root/TEXT("input");
            const FString Script=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tools/World/CanonicalCompilation/tests/identified_basin_mesh_fixture.py"));
            int32 Code=-1; FString Out,Error;
            if (!Test.TestTrue(TEXT("The permanent source-shaped pure model supplies the typed fixture."),
                FPlatformProcess::ExecProcess(TEXT("python.exe"),*FString::Printf(TEXT("-B \"%s\" \"%s\""),*Script,*Destination),&Code,&Out,&Error) && Code==0))
            {Test.AddError(Out+Error);return false;}
            Path=Destination/TEXT("composite-terrain-fixture.json");
        }
        else
        {
            const FString Expected=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_IDENTIFIED_BASIN_SHA256"));
            if (!Test.TestTrue(TEXT("An external typed fixture is scratch-local and byte authenticated."),
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),Tmp) && Expected.Len()==64 &&
                FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash) && Hash==Expected)) return false;
        }
        FString Text; TSharedPtr<FJsonObject> Doc;
        if (!Test.TestTrue(TEXT("The complete inferred input is read and hashed."),
            FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash) && FFileHelper::LoadFileToString(Text,*Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Doc))) return false;
        if (!Test.TestTrue(TEXT("Source frame and reduced original support are explicit."),
            Doc->GetStringField(TEXT("canonical_crs"))==TEXT("EPSG:32618") && Doc->GetStringField(TEXT("vertical_datum"))==TEXT("EPSG:3855") &&
            Doc->GetArrayField(TEXT("quads")).Num()==2 && !Doc->GetStringField(TEXT("reduction")).IsEmpty())) return false;
        const auto& Origin=Doc->GetArrayField(TEXT("engine_georeference_origin_m"));
        Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.CoordinateTransform=TEXT("captured_typed_inferred_basin");
        Bundle.EngineGeoreferenceOriginMeters=FVector2D(Origin[0]->AsNumber(),Origin[1]->AsNumber());
        Bundle.HeightOriginMeters=Doc->GetNumberField(TEXT("height_origin_m"));
        Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
        auto& Cell=Bundle.Cells.AddDefaulted_GetRef(); Cell.CellId=Doc->GetStringField(TEXT("cell_id"));
        const auto& B=Doc->GetArrayField(TEXT("bounds")); Cell.Bounds=FVector4d(B[0]->AsNumber(),B[1]->AsNumber(),B[2]->AsNumber(),B[3]->AsNumber());
        FString Error;
        if (!ProjectWorldBasinFixture::CheckRefusals(*Doc,Bundle,Test)) return false;
        if (!ProjectWorldBasinFixture::AddWater(*Doc,Bundle,Error)) {Test.AddError(Error);return false;}
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldIdentifiedBasinTypedTest,
    "Project.World.Water.IdentifiedBasinTypedConsumer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldIdentifiedBasinTypedTest::RunTest(const FString&)
{
    using namespace ProjectWorldIdentifiedBasinTests;
    FScratch Scratch; Scratch.Test=this; FProjectWorldCanonicalBundle Bundle; FString Hash,Error;
    if (!Read(*this,Scratch,Bundle,Hash)) return false;
    const auto& Cell=Bundle.Cells[0]; const FString Id=Cell.OwnedFeatureIds[0]; auto& Feature=Bundle.Features[Id];
    const double Level=Feature.WaterSurface.LevelMeters;
    TestEqual(TEXT("The identified contained stage has no presentation lift."),ProjectWorldWaterMeshBuilder::PresentationOffset(Feature,.25),0.);
    FProjectWorldPreparedWaterSurface Prepared;
    if (!TestTrue(TEXT("The actual45-node basin prepares natively."),ProjectWorldWaterMeshBuilder::PrepareSurface(Feature,Cell.Bounds,Prepared,Error)))
    {AddError(Error);return false;}
    FProjectWorldWaterMeshBuildResult Source;
    if (!TestTrue(TEXT("The native Water source builder emits the declared stage."),
        ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Feature,Prepared,.25,Source,Error))) {AddError(Error);return false;}
    TestTrue(TEXT("The source stage proof has positive vertices and triangles."),
        Source.TriangleCount>0 && Source.MeshDescription.Vertices().Num()>0 && Source.MeshDescription.Triangles().Num()==Source.TriangleCount);
    const float Expected=FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(0,0,Level))-Source.ActorOrigin).Z;
    const auto Positions=FStaticMeshConstAttributes(Source.MeshDescription).GetVertexPositions();
    int32 SourceMismatches=0;
    for (const auto Vertex : Source.MeshDescription.Vertices().GetElementIDs())
        SourceMismatches+=Positions[Vertex].Z!=Expected ? 1 : 0;
    TestEqual(TEXT("Every actual source vertex stores the zero-lift stage."),SourceMismatches,0);
    const FProjectWorldRealizationProfile Profile; const FProjectWorldRealizationLayer Layer; const FProjectWorldAuthoredOverlaySet Overlays;
    const auto UnitHash=[&]()
    {
        FProjectWorldLayerUnitInputs Units;
        TestTrue(TEXT("Typed Water unit inputs are hashable."),FProjectWorldWaterLayerProducer::Get().HashUnitInputs({Bundle,Profile,Layer,Overlays},Units,Error));
        return Units.HashByUnit.FindRef(Cell.CellId);
    };
    const auto VerifyHash=[&]()
    {
        FProjectWorldVerifyIdentity Identity; Identity.AddFixtureBundle(TEXT("identified"),Bundle);
        return Identity.FixtureInputs[0].Value;
    };
    const FString UnitBefore=UnitHash(), VerifyBefore=VerifyHash();
    const double Spill=*Feature.WaterSurface.ContainmentSpillMeters;
    Feature.WaterSurface.ContainmentSpillMeters=Spill+.01;
    TestNotEqual(TEXT("The inferred spill is a consumed unit dependency."),UnitBefore,UnitHash());
    TestNotEqual(TEXT("The inferred spill is a consumed verification dependency."),VerifyBefore,VerifyHash());
    Feature.WaterSurface.ContainmentSpillMeters=Spill;
    const FVector BadOrigins[]={FVector(std::numeric_limits<double>::quiet_NaN(),0,0),FVector(1,0,0)};
    for (const auto& BadOrigin : BadOrigins)
    {
        FProjectWorldWaterMeshBuildResult Refused=Source;
        TestFalse(TEXT("The joint storage origin cannot be nonfinite or an unqualified private frame."),
            ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Feature,Prepared,.25,Refused,Error,&BadOrigin));
        TestEqual(TEXT("Origin refusal clears the previous mesh."),Refused.MeshDescription.Triangles().Num(),0);
    }
    for (int32 Case=0;Case<4;++Case)
    {
        auto Bad=Feature;
        if (Case==0) Bad.WaterSurface.FunctionVersion=2;
        if (Case==1) Bad.WaterSurface.ContainmentSpillMeters.Reset();
        if (Case==2) Bad.WaterSurface.ContainmentSpillMeters=std::numeric_limits<double>::quiet_NaN();
        if (Case==3) Bad.WaterSurface.ContainmentSpillMeters=Level;
        FProjectWorldWaterMeshBuildResult Refused=Source;
        TestFalse(TEXT("Unknown version or absent/nonfinite/exhausted containment refuses."),
            ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Bad,Prepared,.25,Refused,Error));
        TestEqual(TEXT("Containment refusal leaves no partial mesh."),Refused.MeshDescription.Triangles().Num(),0);
    }
    FMeshDescription Combined; FVector ActorOrigin; FString Semantic; int32 Triangles=0;
    TMap<FString,FProjectWorldPreparedWaterSurface> PreparedMap; PreparedMap.Add(Id,Prepared);
    if (!TestTrue(TEXT("Actual cell composition preserves the declared stage."),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,PreparedMap,{Id},.25,Combined,ActorOrigin,Semantic,Triangles,Error))) {AddError(Error);return false;}
    bool Changed=false; FString ApproximationHash; TArray<const FProjectWorldCanonicalFeature*> Sources; Sources.Init(&Feature,Triangles);
    TestTrue(TEXT("Source approximation consumes the same exact-stage semantics."),
        ProjectWorldWaterSourceApproximation::Refine(Combined,Sources,Bundle,Cell,ActorOrigin,.25,Changed,ApproximationHash,Error) && !Changed);
    Scratch.Mount=TEXT("/ProjectWorldIdentifiedBasin")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("/");
    Scratch.Directory=Scratch.Root/TEXT("packages/"); IFileManager::Get().MakeDirectory(*Scratch.Directory,true);
    FPackageName::RegisterMountPoint(Scratch.Mount,Scratch.Directory); Scratch.PackageName=Scratch.Mount+TEXT("SM_Water");
    auto* Package=CreatePackage(*Scratch.PackageName); auto* Mesh=NewObject<UStaticMesh>(Package,TEXT("Surface"),RF_Public|RF_Standalone);
    Mesh->bAllowCPUAccess=true; Mesh->bGenerateMeshDistanceField=false; Mesh->bHasNavigationData=false;
    Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface))); Mesh->SetNumSourceModels(1);
    UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bUseHashAsGuid=true;
    if (!TestTrue(TEXT("The actual composed Water builds through StaticMesh."),Mesh->BuildFromMeshDescriptions({&Combined},Params))) return false;
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    const FString Filename=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    if (!TestTrue(TEXT("Water saves only under isolated scratch."),UPackage::SavePackage(Package,Mesh,*Filename,Args))) return false;
    const TWeakObjectPtr<UStaticMesh> Before(Mesh); Mesh=nullptr;
    if (!TestTrue(TEXT("Water unloads before saved proof."),Scratch.Unload(Package)) ||
        !TestFalse(TEXT("The in-memory original cannot stand in for saved Water."),Before.IsValid())) return false;
    Mesh=LoadObject<UStaticMesh>(nullptr,*(Scratch.PackageName+TEXT(".Surface")));
    if (!TestNotNull(TEXT("Saved Water reloads."),Mesh)) return false;
    const auto& LOD=Mesh->GetRenderData()->LODResources[0];
    TestTrue(TEXT("Saved Water has positive indexed geometry."),LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices()>0 && LOD.IndexBuffer.GetNumIndices()>0);
    TestEqual(TEXT("Every composed triangle survives save/reload."),LOD.IndexBuffer.GetNumIndices(),Combined.Triangles().Num()*3);
    TArray<TSharedPtr<FJsonValue>> Vertices,Indices;
    TArray<FString> SourceFaces,SavedFaces;
    const auto CombinedPositions=FStaticMeshConstAttributes(Combined).GetVertexPositions();
    for (const auto Triangle : Combined.Triangles().GetElementIDs())
    {
        FVector3f Points[3]; const auto Instances=Combined.GetTriangleVertexInstances(Triangle);
        for (int32 J=0;J<3;++J) Points[J]=CombinedPositions[Combined.GetVertexInstanceVertex(Instances[J])];
        SourceFaces.Add(FaceKey(Points));
    }
    int32 SavedMismatches=0;
    for (uint32 I=0;I<LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();++I)
    {
        const FVector3f P=LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I);
        SavedMismatches+=P.Z!=Expected ? 1 : 0;
        TArray<TSharedPtr<FJsonValue>> Values; for (float V : {P.X,P.Y,P.Z}) Values.Add(MakeShared<FJsonValueNumber>(V));
        Vertices.Add(MakeShared<FJsonValueArray>(Values));
    }
    for (int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3)
    {
        FVector3f Points[3];
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 J=0;J<3;++J)
        {
            const auto Vertex=LOD.IndexBuffer.GetIndex(I+J); Values.Add(MakeShared<FJsonValueNumber>(Vertex));
            Points[J]=LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Vertex);
        }
        Indices.Add(MakeShared<FJsonValueArray>(Values));
        SavedFaces.Add(FaceKey(Points));
    }
    SourceFaces.Sort(); SavedFaces.Sort();
    TestTrue(TEXT("Every oriented indexed XYZ triangle survives vertex/index reordering exactly."),SourceFaces==SavedFaces);
    TestEqual(TEXT("Every saved CPU vertex retains the declared stored stage."),SavedMismatches,0);
    auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("scope"),TEXT("Actual inferred typed basin source/cell/saved CPU Water stage; no production parser, material/render, joint stored shoreline, physics or geographic acceptance."));
    Receipt->SetStringField(TEXT("fixture_sha256"),Hash); Receipt->SetNumberField(TEXT("canonical_level_m"),Level);
    Receipt->SetNumberField(TEXT("canonical_spill_m"),Spill); Receipt->SetNumberField(TEXT("stored_local_z_cm"),Expected);
    Receipt->SetNumberField(TEXT("source_stage_mismatches"),SourceMismatches); Receipt->SetNumberField(TEXT("saved_stage_mismatches"),SavedMismatches);
    TArray<TSharedPtr<FJsonValue>> Origin; for (double V : {ActorOrigin.X,ActorOrigin.Y,ActorOrigin.Z}) Origin.Add(MakeShared<FJsonValueNumber>(V));
    Receipt->SetArrayField(TEXT("actor_origin_cm"),Origin); Receipt->SetArrayField(TEXT("vertices_cm"),Vertices); Receipt->SetArrayField(TEXT("triangles"),Indices);
    FString Text; FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
    TestTrue(TEXT("Whole saved Water geometry is retained for independent joint analysis."),FFileHelper::SaveStringToFile(Text,*(Scratch.Root/TEXT("compiled-water.json"))));
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldIdentifiedBasinTypedTest,
    "Project.World.Water.IdentifiedBasinTypedConsumer","[Fast][Unit][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldIdentifiedBasinCanonicalXYTest,
    "Project.World.Water.IdentifiedBasinCanonicalXY",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldIdentifiedBasinCanonicalXYTest::RunTest(const FString&)
{
    using namespace ProjectWorldIdentifiedBasinTests;
    FScratch Scratch; Scratch.Test=this; FProjectWorldCanonicalBundle Bundle; FString Hash,Error;
    if (!Read(*this,Scratch,Bundle,Hash)) return false;
    const auto& Cell=Bundle.Cells[0]; const auto& Feature=Bundle.Features[Cell.OwnedFeatureIds[0]];
    FProjectWorldWaterMeshBuildResult Result;
    if (!TestTrue(TEXT("The actual joint-model Water builds."),ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle,Cell,Feature,.25,Result,Error)))
    {AddError(Error);return false;}
    TSet<FVector3f> Expected,Actual;
    for (const auto& P : Feature.GeometryPolygons[0].Outer)
        Expected.Add(FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(P.X,P.Y,Feature.WaterSurface.LevelMeters))-Result.ActorOrigin));
    const auto Positions=FStaticMeshConstAttributes(Result.MeshDescription).GetVertexPositions();
    for (const auto Vertex : Result.MeshDescription.Vertices().GetElementIDs()) Actual.Add(Positions[Vertex]);
    int32 Missing=0,Unexpected=0;
    for (const auto& P : Expected) Missing+=Actual.Contains(P) ? 0 : 1;
    for (const auto& P : Actual) Unexpected+=Expected.Contains(P) ? 0 : 1;
    TestEqual(TEXT("Every specifying boundary node survives only its declared float storage conversion."),Missing,0);
    TestEqual(TEXT("Native emission adds no centimeter-snapped replacement nodes."),Unexpected,0);
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldIdentifiedBasinCanonicalXYTest,
    "Project.World.Water.IdentifiedBasinCanonicalXY","[Fast][Unit][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldIdentifiedBasinSharedFrameTest,
    "Project.World.Water.IdentifiedBasinSharedFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldIdentifiedBasinSharedFrameTest::RunTest(const FString&)
{
    using namespace ProjectWorldIdentifiedBasinTests;
    FScratch Scratch; Scratch.Test=this; FProjectWorldCanonicalBundle Bundle; FString Hash,Error;
    if (!Read(*this,Scratch,Bundle,Hash)) return false;
    auto& Cell=Bundle.Cells[0]; const FString JointId=Cell.OwnedFeatureIds[0]; const auto Joint=Bundle.Features[JointId];
    auto Observed=Joint,Ordinary=Joint;
    Observed.FeatureId=TEXT("typed/a_observed_control"); Observed.WaterSurface.FunctionId=TEXT("contained_ground_quantile");
    Ordinary.FeatureId=TEXT("typed/b_ordinary_control"); Ordinary.WaterSurface.FunctionId=TEXT("standing_polygon_quantile");
    const auto Rectangle=[](double X)
    {return FProjectWorldCanonicalPolygon{{{X,4512938.},{X+1,4512938.},{X+1,4512939.},{X,4512939.}}, {}};};
    Observed.GeometryPolygons={Rectangle(586185.)}; Ordinary.GeometryPolygons={Rectangle(586188.)};
    Bundle.Features.Add(Observed.FeatureId,Observed); Bundle.Features.Add(Ordinary.FeatureId,Ordinary);
    Cell.OwnedFeatureIds={Observed.FeatureId,Ordinary.FeatureId,JointId};
    TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
    for (const auto& Id : Cell.OwnedFeatureIds)
        if (!TestTrue(TEXT("Every disjoint control and actual basin prepares."),
            ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features[Id],Cell.Bounds,Prepared.Add(Id),Error))) return false;
    FMeshDescription Mesh; FVector Origin; FString Semantic; int32 Triangles=0;
    if (!TestTrue(TEXT("Mixed ordinary/observed/inferred groups compose in one frame."),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Cell.OwnedFeatureIds,.25,Mesh,Origin,Semantic,Triangles,Error)))
    {AddError(Error);return false;}
    TestEqual(TEXT("A joint-model unit uses the same engine storage frame as its supporting Terrain."),Origin,FVector::ZeroVector);
    TSet<FVector3f> Actual;
    const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
    for (const auto Vertex : Mesh.Vertices().GetElementIDs()) Actual.Add(Positions[Vertex]);
    int32 Missing=0;
    for (const auto& Id : Cell.OwnedFeatureIds)
    {
        const auto& Feature=Bundle.Features[Id];
        const double Level=Feature.WaterSurface.LevelMeters+(Id==Ordinary.FeatureId ? .25 : 0.);
        for (const auto& P : Feature.GeometryPolygons[0].Outer)
        {
            const FVector3f Expected(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(P.X,P.Y,Level)));
            Missing+=Actual.Contains(Expected) ? 0 : 1;
        }
    }
    TestEqual(TEXT("Observed-first equal planes cannot resnap the joint boundary or mix actor frames."),Missing,0);
    const FMeshDescription Complete=Mesh;
    // An unsupported interacting source must refuse before final coplanar union
    // can replace a joint boundary under another source's coordinate policy.
    Bundle.Features[Observed.FeatureId].GeometryPolygons=Joint.GeometryPolygons;
    Prepared[Observed.FeatureId]=Prepared[JointId];
    TestFalse(TEXT("Positive overlap involving a joint boundary requires an explicit relationship."),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Cell.OwnedFeatureIds,.25,Mesh,Origin,Semantic,Triangles,Error));
    TestEqual(TEXT("Refused mixed overlap leaves no partial output."),Mesh.Triangles().Num(),0);
    // The narrow joint-cell variant also refuses an interacting old-old pair;
    // it never drops a source or runs a union that can rewrite the joint loop.
    Bundle.Features[Observed.FeatureId]=Observed;
    Bundle.Features[Ordinary.FeatureId].GeometryPolygons=Observed.GeometryPolygons;
    for (const auto& Id : Cell.OwnedFeatureIds)
        if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features[Id],Cell.Bounds,Prepared[Id],Error)) return false;
    Mesh=Complete;
    TestFalse(TEXT("A joint unit cannot hide old-old overlap in final composition."),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Cell.OwnedFeatureIds,.25,Mesh,Origin,Semantic,Triangles,Error));
    TestEqual(TEXT("Old-old overlap refuses the complete unit."),Mesh.Triangles().Num(),0);
    // These actual prepared triangles are disjoint, but the observed family's
    // existing centimeter encoding moves its edge across the preserved joint.
    const double X=586185.,Y=4512920.;
    Bundle.Features[JointId].GeometryPolygons={FProjectWorldCanonicalPolygon{{{X,Y},{X+1.004,Y},{X+1.004,Y+1}}, {}}};
    Bundle.Features[Observed.FeatureId].GeometryPolygons={FProjectWorldCanonicalPolygon{
        {{X+1.0041,Y+.1},{X+2,Y+.1},{X+1.0041,Y+.9}}, {}}};
    Cell.OwnedFeatureIds={JointId,Observed.FeatureId}; TArray<FVector2D> Canonical;
    for (const auto& Id : Cell.OwnedFeatureIds)
    {
        if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features[Id],Cell.Bounds,Prepared[Id],Error)) return false;
        Canonical.Append(Prepared[Id].TriangleVertices);
    }
    TestTrue(TEXT("Encoding-overlap control starts with certified disjoint prepared triangles."),
        ProjectWorldWaterJointBoundary::ValidateIndependent(Canonical,Error));
    Mesh=Complete;
    TestFalse(TEXT("Encoded overlap refuses even after the canonical preflight passes."),
        ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Cell.OwnedFeatureIds,.25,Mesh,Origin,Semantic,Triangles,Error));
    TestTrue(TEXT("The stored-face overlap is the reason for refusal."),Error.Contains(TEXT("positive-area overlap")));
    TestEqual(TEXT("Encoded overlap clears geometry."),Mesh.Triangles().Num(),0);
    TestEqual(TEXT("Encoded overlap clears its count."),Triangles,0);
    TestTrue(TEXT("Encoded overlap clears semantic output."),Semantic.IsEmpty());
    // The joint group emits first, then a later disjoint old group discovers
    // missing preparation evidence. Failure must remove the earlier geometry.
    FProjectWorldCanonicalBundle Late=Bundle; Late.Features.Reset();
    auto First=Joint,LastA=Observed,LastB=Observed;
    First.FeatureId=TEXT("typed/a_joint"); LastA.FeatureId=TEXT("typed/z_old_a"); LastB.FeatureId=TEXT("typed/z_old_b");
    LastA.GeometryPolygons={Rectangle(586185.)}; LastB.GeometryPolygons={Rectangle(586188.)};
    Late.Features.Add(First.FeatureId,First); Late.Features.Add(LastA.FeatureId,LastA); Late.Features.Add(LastB.FeatureId,LastB);
    auto& LateCell=Late.Cells[0]; LateCell.OwnedFeatureIds={First.FeatureId,LastA.FeatureId,LastB.FeatureId};
    TMap<FString,FProjectWorldPreparedWaterSurface> LatePrepared;
    for (const auto& Id : LateCell.OwnedFeatureIds)
        if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Late.Features[Id],LateCell.Bounds,LatePrepared.Add(Id),Error)) return false;
    LatePrepared[LastB.FeatureId].Footprints.Reset();
    Mesh=Complete;
    TestFalse(TEXT("Missing late preparation evidence refuses after an earlier joint group."),
        ProjectWorldWaterCellMesh::Build(Late,LateCell,LatePrepared,LateCell.OwnedFeatureIds,.25,Mesh,Origin,Semantic,Triangles,Error));
    TestTrue(TEXT("The late footprint evidence is the refusal cause."),Error.Contains(TEXT("original water footprints")));
    TestEqual(TEXT("Late dependent failure removes all earlier geometry."),Mesh.Triangles().Num(),0);
    TestEqual(TEXT("Late dependent failure clears the output count."),Triangles,0);
    TestTrue(TEXT("Late dependent failure clears semantic output."),Semantic.IsEmpty());
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldIdentifiedBasinSharedFrameTest,
    "Project.World.Water.IdentifiedBasinSharedFrame","[Fast][Unit][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterIndependentBoundaryTest,
    "Project.World.Water.IndependentBoundaryGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterIndependentBoundaryTest::RunTest(const FString&)
{
    const TArray<FVector2D> First={{0,0},{1,0},{0,1}};
    TArray<FVector2D> Touch=First; Touch.Append({{0,1},{1,1},{1,0}}); FString Error;
    TestTrue(TEXT("Exact shared-edge contacts and opposite triangle winding are allowed."),
        ProjectWorldWaterJointBoundary::ValidateIndependent(Touch,Error));
    auto Duplicate=First; Duplicate.Append(First);
    TestFalse(TEXT("Positive duplicate area refuses."),ProjectWorldWaterJointBoundary::ValidateIndependent(Duplicate,Error));
    auto Unknown=First; Unknown[0].X=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Unknown coordinates cannot certify independence."),ProjectWorldWaterJointBoundary::ValidateIndependent(Unknown,Error));
    TestFalse(TEXT("A partial triangle census cannot certify independence."),
        ProjectWorldWaterJointBoundary::ValidateIndependent(MakeArrayView(Touch).Left(4),Error));
    const TArray<FVector2D> Collapsed={{0,0},{1,0},{2,0}};
    TestFalse(TEXT("Degenerate orientation is unknown rather than disjoint."),ProjectWorldWaterJointBoundary::ValidateIndependent(Collapsed,Error));
    TArray<FVector2D> Many;
    for (int32 I=0;I<1420;++I) Many.Append({{double(3*I),0},{double(3*I+1),0},{double(3*I),1}});
    TestFalse(TEXT("Pair capacity bounds even entirely bbox-pruned scans."),ProjectWorldWaterJointBoundary::ValidateIndependent(Many,Error));
    TestTrue(TEXT("The pair-work guard reports its actual refusal."),Error.Contains(TEXT("pair-scan capacity")));
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldWaterIndependentBoundaryTest,
    "Project.World.Water.IndependentBoundaryGuard","[Fast][Unit][World]")
#endif
