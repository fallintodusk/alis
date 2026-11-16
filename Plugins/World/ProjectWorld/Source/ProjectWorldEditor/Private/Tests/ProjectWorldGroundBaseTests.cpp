// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGroundBaseFixture.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldGroundBaseTest
{
    using FObject=TSharedPtr<FJsonObject>;
    FObject Parse(const FString& Text)
    {FObject Out;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out);return Out;}
    FObject Object() {return MakeShared<FJsonObject>();}
    void MatrixFault(const FObject& Parent,const TCHAR* Field,double Value)
    {
        auto Rows=Parent->GetArrayField(Field);auto Row=Rows[0]->AsArray();Row[0]=MakeShared<FJsonValueNumber>(Value);
        Rows[0]=MakeShared<FJsonValueArray>(Row);Parent->SetArrayField(Field,Rows);
    }
    bool WriteFixture(const FString& Defect,FString& Receipt)
    {
        const FString Root=FPaths::ProjectDir()/TEXT("tmp/world/canonical_ground_base")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const FString InputsHash=FString::ChrN(64,'b'),BaseHash=FString::ChrN(64,'c'),ConstructionHash=FString::ChrN(64,'d');
        TArray<TSharedPtr<FJsonValue>> Outputs;
        auto Write=[&](const FString& Relative,const FObject& Value)->FObject
        {
            FString Text,Hash;FJsonSerializer::Serialize(Value.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));
            const FString Path=Root/Relative;IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
            if (!FFileHelper::SaveStringToFile(Text,*Path) || !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash)) return nullptr;
            auto Out=Object();Out->SetStringField(TEXT("path"),Relative);Out->SetStringField(TEXT("sha256"),Hash);
            Out->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*Path));
            auto Entry=Object();Entry->Values=Out->Values;Entry->SetStringField(TEXT("kind"),TEXT("compiler_document"));Outputs.Add(MakeShared<FJsonValueObject>(Entry));return Out;
        };
        auto Input=Parse(ProjectWorldGroundBaseFixture::Input);auto Construction=Input->GetObjectField(TEXT("construction"));
        auto Profile=Construction->GetObjectField(TEXT("compiler_profile"));auto Grid=Profile->GetObjectField(TEXT("grid"));
        auto SourceGrid=Construction->GetObjectField(TEXT("grid"));auto Evidence=Construction->GetObjectField(TEXT("evidence"));
        if (Defect==TEXT("version")) Input->SetNumberField(TEXT("schema_version"),2);
        if (Defect==TEXT("profile_version")) Profile->SetNumberField(TEXT("schema_version"),2);
        if (Defect==TEXT("frame")) Evidence->SetStringField(TEXT("canonical_crs"),TEXT("EPSG:32617"));
        if (Defect==TEXT("nodata")) MatrixFault(SourceGrid,TEXT("samples"),SourceGrid->GetNumberField(TEXT("nodata")));
        if (Defect==TEXT("numeric_string"))
        {auto Size=SourceGrid->GetArrayField(TEXT("size"));Size[0]=MakeShared<FJsonValueString>(TEXT("5"));SourceGrid->SetArrayField(TEXT("size"),Size);}
        if (Defect==TEXT("accuracy_string")) Construction->GetObjectField(TEXT("source_metadata"))->SetStringField(TEXT("source_accuracy_m"),TEXT("unknown"));
        if (Defect==TEXT("known_positive")) Construction->GetObjectField(TEXT("source_metadata"))->SetNumberField(TEXT("source_accuracy_m"),.5);
        if (Defect==TEXT("grid_alignment"))
        {auto Origin=SourceGrid->GetArrayField(TEXT("origin"));Origin[0]=MakeShared<FJsonValueNumber>(Origin[0]->AsNumber()+.01);SourceGrid->SetArrayField(TEXT("origin"),Origin);}
        FString InputText;FJsonSerializer::Serialize(Input.ToSharedRef(),TJsonWriterFactory<>::Create(&InputText));
        FString InputHash;
        // Save first, then use the same file-hash authority as the actual loader.
        IFileManager::Get().MakeDirectory(*Root,true);const FString Temporary=Root/TEXT("input.json");
        if (!FFileHelper::SaveStringToFile(InputText,*Temporary) || !FProjectWorldCanonicalLoader::ComputeFileSha256(Temporary,InputHash)) return false;
        auto InputDescriptor=Write(TEXT("refinement/inputs/")+InputHash+TEXT(".json"),Input);if (!InputDescriptor) return false;
        auto Operation=Object();Operation->SetStringField(TEXT("kind"),Defect==TEXT("kind") ? TEXT("unsupported_ground") : TEXT("qualified_ground_base"));
        Operation->SetArrayField(TEXT("feature_ids"),{});Operation->SetObjectField(TEXT("base_features"),Object());
        Operation->SetObjectField(TEXT("input"),InputDescriptor);Operation->SetStringField(TEXT("construction_identity"),ConstructionHash);
        if (Defect==TEXT("feature")) Operation->SetArrayField(TEXT("feature_ids"),{MakeShared<FJsonValueString>(TEXT("invented"))});
        TArray<TSharedPtr<FJsonValue>> Operations={MakeShared<FJsonValueObject>(Operation)};
        if (Defect==TEXT("mixed")) Operations.Add(MakeShared<FJsonValueObject>(Operation));
        auto Request=Object();Request->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/surface-refinement-request-v1.json"));
        Request->SetNumberField(TEXT("schema_version"),1);Request->SetArrayField(TEXT("operations"),Operations);
        if (!Write(TEXT("refinement/request.json"),Request)) return false;
        auto CellValues=Parse(FString(TEXT("{\"cells\":"))+ProjectWorldGroundBaseFixture::Cells+TEXT("}"))->GetArrayField(TEXT("cells"));
        TArray<TSharedPtr<FJsonValue>> CoverageCells;FString GridId;
        for (int32 Index=0;Index<CellValues.Num();++Index)
        {
            auto Terrain=CellValues[Index]->AsObject();GridId=Terrain->GetStringField(TEXT("grid_id"));
            const FString Id=Terrain->GetStringField(TEXT("cell_id"));FString Prefix,Coordinates;Id.Split(TEXT(":x"),&Prefix,&Coordinates);
            FString X,Y;Coordinates.Split(TEXT(":y"),&X,&Y);
            if (Defect==TEXT("known_positive")) Terrain->GetObjectField(TEXT("vertical_provenance"))->SetNumberField(TEXT("source_accuracy_m"),.5);
            if (Index==0)
            {
                if (Defect==TEXT("source_ref")) Terrain->GetObjectField(TEXT("vertical_provenance"))->SetStringField(TEXT("source_ref"),TEXT("old_dsm"));
                if (Defect==TEXT("known_accuracy")) Terrain->GetObjectField(TEXT("vertical_provenance"))->SetNumberField(TEXT("source_accuracy_m"),0);
                if (Defect==TEXT("residual_string")) Terrain->GetObjectField(TEXT("vertical_provenance"))->SetStringField(TEXT("sampling_quantization_residual_m"),TEXT("0.05"));
                if (Defect==TEXT("alignment_scale")) Terrain->GetObjectField(TEXT("raster_alignment"))->SetArrayField(TEXT("resampling_scale"),{MakeShared<FJsonValueNumber>(1),MakeShared<FJsonValueNumber>(1)});
                if (Defect==TEXT("fit_value"))
                {auto Fit=Terrain->GetObjectField(TEXT("water_fit"));MatrixFault(Fit,TEXT("core_samples"),Fit->GetArrayField(TEXT("core_samples"))[0]->AsArray()[0]->AsNumber()+.001);}
                if (Defect==TEXT("halo_value"))
                {auto W=Terrain->GetObjectField(TEXT("water_fit"))->GetObjectField(TEXT("halo_window"));MatrixFault(W,TEXT("samples"),W->GetArrayField(TEXT("samples"))[0]->AsArray()[0]->AsNumber()+.001);}
            }
            auto T=Write(FString::Printf(TEXT("canonical/terrain/cell_%d.json"),Index),Terrain);
            auto Feature=Object();Feature->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/canonical-feature-v1.json"));
            Feature->SetNumberField(TEXT("schema_version"),1);Feature->SetStringField(TEXT("grid_id"),GridId);Feature->SetStringField(TEXT("cell_id"),Id);Feature->SetArrayField(TEXT("features"),{});
            auto F=Write(FString::Printf(TEXT("canonical/features/cell_%d.json"),Index),Feature);if (!T || !F) return false;
            auto Lineage=Object();Lineage->SetStringField(TEXT("kind"),TEXT("qualified_ground_base"));
            Lineage->SetStringField(TEXT("base_result_sha256"),BaseHash);Lineage->SetStringField(TEXT("parent_lineage_sha256"),BaseHash);
            Lineage->SetStringField(TEXT("refinement_inputs_sha256"),InputsHash);
            Lineage->SetStringField(TEXT("source_admission_sha256"),Input->GetObjectField(TEXT("source_admission"))->GetStringField(TEXT("sha256")));
            Lineage->SetStringField(TEXT("ground_grid_sha256"),Defect==TEXT("lineage") ? BaseHash : Evidence->GetStringField(TEXT("grid_content_sha256")));
            Lineage->SetStringField(TEXT("construction_identity"),ConstructionHash);
            auto Manifest=Object();Manifest->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/cell-manifest-v1.json"));
            Manifest->SetNumberField(TEXT("schema_version"),1);Manifest->SetStringField(TEXT("grid_id"),GridId);Manifest->SetStringField(TEXT("cell_id"),Id);
            Manifest->SetNumberField(TEXT("cell_x"),FCString::Atoi(*X));Manifest->SetNumberField(TEXT("cell_y"),FCString::Atoi(*Y));
            Manifest->SetArrayField(TEXT("bounds"),Terrain->GetArrayField(TEXT("bounds")));Manifest->SetArrayField(TEXT("owned_feature_ids"),{});Manifest->SetArrayField(TEXT("referenced_feature_ids"),{});
            Manifest->SetStringField(TEXT("provenance_result"),TEXT("accepted"));Manifest->SetObjectField(TEXT("terrain_lineage"),Lineage);
            auto Artifact=[&](const FObject& D){auto A=Object();A->SetStringField(TEXT("path"),D->GetStringField(TEXT("path")));A->SetStringField(TEXT("content_hash"),D->GetStringField(TEXT("sha256")));return A;};
            auto TerrainArtifact=Artifact(T);auto Component=Terrain->GetArrayField(TEXT("raster_component_dependencies"))[0]->AsObject();
            TerrainArtifact->SetArrayField(TEXT("dependencies"),{MakeShared<FJsonValueString>(TEXT("base-compile:")+BaseHash),
                MakeShared<FJsonValueString>(TEXT("ground-base:")+InputsHash),MakeShared<FJsonValueString>(TEXT("source-admission:")+Input->GetObjectField(TEXT("source_admission"))->GetStringField(TEXT("sha256"))),
                MakeShared<FJsonValueString>(TEXT("raster-component:")+Component->GetStringField(TEXT("component_id"))+TEXT(":")+Component->GetStringField(TEXT("sample_semantic_sha256")))});
            Manifest->SetArrayField(TEXT("terrain_component_dependencies"),Terrain->GetArrayField(TEXT("raster_component_dependencies")));
            if (Index==0 && Defect==TEXT("dependency")) TerrainArtifact->SetArrayField(TEXT("dependencies"),{});
            Manifest->SetObjectField(TEXT("terrain_artifact"),TerrainArtifact);Manifest->SetObjectField(TEXT("feature_artifact"),Artifact(F));
            auto M=Write(FString::Printf(TEXT("canonical/cells/cell_%d.json"),Index),Manifest);if (!M) return false;
            auto Entry=Object();Entry->SetStringField(TEXT("cell_id"),Id);Entry->SetStringField(TEXT("path"),M->GetStringField(TEXT("path")));Entry->SetStringField(TEXT("sha256"),M->GetStringField(TEXT("sha256")));
            Entry->SetObjectField(TEXT("terrain_lineage"),Lineage);if (Defect!=TEXT("missing_cell") || Index+1<CellValues.Num()) CoverageCells.Add(MakeShared<FJsonValueObject>(Entry));
        }
        auto Coverage=Object();Coverage->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/coverage-manifest-v1.json"));Coverage->SetNumberField(TEXT("schema_version"),1);
        Coverage->SetStringField(TEXT("profile_id"),Profile->GetStringField(TEXT("profile_id")));Coverage->SetStringField(TEXT("world_data_plugin"),TEXT("ProjectWorldTestData"));
        Coverage->SetArrayField(TEXT("engine_georeference_origin"),Profile->GetArrayField(TEXT("engine_georeference_origin")));Coverage->SetStringField(TEXT("grid_id"),GridId);
        Coverage->SetObjectField(TEXT("grid"),Grid);Coverage->SetArrayField(TEXT("cells"),CoverageCells);
        auto Provenance=Object();Provenance->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/provenance-report-v1.json"));Provenance->SetNumberField(TEXT("schema_version"),1);
        Provenance->SetStringField(TEXT("profile_id"),Profile->GetStringField(TEXT("profile_id")));Provenance->SetStringField(TEXT("policy_result"),TEXT("accepted"));
        if (!Write(TEXT("canonical/coverage.json"),Coverage) || !Write(TEXT("reports/provenance.json"),Provenance)) return false;
        auto Result=Object();Result->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/refinement-result-v1.json"));Result->SetNumberField(TEXT("schema_version"),1);
        Result->SetStringField(TEXT("operation"),TEXT("refine"));Result->SetStringField(TEXT("status"),TEXT("accepted"));Result->SetStringField(TEXT("profile_id"),Profile->GetStringField(TEXT("profile_id")));
        Result->SetStringField(TEXT("inputs_hash"),InputsHash);Result->SetStringField(TEXT("base_result_sha256"),BaseHash);Result->SetStringField(TEXT("path_base"),TEXT("output_root"));Result->SetArrayField(TEXT("outputs"),Outputs);Result->SetArrayField(TEXT("errors"),{});
        Receipt=Root/TEXT("refinement_result.json");FString Text;FJsonSerializer::Serialize(Result.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Receipt);
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldGroundBaseInputTest,"Project.World.Terrain.QualifiedGroundBaseInput",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldGroundBaseInputTest::RunTest(const FString&)
{
    FString Path;FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("The source-faithful reduced whole-domain fixture writes."),ProjectWorldGroundBaseTest::WriteFixture(TEXT(""),Path))) return false;
    if (!TestTrue(FString::Printf(TEXT("Closed Ground input loads through the actual canonical reader: %s"),*Validation.Message),FProjectWorldCanonicalLoader::Load(Path,Bundle,Validation)))
    {AddError(Validation.ErrorCode+TEXT(": ")+Validation.Message);return false;}
    TestEqual(TEXT("All original reduced cells are retained."),Bundle.Cells.Num(),6);
    for (const auto& Cell : Bundle.Cells) TestFalse(TEXT("Unknown source accuracy remains unknown."),Cell.Terrain.bHasVerticalSourceAccuracy);
    if (!TestTrue(TEXT("A retained known-accuracy control writes."),ProjectWorldGroundBaseTest::WriteFixture(TEXT("known_positive"),Path)) ||
        !TestTrue(TEXT("Known source accuracy retains its actual value."),FProjectWorldCanonicalLoader::Load(Path,Bundle,Validation))) return false;
    for (const auto& Cell : Bundle.Cells) {TestTrue(TEXT("Known accuracy stays known."),Cell.Terrain.bHasVerticalSourceAccuracy);TestEqual(TEXT("Known accuracy is not fabricated or zeroed."),Cell.Terrain.VerticalSourceAccuracyMeters,.5);}
    for (const TCHAR* Defect : {TEXT("kind"),TEXT("mixed"),TEXT("feature"),TEXT("version"),TEXT("profile_version"),TEXT("frame"),TEXT("nodata"),TEXT("numeric_string"),TEXT("accuracy_string"),TEXT("source_ref"),TEXT("known_accuracy"),TEXT("fit_value"),TEXT("lineage"),TEXT("missing_cell"),TEXT("grid_alignment"),TEXT("residual_string"),TEXT("alignment_scale"),TEXT("halo_value"),TEXT("dependency")})
    {
        if (!TestTrue(TEXT("A fully resealed owned control writes."),ProjectWorldGroundBaseTest::WriteFixture(Defect,Path))) return false;
        TestFalse(FString::Printf(TEXT("Canonical Ground binding refuses %s."),Defect),FProjectWorldCanonicalLoader::Load(Path,Bundle,Validation));
        TestFalse(TEXT("Refusal retains a diagnostic."),Validation.ErrorCode.IsEmpty());
    }
    return !HasAnyErrors();
}
#endif
