// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureGradedParticleControls.h"
#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"
#include "Utilities/ProjectSha256.h"

bool ProjectTextureGradedParticleControls::Parse(const TSharedPtr<FJsonObject>& Root,TMap<FName,double>& Out,FString& Error)
{
    TMap<FString,FVector2D> Bounds={
        {TEXT("PatchFrequency"),{2,256}}, {TEXT("MicroFrequency"),{2,256}},
        {TEXT("BaseMicroHeight"),{0,1}}, {TEXT("FacetStrength"),{0,1}},
        {TEXT("EdgeMicroHeight"),{0,1}}, {TEXT("AggregateSurfaceGrain"),{0,1}}, {TEXT("RaisedGrainHeight"),{0,1}}};
    for (const TCHAR* Band : {TEXT("Large"),TEXT("Medium"),TEXT("Small"),TEXT("Grain")})
    {
        Bounds.Add(FString(Band)+TEXT("Frequency"),{2,256});
        Bounds.Add(FString(Band)+TEXT("Profile"),{.05,4});
        Bounds.Add(FString(Band)+TEXT("Density"),{0,1});
        Bounds.Add(FString(Band)+TEXT("Height"),{0,1});
    }
    const TSharedPtr<FJsonObject>* Values=nullptr; Out.Reset();
    if (!Root || !Root->TryGetObjectField(TEXT("structure"),Values) || !Values || (*Values)->Values.Num()!=Bounds.Num())
    { Error=TEXT("Graded particles require every structural control exactly once."); return false; }
    for (const auto& Bound : Bounds)
    {
        double V=0;
        if (!(*Values)->TryGetNumberField(Bound.Key,V) || !FMath::IsFinite(V) || V<Bound.Value.X || V>Bound.Value.Y ||
            (Bound.Key.EndsWith(TEXT("Frequency")) && (V!=FMath::FloorToDouble(V) || !FMath::IsPowerOfTwo(int32(V)))))
        { Out.Reset(); Error=TEXT("Invalid graded-particle control: ")+Bound.Key; return false; }
        Out.Add(FName(Bound.Key),V);
    }
    return true;
}

bool ProjectTextureGradedParticleControls::Validate(const TMap<FName,double>& Controls,FString& Error)
{
    auto Root=MakeShared<FJsonObject>(); auto Values=MakeShared<FJsonObject>();
    for (const auto& C : Controls) Values->SetNumberField(C.Key.ToString(),C.Value);
    Root->SetObjectField(TEXT("structure"),Values);
    TMap<FName,double> Checked;
    return Parse(Root,Checked,Error);
}

bool ProjectTextureGradedParticleControls::NativeFilterIdentity(FString& Identity,FString& Error)
{
    FString Filename;
    if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(FilterFunctionPath)),&Filename) ||
        !FProjectSha256::HashFile(Filename,Identity))
    { Error=TEXT("Native graded-particle filter cannot be authenticated."); return false; }
    return true;
}
