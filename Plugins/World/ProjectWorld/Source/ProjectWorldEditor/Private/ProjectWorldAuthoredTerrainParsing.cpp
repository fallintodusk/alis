#include "ProjectWorldAuthoredTerrainParsing.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldAuthoredTerrainParsing
{
	namespace
	{
		using FObject=TSharedPtr<FJsonObject>;
		bool String(const FObject& Object,const TCHAR* Field,FString& Out)
		{
			const auto Value=Object ? Object->TryGetField(Field) : nullptr;
			return Value && Value->Type==EJson::String && Value->TryGetString(Out) && !Out.IsEmpty();
		}
		bool Number(const FObject& Object,const TCHAR* Field,double& Out)
		{
			const auto Value=Object ? Object->TryGetField(Field) : nullptr;
			return Value && Value->Type==EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out);
		}
		bool Identifier(const FString& Value)
		{
			if (Value.IsEmpty()) return false;
			for (TCHAR C : Value) if (!((C>='a' && C<='z') || (C>='0' && C<='9') || C=='_')) return false;
			return true;
		}
		bool Hash(const FString& Value)
		{
			if (Value.Len()!=64) return false;
			for (TCHAR C : Value) if (!((C>='a' && C<='f') || (C>='0' && C<='9'))) return false;
			return true;
		}
		bool Object(const FObject& Parent,const TCHAR* Field,FObject& Out)
		{
			const FObject* Value=nullptr;
			if (!Parent || !Parent->TryGetObjectField(Field,Value) || !Value || !*Value) return false;
			Out=*Value;return true;
		}
	}
	bool ReadInput(const FObject& Operation,const FObject& Input,const FString& HashValue,FRetainedInputs& Out)
	{
		const TArray<TSharedPtr<FJsonValue>> *Features=nullptr,*Provenance=nullptr,*Patches=nullptr,*Overrides=nullptr;
		FObject Base;FString Schema,Overlay,Grid,Missing;double Version=0;
		if (!Operation || Operation->Values.Num()!=4 ||
			!Operation->TryGetArrayField(TEXT("feature_ids"),Features) || !Features->IsEmpty() ||
			!Object(Operation,TEXT("base_features"),Base) || !Base->Values.IsEmpty() ||
			!Input || Input->Values.Num()!=8 || !String(Input,TEXT("$schema"),Schema) ||
			Schema!=TEXT("https://alis.world/schemas/world-compiler/authored-overlay-v2.json") ||
			!Number(Input,TEXT("schema_version"),Version) || Version!=2 ||
			!String(Input,TEXT("overlay_id"),Overlay) || !String(Input,TEXT("grid_id"),Grid) ||
			Grid.Len()!=21 || !Grid.StartsWith(TEXT("grid_")) ||
			!Input->TryGetArrayField(TEXT("provenance"),Provenance) || Provenance->IsEmpty() ||
			!Input->TryGetArrayField(TEXT("terrain_patches"),Patches) || Patches->IsEmpty() ||
			!Input->TryGetArrayField(TEXT("feature_overrides"),Overrides) || !Overrides->IsEmpty() ||
			!String(Input,TEXT("missing_target_policy"),Missing) || Missing!=TEXT("reject_and_require_rebase") ||
			!Hash(HashValue) || Out.Contains(HashValue)) return false;
		for (TCHAR C : Grid.Mid(5)) if (!((C>='0' && C<='9') || (C>='a' && C<='f'))) return false;
		TSet<FString> Authorities,PatchIds;
		for (const auto& Value : *Provenance)
		{
			const FObject* Record=nullptr;FString Id,Authority;
			if (!Value || !Value->TryGetObject(Record) || !Record || (*Record)->Values.Num()!=2 ||
				!String(*Record,TEXT("provenance_id"),Id) || !Identifier(Id) || Authorities.Contains(Id) ||
				!String(*Record,TEXT("authority"),Authority) ||
				(Authority!=TEXT("authored_fixture") && Authority!=TEXT("authored_operator"))) return false;
			Authorities.Add(Id);
		}
		for (const auto& Value : *Patches)
		{
			const FObject* Patch=nullptr;const TArray<TSharedPtr<FJsonValue>>* Center=nullptr;
			FString Id,Reference;double Radius=0,Delta=0;
			if (!Value || !Value->TryGetObject(Patch) || !Patch || (*Patch)->Values.Num()!=5 ||
				!String(*Patch,TEXT("patch_id"),Id) || !Identifier(Id) || PatchIds.Contains(Id) ||
				!String(*Patch,TEXT("provenance_ref"),Reference) || !Authorities.Contains(Reference) ||
				!Number(*Patch,TEXT("radius_m"),Radius) || Radius<0 || !Number(*Patch,TEXT("delta_m"),Delta) ||
				!(*Patch)->TryGetArrayField(TEXT("center"),Center) || Center->Num()!=2) return false;
			for (const auto& Coordinate : *Center)
			{
				double N=0;
				if (!Coordinate || Coordinate->Type!=EJson::Number || !Coordinate->TryGetNumber(N) || !FMath::IsFinite(N)) return false;
			}
			PatchIds.Add(Id);
		}
		Out.Add(HashValue,Grid);return true;
	}
	bool ReadTerrain(const FObject& ObjectValue,const FString& Grid,const FRetainedInputs& Inputs)
	{
		FString Construction,InputHash,Rule;FObject Consumed,Policy,Metrics;
		if (!ObjectValue || ObjectValue->Values.Num()!=11 || !String(ObjectValue,TEXT("construction"),Construction) ||
			Construction!=TEXT("authored_dry_terrain_v1") || !Object(ObjectValue,TEXT("consumed"),Consumed) ||
			Consumed->Values.Num()!=4 || !String(Consumed,TEXT("authored_input_sha256"),InputHash) ||
			!Inputs.Contains(InputHash) || Inputs[InputHash]!=Grid ||
			!Object(ObjectValue,TEXT("policy"),Policy) || Policy->Values.Num()!=1 ||
			!String(Policy,TEXT("height_rule"),Rule) || Rule!=TEXT("quantized_sum_on_immutable_final_grid_v1") ||
			!Object(ObjectValue,TEXT("metrics"),Metrics) || Metrics->Values.Num()!=4) return false;
		for (const TCHAR* Field : {TEXT("authored_input_sha256"),TEXT("domain_sha256"),TEXT("implementation_sha256"),TEXT("execution_identity")})
		{
			FString Digest;if (!String(Consumed,Field,Digest) || !Hash(Digest)) return false;
		}
		for (const TCHAR* Field : {TEXT("changed_nodes"),TEXT("affected_quads")})
		{
			double N=0;if (!Number(Metrics,Field,N) || N<1 || N>MAX_int32 || N!=FMath::FloorToDouble(N)) return false;
		}
		for (const TCHAR* Field : {TEXT("maximum_cut_m"),TEXT("maximum_fill_m")})
		{
			double N=0;if (!Number(Metrics,Field,N) || N<0) return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Quads=nullptr;double Count=0;
		return ObjectValue->TryGetArrayField(TEXT("quads"),Quads) && Number(Metrics,TEXT("affected_quads"),Count) && Count==Quads->Num();
	}
	bool ValidateUsage(const FRetainedInputs& Inputs,const FString& Grid,const TSet<FString>& Used)
	{
		if (Used.Num()!=Inputs.Num()) return false;
		for (const auto& Pair : Inputs) if (Pair.Value!=Grid || !Used.Contains(Pair.Key)) return false;
		return true;
	}
}
