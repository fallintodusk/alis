// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureParticleControls.h"
#include "Dom/JsonObject.h"

bool ProjectTextureParticleControls::Parse(const TSharedPtr<FJsonObject>& Root,TMap<FName,double>& Out,FString& Error)
{
	const TSharedPtr<FJsonObject>* Controls=nullptr;
	const TMap<FString,FVector2D> Bounds={
		{TEXT("WarpFrequency"),{1,256}}, {TEXT("FineFrequency"),{1,256}}, {TEXT("CoarseFrequency"),{1,256}},
		{TEXT("MicroFrequency"),{1,256}}, {TEXT("PatchFrequency"),{1,256}}, {TEXT("WarpAmplitude"),{0,.05}},
		{TEXT("GrainRadius"),{.1,2}}, {TEXT("GrainRadiusVariation"),{0,1}}, {TEXT("GrainContrast"),{0,4}},
		{TEXT("FineOccupancyThreshold"),{0,1}}, {TEXT("FineOccupancyContrast"),{0,4}},
		{TEXT("CoarseOccupancyThreshold"),{0,1}}, {TEXT("CoarseOccupancyContrast"),{0,4}},
		{TEXT("FineReliefBase"),{0,1}}, {TEXT("FineReliefVariation"),{0,1}}, {TEXT("CoarseRadiusFactor"),{.1,8}},
		{TEXT("CoarseMix"),{0,1}}, {TEXT("HeightAggregate"),{0,1}}, {TEXT("HeightMicro"),{0,1}},
		{TEXT("PigmentAggregate"),{0,2}}, {TEXT("PigmentPatch"),{0,1}}, {TEXT("RoughnessMicroMix"),{0,1}}};
	Out.Reset();
	if (!Root->TryGetObjectField(TEXT("structure"),Controls) || !Controls || (*Controls)->Values.Num()!=Bounds.Num())
	{ Error=TEXT("Particle patterns require every bounded structural control exactly once."); return false; }
	for (const auto& Entry : Bounds)
	{
		double V=0;
		if (!(*Controls)->TryGetNumberField(Entry.Key,V) || !FMath::IsFinite(V) || V<Entry.Value.X || V>Entry.Value.Y ||
			(Entry.Key.EndsWith(TEXT("Frequency")) && V!=FMath::FloorToDouble(V)))
		{ Error=TEXT("Invalid or missing structural control: ")+Entry.Key; Out.Reset(); return false; }
		Out.Add(FName(Entry.Key),V);
	}
	return true;
}

FString ProjectTextureParticleControls::Normalize(const TMap<FName,double>& Values)
{
	TArray<FName> Keys; Values.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
	FString Result;
	for (FName Key : Keys) Result+=FString::Printf(TEXT("|structure:%s=%.17g"),*Key.ToString(),Values.FindChecked(Key));
	return Result;
}

bool ProjectTextureParticleControls::Validate(const TMap<FName,double>& Values,FString& Error)
{
	auto Root=MakeShared<FJsonObject>(); auto Fields=MakeShared<FJsonObject>();
	for (const auto& V : Values) Fields->SetNumberField(V.Key.ToString(),V.Value);
	Root->SetObjectField(TEXT("structure"),Fields);
	TMap<FName,double> Checked;
	return Parse(Root,Checked,Error);
}
