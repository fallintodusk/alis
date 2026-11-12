#include "ProjectWorldBuildingSelection.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldSchemaReference.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldBuildingSelection
{
    namespace
    {
        bool String(const FJsonObject& O,const TCHAR* Key,FString& Value)
        {const auto V=O.TryGetField(Key);return V && V->Type==EJson::String && V->TryGetString(Value);}
        bool Relative(const FString& Path)
        {return FPaths::IsRelative(Path) && !Path.Contains(TEXT("..")) && !Path.Contains(TEXT("\\")) && !Path.Contains(TEXT(":")) && Path.EndsWith(TEXT(".json"));}
        bool Scratch(const FProjectWorldRealizationProfile& Profile,const FString& Path)
        {return Profile.WorldDataPluginName==TEXT("ProjectWorldTestData") &&
            Path.StartsWith(TEXT("tmp/world/inputs/"),ESearchCase::CaseSensitive) &&
            FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world/inputs")));}
    }
    bool ValidateSetting(const FString& Value)
    {return Relative(Value) && (Value.StartsWith(TEXT("BuildingAssembly/"),ESearchCase::CaseSensitive) || Value.StartsWith(TEXT("tmp/world/inputs/"),ESearchCase::CaseSensitive));}
    bool Load(const FProjectWorldLayerInputs& Inputs,FSet& Out,FString& Error)
    {
        Out={};TSharedPtr<FJsonObject> Settings;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Inputs.Layer.NormalizedSettings),Settings) || !Settings)
        {Error=TEXT("Building settings cannot be read.");return false;}
        if (!Settings->HasField(TEXT("assembly_source"))) return true;
        FString Source;FProjectWorldDataRoots Roots;
        if (!String(*Settings,TEXT("assembly_source"),Source) || !ValidateSetting(Source) ||
            !FProjectWorldDataRoots::Resolve(Inputs.Profile.WorldDataPluginName,Roots,Error)) return false;
        const bool bScratch=Scratch(Inputs.Profile,Source);
        const FString Path=FPaths::ConvertRelativePathToFull((bScratch ? FPaths::ProjectDir() : Roots.DataRoot)/Source);
        if ((!bScratch && Source.StartsWith(TEXT("tmp/"))) || !FPaths::IsUnderDirectory(Path,bScratch ? FPaths::ProjectDir()/TEXT("tmp/world") : Roots.DataRoot))
        {Error=TEXT("Assembly selection escapes its data owner.");return false;}
        FString Text,Schema;TSharedPtr<FJsonObject> D;const TArray<TSharedPtr<FJsonValue>>* Records=nullptr;
        if (!FFileHelper::LoadFileToString(Text,*Path) || Text.Len()>65536 ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),D) || !D || D->Values.Num()!=3 ||
            !String(*D,TEXT("$schema"),Schema) || !ProjectWorldSchemaReference::ResolvesToCanonical(Path,Schema,TEXT("project_world_building_selection.schema.json"),Error) ||
            !D->HasTypedField<EJson::Number>(TEXT("schema_version")) || D->GetNumberField(TEXT("schema_version"))!=1 ||
            !D->TryGetArrayField(TEXT("assemblies"),Records) || Records->Num()>5)
        {Error=TEXT("Assembly selection must be a closed, bounded version-one document.");return false;}
        TSet<FString> Seen;
        for (const auto& Record : *Records)
        {
            const TSharedPtr<FJsonObject>* O=nullptr;FSelected S;FString KitSource,Before;
            if (!Record->TryGetObject(O) || !O || !*O || (*O)->Values.Num()!=2 || !String(**O,TEXT("feature_id"),S.FeatureId) ||
                !String(**O,TEXT("kit_source"),KitSource) || !Relative(KitSource) || Seen.Contains(S.FeatureId) ||
                (!KitSource.StartsWith(TEXT("Plugins/World/ProjectBuildingAssembly/Data/Kits/")) && !Scratch(Inputs.Profile,KitSource)))
            {Error=TEXT("Assembly selection has a duplicate identity or unsupported kit source.");return false;}
            const auto* Feature=Inputs.Bundle.Features.Find(S.FeatureId);
            if (!Feature || Feature->FeatureClass!=TEXT("building")) {Error=TEXT("Selected canonical building is unavailable.");return false;}
            const FString KitPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/KitSource);
            if (!FProjectSha256::HashFile(KitPath,Before) || !ProjectBuildingAssembly::ReadKit(KitPath,S.Kit,Error) ||
                !FProjectSha256::HashFile(KitPath,S.KitHash) || Before!=S.KitHash)
            {if (Error.IsEmpty()) Error=TEXT("Kit source changed or could not be authenticated.");return false;}
            S.CellId=Feature->OwnerCellId;Seen.Add(S.FeatureId);Out.Buildings.Add(MoveTemp(S));
        }
        Out.Buildings.Sort([](const auto& A,const auto& B){return A.FeatureId<B.FeatureId;});Out.bConfigured=true;return true;
    }
    bool Prepare(const FProjectWorldLayerInputs& Inputs,const FSelected& Selection,ProjectWorldBuildingAssembly::FPrepared& Out,FString& Error)
    {
        const auto* Cell=Inputs.Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Selection.CellId;});
        if (!Cell) {Error=TEXT("Selected building has no canonical owner cell.");return false;}
        return ProjectWorldBuildingAssembly::Prepare(Inputs.Bundle,*Cell,Inputs.AuthoredOverlays,Selection.FeatureId,Selection.Kit,Out,Error,false);
    }
    bool HashCell(const FProjectWorldLayerInputs& Inputs,const FSet& Set,const FProjectWorldCanonicalCell& Cell,FString& Hash,FString& Error)
    {
        if (!ProjectWorldBuildingRealization::HashCellInput(Inputs.Bundle,Cell,Inputs.Layer,Inputs.AuthoredOverlays,Hash,Error)) return false;
        FString Identity=Hash;bool Selected=false;
        for (const auto& S : Set.Buildings) if (S.CellId==Cell.CellId)
        {
            ProjectWorldBuildingAssembly::FPrepared P;if (!Prepare(Inputs,S,P,Error)) return false;
            Identity+=TEXT("|")+S.FeatureId+TEXT("|")+S.KitHash+TEXT("|")+
                ProjectBuildingAssembly::GeometryIdentity(S.FeatureId,P.Plan,S.Kit,P.Transform);Selected=true;
        }
        if (!Selected) return true;
        return FProjectSha256::HashNormalizedText(Identity,Hash);
    }
    bool BuildCell(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FProjectWorldAuthoredOverlaySet& Overlays,const FProjectWorldBuildingSettings& Settings,
        const FSet& Set,FProjectWorldBuildingMeshBuildResult& Out,FString& Error)
    {
        TSet<FString> Union;
        for (const auto& S : Set.Buildings) if (S.CellId==Cell.CellId)
        {
            ProjectWorldBuildingAssembly::FPrepared P;
            if (!ProjectWorldBuildingAssembly::Prepare(Bundle,Cell,Overlays,S.FeatureId,S.Kit,P,Error,false)) return false;
            Union.Add(S.FeatureId);
        }
        return ProjectWorldBuildingMeshBuilder::BuildCell(Bundle,Cell,Overlays,Settings,Out,Error,&Union);
    }
}
