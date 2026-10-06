// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPipelineVerifyProjection.h"

#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldVerify.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldPipelineVerifyProjection
{
	namespace
	{
		bool ReadObject(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path) ||
				!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out.IsValid())
			{
				Error = TEXT("Pipeline verify cannot read JSON: ") + Path;
				return false;
			}
			return true;
		}

		bool String(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name,
			FString& Out, FString& Error)
		{
			if (!Object.IsValid() || !Object->TryGetStringField(Name, Out))
			{
				Error = FString::Printf(TEXT("Pipeline manifest lacks string %s."), Name);
				return false;
			}
			return true;
		}

		bool Object(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name,
			TSharedPtr<FJsonObject>& Out, FString& Error)
		{
			if (!Parent.IsValid() || !Parent->HasTypedField<EJson::Object>(Name))
			{
				Error = FString::Printf(TEXT("Pipeline manifest lacks object %s."), Name);
				return false;
			}
			Out = Parent->GetObjectField(Name);
			return true;
		}

		bool Array(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name,
			const TArray<TSharedPtr<FJsonValue>>*& Out, FString& Error)
		{
			if (!Parent.IsValid() || !Parent->TryGetArrayField(Name, Out) || Out == nullptr)
			{
				Error = FString::Printf(TEXT("Pipeline manifest lacks array %s."), Name);
				return false;
			}
			return true;
		}

		bool StringArray(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name,
			FString& Out, FString& Error)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Array(Parent, Name, Values, Error))
			{
				return false;
			}
			TArray<FString> Items;
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Item;
				if (!Value.IsValid() || !Value->TryGetString(Item))
				{
					Error = FString::Printf(TEXT("Pipeline manifest has invalid %s entry."), Name);
					return false;
				}
				Items.Add(Item);
			}
			Items.Sort();
			Out = FString::Join(Items, TEXT(","));
			return true;
		}

		bool InputPairs(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name,
			FString& Out, FString& Error)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Array(Parent, Name, Values, Error))
			{
				return false;
			}
			TArray<FString> Items;
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TSharedPtr<FJsonObject> Pair = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Unit;
				FString Hash;
				if (!String(Pair, TEXT("unit_id"), Unit, Error) ||
					!String(Pair, TEXT("sha256"), Hash, Error))
				{
					return false;
				}
				Items.Add(Unit + TEXT("=") + Hash);
			}
			Items.Sort();
			Out = FString::Join(Items, TEXT(","));
			return true;
		}

		bool AddScope(const TSharedPtr<FJsonObject>& Manifest, const FString& ScopeId,
			const FString& ContentRoot, TSet<FString>& Artifacts,
			TMap<FString, FString>& PackageDigests,
			FProjectWorldProjection& Projection, FString& Error)
		{
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("scope"), ScopeId);
			if (!String(Manifest, TEXT("owning_layer"), Record.Fields.Add(TEXT("layer")), Error))
			{
				return false;
			}
			TSharedPtr<FJsonObject> Input;
			if (!Object(Manifest, TEXT("input_identity"), Input, Error))
			{
				return false;
			}
			for (const TCHAR* Name : {TEXT("map_package"), TEXT("runtime_profile_sha256"),
				TEXT("authored_overlay_profile_sha256"), TEXT("presentation_profile_sha256")})
			{
				FString Value;
				if (!String(Input, Name, Value, Error))
				{
					return false;
				}
				Record.Fields.Add(FString(TEXT("input.")) + Name, Value);
			}
			if (!StringArray(Manifest, TEXT("consumer_references"),
				Record.Fields.Add(TEXT("consumers")), Error))
			{
				return false;
			}
			if (Manifest->HasTypedField<EJson::Object>(TEXT("layer_contract")))
			{
				TSharedPtr<FJsonObject> Contract = Manifest->GetObjectField(TEXT("layer_contract"));
				for (const TCHAR* Name : {TEXT("realization_profile_id"),
					TEXT("realization_profile_sha256"), TEXT("normalized_layer_contract_sha256"),
					TEXT("generator_id"), TEXT("artifact_root")})
				{
					FString Value;
					if (!String(Contract, Name, Value, Error))
					{
						return false;
					}
					Record.Fields.Add(FString(TEXT("contract.")) + Name, Value);
				}
				Record.Fields.Add(TEXT("contract.generator_version"), FString::FromInt(
					static_cast<int32>(Contract->GetNumberField(TEXT("generator_version")))));
				if (!InputPairs(Contract, TEXT("canonical_inputs"),
					Record.Fields.Add(TEXT("contract.canonical_inputs")), Error) ||
					!InputPairs(Contract, TEXT("dependency_inputs"),
					Record.Fields.Add(TEXT("contract.dependency_inputs")), Error))
				{
					return false;
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Array(Manifest, TEXT("artifacts"), Values, Error))
			{
				return false;
			}
			int32 ExternalCount = 0;
			TArray<FString> Named;
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TSharedPtr<FJsonObject> Artifact = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Path;
				FString Kind;
				if (!String(Artifact, TEXT("path"), Path, Error) ||
					!String(Artifact, TEXT("kind"), Kind, Error))
				{
					return false;
				}
				const FString Full = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Path));
				if (!FPaths::IsUnderDirectory(Full, ContentRoot) ||
					!IFileManager::Get().FileExists(*Full) || Artifacts.Contains(Full))
				{
					Error = TEXT("Pipeline artifact is missing, duplicated, or outside TestData Content: ") + Path;
					return false;
				}
				Artifacts.Add(Full);
				FString Digest;
				if (!FProjectSha256::HashFile(Full, Digest))
				{
					Error = TEXT("Pipeline artifact cannot be hashed: ") + Path;
					return false;
				}
				PackageDigests.Add(Path, Digest);
				if (Kind == TEXT("external_actor"))
				{
					++ExternalCount;
				}
				else
				{
					Named.Add(Kind + TEXT(":") + Path.RightChop(
						FString(TEXT("Plugins/World/ProjectWorldTestData/Content/")).Len()));
				}
			}
			Named.Sort();
			Record.Fields.Add(TEXT("artifact.external_actor_count"), FString::FromInt(ExternalCount));
			Record.Fields.Add(TEXT("artifact.named"), FString::Join(Named, TEXT(",")));
			return true;
		}
	}

	bool Project(UWorld& World, const FProjectWorldCanonicalBundle& Bundle, const FString& ManifestRoot,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldVerifyIdentity& Identity, FProjectWorldProjection& Projection,
		TMap<FString, FString>& PackageDigests, FString& OutError)
	{
		if (Profile.WorldDataPluginName != TEXT("ProjectWorldTestData"))
		{
			OutError = TEXT("Pipeline verify requires ProjectWorldTestData.");
			return false;
		}
		const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Plugins/World/ProjectWorldTestData/Content")));
		TSharedPtr<FJsonObject> Active;
		if (!ReadObject(FPaths::Combine(ManifestRoot, TEXT("active_set.json")), Active, OutError))
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Scopes = nullptr;
		if (!Array(Active, TEXT("scopes"), Scopes, OutError) || Scopes->IsEmpty())
		{
			return false;
		}
		TSet<FString> ScopeIds;
		TSet<FString> Artifacts;
		TSet<FString> Layers;
		for (const TSharedPtr<FJsonValue>& Value : *Scopes)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			FString ScopeId;
			FString Relative;
			if (!String(Entry, TEXT("scope_id"), ScopeId, OutError) ||
				!String(Entry, TEXT("manifest_path"), Relative, OutError))
			{
				return false;
			}
			const FString ManifestPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ManifestRoot, Relative));
			if (ScopeIds.Contains(ScopeId) || !FPaths::IsUnderDirectory(ManifestPath, ManifestRoot))
			{
				OutError = TEXT("Pipeline scope is duplicated or escaped its manifest root: ") + ScopeId;
				return false;
			}
			ScopeIds.Add(ScopeId);
			TSharedPtr<FJsonObject> Manifest;
			FString ManifestScope;
			if (!ReadObject(ManifestPath, Manifest, OutError) ||
				!String(Manifest, TEXT("scope_id"), ManifestScope, OutError) || ManifestScope != ScopeId ||
				!AddScope(Manifest, ScopeId, ContentRoot, Artifacts,
					PackageDigests, Projection, OutError))
			{
				return false;
			}
			FString Layer;
			if (String(Manifest, TEXT("owning_layer"), Layer, OutError))
			{
				Layers.Add(Layer);
			}
		}
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			if (!Layers.Contains(Layer.LayerId))
			{
				OutError = TEXT("Pipeline lacks a realized layer scope: ") + Layer.LayerId;
				return false;
			}
			int32 ComparisonVersion = 0;
			FProjectWorldProjectFn Function;
			if (!ProjectWorldVerify::FindProjection(Layer.GeneratorId, Layer.GeneratorVersion,
				ComparisonVersion, Function))
			{
				OutError = TEXT("Pipeline lacks projection for ") + Layer.GeneratorId;
				return false;
			}
			FProjectWorldProjection Owned;
			if (!Function({&World, Layer.ArtifactRoot, &Bundle}, Owned, OutError) ||
				!Owned.IsValid(OutError) || Owned.IsEmpty())
			{
				if (OutError.IsEmpty()) { OutError = TEXT("Pipeline layer projection is empty: ") + Layer.LayerId; }
				return false;
			}
			FProjectWorldVerifyIdentity Producer;
			if (!FProjectWorldVerifyIdentity::ForProducer(Identity.Verify,
				Layer.GeneratorId, Layer.GeneratorVersion, ComparisonVersion, Producer, OutError))
			{
				return false;
			}
			Identity.Embedded.Add(FString::Printf(TEXT("%s:v%d|%d|%d"), *Layer.GeneratorId,
				Layer.GeneratorVersion, Producer.OutputRevision, ComparisonVersion));
			for (const TPair<FString, FString>& Input : Producer.FixtureInputs)
			{
				Identity.FixtureInputs.Emplace(TEXT("layer:") + Layer.LayerId + TEXT(":") + Input.Key, Input.Value);
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("layer_output"), Layer.LayerId);
			Record.Fields.Add(TEXT("projection_sha256"), Owned.Sha256());
			Record.Fields.Add(TEXT("records"), FString::FromInt(Owned.Sorted().Num()));
		}
		Identity.Embedded.Sort();
		TArray<FString> OwnedRoots;
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			const FString Mount(TEXT("/ProjectWorldTestData/"));
			if (!Layer.ArtifactRoot.StartsWith(Mount))
			{
				OutError = TEXT("Pipeline layer artifact root escapes ProjectWorldTestData.");
				return false;
			}
			OwnedRoots.Add(FPaths::Combine(ContentRoot, Layer.ArtifactRoot.RightChop(Mount.Len())));
		}
		const FString ExternalRoot = ULevel::GetExternalActorsPath(Profile.MapPackagePath);
		const FString Mount(TEXT("/ProjectWorldTestData/"));
		if (!ExternalRoot.StartsWith(Mount))
		{
			OutError = TEXT("Pipeline map external actor root escapes ProjectWorldTestData.");
			return false;
		}
		OwnedRoots.Add(FPaths::Combine(ContentRoot, ExternalRoot.RightChop(Mount.Len())));
		for (const FString& Root : OwnedRoots)
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Root,
				TEXT("*.*"), true, false);
			for (const FString& File : Files)
			{
				if (!Artifacts.Contains(FPaths::ConvertRelativePathToFull(File)))
				{
					OutError = TEXT("Pipeline generated file has no manifest owner: ") + File;
					return false;
				}
			}
		}
		return true;
	}
}
