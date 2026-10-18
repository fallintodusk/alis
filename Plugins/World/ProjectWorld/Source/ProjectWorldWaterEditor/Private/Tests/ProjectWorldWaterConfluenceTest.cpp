// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterSourceContext.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldWaterRealization.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldWaterConfluenceTest
{
    bool Json(const FString& Path,TSharedPtr<FJsonObject>& Out)
    {
        FString Text;
        return FFileHelper::LoadFileToString(Text,*Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out);
    }
    TArray<FProjectWorldSurfaceTriangle> Faces(const FMeshDescription& Mesh)
    {
        TArray<FProjectWorldSurfaceTriangle> Out;
        const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        for (const auto Id : Mesh.Triangles().GetElementIDs())
        {
            auto& Face=Out.AddDefaulted_GetRef();
            const auto Instances=Mesh.GetTriangleVertexInstances(Id);
            for (int32 I=0;I<3;++I) Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Instances[I])]);
        }
        return Out;
    }
    double Contacts(const TArray<FProjectWorldSurfaceTriangle>& Input)
    {
        double Area=0;
        for (int32 A=0;A<Input.Num();++A) for (int32 B=A+1;B<Input.Num();++B)
        {
            TArray<FVector2D> Polygon;FString Error;
            if (!ProjectWorldTerrainSurface::IntersectXY(Input[A],Input[B],Polygon,Error))
                return TNumericLimits<double>::Max();
            Area+=ProjectWorldTerrainSurface::Area(Polygon)/10000;
        }
        return Area;
    }
    TArray<TSharedPtr<FJsonValue>> Points(const TArray<FProjectWorldSurfaceTriangle>& Input,
        const FProjectWorldCanonicalBundle& Bundle,const FVector& Origin)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const auto& Face : Input)
        {
            TArray<TSharedPtr<FJsonValue>> Corners;
            for (const auto& Local : Face.Points)
            {
                const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Local+Origin);
                Corners.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                    MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
            }
            Out.Add(MakeShared<FJsonValueArray>(Corners));
        }
        return Out;
    }
    FProjectWorldCanonicalFeature Channel(const FString& Id,const TArray<FVector>& Knots)
    {
        FProjectWorldCanonicalFeature F;F.FeatureId=Id;F.FeatureClass=TEXT("water");F.WidthMeters=1;
        F.GeometryType=TEXT("LineString");
        for (const auto& P : Knots) F.GeometryPoints.Add(FVector2D(P.X,P.Y));
        F.GeometryParts.Add(F.GeometryPoints);
        auto& S=F.WaterSurface;S.bValid=true;S.Geometry=TEXT("ribbon");S.Behavior=TEXT("flowing");
        S.Role=TEXT("flow_axis");S.SurfaceGroupId=Id;S.SurfaceGroupMembers={Id};
        S.FunctionId=TEXT("rolling_quantile_l1_isotonic");S.FunctionVersion=1;S.Knots=Knots;
        return F;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterConfluenceTest,
    "Project.World.Water.ConstrainedConfluence",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterConfluenceTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterConfluenceTest;
    FProjectWorldCanonicalBundle Base,Candidate;
    FString Tributary=TEXT("water/tributary"),Receiver=TEXT("water/receiver"),Output;
    const FString Fixture=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_WATER_CONFLUENCE_FIXTURE"));
    if (!Fixture.IsEmpty())
    {
        const FString Scratch=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"));
        TSharedPtr<FJsonObject> Request;
        if (!TestTrue(TEXT("Optional native evidence fixture is confined to scratch"),
            FPaths::IsUnderDirectory(Fixture,Scratch) && Json(Fixture,Request))) return false;
        for (const FString Name : {FString(TEXT("base")),FString(TEXT("candidate"))})
        {
            const FString Path=Request->GetStringField(Name+TEXT("_result"));FString Digest;
            if (!TestTrue(TEXT("Exact candidate/base receipt pin authenticates"),
                FPaths::IsUnderDirectory(Path,Scratch) && FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Digest) &&
                Digest==Request->GetStringField(Name+TEXT("_sha256")))) return false;
            FProjectWorldCanonicalValidation Validation;
            if (!TestTrue(TEXT("Native loader authenticates every canonical output"),
                FProjectWorldCanonicalLoader::Load(Path,Name==TEXT("base")?Base:Candidate,Validation)))
            {AddError(Validation.Message);return false;}
        }
        Tributary=Request->GetStringField(TEXT("tributary_id"));Receiver=Request->GetStringField(TEXT("receiver_id"));
        Output=Request->GetStringField(TEXT("output"));
        if (!TestTrue(TEXT("Evidence output stays in scratch"),FPaths::IsUnderDirectory(Output,Scratch))) return false;
    }
    else
    {
        Base.CoordinateQuantizationMeters=.01;Base.HeightQuantizationMeters=.1;
        FProjectWorldCanonicalCell Cell;Cell.CellId=TEXT("fixture:x0:y0");Cell.Bounds=FVector4d(0,0,100,100);
        Cell.OwnedFeatureIds={Tributary,Receiver};Base.Cells.Add(Cell);
        Base.Features.Add(Tributary,Channel(Tributary,{FVector(60,60,.8),FVector(48.27,38.72,.2),FVector(30,30,.2)}));
        Base.Features.Add(Receiver,Channel(Receiver,{FVector(65.52,11.72,.4),FVector(48.31,17.78,.1),
            FVector(30,30,0),FVector(.36,32.16,0)}));
        Candidate=Base;
        Candidate.Features[Tributary].WaterSurface.FunctionId=TEXT("constrained_flowing_knots");
        Candidate.Features[Tributary].WaterSurface.Knots.Last().Z=0;
    }
    const auto* Before=Base.Features.Find(Tributary);const auto* After=Candidate.Features.Find(Tributary);
    if (!TestNotNull(TEXT("Original tributary exists"),Before) || !TestNotNull(TEXT("Constrained tributary exists"),After) ||
        !TestTrue(TEXT("Both exact receiver functions exist"),Base.Features.Contains(Receiver) && Candidate.Features.Contains(Receiver))) return false;
    TestTrue(TEXT("Canonical model identity changes explicitly"),Before->WaterSurface.FunctionId==TEXT("rolling_quantile_l1_isotonic") &&
        After->WaterSurface.FunctionId==TEXT("constrained_flowing_knots"));
    TestTrue(TEXT("Receiver complete knot function remains unchanged"),
        Base.Features[Receiver].WaterSurface.Knots==Candidate.Features[Receiver].WaterSurface.Knots);
    const auto BaseDomains=ProjectWorldWaterSourceContext::PreparationBounds(Base);
    const auto CandidateDomains=ProjectWorldWaterSourceContext::PreparationBounds(Candidate);
    TArray<TSharedPtr<FJsonValue>> Records;TArray<FString> SelectedCells;
    double OldArea=0,NewArea=0;int32 Cells=0;
    for (const auto& Cell : Base.Cells)
    {
        const auto Membership=ProjectWorldWaterSourceContext::FeatureIds(Base,Cell);
        if (!Membership.Contains(Tributary) && !Membership.Contains(Receiver)) continue;
        const auto* OtherCell=Candidate.Cells.FindByPredicate([&](const auto& C){return C.CellId==Cell.CellId;});
        if (!TestNotNull(TEXT("Every consuming cell remains present"),OtherCell)) return false;
        TestEqual(TEXT("Terrain artifact remains immutable"),OtherCell->Terrain.ArtifactHash,Cell.Terrain.ArtifactHash);
        // Retain every Water source in the selected owner cell, including lakes
        // and unrelated channels; a two-feature success is not integration proof.
        const TArray<FString> Ids=Membership;
        auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("cell"),Cell.CellId);
        Record->SetArrayField(TEXT("bounds"),{MakeShared<FJsonValueNumber>(Cell.Bounds.X),MakeShared<FJsonValueNumber>(Cell.Bounds.Y),
            MakeShared<FJsonValueNumber>(Cell.Bounds.Z),MakeShared<FJsonValueNumber>(Cell.Bounds.W)});
        FString Semantics[2];
        for (int32 Index=0;Index<2;++Index)
        {
            const auto& Bundle=Index==0?Base:Candidate;
            const auto& Domains=Index==0?BaseDomains:CandidateDomains;
            TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;FString Error;
            for (const auto& Id : Ids)
            {
                auto& Surface=Prepared.Add(Id);
                if (!TestTrue(TEXT("Real native source preparation succeeds"),ProjectWorldWaterMeshBuilder::PrepareSurface(
                    Bundle.Features.FindChecked(Id),Domains.FindChecked(Id),Surface,Error))) {AddError(Error);return false;}
            }
            FMeshDescription Mesh;FVector Origin;int32 Count=0;
            if (!TestTrue(TEXT("Existing Water composition builds complete finite-width channels"),
                ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Ids,.25,Mesh,Origin,Semantics[Index],Count,Error)))
            {AddError(Error);return false;}
            const auto Triangles=Faces(Mesh);const double Area=Contacts(Triangles);
            Record->SetArrayField(Index==0?TEXT("before_triangles"):TEXT("after_triangles"),Points(Triangles,Bundle,Origin));
            Record->SetNumberField(Index==0?TEXT("before_overlap_m2"):TEXT("after_overlap_m2"),Area);
            if (Index==0) OldArea+=Area;else NewArea+=Area;
        }
        TestTrue(TEXT("Only cells consuming the constrained tributary change Water identity"),
            Membership.Contains(Tributary)?Semantics[0]!=Semantics[1]:Semantics[0]==Semantics[1]);
        Records.Add(MakeShared<FJsonValueObject>(Record));SelectedCells.Add(Cell.CellId);++Cells;
    }
    TestTrue(TEXT("At least one complete consuming cell is exercised"),Cells>0);
    TestTrue(FString::Printf(TEXT("Original independent fits demonstrate positive overlap (%.12g m2)"),OldArea),OldArea>1.e-4);
    TestTrue(FString::Printf(TEXT("Constrained native output has no positive-area double surface (%.12g m2)"),NewArea),NewArea<1.e-6);
    // The same native writer and cold reload used by city outputs, confined to
    // the existing disposable verification mount and the selected consumer cells.
    FProjectWorldCanonicalBundle Saved=Candidate;
    Saved.Cells.RemoveAll([&](const auto& C){return !SelectedCells.Contains(C.CellId);});
    const auto SavedDomains=ProjectWorldWaterSourceContext::PreparationBounds(Saved);
    // The complete city is the read-only geometry acceptance surface above.
    // This bounded saved fixture has its own declared preparation context.
    // Both confluence axes retain all consuming cells; other Water CDT domains
    // may differ, so saved bytes are not presented as production-city bytes.
    for (const auto& Id : {Tributary,Receiver})
        TestTrue(TEXT("Bounded saved fixture retains the complete confluence source preparation domain"),
            SavedDomains.FindChecked(Id)==CandidateDomains.FindChecked(Id));
    FProjectWorldVerifyWorld World(TEXT("WaterConfluence"));FString Error,Code;
    FProjectWorldRealizationProfile Profile;
    if (!World.IsReady(Error) || !ProjectWorldRealizationProfile::Load(FPaths::ProjectPluginsDir()/
        TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_territory_twin.realization.json"),
        Profile,Code,Error)) {AddError(Error);return false;}
    auto* Layer=Profile.Layers.FindByPredicate([](const auto& L){return L.GeneratorId==TEXT("project_water_mesh");});
    if (!TestNotNull(TEXT("Existing Water writer profile exists"),Layer)) return false;
    Layer->ArtifactRoot=World.GetRoot()+TEXT("Water/");
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/ProjectMaterial/Surfaces/Water/MI_ProjectRiverCalm_Default.MI_ProjectRiverCalm_Default"));
    if (!TestNotNull(TEXT("Accepted RiverCalm presentation loads without generation"),Material)) return false;
    FProjectWorldRealizationResult Result;Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(*Layer));
    Result.LayerInventories[0].FinalDirtyUnits=SelectedCells;
    if (!ProjectWorldWaterRealization::Apply(World.GetWorld(),Saved,Profile,Result,Error,Material) ||
        !World.SaveOwned(Error) || !World.Reload(Error)) {AddError(Error);return false;}
    TArray<TSharedPtr<FJsonValue>> SavedRecords;int32 SavedActors=0;
    for (TActorIterator<AStaticMeshActor> It(World.GetWorld());It;++It)
    {
        FString CellId,Semantic;if (!ProjectWorldWaterRealization::ReadActorIdentity(*It,CellId,Semantic)) continue;
        ++SavedActors;auto* Component=It->GetStaticMeshComponent();UStaticMesh* Mesh=Component->GetStaticMesh();
        FAssetCompilingManager::Get().FinishAllCompilation();
        const auto* Description=Mesh?Mesh->GetMeshDescription(0):nullptr;
        if (!TestNotNull(TEXT("Saved source MeshDescription reloads"),Description)) return false;
        const auto Native=Faces(*Description);const auto* Cell=Saved.Cells.FindByPredicate([&](const auto& C){return C.CellId==CellId;});
        if (!TestNotNull(TEXT("Saved actor has canonical cell ownership"),Cell)) return false;
        const auto Encoded=Points(Native,Saved,It->GetActorLocation());
        const auto Ids=ProjectWorldWaterSourceContext::FeatureIds(Saved,*Cell);
        TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
        for (const auto& Id : Ids)
            if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Saved.Features.FindChecked(Id),
                SavedDomains.FindChecked(Id),Prepared.Add(Id),Error)) {AddError(Error);return false;}
        FMeshDescription ExpectedMesh;FVector ExpectedOrigin;FString ExpectedSemantic;int32 ExpectedCount=0;
        if (!ProjectWorldWaterCellMesh::Build(Saved,*Cell,Prepared,Ids,.25,ExpectedMesh,ExpectedOrigin,
            ExpectedSemantic,ExpectedCount,Error)) {AddError(Error);return false;}
        const auto ExpectedFaces=Points(Faces(ExpectedMesh),Saved,ExpectedOrigin);
        bool Equal=Encoded.Num()==ExpectedFaces.Num();
        for (int32 I=0;Equal && I<Encoded.Num();++I) for (int32 J=0;Equal && J<3;++J)
            for (int32 K=0;Equal && K<3;++K) Equal=Encoded[I]->AsArray()[J]->AsArray()[K]->AsNumber()==
                ExpectedFaces[I]->AsArray()[J]->AsArray()[K]->AsNumber();
        TestTrue(TEXT("Every saved source triangle and position matches native candidate"),Equal);
        TestEqual(TEXT("Water retains its no-collision contract"),Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Loaded reference retains accepted material"),Component->GetMaterial(0)==Material);
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];const auto Indices=LOD.IndexBuffer.GetArrayView();
        TestEqual(TEXT("Compiled unsimplified Water retains source triangle count"),Indices.Num()/3,Native.Num());
        auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("cell"),CellId);
        Entry->SetStringField(TEXT("mesh"),Mesh->GetPathName());Entry->SetArrayField(TEXT("triangles"),Encoded);
        Entry->SetArrayField(TEXT("expected_triangles"),ExpectedFaces);
        TArray<FProjectWorldSurfaceTriangle> Compiled;
        for (int32 I=0;I<Indices.Num();I+=3)
        {
            auto& Face=Compiled.AddDefaulted_GetRef();
            for (int32 J=0;J<3;++J) Face.Points[J]=FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+J]));
        }
        Entry->SetArrayField(TEXT("compiled_triangles"),Points(Compiled,Saved,It->GetActorLocation()));
        if (!Output.IsEmpty())
        {
            const FString Package=Mesh->GetOutermost()->GetName();
            if (!Package.StartsWith(World.GetRoot())) {AddError(TEXT("Snapshot escaped fixture ownership"));return false;}
            FString Relative=Package;Relative.RemoveFromStart(TEXT("/ProjectWorldVerify/"));
            const FString Destination=FPaths::GetPath(Output)/TEXT("native-mesh-content")/Relative;
            const FString Source=FPackageName::LongPackageNameToFilename(Package);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination),true);
            for (const TCHAR* Extension : {TEXT(".uasset"),TEXT(".uexp"),TEXT(".ubulk")})
                if (IFileManager::Get().FileExists(*(Source+Extension)) &&
                    IFileManager::Get().Copy(*(Destination+Extension),*(Source+Extension))!=COPY_OK) return false;
        }
        SavedRecords.Add(MakeShared<FJsonValueObject>(Entry));
    }
    TestEqual(TEXT("Every consuming cell has a saved Water actor"),SavedActors,Cells);
    FProjectWorldRealizationResult NoOp;NoOp.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(*Layer));
    NoOp.LayerInventories[0].FinalDirtyUnits.Reset();
    if (!ProjectWorldWaterRealization::Apply(World.GetWorld(),Saved,Profile,NoOp,Error,Material)) {AddError(Error);return false;}
    TestEqual(TEXT("Cold replay creates no actors"),NoOp.CreatedActorCount,0);
    TestEqual(TEXT("Cold replay updates no actors"),NoOp.UpdatedActorCount,0);
    TestEqual(TEXT("Cold replay saves no external actors"),NoOp.SelfSavedActorMutationCount,0);
    if (!Output.IsEmpty())
    {
        auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("fixture"),Fixture);
        Root->SetNumberField(TEXT("consuming_cells"),Cells);Root->SetNumberField(TEXT("before_overlap_m2"),OldArea);
        Root->SetNumberField(TEXT("after_overlap_m2"),NewArea);Root->SetArrayField(TEXT("cells"),Records);
        Root->SetArrayField(TEXT("saved_cells"),SavedRecords);
        TArray<TSharedPtr<FJsonValue>> DomainRecords;
        for (const auto& Pair : SavedDomains)
        {
            auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("feature_id"),Pair.Key);
            const FVector4d A=CandidateDomains.FindChecked(Pair.Key),B=Pair.Value;
            Item->SetArrayField(TEXT("production_bounds"),{MakeShared<FJsonValueNumber>(A.X),MakeShared<FJsonValueNumber>(A.Y),
                MakeShared<FJsonValueNumber>(A.Z),MakeShared<FJsonValueNumber>(A.W)});
            Item->SetArrayField(TEXT("saved_fixture_bounds"),{MakeShared<FJsonValueNumber>(B.X),MakeShared<FJsonValueNumber>(B.Y),
                MakeShared<FJsonValueNumber>(B.Z),MakeShared<FJsonValueNumber>(B.W)});
            DomainRecords.Add(MakeShared<FJsonValueObject>(Item));
        }
        Root->SetArrayField(TEXT("saved_preparation_domains"),DomainRecords);
        FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
        TestTrue(TEXT("Native triangle evidence is written"),FFileHelper::SaveStringToFile(Text,*Output));
    }
    return true;
}
#endif
