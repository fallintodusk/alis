// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterSourceContext.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldWaterHorizontalComposition.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldWaterSourceReplayTest
{
    using FTriangle = TArray<FVector3f>;
    TArray<FTriangle> Triangles(const FMeshDescription& Mesh)
    {
        TArray<FTriangle> Out;
        const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
        for (const auto Id : Mesh.Triangles().GetElementIDs())
        {
            FTriangle& Triangle = Out.AddDefaulted_GetRef();
            for (const auto Instance : Mesh.GetTriangleVertexInstances(Id))
                Triangle.Add(Positions[Mesh.GetVertexInstanceVertex(Instance)]);
        }
        return Out;
    }
    bool Same(const TArray<FTriangle>& A, const FVector& OriginA,
        const TArray<FTriangle>& B, const FVector& OriginB)
    {
        if (A.Num() != B.Num() || OriginA != OriginB) return false;
        for (int32 I = 0; I < A.Num(); ++I)
            if (A[I] != B[I]) return false;
        return true;
    }
    bool Json(const FString& Path, TSharedPtr<FJsonObject>& Out)
    {
        FString Text;
        return FFileHelper::LoadFileToString(Text, *Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out);
    }
    bool Extract(const FString& Profile, const FString& Root, FString& Result, FString& ZipHash)
    {
        const FString Source = FPaths::ProjectPluginsDir()/TEXT("World/ProjectWorldData/Data/Canonical")/Profile;
        TSharedPtr<FJsonObject> Active;
        if (!Json(Source/TEXT("active.json"), Active)) return false;
        const auto Bundle = Active->GetObjectField(TEXT("bundle"));
        FString ArchivePath;
        if (!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Source, Bundle->GetStringField(TEXT("path")), ArchivePath) ||
            !FProjectWorldCanonicalLoader::ComputeFileSha256(ArchivePath, ZipHash) ||
            ZipHash != Bundle->GetStringField(TEXT("sha256")) ||
            IFileManager::Get().FileSize(*ArchivePath) != int64(Bundle->GetNumberField(TEXT("byte_size")))) return false;
        FZipArchiveReader Archive(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ArchivePath), GWarn);
        if (!Archive.IsValid()) return false;
        for (const FString& Name : Archive.GetFileNames())
        {
            FString Destination;
            TArray<uint8> Bytes;
            if (!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root, Name, Destination) ||
                !Archive.TryReadFile(Name, Bytes, GWarn) ||
                !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true) ||
                !FFileHelper::SaveArrayToFile(Bytes, *Destination)) return false;
        }
        Result = Root/TEXT("compile_result.json");
        FString Digest;
        return FProjectWorldCanonicalLoader::ComputeFileSha256(Result, Digest) &&
            Digest == Active->GetStringField(TEXT("compile_result_sha256"));
    }
    TArray<TArray<FString>> Groups(const FProjectWorldCanonicalBundle& Bundle, const TArray<FString>& Features)
    {
        TArray<TArray<FString>> Out;
        TMap<double, int32> Constants;
        for (const FString& Id : Features)
        {
            const auto& Surface = Bundle.Features.FindChecked(Id).WaterSurface;
            bool Constant = Surface.Behavior == TEXT("standing");
            double Z = Surface.LevelMeters;
            if (Surface.Behavior == TEXT("flowing") && Surface.Knots.Num() >= 2)
            {
                Z = FMath::RoundToDouble(Surface.Knots[0].Z/Bundle.HeightQuantizationMeters)*Bundle.HeightQuantizationMeters;
                Constant = true;
                for (const auto& Knot : Surface.Knots)
                    Constant &= FMath::RoundToDouble(Knot.Z/Bundle.HeightQuantizationMeters)*Bundle.HeightQuantizationMeters == Z;
            }
            if (Constant)
            {
                if (const int32* Index = Constants.Find(Z)) {Out[*Index].Add(Id); continue;}
                Constants.Add(Z, Out.Num());
            }
            Out.Add({Id});
        }
        return Out;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterSourceEmissionReplayTest,
    "Project.World.Water.SourceEmissionReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterSourceEmissionReplayTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterSourceReplayTest;
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world/water_source_replay")/
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT {IFileManager::Get().DeleteDirectory(*(Root/TEXT("extracted")), false, true);};
    struct FSelection {const TCHAR* Profile; int32 X; int32 Y; const TCHAR* Artifact;};
    const FSelection Selections[] = {
        {TEXT("kazan_territory_v1"),4,-6,TEXT("/ProjectWorldData/Generated/Territory/Water/Cells")},
        {TEXT("manhattan_showcase_v1"),6,6,TEXT("/ProjectWorldData/Generated/Showcase/Manhattan/Water/Cells")}};
    TArray<TSharedPtr<FJsonValue>> Records;
    for (const auto& Selection : Selections)
    {
        FString Receipt, ZipHash;
        if (!TestTrue(TEXT("Tracked canonical authority authenticates fresh native ZIP extraction"),
            Extract(Selection.Profile, Root/TEXT("extracted")/Selection.Profile, Receipt, ZipHash))) return false;
        FProjectWorldCanonicalBundle Bundle;
        FProjectWorldCanonicalValidation Validation;
        if (!TestTrue(TEXT("Existing native loader validates complete canonical output hashes"),
            FProjectWorldCanonicalLoader::Load(Receipt, Bundle, Validation))) {AddError(Validation.Message); return false;}
        const auto* Cell = Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellX == Selection.X && C.CellY == Selection.Y;});
        if (!TestNotNull(TEXT("Pinned representative cell exists"), Cell)) return false;
        const auto Features = ProjectWorldWaterSourceContext::FeatureIds(Bundle, *Cell);
        if (!TestTrue(TEXT("Selected cell consumes actual water sources"), !Features.IsEmpty())) return false;
        TMap<FString, FProjectWorldPreparedWaterSurface> Prepared;
        const auto Domains=ProjectWorldWaterSourceContext::PreparationBounds(Bundle);
        FString Error;
        for (const FString& Id : Features)
        {
            FProjectWorldPreparedWaterSurface Surface;
            if (!TestTrue(TEXT("Actual producer feature-domain preparation succeeds"),
                ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features.FindChecked(Id), Domains.FindChecked(Id), Surface, Error))) {AddError(Error); return false;}
            Prepared.Add(Id, MoveTemp(Surface));
        }
        // Native helpers alone emit geometry; this test only authenticates and attributes it.
        FMeshDescription Replay;
        FVector Origin;
        FString Semantic;
        int32 Count = 0;
        if (!TestTrue(TEXT("Complete native cell replay succeeds without Apply or asset saving"),
            ProjectWorldWaterCellMesh::Build(Bundle,*Cell,Prepared,Features,.25,Replay,Origin,Semantic,Count,Error))) {AddError(Error); return false;}
        FString Token = Cell->CellId;
        for (TCHAR& C : Token) if (!FChar::IsAlnum(C) && C != TEXT('_')) C = TEXT('_');
        const FString Name = TEXT("SM_ProjectWorldWater_")+Token.Left(96);
        const FString ObjectPath = FString(Selection.Artifact)/Name+TEXT(".")+Name;
        const FString SourcePath = FPackageName::LongPackageNameToFilename(FString(Selection.Artifact)/Name,TEXT(".uasset"));
        FString HashBefore, HashAfter;
        if (!FProjectWorldCanonicalLoader::ComputeFileSha256(SourcePath,HashBefore)) return false;
        auto* Asset = LoadObject<UStaticMesh>(nullptr,*ObjectPath);
        if (!TestNotNull(TEXT("Existing saved Water source mesh loads"), Asset)) return false;
        const auto* Description = Asset->GetMeshDescription(0);
        if (!TestNotNull(TEXT("Saved source MeshDescription exists"),Description)) return false;
        const bool DirtyBefore = Asset->GetOutermost()->IsDirty();
        const auto Expected = Triangles(*Description);
        const auto Actual = Triangles(Replay);
        if (!TestTrue(TEXT("Every saved float vertex, corner order and triangle order matches native replay"),
            Same(Expected,Origin,Actual,Origin))) return false;
        if (!TestTrue(TEXT("Nonempty source triangles are exercised"),Actual.Num()>1)) return false;
        auto Bad = Actual;
        Bad[0][0].Z += 1;
        TestFalse(TEXT("Wrong emitted vertex refuses exact mapping"),Same(Expected,Origin,Bad,Origin));
        Bad=Actual; Swap(Bad[0],Bad.Last());
        TestFalse(TEXT("Wrong triangle ordering refuses attribution"),Same(Expected,Origin,Bad,Origin));
        TestFalse(TEXT("Wrong world origin refuses attribution"),Same(Expected,Origin,Actual,Origin+FVector(1,0,0)));
        TArray<FTriangle> Concatenated;
        FMeshDescription BeforeFinalComposition;
        TArray<TSharedPtr<FJsonValue>> GroupRecords;
        for (const auto& Group : Groups(Bundle,Features))
        {
            FMeshDescription Part; FVector PartOrigin; FString PartSemantic; int32 PartCount=0;
            if (!TestTrue(TEXT("Native group replay includes complete constant unions"),
                ProjectWorldWaterCellMesh::Build(Bundle,*Cell,Prepared,Group,.25,Part,PartOrigin,PartSemantic,PartCount,Error))) {AddError(Error); return false;}
            if (PartCount>0 && !TestTrue(TEXT("Group replay origin equals full cell origin"),PartOrigin==Origin)) return false;
            auto Entry=MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("first_triangle"),Concatenated.Num());
            Entry->SetNumberField(TEXT("triangle_count"),PartCount);
            TArray<TSharedPtr<FJsonValue>> Sources;
            for (const auto& Id : Group) Sources.Add(MakeShared<FJsonValueString>(Id));
            Entry->SetArrayField(TEXT("source_ids"),Sources);
            GroupRecords.Add(MakeShared<FJsonValueObject>(Entry));
            Concatenated.Append(Triangles(Part));
            if (PartCount>0)
            {
                if (BeforeFinalComposition.Triangles().Num()==0) BeforeFinalComposition=MoveTemp(Part);
                else FStaticMeshOperations::AppendMeshDescription(Part,BeforeFinalComposition,FStaticMeshOperations::FAppendSettings());
            }
        }
        bool Composed=false;FString CompositionHash;
        if (!ProjectWorldWaterHorizontalComposition::Compose(BeforeFinalComposition,Composed,CompositionHash,Error))
        {AddError(Error);return false;}
        if (!TestTrue(TEXT("Native source groups plus final composition reproduce saved triangles"),
            Same(Expected,Origin,Triangles(BeforeFinalComposition),Origin))) return false;
        auto Record=MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("profile"),Selection.Profile); Record->SetStringField(TEXT("cell"),Cell->CellId);
        Record->SetStringField(TEXT("mesh"),ObjectPath); Record->SetStringField(TEXT("source_sha256"),HashBefore);
        Record->SetStringField(TEXT("canonical_zip_sha256"),ZipHash); Record->SetStringField(TEXT("semantic"),Semantic);
        Record->SetArrayField(TEXT("before_final_composition_groups"),GroupRecords);
        const auto WorldPoints=[&](const TArray<FTriangle>& Input)
        {
            TArray<TSharedPtr<FJsonValue>> Points;
            for (const auto& Triangle : Input)
            {
                TArray<TSharedPtr<FJsonValue>> Corners;
                for (const auto& P : Triangle)
                {
                    const FVector World=FVector(P)+Origin;
                    Corners.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                        MakeShared<FJsonValueNumber>(World.X),MakeShared<FJsonValueNumber>(World.Y),MakeShared<FJsonValueNumber>(World.Z)}));
                }
                Points.Add(MakeShared<FJsonValueArray>(Corners));
            }
            return Points;
        };
        Record->SetArrayField(TEXT("before_final_composition_world_triangles_cm"),WorldPoints(Concatenated));
        Record->SetArrayField(TEXT("world_triangles_cm"),WorldPoints(Actual));
        if (!TestTrue(TEXT("Source package bytes and loaded dirty state remain unchanged"),
            FProjectWorldCanonicalLoader::ComputeFileSha256(SourcePath,HashAfter) && HashBefore==HashAfter &&
            Asset->GetOutermost()->IsDirty()==DirtyBefore)) return false;
        Records.Add(MakeShared<FJsonValueObject>(Record));
    }
    FString Text;
    const FString Output=Root/TEXT("source-emission.json");
    if (!FJsonSerializer::Serialize(Records,TJsonWriterFactory<>::Create(&Text)) ||
        !FFileHelper::SaveStringToFile(Text,*Output)) return false;
    AddInfo(TEXT("Source emission receipt: ")+Output);
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldWaterSourceEmissionReplayTest,
    "Project.World.Water.SourceEmissionReplay","[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterCompositionCandidateReplayTest,
    "Project.World.Water.CompositionCandidateReplay",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterCompositionCandidateReplayTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterSourceReplayTest;
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world/water_composition_replay")/
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT {IFileManager::Get().DeleteDirectory(*(Root/TEXT("extracted")),false,true);};
    const auto FlatContacts=[&](const TArray<FTriangle>& Faces,int32& Count,double& Area,FString& Error)
    {
        Count=0;Area=0;
        for (int32 A=0;A<Faces.Num();++A)
        {
            const auto& P=Faces[A];
            if (P[0].Z!=P[1].Z || P[0].Z!=P[2].Z) continue;
            for (int32 B=A+1;B<Faces.Num();++B)
            {
                const auto& Q=Faces[B];
                if (Q[0].Z!=Q[1].Z || Q[0].Z!=Q[2].Z || P[0].Z!=Q[0].Z) continue;
                FProjectWorldSurfaceTriangle First,Second;
                for (int32 I=0;I<3;++I) {First.Points[I]=FVector(P[I]);Second.Points[I]=FVector(Q[I]);}
                TArray<FVector2D> Intersection;
                if (!ProjectWorldTerrainSurface::IntersectXY(First,Second,Intersection,Error)) return false;
                const double Overlap=ProjectWorldTerrainSurface::Area(Intersection);
                // Keep the retained detector's one-square-centimeter qualification explicit.
                if (Overlap>1) {++Count;Area+=Overlap;}
            }
        }
        return true;
    };
    struct FSelection {const TCHAR* Profile;int32 X,Y;const TCHAR* Artifact;};
    const FSelection Selections[]={{TEXT("kazan_territory_v1"),4,-6,TEXT("/ProjectWorldData/Generated/Territory/Water/Cells")},
        {TEXT("manhattan_showcase_v1"),6,6,TEXT("/ProjectWorldData/Generated/Showcase/Manhattan/Water/Cells")}};
    TArray<TSharedPtr<FJsonValue>> Records;
    for (const auto& Selection : Selections)
    {
        FString Receipt,ZipHash,Error;
        if (!TestTrue(TEXT("Read-only canonical authority extraction authenticates"),
            Extract(Selection.Profile,Root/TEXT("extracted")/Selection.Profile,Receipt,ZipHash))) return false;
        FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation Validation;
        if (!FProjectWorldCanonicalLoader::Load(Receipt,Bundle,Validation)) {AddError(Validation.Message);return false;}
        const auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellX==Selection.X && C.CellY==Selection.Y;});
        if (!TestNotNull(TEXT("Selected real cell exists"),Cell)) return false;
        const auto Features=ProjectWorldWaterSourceContext::FeatureIds(Bundle,*Cell);
        const auto Domains=ProjectWorldWaterSourceContext::PreparationBounds(Bundle);
        TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
        for (const auto& Id : Features)
        {
            FProjectWorldPreparedWaterSurface Surface;
            if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features.FindChecked(Id),Domains.FindChecked(Id),Surface,Error))
            {AddError(Error);return false;}
            Prepared.Add(Id,MoveTemp(Surface));
        }
        FString Token=Cell->CellId;
        for (auto& C : Token) if (!FChar::IsAlnum(C) && C!=TEXT('_')) C=TEXT('_');
        const FString Name=TEXT("SM_ProjectWorldWater_")+Token.Left(96);
        const FString Package=FString(Selection.Artifact)/Name;
        const FString Path=FPackageName::LongPackageNameToFilename(Package,TEXT(".uasset"));
        FString Before,After;
        if (!FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Before)) return false;
        auto* Asset=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+Name));
        if (!TestNotNull(TEXT("Accepted saved Water loads without mutation"),Asset) ||
            !TestNotNull(TEXT("Accepted saved Water has source geometry"),Asset->GetMeshDescription(0))) return false;
        const bool Dirty=Asset->GetOutermost()->IsDirty();
        FMeshDescription Candidate;FVector Origin;FString Semantic;int32 TriangleCount=0;
        if (!ProjectWorldWaterCellMesh::Build(Bundle,*Cell,Prepared,Features,.25,Candidate,Origin,Semantic,TriangleCount,Error))
        {AddError(Error);return false;}
        if (!TestTrue(TEXT("Real candidate is nonempty with consistent triangle accounting"),
            TriangleCount>0 && TriangleCount==Candidate.Triangles().Num())) return false;
        int32 SavedContacts=0,CandidateContacts=0;double SavedArea=0,CandidateArea=0;
        if (!FlatContacts(Triangles(*Asset->GetMeshDescription(0)),SavedContacts,SavedArea,Error) ||
            !FlatContacts(Triangles(Candidate),CandidateContacts,CandidateArea,Error)) {AddError(Error);return false;}
        TestEqual(TEXT("Candidate has no qualified exact-horizontal overlap"),CandidateContacts,0);
        TestTrue(TEXT("Read-only replay leaves saved bytes and dirty state exact"),
            FProjectWorldCanonicalLoader::ComputeFileSha256(Path,After) && Before==After && Asset->GetOutermost()->IsDirty()==Dirty);
        auto Record=MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("profile"),Selection.Profile);Record->SetStringField(TEXT("cell"),Cell->CellId);
        Record->SetStringField(TEXT("saved_package_sha256"),Before);Record->SetStringField(TEXT("canonical_zip_sha256"),ZipHash);
        Record->SetStringField(TEXT("candidate_semantic_sha256"),Semantic);
        Record->SetNumberField(TEXT("candidate_triangles"),TriangleCount);
        Record->SetNumberField(TEXT("saved_qualified_horizontal_contacts"),SavedContacts);
        Record->SetNumberField(TEXT("candidate_qualified_horizontal_contacts"),CandidateContacts);
        Record->SetNumberField(TEXT("saved_qualified_horizontal_overlap_cm2"),SavedArea);
        Record->SetNumberField(TEXT("candidate_qualified_horizontal_overlap_cm2"),CandidateArea);
        Records.Add(MakeShared<FJsonValueObject>(Record));
    }
    FString Text;const FString Output=Root/TEXT("composition-candidate.json");
    if (!FJsonSerializer::Serialize(Records,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Output)) return false;
    AddInfo(TEXT("Read-only composition candidate: ")+Output);
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldWaterCompositionCandidateReplayTest,
    "Project.World.Water.CompositionCandidateReplay","[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterCityCandidateCensusTest,
    "Project.World.Water.CityCandidateCensus",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterCityCandidateCensusTest::RunTest(const FString&)
{
    using namespace ProjectWorldWaterSourceReplayTest;
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world/water_candidate_census")/
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT {IFileManager::Get().DeleteDirectory(*(Root/TEXT("extracted")),false,true);};
    struct FCity {const TCHAR* Profile;const TCHAR* Artifact;};
    const FCity Cities[]={{TEXT("kazan_territory_v1"),TEXT("/ProjectWorldData/Generated/Territory/Water/Cells")},
        {TEXT("manhattan_showcase_v1"),TEXT("/ProjectWorldData/Generated/Showcase/Manhattan/Water/Cells")}};
    TArray<TSharedPtr<FJsonValue>> Records;
    bool Valid=true;
    for (const auto& City : Cities)
    {
        FString Receipt,ZipHash,Error;
        if (!Extract(City.Profile,Root/TEXT("extracted")/City.Profile,Receipt,ZipHash)) return false;
        FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation Validation;
        if (!FProjectWorldCanonicalLoader::Load(Receipt,Bundle,Validation)) {AddError(Validation.Message);return false;}
        const auto Domains=ProjectWorldWaterSourceContext::PreparationBounds(Bundle);
        TMap<FString,FProjectWorldPreparedWaterSurface> Prepared;
        TArray<FString> Ids;Domains.GetKeys(Ids);Ids.Sort();
        for (const auto& Id : Ids)
        {
            FProjectWorldPreparedWaterSurface Surface;
            if (!ProjectWorldWaterMeshBuilder::PrepareSurface(Bundle.Features.FindChecked(Id),Domains.FindChecked(Id),Surface,Error))
            {AddError(Error);return false;}
            Prepared.Add(Id,MoveTemp(Surface));
        }
        int32 Nonempty=0,Changed=0;
        auto Record=MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("profile"),City.Profile);
        Record->SetStringField(TEXT("canonical_zip_sha256"),ZipHash);
        Record->SetNumberField(TEXT("canonical_cells"),Bundle.Cells.Num());
        Record->SetNumberField(TEXT("prepared_water_features"),Prepared.Num());
        TArray<TSharedPtr<FJsonValue>> Cells;
        for (const auto& Cell : Bundle.Cells)
        {
            const auto Features=ProjectWorldWaterSourceContext::FeatureIds(Bundle,Cell);
            if (Features.IsEmpty()) continue;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("cell"),Cell.CellId);
            FMeshDescription Candidate;FVector Origin;FString Semantic;int32 Count=0;
            const double Started=FPlatformTime::Seconds();
            const bool Built=ProjectWorldWaterCellMesh::Build(Bundle,Cell,Prepared,Features,.25,Candidate,Origin,Semantic,Count,Error);
            Row->SetNumberField(TEXT("candidate_build_seconds"),FPlatformTime::Seconds()-Started);
            Row->SetBoolField(TEXT("built"),Built);
            if (!Built || Count!=Candidate.Triangles().Num())
            {
                AddError(Cell.CellId+TEXT(": ")+Error);Row->SetStringField(TEXT("error"),Error);Valid=false;
                Cells.Add(MakeShared<FJsonValueObject>(Row));continue;
            }
            FString Token=Cell.CellId;
            for (auto& C : Token) if (!FChar::IsAlnum(C) && C!=TEXT('_')) C=TEXT('_');
            const FString Name=TEXT("SM_ProjectWorldWater_")+Token.Left(96);
            const FString Package=FString(City.Artifact)/Name;
            const FString Path=FPackageName::LongPackageNameToFilename(Package,TEXT(".uasset"));
            Row->SetNumberField(TEXT("candidate_triangles"),Count);
            Row->SetStringField(TEXT("candidate_semantic_sha256"),Semantic);
            Row->SetStringField(TEXT("package"),Package);
            const bool Exists=IFileManager::Get().FileExists(*Path);
            if (Exists)
            {
                FString Before,After;
                if (!FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Before)) return false;
                auto* Asset=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+Name));
                if (!Asset || !Asset->GetMeshDescription(0)) {AddError(TEXT("Saved Water has no source geometry: ")+Package);return false;}
                const bool Dirty=Asset->GetOutermost()->IsDirty();
                const auto Saved=Triangles(*Asset->GetMeshDescription(0));
                const bool Different=!Same(Saved,FVector::ZeroVector,Triangles(Candidate),FVector::ZeroVector);
                Changed+=Different?1:0;
                Row->SetBoolField(TEXT("source_triangle_order_or_positions_changed"),Different);
                Row->SetNumberField(TEXT("saved_triangles"),Saved.Num());
                Row->SetStringField(TEXT("saved_package_sha256"),Before);
                if (!TestTrue(TEXT("Census preserves saved package bytes and dirty state"),
                    FProjectWorldCanonicalLoader::ComputeFileSha256(Path,After) && Before==After && Asset->GetOutermost()->IsDirty()==Dirty)) Valid=false;
                if (Count==0) {AddError(TEXT("Candidate unexpectedly removes existing Water: ")+Package);Valid=false;}
            }
            else if (Count>0) {AddError(TEXT("Candidate needs a previously absent Water output: ")+Package);Valid=false;}
            Nonempty+=Count>0?1:0;
            Cells.Add(MakeShared<FJsonValueObject>(Row));
        }
        TestTrue(TEXT("The authenticated city exercises nonempty Water output"),Nonempty>0);
        Record->SetNumberField(TEXT("nonempty_candidate_cells"),Nonempty);
        Record->SetNumberField(TEXT("changed_source_geometry_cells"),Changed);
        Record->SetArrayField(TEXT("cells"),Cells);
        Records.Add(MakeShared<FJsonValueObject>(Record));
    }
    FString Text;const FString Output=Root/TEXT("candidate-census.json");
    if (!FJsonSerializer::Serialize(Records,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Output)) return false;
    AddInfo(TEXT("Read-only all-Water candidate census: ")+Output);
    return Valid;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldWaterCityCandidateCensusTest,
    "Project.World.Water.CityCandidateCensus","[Slow][Integration][World]")
#endif
