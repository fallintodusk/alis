// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldContainedWaterParsing.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldContainedWaterParsing
{
    bool Read(const TSharedPtr<FJsonObject>& Function)
    {
        if (!Function.IsValid() || Function->Values.Num()!=16) return false;
        const TPair<const TCHAR*,const TCHAR*> Constants[]={
            {TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/contained-water-v1.json")},
            {TEXT("function_id"),TEXT("contained_ground_quantile")},
            {TEXT("vertical_basis"),TEXT("qualified_ground_tin")},
            {TEXT("accuracy"),TEXT("derived_not_surveyed")}};
        for (const auto& Pair : Constants)
        {
            const auto Value=Function->TryGetField(Pair.Key);
            FString Text;
            if (!Value.IsValid() || Value->Type!=EJson::String || !Value->TryGetString(Text) || Text!=Pair.Value) return false;
        }
        for (const TCHAR* Key : {TEXT("ground_tin_sha256"),TEXT("context_sha256"),
            TEXT("wet_geometry_sha256"),TEXT("baseline_function_sha256"),TEXT("mapped_geometry_sha256")})
        {
            const auto Value=Function->TryGetField(Key);
            FString Hash;
            if (!Value.IsValid() || Value->Type!=EJson::String || !Value->TryGetString(Hash) || Hash.Len()!=64) return false;
            for (const TCHAR C : Hash) if (!((C>=TEXT('0') && C<=TEXT('9')) || (C>=TEXT('a') && C<=TEXT('f')))) return false;
        }
        const auto ModelValue=Function->TryGetField(TEXT("footprint_model"));
        FString Model;
        if (!ModelValue.IsValid() || ModelValue->Type!=EJson::String || !ModelValue->TryGetString(Model) ||
            (Model!=TEXT("mapped_containment") && Model!=TEXT("observed_ground_component"))) return false;
        double Numbers[6]={};
        const TCHAR* Keys[]={TEXT("function_version"),TEXT("level_m"),TEXT("spill_level_m"),
            TEXT("sample_count"),TEXT("quantile"),TEXT("quantization_m")};
        for (int32 I=0;I<6;++I)
        {
            const auto Value=Function->TryGetField(Keys[I]);
            if (!Value.IsValid() || Value->Type!=EJson::Number || !Value->TryGetNumber(Numbers[I]) || !FMath::IsFinite(Numbers[I])) return false;
        }
        return Numbers[0]==1 && Numbers[1]<Numbers[2] && Numbers[3]>=1 &&
            Numbers[3]==FMath::FloorToDouble(Numbers[3]) && Numbers[4]>=0 && Numbers[4]<=1 && Numbers[5]>0;
    }
}
