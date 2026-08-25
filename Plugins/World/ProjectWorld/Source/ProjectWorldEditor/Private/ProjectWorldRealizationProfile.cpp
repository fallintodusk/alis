// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizationProfile.h"

#include "ProjectWorldDataRoots.h"
#include "ProjectWorldRealizationGeneratorRegistry.h"
#include "ProjectWorldSchemaReference.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldRealizationProfile
{
	namespace
	{
		const TCHAR* ExpectedSchemaFilename = TEXT("project_world_realization_profile.schema.json");

		bool HasOnlyFields(
			const TSharedPtr<FJsonObject>& Object,
			std::initializer_list<const TCHAR*> Allowed,
			FString& OutError)
		{
			TSet<FString> AllowedFields;
			for (const TCHAR* Field : Allowed)
			{
				AllowedFields.Add(Field);
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
			{
				if (!AllowedFields.Contains(Field.Key))
				{
					OutError = FString::Printf(TEXT("Unknown field: %s"), *Field.Key);
					return false;
				}
			}
			return true;
		}

		bool RequireString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			FString& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Missing or empty string: %s"), Name);
				return false;
			}
			return true;
		}

		bool IsIdentifier(const FString& Value)
		{
			if (Value.IsEmpty())
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				const bool bLower = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLower && !bDigit && Character != TEXT('_'))
				{
					return false;
				}
			}
			return true;
		}

		bool ReadStringArray(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			bool bRequireNonEmpty,
			TArray<FString>& OutValues,
			FString& OutError)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Object->TryGetArrayField(Name, Values) || Values == nullptr ||
				(bRequireNonEmpty && Values->IsEmpty()))
			{
				OutError = FString::Printf(TEXT("Missing array: %s"), Name);
				return false;
			}
			TSet<FString> Unique;
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Item;
				if (!Value.IsValid() || !Value->TryGetString(Item) || Item.IsEmpty() || Unique.Contains(Item))
				{
					OutError = FString::Printf(TEXT("Array contains an invalid or duplicate value: %s"), Name);
					return false;
				}
				Unique.Add(Item);
				OutValues.Add(Item);
			}
			return true;
		}

		bool ParseLayerKind(const FString& Value, EProjectWorldLayerKind& OutKind)
		{
			if (Value == TEXT("generated_geography"))
			{
				OutKind = EProjectWorldLayerKind::GeneratedGeography;
			}
			else if (Value == TEXT("generated_gameplay_placement"))
			{
				OutKind = EProjectWorldLayerKind::GeneratedGameplayPlacement;
			}
			else if (Value == TEXT("protected_authored_overlay"))
			{
				OutKind = EProjectWorldLayerKind::ProtectedAuthoredOverlay;
			}
			else if (Value == TEXT("runtime_state_exclusion"))
			{
				OutKind = EProjectWorldLayerKind::RuntimeStateExclusion;
			}
			else
			{
				return false;
			}
			return true;
		}

		bool ParseDirtyGranularity(const FString& Value, EProjectWorldDirtyGranularity& OutGranularity)
		{
			if (Value == TEXT("whole_layer"))
			{
				OutGranularity = EProjectWorldDirtyGranularity::WholeLayer;
			}
			else if (Value == TEXT("canonical_cell"))
			{
				OutGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
			}
			else if (Value == TEXT("source_tile"))
			{
				OutGranularity = EProjectWorldDirtyGranularity::SourceTile;
			}
			else if (Value == TEXT("object_id"))
			{
				OutGranularity = EProjectWorldDirtyGranularity::ObjectId;
			}
			else if (Value == TEXT("never"))
			{
				OutGranularity = EProjectWorldDirtyGranularity::Never;
			}
			else
			{
				return false;
			}
			return true;
		}

		FString NormalizeJsonValue(const TSharedPtr<FJsonValue>& Value)
		{
			if (!Value.IsValid())
			{
				return TEXT("null");
			}
			if (Value->Type == EJson::Object)
			{
				TArray<TPair<FString, TSharedPtr<FJsonValue>>> SortedFields;
				for (const auto& Field : Value->AsObject()->Values)
				{
					SortedFields.Emplace(FString(Field.Key.ToView()), Field.Value);
				}
				SortedFields.Sort([](const auto& Left, const auto& Right)
				{
					return Left.Key < Right.Key;
				});
				TArray<FString> Fields;
				for (const auto& Field : SortedFields)
				{
					Fields.Add(
						TEXT("\"") + Field.Key + TEXT("\":") + NormalizeJsonValue(Field.Value));
				}
				return TEXT("{") + FString::Join(Fields, TEXT(",")) + TEXT("}");
			}
			if (Value->Type == EJson::Array)
			{
				TArray<FString> Items;
				for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
				{
					Items.Add(NormalizeJsonValue(Item));
				}
				return TEXT("[") + FString::Join(Items, TEXT(",")) + TEXT("]");
			}
			FString Serialized;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
			FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
			return Serialized;
		}

		bool HashString(const FString& Value, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Value);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		FString KindName(EProjectWorldLayerKind Kind)
		{
			switch (Kind)
			{
			case EProjectWorldLayerKind::GeneratedGeography: return TEXT("generated_geography");
			case EProjectWorldLayerKind::GeneratedGameplayPlacement: return TEXT("generated_gameplay_placement");
			case EProjectWorldLayerKind::ProtectedAuthoredOverlay: return TEXT("protected_authored_overlay");
			case EProjectWorldLayerKind::RuntimeStateExclusion: return TEXT("runtime_state_exclusion");
			default: return TEXT("invalid");
			}
		}

		FString GranularityName(EProjectWorldDirtyGranularity Granularity)
		{
			switch (Granularity)
			{
			case EProjectWorldDirtyGranularity::WholeLayer: return TEXT("whole_layer");
			case EProjectWorldDirtyGranularity::CanonicalCell: return TEXT("canonical_cell");
			case EProjectWorldDirtyGranularity::SourceTile: return TEXT("source_tile");
			case EProjectWorldDirtyGranularity::ObjectId: return TEXT("object_id");
			case EProjectWorldDirtyGranularity::Never: return TEXT("never");
			default: return TEXT("invalid");
			}
		}

		bool IsRootUnder(const FString& Root, const FString& Parent)
		{
			return Root.StartsWith(Parent, ESearchCase::CaseSensitive) &&
				Root.EndsWith(TEXT("/"));
		}

		bool IsStrictRootUnder(const FString& Root, const FString& Parent)
		{
			return Root.Len() > Parent.Len() && IsRootUnder(Root, Parent);
		}

		bool ExpandUnits(
			const TSet<FString>& Units,
			EProjectWorldDirtyGranularity Granularity,
			int32 Halo,
			const TSet<FString>& ValidUnits,
			TSet<FString>& OutExpanded,
			FString& OutError)
		{
			OutExpanded.Reset();
			if (Granularity == EProjectWorldDirtyGranularity::WholeLayer || Units.Contains(TEXT("*")))
			{
				OutExpanded.Add(TEXT("*"));
				return true;
			}
			if (Granularity != EProjectWorldDirtyGranularity::CanonicalCell || Halo == 0)
			{
				OutExpanded = Units;
				return true;
			}
			for (const FString& Unit : Units)
			{
				int32 XMarker = INDEX_NONE;
				int32 YMarker = INDEX_NONE;
				if (!Unit.FindLastChar(TEXT('x'), XMarker) || !Unit.FindLastChar(TEXT('y'), YMarker) ||
					XMarker >= YMarker || XMarker == 0 || Unit[XMarker - 1] != TEXT(':') || Unit[YMarker - 1] != TEXT(':'))
				{
					OutError = FString::Printf(TEXT("Canonical-cell unit is malformed: %s"), *Unit);
					return false;
				}
				int32 X = 0;
				int32 Y = 0;
				if (!LexTryParseString(X, *Unit.Mid(XMarker + 1, YMarker - XMarker - 2)) ||
					!LexTryParseString(Y, *Unit.Mid(YMarker + 1)))
				{
					OutError = FString::Printf(TEXT("Canonical-cell unit is malformed: %s"), *Unit);
					return false;
				}
				const FString Prefix = Unit.Left(XMarker);
				for (int32 DeltaY = -Halo; DeltaY <= Halo; ++DeltaY)
				{
					for (int32 DeltaX = -Halo; DeltaX <= Halo; ++DeltaX)
					{
						const FString Candidate = FString::Printf(
							TEXT("%sx%d:y%d"),
							*Prefix,
							X + DeltaX,
							Y + DeltaY);
						if (ValidUnits.Contains(Candidate))
						{
							OutExpanded.Add(Candidate);
						}
					}
				}
			}
			return true;
		}
	}

	bool IsGeneratorRegistered(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		EProjectWorldLayerKind LayerKind)
	{
		return ProjectWorldRealizationGeneratorRegistry::IsRegistered(
			GeneratorId,
			GeneratorVersion,
			LayerKind);
	}

	bool ValidateAndFinalize(FProjectWorldRealizationProfile& Profile, FString& OutError)
	{
		OutError.Reset();
		TArray<FString> ProtectedRoots = Profile.ProtectedAuthoredRoots;
		TArray<FString> RuntimeRoots = Profile.ExcludedRuntimeStateRoots;
		ProtectedRoots.Sort();
		RuntimeRoots.Sort();
		const FString ExecutionIdentity = FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%s|%d|%s|%s"),
			*Profile.ProfileId,
			*Profile.WorldDataPluginName,
			*Profile.CanonicalProfileId,
			*Profile.MapPackagePath,
			*Profile.RuntimeProfileId,
			*Profile.LogicalLandscapeId,
			Profile.ComponentsPerProxy,
			*FString::Join(ProtectedRoots, TEXT(",")),
			*FString::Join(RuntimeRoots, TEXT(",")));
		if (!HashString(ExecutionIdentity, Profile.ExecutionHash))
		{
			OutError = TEXT("Cannot hash the realization execution contract.");
			return false;
		}
		const FString LayerExecutionIdentity = FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%d|%s|%s"),
			*Profile.ProfileId,
			*Profile.WorldDataPluginName,
			*Profile.CanonicalProfileId,
			*Profile.MapPackagePath,
			*Profile.LogicalLandscapeId,
			Profile.ComponentsPerProxy,
			*FString::Join(ProtectedRoots, TEXT(",")),
			*FString::Join(RuntimeRoots, TEXT(",")));
		FString LayerExecutionHash;
		if (!HashString(LayerExecutionIdentity, LayerExecutionHash))
		{
			OutError = TEXT("Cannot hash the realization layer contract.");
			return false;
		}
		TMap<FString, int32> LayerIndices;
		TArray<FString> Roots;
		const FString GeneratedRoot = FString::Printf(TEXT("/%s/Generated/"), *Profile.WorldDataPluginName);
		const FString AuthoredRoot = FString::Printf(TEXT("/%s/Authored/"), *Profile.WorldDataPluginName);
		const FString RuntimeRoot = FString::Printf(TEXT("/%s/Runtime/"), *Profile.WorldDataPluginName);
		for (const FString& Root : Profile.ProtectedAuthoredRoots)
		{
			if (!IsRootUnder(Root, AuthoredRoot))
			{
				OutError = FString::Printf(TEXT("Protected authored root escapes its owner boundary: %s"), *Root);
				return false;
			}
		}
		for (const FString& Root : Profile.ExcludedRuntimeStateRoots)
		{
			if (!IsRootUnder(Root, RuntimeRoot))
			{
				OutError = FString::Printf(TEXT("Runtime-state exclusion escapes its owner boundary: %s"), *Root);
				return false;
			}
		}
		for (int32 Index = 0; Index < Profile.Layers.Num(); ++Index)
		{
			FProjectWorldRealizationLayer& Layer = Profile.Layers[Index];
			if (!IsIdentifier(Layer.LayerId) || LayerIndices.Contains(Layer.LayerId))
			{
				OutError = FString::Printf(TEXT("Layer ID is invalid or duplicated: %s"), *Layer.LayerId);
				return false;
			}
			if (!IsGeneratorRegistered(Layer.GeneratorId, Layer.GeneratorVersion, Layer.LayerKind))
			{
				OutError = FString::Printf(TEXT("Generator pair is not registered: %s:v%d"), *Layer.GeneratorId, Layer.GeneratorVersion);
				return false;
			}
			if (!ProjectWorldRealizationGeneratorRegistry::ValidateSettings(Layer, OutError))
			{
				return false;
			}
			const FString RequiredRoot = Layer.IsGenerated()
				? GeneratedRoot
				: (Layer.LayerKind == EProjectWorldLayerKind::ProtectedAuthoredOverlay ? AuthoredRoot : RuntimeRoot);
			if (!IsStrictRootUnder(Layer.ArtifactRoot, RequiredRoot))
			{
				OutError = FString::Printf(TEXT("Layer artifact root is outside its owner boundary: %s"), *Layer.ArtifactRoot);
				return false;
			}
			for (const FString& ExistingRoot : Roots)
			{
				if (Layer.ArtifactRoot.StartsWith(ExistingRoot) || ExistingRoot.StartsWith(Layer.ArtifactRoot))
				{
					OutError = FString::Printf(TEXT("Layer artifact roots overlap: %s and %s"), *ExistingRoot, *Layer.ArtifactRoot);
					return false;
				}
			}
			Roots.Add(Layer.ArtifactRoot);
			LayerIndices.Add(Layer.LayerId, Index);
		}

		TMap<FString, int32> InDegree;
		TMap<FString, TArray<FString>> Dependents;
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			InDegree.Add(Layer.LayerId, Layer.DependsOn.Num());
			for (const FString& Dependency : Layer.DependsOn)
			{
				if (!LayerIndices.Contains(Dependency) || Dependency == Layer.LayerId)
				{
					OutError = FString::Printf(TEXT("Layer dependency is unknown or self-referential: %s -> %s"), *Layer.LayerId, *Dependency);
					return false;
				}
				Dependents.FindOrAdd(Dependency).Add(Layer.LayerId);
			}
		}
		Profile.TopologicalLayerIds.Reset();
		while (Profile.TopologicalLayerIds.Num() < Profile.Layers.Num())
		{
			TArray<FString> Ready;
			for (const TPair<FString, int32>& Pair : InDegree)
			{
				if (Pair.Value == 0 && !Profile.TopologicalLayerIds.Contains(Pair.Key))
				{
					Ready.Add(Pair.Key);
				}
			}
			Ready.Sort();
			if (Ready.IsEmpty())
			{
				OutError = TEXT("Layer dependencies contain a cycle.");
				return false;
			}
			const FString Current = Ready[0];
			Profile.TopologicalLayerIds.Add(Current);
			for (const FString& Dependent : Dependents.FindRef(Current))
			{
				--InDegree[Dependent];
			}
		}

		for (FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			TArray<FString> Dependencies = Layer.DependsOn;
			TArray<FString> Selectors = Layer.CanonicalSelectors;
			Dependencies.Sort();
			Selectors.Sort();
			const FString Identity = FString::Printf(
				TEXT("%s|%s|%s|%s|%d|%s|%s|%s|%s|%s|%d|%s|%s"),
				*LayerExecutionHash,
				*Layer.LayerId,
				*KindName(Layer.LayerKind),
				*Layer.GeneratorId,
				Layer.GeneratorVersion,
				*FString::Join(Dependencies, TEXT(",")),
				*FString::Join(Selectors, TEXT(",")),
				*Layer.ArtifactRoot,
				*Layer.SpatialOwnership,
				*GranularityName(Layer.DirtyGranularity),
				Layer.DependencyHaloCells,
				*Layer.RuntimeMapping,
				*Layer.NormalizedSettings);
			if (!HashString(Identity, Layer.ContractHash))
			{
				OutError = FString::Printf(TEXT("Cannot hash layer contract: %s"), *Layer.LayerId);
				return false;
			}
		}
		return true;
	}

	bool Load(
		const FString& Path,
		FProjectWorldRealizationProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError,
		const FString& TransientProfileRoot)
	{
		OutProfile = FProjectWorldRealizationProfile();
		OutErrorCode.Reset();
		OutError.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutErrorCode = TEXT("realization-profile-read");
			OutError = Path;
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!HasOnlyFields(Root, {
				TEXT("$schema"), TEXT("schema_version"), TEXT("profile_id"), TEXT("world_data_plugin"),
				TEXT("canonical_profile_id"), TEXT("map_package"), TEXT("runtime_profile_id"),
				TEXT("landscape"), TEXT("protected_authored_roots"),
				TEXT("excluded_runtime_state_roots"), TEXT("layers")}, OutError))
		{
			OutErrorCode = TEXT("realization-profile-json");
			return false;
		}
		FString Schema;
		double SchemaVersion = 0.0;
		if (!RequireString(Root, TEXT("$schema"), Schema, OutError) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(Path, Schema, ExpectedSchemaFilename, OutError) ||
			!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1.0 ||
			!RequireString(Root, TEXT("profile_id"), OutProfile.ProfileId, OutError) || !IsIdentifier(OutProfile.ProfileId) ||
			!RequireString(Root, TEXT("world_data_plugin"), OutProfile.WorldDataPluginName, OutError) ||
			!RequireString(Root, TEXT("canonical_profile_id"), OutProfile.CanonicalProfileId, OutError) || !IsIdentifier(OutProfile.CanonicalProfileId) ||
			!RequireString(Root, TEXT("map_package"), OutProfile.MapPackagePath, OutError) ||
			!RequireString(Root, TEXT("runtime_profile_id"), OutProfile.RuntimeProfileId, OutError))
		{
			OutErrorCode = TEXT("realization-profile-contract");
			return false;
		}

		FProjectWorldDataRoots DataRoots;
		const FString FullProfilePath = FPaths::ConvertRelativePathToFull(Path);
		const FString FullTransientRoot = FPaths::ConvertRelativePathToFull(TransientProfileRoot);
		const FString ProjectTmpWorldRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world")));
		const bool bTransientProfileOwned = !TransientProfileRoot.IsEmpty() &&
			FPaths::IsUnderDirectory(FullTransientRoot, ProjectTmpWorldRoot) &&
			FPaths::IsUnderDirectory(FullProfilePath, FullTransientRoot);
		if (!FProjectWorldDataRoots::Resolve(OutProfile.WorldDataPluginName, DataRoots, OutError) ||
			(!FPaths::IsUnderDirectory(FullProfilePath, DataRoots.DataRoot) && !bTransientProfileOwned) ||
			!DataRoots.IsGeneratedPackage(OutProfile.MapPackagePath))
		{
			OutErrorCode = TEXT("realization-profile-owner");
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Profile is outside its Data root and confined test root: %s"), *Path);
			}
			return false;
		}
		const TSharedPtr<FJsonObject>* Landscape = nullptr;
		double ComponentsPerProxy = 0.0;
		if (!Root->TryGetObjectField(TEXT("landscape"), Landscape) || Landscape == nullptr ||
			!HasOnlyFields(*Landscape, {TEXT("logical_landscape_id"), TEXT("components_per_proxy")}, OutError) ||
			!RequireString(*Landscape, TEXT("logical_landscape_id"), OutProfile.LogicalLandscapeId, OutError) ||
			!IsIdentifier(OutProfile.LogicalLandscapeId) ||
			!(*Landscape)->TryGetNumberField(TEXT("components_per_proxy"), ComponentsPerProxy) || ComponentsPerProxy != 1.0 ||
			!ReadStringArray(Root, TEXT("protected_authored_roots"), true, OutProfile.ProtectedAuthoredRoots, OutError) ||
			!ReadStringArray(Root, TEXT("excluded_runtime_state_roots"), true, OutProfile.ExcludedRuntimeStateRoots, OutError))
		{
			OutErrorCode = TEXT("realization-profile-landscape");
			return false;
		}
		OutProfile.ComponentsPerProxy = 1;

		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		if (!Root->TryGetArrayField(TEXT("layers"), Layers) || Layers == nullptr || Layers->IsEmpty())
		{
			OutErrorCode = TEXT("realization-profile-layers");
			OutError = TEXT("Realization profile requires at least one layer.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& LayerValue : *Layers)
		{
			const TSharedPtr<FJsonObject> LayerObject = LayerValue.IsValid() ? LayerValue->AsObject() : nullptr;
			FProjectWorldRealizationLayer Layer;
			FString LayerKind;
			FString DirtyGranularity;
			double GeneratorVersion = 0.0;
			double DependencyHalo = 0.0;
			const TSharedPtr<FJsonObject>* Settings = nullptr;
			if (!LayerObject.IsValid() ||
				!HasOnlyFields(LayerObject, {
					TEXT("layer_id"), TEXT("layer_kind"), TEXT("generator_id"), TEXT("generator_version"),
					TEXT("depends_on"), TEXT("canonical_selectors"), TEXT("artifact_root"),
					TEXT("spatial_ownership"), TEXT("dirty_granularity"), TEXT("dependency_halo_cells"),
					TEXT("runtime_mapping"), TEXT("settings")}, OutError) ||
				!RequireString(LayerObject, TEXT("layer_id"), Layer.LayerId, OutError) ||
				!RequireString(LayerObject, TEXT("layer_kind"), LayerKind, OutError) || !ParseLayerKind(LayerKind, Layer.LayerKind) ||
				!RequireString(LayerObject, TEXT("generator_id"), Layer.GeneratorId, OutError) ||
				!LayerObject->TryGetNumberField(TEXT("generator_version"), GeneratorVersion) || GeneratorVersion < 1.0 || GeneratorVersion != FMath::FloorToDouble(GeneratorVersion) ||
				!ReadStringArray(LayerObject, TEXT("depends_on"), false, Layer.DependsOn, OutError) ||
				!ReadStringArray(LayerObject, TEXT("canonical_selectors"), true, Layer.CanonicalSelectors, OutError) ||
				!RequireString(LayerObject, TEXT("artifact_root"), Layer.ArtifactRoot, OutError) ||
				!RequireString(LayerObject, TEXT("spatial_ownership"), Layer.SpatialOwnership, OutError) ||
				!RequireString(LayerObject, TEXT("dirty_granularity"), DirtyGranularity, OutError) || !ParseDirtyGranularity(DirtyGranularity, Layer.DirtyGranularity) ||
				!LayerObject->TryGetNumberField(TEXT("dependency_halo_cells"), DependencyHalo) || DependencyHalo < 0.0 || DependencyHalo > 16.0 || DependencyHalo != FMath::FloorToDouble(DependencyHalo) ||
				!RequireString(LayerObject, TEXT("runtime_mapping"), Layer.RuntimeMapping, OutError) ||
				!LayerObject->TryGetObjectField(TEXT("settings"), Settings) || Settings == nullptr)
			{
				OutErrorCode = TEXT("realization-profile-layer-contract");
				return false;
			}
			Layer.GeneratorVersion = static_cast<int32>(GeneratorVersion);
			Layer.DependencyHaloCells = static_cast<int32>(DependencyHalo);
			Layer.NormalizedSettings = NormalizeJsonValue(MakeShared<FJsonValueObject>(*Settings));
			OutProfile.Layers.Add(MoveTemp(Layer));
		}
		if (!ValidateAndFinalize(OutProfile, OutError))
		{
			OutErrorCode = TEXT("realization-profile-graph");
			return false;
		}
		if (!FProjectSha256::HashFile(Path, OutProfile.ProfileHash))
		{
			OutErrorCode = TEXT("realization-profile-hash");
			OutError = Path;
			return false;
		}
		return true;
	}

	bool BuildDirtyPlan(
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldDirtyInputs& Inputs,
		TArray<FProjectWorldLayerDirtyPlan>& OutPlan,
		FString& OutError)
	{
		OutPlan.Reset();
		OutError.Reset();
		TMap<FString, const FProjectWorldRealizationLayer*> Layers;
		TMap<FString, TSet<FString>> Dirty;
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			Layers.Add(Layer.LayerId, &Layer);
			if (Layer.IsGenerated())
			{
				Dirty.Add(Layer.LayerId);
			}
		}
		auto MergeInputs = [&Layers, &Dirty, &Inputs, &OutError](
			const TMap<FString, TSet<FString>>& Source,
			bool bOperator)
		{
			for (const TPair<FString, TSet<FString>>& Pair : Source)
			{
				const FProjectWorldRealizationLayer* const* Layer = Layers.Find(Pair.Key);
				if (Layer == nullptr || !(*Layer)->IsGenerated())
				{
					OutError = FString::Printf(TEXT("Dirty input targets an unknown or non-generated layer: %s"), *Pair.Key);
					return false;
				}
				const TSet<FString>* ValidUnits = (bOperator
					? Inputs.OperatorValidUnits
					: Inputs.ValidUnits).Find(Pair.Key);
				for (const FString& Unit : Pair.Value)
				{
					if (Unit == TEXT("*"))
					{
						continue;
					}
					if (((*Layer)->DirtyGranularity != EProjectWorldDirtyGranularity::CanonicalCell &&
						(*Layer)->DirtyGranularity != EProjectWorldDirtyGranularity::ObjectId) ||
						ValidUnits == nullptr || !ValidUnits->Contains(Unit))
					{
						OutError = FString::Printf(
							TEXT("Dirty unit is outside the registered %s domain for layer %s: %s"),
							*GranularityName((*Layer)->DirtyGranularity),
							*Pair.Key,
							*Unit);
						return false;
					}
				}
				Dirty.FindChecked(Pair.Key).Append(Pair.Value);
			}
			return true;
		};
		if (Inputs.bFirstApply)
		{
			for (TPair<FString, TSet<FString>>& Pair : Dirty)
			{
				Pair.Value.Add(TEXT("*"));
			}
		}
		else if (!MergeInputs(Inputs.ComputedUnits, false))
		{
			return false;
		}
		if (!MergeInputs(Inputs.OperatorAdditions, true))
		{
			return false;
		}

		for (const FString& LayerId : Profile.TopologicalLayerIds)
		{
			const FProjectWorldRealizationLayer* Layer = Layers.FindChecked(LayerId);
			if (!Layer->IsGenerated())
			{
				continue;
			}
			for (const FString& DependencyId : Layer->DependsOn)
			{
				if (const TSet<FString>* DependencyDirty = Dirty.Find(DependencyId); DependencyDirty != nullptr && !DependencyDirty->IsEmpty())
				{
					TSet<FString> Expanded;
					const FProjectWorldRealizationLayer* Dependency = Layers.FindChecked(DependencyId);
					const TSet<FString>* ValidUnits = Inputs.ValidUnits.Find(LayerId);
					const TSet<FString> EmptyDomain;
					if (Dependency->DirtyGranularity != Layer->DirtyGranularity)
					{
						if (DependencyDirty->Contains(TEXT("*")))
						{
							Expanded.Add(TEXT("*"));
						}
						else if (const TMap<FString, TSet<FString>>* Mapping =
							Inputs.DependencyUnitMappings.Find(LayerId + TEXT("|") + DependencyId))
						{
							for (const FString& Unit : *DependencyDirty) Expanded.Append(Mapping->FindRef(Unit));
						}
						else
						{
							OutError = FString::Printf(TEXT("Dirty dependency has no typed unit mapping: %s -> %s"), *DependencyId, *LayerId);
							return false;
						}
					}
					else if (!ExpandUnits(
						*DependencyDirty,
						Layer->DirtyGranularity,
						Layer->DependencyHaloCells,
						ValidUnits != nullptr ? *ValidUnits : EmptyDomain,
						Expanded,
						OutError))
					{
						return false;
					}
					Dirty.FindChecked(LayerId).Append(Expanded);
				}
			}
			FProjectWorldLayerDirtyPlan Entry;
			Entry.LayerId = LayerId;
			for (const FString& Unit : Dirty.FindChecked(LayerId))
			{
				Entry.DirtyUnits.Add(Unit);
			}
			Entry.DirtyUnits.Sort();
			OutPlan.Add(MoveTemp(Entry));
		}
		return true;
	}
}
