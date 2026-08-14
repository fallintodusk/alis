// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLayerDirtyInput.h"

#include "ProjectWorldSchemaReference.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldLayerDirtyInput
{
	namespace
	{
		const TCHAR* ExpectedSchemaFilename = TEXT("project_world_layer_dirty_input.schema.json");

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

		bool IsSha256(const FString& Value)
		{
			if (Value.Len() != 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
				const bool bHex = Character >= TEXT('a') && Character <= TEXT('f');
				if (!bDigit && !bHex)
				{
					return false;
				}
			}
			return true;
		}

		bool HasOnlyFields(
			const TSharedPtr<FJsonObject>& Object,
			std::initializer_list<const TCHAR*> Allowed,
			FString& OutError)
		{
			TSet<FString> Fields;
			for (const TCHAR* Field : Allowed)
			{
				Fields.Add(Field);
			}
			for (const auto& Field : Object->Values)
			{
				if (!Fields.Contains(FString(Field.Key.ToView())))
				{
					OutError = FString::Printf(TEXT("Unknown dirty-input field: %s"), *FString(Field.Key.ToView()));
					return false;
				}
			}
			return true;
		}

		bool ReadInputRecords(
			const TSharedPtr<FJsonObject>& Object,
			TMap<FString, FString>& OutInputs,
			FString& OutError)
		{
			const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
			if (!Object->TryGetArrayField(TEXT("canonical_inputs"), Inputs) || Inputs == nullptr)
			{
				OutError = TEXT("Layer base has no canonical_inputs array.");
				return false;
			}
			for (const TSharedPtr<FJsonValue>& InputValue : *Inputs)
			{
				const TSharedPtr<FJsonObject> Input = InputValue.IsValid() ? InputValue->AsObject() : nullptr;
				FString UnitId;
				FString Hash;
				if (!Input.IsValid() ||
					!HasOnlyFields(Input, {TEXT("unit_id"), TEXT("sha256")}, OutError) ||
					!Input->TryGetStringField(TEXT("unit_id"), UnitId) || UnitId.IsEmpty() ||
					!Input->TryGetStringField(TEXT("sha256"), Hash) || !IsSha256(Hash) ||
					OutInputs.Contains(UnitId))
				{
					OutError = TEXT("Layer base contains an invalid or duplicate canonical input.");
					return false;
				}
				OutInputs.Add(UnitId, Hash);
			}
			return true;
		}
	}

	bool Load(
		const FString& Path,
		FProjectWorldLayerDirtyInput& OutInput,
		FString& OutErrorCode,
		FString& OutError)
	{
		OutInput = FProjectWorldLayerDirtyInput();
		OutErrorCode.Reset();
		OutError.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutErrorCode = TEXT("layer-dirty-input-read");
			OutError = Path;
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FString Schema;
		double SchemaVersion = 0.0;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!HasOnlyFields(Root, {
				TEXT("$schema"), TEXT("schema_version"), TEXT("realization_profile_id"),
				TEXT("base_layers"), TEXT("operator_additions")}, OutError) ||
			!Root->TryGetStringField(TEXT("$schema"), Schema) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(
				Path,
				Schema,
				ExpectedSchemaFilename,
				OutError) ||
			!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1.0 ||
			!Root->TryGetStringField(TEXT("realization_profile_id"), OutInput.RealizationProfileId) ||
			!IsIdentifier(OutInput.RealizationProfileId))
		{
			OutErrorCode = TEXT("layer-dirty-input-contract");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* BaseLayers = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Additions = nullptr;
		if (!Root->TryGetArrayField(TEXT("base_layers"), BaseLayers) || BaseLayers == nullptr ||
			!Root->TryGetArrayField(TEXT("operator_additions"), Additions) || Additions == nullptr)
		{
			OutErrorCode = TEXT("layer-dirty-input-contract");
			OutError = TEXT("Dirty input requires base_layers and operator_additions arrays.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& BaseValue : *BaseLayers)
		{
			const TSharedPtr<FJsonObject> Base = BaseValue.IsValid() ? BaseValue->AsObject() : nullptr;
			FString LayerId;
			FProjectWorldLayerBaseIdentity Identity;
			if (!Base.IsValid() ||
				!HasOnlyFields(Base, {
					TEXT("layer_id"), TEXT("normalized_layer_contract_sha256"), TEXT("canonical_inputs")}, OutError) ||
				!Base->TryGetStringField(TEXT("layer_id"), LayerId) || !IsIdentifier(LayerId) ||
				!Base->TryGetStringField(
					TEXT("normalized_layer_contract_sha256"),
					Identity.NormalizedLayerContractHash) ||
				!IsSha256(Identity.NormalizedLayerContractHash) ||
				OutInput.BaseLayers.Contains(LayerId) ||
				!ReadInputRecords(Base, Identity.CanonicalInputs, OutError))
			{
				OutErrorCode = TEXT("layer-dirty-input-base");
				return false;
			}
			OutInput.BaseLayers.Add(LayerId, MoveTemp(Identity));
		}
		for (const TSharedPtr<FJsonValue>& AdditionValue : *Additions)
		{
			const TSharedPtr<FJsonObject> Addition = AdditionValue.IsValid() ? AdditionValue->AsObject() : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
			FString LayerId;
			if (!Addition.IsValid() ||
				!HasOnlyFields(Addition, {TEXT("layer_id"), TEXT("units")}, OutError) ||
				!Addition->TryGetStringField(TEXT("layer_id"), LayerId) || !IsIdentifier(LayerId) ||
				OutInput.OperatorAdditions.Contains(LayerId) ||
				!Addition->TryGetArrayField(TEXT("units"), Units) || Units == nullptr || Units->IsEmpty())
			{
				OutErrorCode = TEXT("layer-dirty-input-addition");
				return false;
			}
			TSet<FString>& LayerUnits = OutInput.OperatorAdditions.Add(LayerId);
			for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
			{
				FString Unit;
				if (!UnitValue.IsValid() || !UnitValue->TryGetString(Unit) || Unit.IsEmpty() || LayerUnits.Contains(Unit))
				{
					OutErrorCode = TEXT("layer-dirty-input-addition");
					OutError = TEXT("Operator additions contain an invalid or duplicate unit.");
					return false;
				}
				LayerUnits.Add(Unit);
			}
		}
		if (!FProjectSha256::HashFile(Path, OutInput.InputHash))
		{
			OutErrorCode = TEXT("layer-dirty-input-hash");
			OutError = Path;
			return false;
		}
		return true;
	}
}
