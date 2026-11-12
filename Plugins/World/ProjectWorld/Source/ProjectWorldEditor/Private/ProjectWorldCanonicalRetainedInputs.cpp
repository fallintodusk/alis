// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalRetainedInputs.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldCanonicalFiniteJson.h"
#include "ProjectWorldConstructedContractParsing.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectWorldCanonicalRefinement
{
	namespace
	{
		using FObject=TSharedPtr<FJsonObject>;
		bool RejectRetainedInput(FProjectWorldCanonicalValidation& Validation,const FString& Message)
		{
			Validation.ErrorCode=TEXT("refinement-retained-input"); Validation.Message=Message; return false;
		}
		bool String(const FObject& Object,const TCHAR* Field,FString& Out)
		{
			const auto Value=Object ? Object->TryGetField(Field) : nullptr;
			return Value && Value->Type==EJson::String && Value->TryGetString(Out) && !Out.IsEmpty();
		}
		bool Object(const FObject& Parent,const TCHAR* Field,FObject& Out)
		{
			const FObject* Value=nullptr;
			if (!Parent || !Parent->TryGetObjectField(Field,Value) || !Value || !*Value) return false;
			Out=*Value; return true;
		}
		bool Load(const FString& Root,const FString& Relative,FObject& Out)
		{
			FString FullPath,Text;
			return FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root,Relative,FullPath) &&
				IFileManager::Get().FileSize(*FullPath)<=16000000 &&
				FFileHelper::LoadFileToString(Text,*FullPath) &&
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out) &&
				ProjectWorldCanonicalJson::FiniteDocument(Out);
		}
		struct FInput
		{
			FString Path,Hash,Role,Format;
			int64 Bytes=0;
		};
		bool Descriptor(const FObject& Value,const FString& Role,bool ContentAddressed,FInput& Out)
		{
			Out.Role=Role; const bool Native=Role==TEXT("native"); double Bytes=0;
			const auto Number=Value ? Value->TryGetField(TEXT("byte_size")) : nullptr;
			if (!Value || Value->Values.Num()!=(Native ? 4 : 3) ||
				!String(Value,TEXT("path"),Out.Path) || !String(Value,TEXT("sha256"),Out.Hash) || Out.Hash.Len()!=64 ||
				!Number || Number->Type!=EJson::Number || !Number->TryGetNumber(Bytes) || !FMath::IsFinite(Bytes) ||
				Bytes<1 || Bytes>=static_cast<double>(MAX_int64) || Bytes!=FMath::FloorToDouble(Bytes) ||
				(ContentAddressed && Bytes>16000000)) return false;
			for (TCHAR C : Out.Hash) if (!((C>='0' && C<='9') || (C>='a' && C<='f'))) return false;
			if (ContentAddressed && Out.Path!=TEXT("refinement/inputs/")+Out.Hash+TEXT(".json")) return false;
			if (Native && (!String(Value,TEXT("format"),Out.Format) ||
				(Out.Format!=TEXT("osm_json") && Out.Format!=TEXT("sanitized_osm_full_response")))) return false;
			Out.Bytes=static_cast<int64>(Bytes); return true;
		}
	}
	bool ValidateRetainedInputs(const FString& Root,bool Refinement,bool HasRequest,const TSet<FString>& EvidencePaths,
		TFunctionRef<bool(const FString&,const FString&,int64,bool)> IsVerifiedOutput,
		TMap<FString,FString>& OutRoles,FRetainedFeatureContexts& OutContexts,
		ProjectWorldIdentifiedBasinParsing::FRetainedInputs& OutBasins,
		ProjectWorldAuthoredTerrainParsing::FRetainedInputs& OutAuthored,FProjectWorldCanonicalValidation& Validation)
	{
		OutRoles.Reset(); OutContexts.Reset(); OutBasins.Reset(); OutAuthored.Reset();
		if (!Refinement || !HasRequest)
			return EvidencePaths.IsEmpty() || RejectRetainedInput(Validation,TEXT("Raw source evidence requires an authenticated refinement request."));
		FObject Request; FString RequestSchema;
		if (!Load(Root,TEXT("refinement/request.json"),Request) || !String(Request,TEXT("$schema"),RequestSchema))
			return RejectRetainedInput(Validation,TEXT("The verified refinement request is unreadable."));
		if (RequestSchema!=TEXT("https://alis.world/schemas/world-compiler/surface-refinement-request-v1.json"))
			return EvidencePaths.IsEmpty() || RejectRetainedInput(Validation,TEXT("This refinement request cannot declare raw source evidence."));
		const auto Version=Request->TryGetField(TEXT("schema_version")); double Number=0;
		const TArray<TSharedPtr<FJsonValue>>* Operations=nullptr;
		if (!Version || Version->Type!=EJson::Number || !Version->TryGetNumber(Number) || Number!=1 ||
			!Request->TryGetArrayField(TEXT("operations"),Operations) || !Operations || Operations->IsEmpty())
			return RejectRetainedInput(Validation,TEXT("The surface refinement request version or operations are invalid."));
		TMap<FString,FInput> Inputs;
		auto Add = [&](const FObject& Value,const FString& Role,bool Addressed)
		{
			FInput Candidate;
			if (!Descriptor(Value,Role,Addressed,Candidate) ||
				!IsVerifiedOutput(Candidate.Path,Candidate.Hash,Candidate.Bytes,Role==TEXT("native"))) return false;
			if (const FInput* Existing=Inputs.Find(Candidate.Path))
				return Existing->Hash==Candidate.Hash && Existing->Bytes==Candidate.Bytes &&
					Existing->Role==Candidate.Role && Existing->Format==Candidate.Format;
			Inputs.Add(Candidate.Path,Candidate); return true;
		};
		for (const auto& OperationValue : *Operations)
		{
			const FObject* Operation=nullptr; FString Kind;
			if (!OperationValue->TryGetObject(Operation) || !Operation || !String(*Operation,TEXT("kind"),Kind))
				return RejectRetainedInput(Validation,TEXT("A surface operation is malformed."));
			FObject DescriptorObject;
			if (Kind==TEXT("authored_dry_terrain"))
			{
				FObject Document; FInput Input;
				if (!Object(*Operation,TEXT("input"),DescriptorObject) || !Add(DescriptorObject,TEXT("authored"),true) ||
					!Descriptor(DescriptorObject,TEXT("authored"),true,Input) || !Load(Root,Input.Path,Document) ||
					!ProjectWorldAuthoredTerrainParsing::ReadInput(*Operation,Document,Input.Hash,OutAuthored))
					return RejectRetainedInput(Validation,TEXT("Authored dry Terrain input is absent, unverified or unsupported."));
				continue;
			}
			if (Kind==TEXT("identified_basin"))
			{
				FObject Document; FInput Input;
				if (!Object(*Operation,TEXT("input"),DescriptorObject) || !Add(DescriptorObject,TEXT("basin"),true) ||
					!Descriptor(DescriptorObject,TEXT("basin"),true,Input) || !Load(Root,Input.Path,Document) ||
					!ProjectWorldIdentifiedBasinParsing::ReadInput(*Operation,Document,{Input.Path,Input.Hash,Input.Bytes},OutBasins))
					return RejectRetainedInput(Validation,TEXT("Identified basin input is absent, unverified or unsupported."));
				continue;
			}
			if (Kind!=TEXT("constructed_surface_import"))
			{
				if ((Kind!=TEXT("bridge_elevation") && Kind!=TEXT("water_confluence")) ||
					!Object(*Operation,TEXT("context"),DescriptorObject) || !Add(DescriptorObject,TEXT("source"),false))
					return RejectRetainedInput(Validation,TEXT("A surface operation context is absent or contradictory."));
				continue;
			}
			FObject Declared; const TArray<TSharedPtr<FJsonValue>>* Contexts=nullptr;
			if (!Object(*Operation,TEXT("inputs"),Declared) || !Object(Declared,TEXT("model"),DescriptorObject) ||
				!Add(DescriptorObject,TEXT("model"),true) || !Object(Declared,TEXT("policy"),DescriptorObject) ||
				!Add(DescriptorObject,TEXT("policy"),true) || !Declared->TryGetArrayField(TEXT("source_contexts"),Contexts) ||
				!Contexts || Contexts->IsEmpty() || Contexts->Num()>16)
				return RejectRetainedInput(Validation,TEXT("Structural import inputs are incomplete or unverified."));
			FObject PolicyDocument; FProjectWorldCanonicalConstructedPolicy Policy;
			const auto PolicyDescriptor=Declared->GetObjectField(TEXT("policy"));
			if (!Load(Root,PolicyDescriptor->GetStringField(TEXT("path")),PolicyDocument) ||
				!ProjectWorldConstructedContractParsing::ReadPolicy(PolicyDocument,Policy))
				return RejectRetainedInput(Validation,TEXT("The verified structural policy is malformed or unsupported."));
			TMap<FString,FRetainedContext> ContextRecords;
			for (const auto& ContextValue : *Contexts)
			{
				const FObject* Context=nullptr; FString Id;
				if (!ContextValue->TryGetObject(Context) || !Context || !String(*Context,TEXT("id"),Id) || ContextRecords.Contains(Id) ||
					!Object(*Context,TEXT("normalized"),DescriptorObject) || !Add(DescriptorObject,TEXT("source"),true))
					return RejectRetainedInput(Validation,TEXT("A structural source context is missing or contradictory."));
				FRetainedContext Record; Record.NormalizedPath=DescriptorObject->GetStringField(TEXT("path"));
				const auto Native=(*Context)->TryGetField(TEXT("native"));
				if (!Native || (Native->Type!=EJson::Null &&
					(!Object(*Context,TEXT("native"),DescriptorObject) || !Add(DescriptorObject,TEXT("native"),true))))
					return RejectRetainedInput(Validation,TEXT("A structural native descriptor is missing or contradictory."));
				if (Native->Type!=EJson::Null) Record.NativePath=DescriptorObject->GetStringField(TEXT("path"));
				ContextRecords.Add(Id,MoveTemp(Record));
			}
			const TArray<TSharedPtr<FJsonValue>>* FeatureIds=nullptr;
			if (!(*Operation)->TryGetArrayField(TEXT("feature_ids"),FeatureIds) || !FeatureIds || FeatureIds->IsEmpty() || FeatureIds->Num()>16)
				return RejectRetainedInput(Validation,TEXT("Structural source contexts have no bounded feature association."));
			for (const auto& IdValue : *FeatureIds)
			{
				FString Id;
				if (IdValue->Type!=EJson::String || !IdValue->TryGetString(Id) || Id.IsEmpty() || OutContexts.Contains(Id))
					return RejectRetainedInput(Validation,TEXT("Structural feature context ownership is ambiguous."));
				OutContexts.Add(Id,{PolicyDescriptor->GetStringField(TEXT("sha256")),ContextRecords,Policy});
			}
		}
		TSet<FString> DeclaredEvidence;
		for (const auto& Basin : OutBasins) if (OutContexts.Contains(Basin.Key))
			return RejectRetainedInput(Validation,TEXT("Retained structural and basin input ownership conflicts."));
		for (const auto& Pair : Inputs)
		{
			OutRoles.Add(Pair.Key,Pair.Value.Role);
			if (Pair.Value.Role!=TEXT("native")) continue;
			DeclaredEvidence.Add(Pair.Key); FObject Native; FString VersionText;
			if (!Load(Root,Pair.Key,Native)) return RejectRetainedInput(Validation,TEXT("Retained native JSON is unreadable."));
			if (Pair.Value.Format==TEXT("sanitized_osm_full_response"))
			{
				FObject Wrapped;
				if (!Object(Native,TEXT("document"),Wrapped)) return RejectRetainedInput(Validation,TEXT("Retained native response has no document."));
				Native=Wrapped;
			}
			const TArray<TSharedPtr<FJsonValue>>* Elements=nullptr;
			if (!String(Native,TEXT("version"),VersionText) || VersionText!=TEXT("0.6") ||
				!Native->TryGetArrayField(TEXT("elements"),Elements) || !Elements)
				return RejectRetainedInput(Validation,TEXT("Retained native evidence differs from its declared format."));
		}
		if (DeclaredEvidence.Num()!=EvidencePaths.Num()) return RejectRetainedInput(Validation,TEXT("Raw source evidence is undeclared or absent."));
		for (const auto& Path : EvidencePaths) if (!DeclaredEvidence.Contains(Path))
			return RejectRetainedInput(Validation,TEXT("An extra raw source evidence output has no exact declared role."));
		return true;
	}
}
