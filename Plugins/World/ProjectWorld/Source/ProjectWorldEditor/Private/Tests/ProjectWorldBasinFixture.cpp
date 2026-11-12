// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldBasinFixture.h"
#include "ProjectWorldRuntimePartitionPolicy.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldPresentationMaterialBinding.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinFixture
{
    bool ReadStreamingRange(UWorld& World,int32& Range,FString& Error)
    {
        Range=0;FProjectWorldRuntimePartitionSettings Settings;
        if (!ProjectWorldRuntimePartitionPolicy::Read(&World,Settings,Error)) return false;
        if (Settings.LoadingRangeCentimeters<=0) {Error=TEXT("The actual native streaming range is unknown or nonpositive.");return false;}
        Range=Settings.LoadingRangeCentimeters;return true;
    }
    bool AddWater(const FJsonObject& Document,FProjectWorldCanonicalBundle& Bundle,FString& Error)
    {
        if (Bundle.Cells.Num()!=1 || !Bundle.Features.IsEmpty() || !Bundle.Cells[0].OwnedFeatureIds.IsEmpty())
        {Error=TEXT("The captured basin requires one complete cell with no preexisting feature ownership.");return false;}
        FProjectWorldCanonicalFeature Feature;Feature.FeatureId=TEXT("typed/identified_basin");Feature.FeatureClass=TEXT("water");
        Feature.OwnerCellId=Bundle.Cells[0].CellId;
        const TSharedPtr<FJsonObject>* Water=nullptr;const TSharedPtr<FJsonObject>* Geometry=nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Rings=nullptr;const TArray<TSharedPtr<FJsonValue>>* Ring=nullptr;
        if (!Document.TryGetObjectField(TEXT("modeled_water"),Water) || !Water || !Water->IsValid())
        {Error=TEXT("The captured basin requires its explicit model object.");return false;}
        const auto Number=[](const TSharedPtr<FJsonValue>& Value,double& Out)
        {return Value.IsValid() && Value->Type==EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out);};
        auto& Surface=Feature.WaterSurface;Surface.bValid=true;Surface.Behavior=TEXT("standing");
        Surface.Geometry=TEXT("polygon");Surface.Role=TEXT("area");Surface.SurfaceGroupId=Feature.FeatureId;
        Surface.SurfaceGroupMembers={Feature.FeatureId};double Version=0,Spill=0;FString GeometryType;
        if (!(*Water)->TryGetStringField(TEXT("function_id"),Surface.FunctionId) || Surface.FunctionId!=TEXT("identified_static_lined_basin") ||
            !Number((*Water)->TryGetField(TEXT("function_version")),Version) || Version!=1 ||
            !Number((*Water)->TryGetField(TEXT("level_m")),Surface.LevelMeters) ||
            !Number((*Water)->TryGetField(TEXT("spill_level_m")),Spill) || Spill<=Surface.LevelMeters ||
            !(*Water)->TryGetObjectField(TEXT("geometry"),Geometry) || !Geometry || !Geometry->IsValid() ||
            !(*Geometry)->TryGetStringField(TEXT("type"),GeometryType) || GeometryType!=TEXT("Polygon") ||
            !(*Geometry)->TryGetArrayField(TEXT("coordinates"),Rings) || !Rings || Rings->Num()!=1 ||
            !(*Rings)[0].IsValid() || !(*Rings)[0]->TryGetArray(Ring) || !Ring || Ring->Num()!=46)
        {Error=TEXT("The typed fixture must identify the complete versioned inferred basin model.");return false;}
        Surface.FunctionVersion=1;Surface.ContainmentSpillMeters=Spill;
        FProjectWorldCanonicalPolygon Polygon;
        for (const auto& Value : *Ring)
        {
            const TArray<TSharedPtr<FJsonValue>>* P=nullptr;double X=0,Y=0;
            if (!Value.IsValid() || !Value->TryGetArray(P) || !P || P->Num()!=2 || !Number((*P)[0],X) || !Number((*P)[1],Y))
            {Error=TEXT("The captured basin contains unknown coordinates.");return false;}
            Polygon.Outer.Add(FVector2D(X,Y));
        }
        if (Polygon.Outer[0]!=Polygon.Outer.Last()) {Error=TEXT("The complete captured basin ring must be closed.");return false;}
        Polygon.Outer.Pop();
        Feature.GeometryPolygons.Add(MoveTemp(Polygon));
        Bundle.Cells[0].OwnedFeatureIds={Feature.FeatureId};Bundle.Features.Add(Feature.FeatureId,MoveTemp(Feature));return true;
    }
    bool CheckRefusals(const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle,FAutomationTestBase& Test)
    {
        const auto Refuses=[&](const FJsonObject& Invalid)
        {
            auto Probe=Bundle;FString Error;
            const bool Refused=!AddWater(Invalid,Probe,Error);
            return Test.TestTrue(TEXT("Invalid captured Water refuses observably without changing typed feature ownership."),
                Refused && !Error.IsEmpty() && Probe.Features.IsEmpty() && Probe.Cells.Num()==1 && Probe.Cells[0].OwnedFeatureIds.IsEmpty() &&
                Probe.Cells[0].CellId==Bundle.Cells[0].CellId && Probe.Cells[0].Bounds==Bundle.Cells[0].Bounds);
        };
        FJsonObject Missing=Document;Missing.SetNumberField(TEXT("modeled_water"),0);
        if (!Refuses(Missing)) return false;
        for (int32 Case=0;Case<4;++Case)
        {
            FJsonObject Invalid=Document;auto Water=MakeShared<FJsonObject>(*Document.GetObjectField(TEXT("modeled_water")));
            if (Case==0) Water->SetNumberField(TEXT("function_version"),1.5);
            if (Case==1) Water->SetStringField(TEXT("level_m"),TEXT("20.2"));
            if (Case==2) Water->SetNumberField(TEXT("spill_level_m"),std::numeric_limits<double>::quiet_NaN());
            if (Case==3)
            {
                auto Geometry=MakeShared<FJsonObject>(*Water->GetObjectField(TEXT("geometry")));
                auto Rings=Geometry->GetArrayField(TEXT("coordinates"));auto Ring=Rings[0]->AsArray();
                auto Point=Ring[0]->AsArray();Point[0]=MakeShared<FJsonValueString>(TEXT("586193.88"));
                Ring[0]=MakeShared<FJsonValueArray>(Point);Rings[0]=MakeShared<FJsonValueArray>(Ring);
                Geometry->SetArrayField(TEXT("coordinates"),Rings);Water->SetObjectField(TEXT("geometry"),Geometry);
            }
            Invalid.SetObjectField(TEXT("modeled_water"),Water);if (!Refuses(Invalid)) return false;
        }
        return true;
    }
    bool ResolveWaterMaterial(UMaterialInterface*& Material,TSharedPtr<FJsonObject>& Identity,FString& Error)
    {
        Material=nullptr;Identity.Reset();FProjectWorldMaterialReference Reference;
        if (!ProjectWorldPresentationMaterialBinding::Resolve(TEXT("water.calm_river"),Reference,Error)) return false;
        Material=Reference.Material;Identity=MakeShared<FJsonObject>();
        Identity->SetStringField(TEXT("binding_id"),TEXT("water.calm_river"));
        Identity->SetStringField(TEXT("object_path"),Reference.ObjectPath);
        Identity->SetStringField(TEXT("package_sha256"),Reference.PackageSha256);
        Identity->SetStringField(TEXT("semantic_identity"),Reference.SemanticIdentity);
        Identity->SetStringField(TEXT("manifest_sha256"),Reference.ManifestSha256);return true;
    }
}
#endif
