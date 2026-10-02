#include "ProjectWorldTerrainProducerRegistry.h"

namespace ProjectWorldTerrainProducerRegistry
{
	namespace
	{
		TMap<FString, FProjectWorldTerrainProducerContract>& Contracts()
		{
			static TMap<FString, FProjectWorldTerrainProducerContract> Value;
			return Value;
		}

		FString Key(const FString& GeneratorId, int32 GeneratorVersion)
		{
			return FString::Printf(TEXT("%s:v%d"), *GeneratorId, GeneratorVersion);
		}

		const FProjectWorldTerrainProducerContract* Find(const FString& GeneratorId, int32 GeneratorVersion)
		{
			return Contracts().Find(Key(GeneratorId, GeneratorVersion));
		}
	}

	bool Register(FProjectWorldTerrainProducerContract Contract, FString& OutError)
	{
		const FString ContractKey = Key(Contract.GeneratorId, Contract.GeneratorVersion);
		if (Contract.GeneratorId.IsEmpty() || Contract.GeneratorVersion < 1 ||
			Contract.CanonicalSelectors.IsEmpty() || !Contract.ValidateSettings || !Contract.Apply ||
			!Contract.Delete || !Contract.CaptureArtifacts || Contracts().Contains(ContractKey))
		{
			OutError = FString::Printf(TEXT("Terrain producer registration is invalid or duplicated: %s"), *ContractKey);
			return false;
		}
		Contracts().Add(ContractKey, MoveTemp(Contract));
		return true;
	}

	void Unregister(const FString& GeneratorId, int32 GeneratorVersion)
	{
		Contracts().Remove(Key(GeneratorId, GeneratorVersion));
	}

	bool IsRegistered(const FString& GeneratorId, int32 GeneratorVersion)
	{
		return Find(GeneratorId, GeneratorVersion) != nullptr;
	}

	bool ValidateLayer(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		const TArray<FString>& CanonicalSelectors,
		const FString& SpatialOwnership,
		const FString& RuntimeMapping,
		int32 DependencyHaloCells,
		const FString& NormalizedSettings,
		FString& OutError)
	{
		const FProjectWorldTerrainProducerContract* Contract = Find(GeneratorId, GeneratorVersion);
		if (Contract == nullptr || Contract->CanonicalSelectors != CanonicalSelectors ||
			Contract->SpatialOwnership != SpatialOwnership || Contract->RuntimeMapping != RuntimeMapping ||
			Contract->DependencyHaloCells != DependencyHaloCells)
		{
			OutError = FString::Printf(TEXT("Terrain producer layer contract does not match registration: %s"),
				*Key(GeneratorId, GeneratorVersion));
			return false;
		}
		return Contract->ValidateSettings(NormalizedSettings, OutError);
	}

	bool Apply(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& NormalizedSettings,
		UMaterialInterface* Material,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldTerrainProducerContract* Contract = Find(GeneratorId, GeneratorVersion);
		return Contract != nullptr && Contract->Apply(
			World, Bundle, NormalizedSettings, Material, OutResult, OutError);
	}

	bool Delete(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldTerrainProducerContract* Contract = Find(GeneratorId, GeneratorVersion);
		return Contract != nullptr && Contract->Delete(World, Bundle, OutResult, OutError);
	}

	bool CaptureArtifacts(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldTerrainProducerContract* Contract = Find(GeneratorId, GeneratorVersion);
		return Contract != nullptr && Contract->CaptureArtifacts(
			World, Bundle, Inventory, OutResult, OutError);
	}
}
