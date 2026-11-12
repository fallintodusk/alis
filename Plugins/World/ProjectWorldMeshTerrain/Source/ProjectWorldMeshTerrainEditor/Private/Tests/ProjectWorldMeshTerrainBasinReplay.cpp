// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "ProjectWorldMeshTerrainCoastalFixture.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    namespace
    {
        bool Separate(const FString& Source,const FString& Root)
        {
            return !FPaths::IsSamePath(Source,Root) && !FPaths::IsUnderDirectory(Root,Source) && !FPaths::IsUnderDirectory(Source,Root);
        }
        bool ContentFile(const FString& Path)
        {
            if (!Path.StartsWith(TEXT("runtime_plugin/Content/"))) return false;
            for (const TCHAR* Extension : {TEXT(".uasset"),TEXT(".umap"),TEXT(".ubulk"),TEXT(".uexp"),TEXT(".uptnl")})
                if (Path.EndsWith(Extension)) return true;
            return false;
        }
        bool Inventory(const FString& Root,const FString& Plugin,const TSet<FString>& Declared)
        {
            TSet<FString> Actual;Actual.Add(TEXT("sections.json"));int32 Visited=0;bool Valid=true;
            FString Prefix=Root;FPaths::NormalizeDirectoryName(Prefix);Prefix+=TEXT("/");
            const bool Complete=IFileManager::Get().IterateDirectoryRecursively(*(Root/TEXT("runtime_plugin")),
                [&](const TCHAR* Filename,bool Directory)
                {
                    if (++Visited>4096) {Valid=false;return false;}
                    if (Directory) return true;
                    FString Path=FPaths::ConvertRelativePathToFull(Filename);FPaths::NormalizeFilename(Path);
                    if (!FPaths::MakePathRelativeTo(Path,*Prefix) || Actual.Num()>=1024 ||
                        (!ContentFile(Path) && Path!=TEXT("runtime_plugin/")+Plugin+TEXT(".uplugin")) || !Declared.Contains(Path))
                    {Valid=false;return false;}
                    Actual.Add(Path);return true;
                });
            return Complete && Valid && Actual.Num()==Declared.Num();
        }
    }
    bool Replay(FAutomationTestBase& Test,const FString& Root,FProjectWorldCanonicalBundle Bundle,
        const FJsonObject& Document,const FString& FixtureHash,const FString& DefinitionHash,const bool Coastal)
    {
        const FString Manifest=FPaths::ConvertRelativePathToFull(FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_REPLAY")));
        const FString ExpectedHash=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_REPLAY_SHA256"));
        const FString Tmp=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"));
        auto Fail=[&](const TCHAR* Message){Test.AddError(Message);return false;};
        FString Hash,Text,Source,Plugin,MapHash;
        TSharedPtr<FJsonObject> Input;double Version=0;const TArray<TSharedPtr<FJsonValue>>* Files=nullptr;
        if (!FPaths::IsUnderDirectory(Manifest,Tmp) || ExpectedHash.Len()!=64 ||
            !FProjectWorldCanonicalLoader::ComputeFileSha256(Manifest,Hash) || Hash!=ExpectedHash ||
            !FFileHelper::LoadFileToString(Text,*Manifest) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Input) ||
            !Input.IsValid() || !Input->TryGetNumberField(TEXT("schema_version"),Version) || Version!=1 ||
            !Input->TryGetStringField(TEXT("source_root"),Source) || !Input->TryGetStringField(TEXT("plugin"),Plugin) ||
            !Input->TryGetStringField(TEXT("map_sha256"),MapHash) || MapHash.Len()!=64 ||
            !Input->TryGetArrayField(TEXT("files"),Files) || !Files || Files->IsEmpty() || Files->Num()>1024)
            return Fail(TEXT("Saved-scene replay requires an authenticated bounded scratch manifest."));
        Source=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Source);
        FPaths::NormalizeDirectoryName(Source);
        bool Plain=Plugin.StartsWith(TEXT("ProjectWorldBasinSections")) && Plugin.Len()<96;
        for (TCHAR C : Plugin) Plain &= FChar::IsAlnum(C);
        if (!Plain || !FPaths::IsUnderDirectory(Source,Tmp) || !FPaths::IsUnderDirectory(Root,Tmp) ||
            !Separate(Source,Root))
            return Fail(TEXT("Replay source, destination and plugin must have distinct confined ownership."));
        TArray<FString> Relative;TSet<FString> Unique;TMap<FString,FString> Digests;int64 Bytes=0;
        for (const auto& Value : *Files)
        {
            const TSharedPtr<FJsonObject>* Row=nullptr;FString Path,Expected;double Size=0;
            if (!Value.IsValid() || !Value->TryGetObject(Row) || !Row || !Row->IsValid() ||
                !(*Row)->TryGetStringField(TEXT("path"),Path) || !(*Row)->TryGetStringField(TEXT("sha256"),Expected) ||
                !(*Row)->TryGetNumberField(TEXT("bytes"),Size) || !FMath::IsFinite(Size) || Size<0 || Size>64*1024*1024 || Size!=FMath::FloorToDouble(Size) ||
                Path.IsEmpty() || Path.Contains(TEXT("..")) || Path.Contains(TEXT("\\")) || Path.Contains(TEXT(":")) || Unique.Contains(Path) ||
                (Path!=TEXT("sections.json") && Path!=TEXT("runtime_plugin/")+Plugin+TEXT(".uplugin") && !ContentFile(Path)) ||
                !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Source/Path),Source) ||
                !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Root/Path),Root) ||
                IFileManager::Get().FileSize(*(Source/Path))!=int64(Size) || Expected.Len()!=64 ||
                !FProjectWorldCanonicalLoader::ComputeFileSha256(Source/Path,Hash) || Hash!=Expected)
                return Fail(TEXT("Replay refuses missing, stale, duplicate, oversized or escaping saved inputs before copying."));
            Bytes+=int64(Size);if (Bytes>64*1024*1024) return Fail(TEXT("Replay exceeds its declared saved-fixture byte budget."));
            Unique.Add(Path);Relative.Add(Path);Digests.Add(Path,Expected);
        }
        if (!Unique.Contains(TEXT("sections.json")) || !Unique.Contains(TEXT("runtime_plugin/Content/SectionMap.umap")) ||
            !Unique.Contains(TEXT("runtime_plugin/")+Plugin+TEXT(".uplugin"))) return Fail(TEXT("Replay requires the exact native descriptor, saved map and section observations."));
        if (!Inventory(Source,Plugin,Unique)) return Fail(TEXT("Replay source inventory differs from the complete declared saved inputs."));
        Test.TestFalse(TEXT("Replay ownership rejects a destination inside its source."),Separate(Source,Source/TEXT("nested")));
        Test.TestFalse(TEXT("Replay ownership rejects a source inside its destination."),Separate(Root/TEXT("nested"),Root));
        TSet<FString> Missing=Unique;bool Removed=false;
        for (const auto& Path : Relative) if (Path.EndsWith(TEXT(".uasset"))) {Missing.Remove(Path);Removed=true;break;}
        if (!Test.TestTrue(TEXT("The saved-input control omits an existing native package while all other source files remain."),Removed &&
            !Inventory(Source,Plugin,Missing))) return false;
        for (const auto& Path : Relative)
        {
            if (IFileManager::Get().FileExists(*(Root/Path)) || !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Root/Path),true) ||
                IFileManager::Get().Copy(*(Root/Path),*(Source/Path),false,true)!=COPY_OK ||
                !FProjectWorldCanonicalLoader::ComputeFileSha256(Root/Path,Hash) || Hash!=Digests.FindChecked(Path))
                return Fail(TEXT("Cannot copy authenticated saved inputs into the fresh disposable output."));
        }
        FInput Scene;Scene.Root=Root;Scene.Mount=TEXT("/")+Plugin+TEXT("/");Scene.Directory=Root/TEXT("runtime_plugin/Content/");
        Scene.PackageName=Scene.Mount+TEXT("SectionMap");Scene.PluginManaged=true;Scene.NavigationOnly=true;Scene.MapSha256=MapHash;
        const FString Full=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_REPLAY_FULL"));
        Scene.StreamingOnly=Coastal && Full==TEXT("streaming");
        if (!Full.IsEmpty() && Full!=TEXT("1") && !Scene.StreamingOnly)
            return Fail(TEXT("Saved-scene replay requires literal one, or coastal-only streaming diagnosis."));
        Scene.NavigationOnly=Full!=TEXT("1") && !Scene.StreamingOnly;
        if (Coastal && Scene.NavigationOnly) return Fail(TEXT("Coastal replay requires its full declared joined execution."));
        if (!FFileHelper::LoadFileToString(Text,*(Root/TEXT("sections.json"))) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Scene.Expected) || !Scene.Expected.IsValid())
            return Fail(TEXT("Replay section observations cannot be read."));
        FString OriginalFixture,OriginalDefinition;
        if (!Scene.Expected->TryGetStringField(TEXT("fixture_sha256"),OriginalFixture) || OriginalFixture!=FixtureHash ||
            !Scene.Expected->TryGetStringField(TEXT("source_definition_sha256"),OriginalDefinition) || OriginalDefinition!=DefinitionHash)
            return Fail(TEXT("Replay refuses changed typed fixture or accepted definition source."));
        Scene.Expected->SetStringField(TEXT("replay_manifest_sha256"),ExpectedHash);
        FString Error;
        if (Coastal)
        {
            if (!Scene.Expected->HasTypedField<EJson::Array>(TEXT("coastal_upper")) || !Scene.Expected->HasTypedField<EJson::Object>(TEXT("water")) ||
                !Scene.Expected->GetObjectField(TEXT("water"))->HasTypedField<EJson::Array>(TEXT("actors")))
                return Fail(TEXT("Coastal replay requires complete current saved Upper and Water bindings."));
            for (const auto& Row : Scene.Expected->GetArrayField(TEXT("variants")))
                if (!Row->AsObject()->HasTypedField<EJson::Array>(TEXT("native_sections")))
                    return Fail(TEXT("Coastal replay requires current native section bindings."));
            const auto Context=Document.GetObjectField(TEXT("coastal_context"));
            if (!ProjectWorldCoastalFixture::AddOriginalWater(Context->GetStringField(TEXT("base_result")),
                Context->GetStringField(TEXT("base_result_sha256")),Bundle,Error)) {Test.AddError(Error);return false;}
        }
        else if (Scene.Expected->HasField(TEXT("water")) && !ProjectWorldBasinFixture::AddWater(Document,Bundle,Error)) {Test.AddError(Error);return false;}
        Scene.Bundle=MoveTemp(Bundle);const auto& B=Scene.Bundle.Cells[0].Bounds;
        Scene.Start=FProjectWorldCanonicalLoader::CanonicalToUnreal(Scene.Bundle,FVector(B.X+3,B.Y+3,21));
        if (!ProjectWorldConstructedRuntimeTest::ReadPawnSelection(Test,Scene.Pawn) || !Scene.Pawn.GameModeClass ||
            !PrepareNativePlugin(Test,Scene)) return false;
        for (const auto& Path : Relative)
            if (!FProjectWorldCanonicalLoader::ComputeFileSha256(Source/Path,Hash) || Hash!=Digests.FindChecked(Path) ||
                !FProjectWorldCanonicalLoader::ComputeFileSha256(Root/Path,Hash) || Hash!=Digests.FindChecked(Path))
                return Fail(TEXT("Replay source or copied input changed during native plugin preparation."));
        if (!Inventory(Source,Plugin,Unique) || !Inventory(Root,Plugin,Unique))
            return Fail(TEXT("Replay source or copied file census changed during native plugin preparation."));
        for (const auto& Path : Relative) if (Path.StartsWith(TEXT("runtime_plugin/Content/")) && (Path.EndsWith(TEXT(".uasset")) || Path.EndsWith(TEXT(".umap"))))
        {
            FString Name;
            if (!FPackageName::TryConvertFilenameToLongPackageName(Root/Path,Name) || !Name.StartsWith(Scene.Mount))
                return Fail(TEXT("Replay package inventory cannot resolve through the exact native mount."));
            Scene.Packages.Add(Name);
        }
        if (!Scene.Packages.Contains(Scene.PackageName) || !FProjectWorldCanonicalLoader::ComputeFileSha256(
            Root/TEXT("runtime_plugin/Content/SectionMap.umap"),Hash) || Hash!=MapHash)
            return Fail(TEXT("Replay saved map identity is stale."));
        Test.AddInfo(FString::Printf(TEXT("BasinReplay files=%d bytes=%lld manifest=%s; copied saved bytes, no builder or save"),Relative.Num(),Bytes,*ExpectedHash));
        return Coastal ? ProjectWorldCoastalSceneTest::Queue(Test,MoveTemp(Scene),Document) : Queue(Test,MoveTemp(Scene));
    }
}
#endif
