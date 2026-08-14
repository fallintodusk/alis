// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPartitionPolicy.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/WorldPartition.h"

namespace ProjectWorldPartitionPolicy
{
	namespace
	{
		struct FHLODReflection
		{
			FArrayProperty* RuntimePartitions = nullptr;
			FArrayProperty* HLODSetups = nullptr;
			FArrayProperty* HLODLayers = nullptr;

			bool Resolve(UWorldPartitionRuntimeHashSet* RuntimeHash)
			{
				RuntimePartitions = FindFProperty<FArrayProperty>(
					RuntimeHash->GetClass(), TEXT("RuntimePartitions"));
				FStructProperty* RuntimePartition = RuntimePartitions != nullptr
					? CastField<FStructProperty>(RuntimePartitions->Inner)
					: nullptr;
				HLODSetups = RuntimePartition != nullptr
					? FindFProperty<FArrayProperty>(RuntimePartition->Struct, TEXT("HLODSetups"))
					: nullptr;
				FStructProperty* HLODSetup = HLODSetups != nullptr
					? CastField<FStructProperty>(HLODSetups->Inner)
					: nullptr;
				HLODLayers = HLODSetup != nullptr
					? FindFProperty<FArrayProperty>(HLODSetup->Struct, TEXT("HLODLayers"))
					: nullptr;
				return RuntimePartitions != nullptr && HLODSetups != nullptr && HLODLayers != nullptr;
			}
		};

		int32 CountRuntimeHLODLayerReferences(
			UWorldPartitionRuntimeHashSet* RuntimeHash,
			const FHLODReflection& Reflection)
		{
			int32 Count = 0;
			FScriptArrayHelper Partitions(
				Reflection.RuntimePartitions,
				Reflection.RuntimePartitions->ContainerPtrToValuePtr<void>(RuntimeHash));
			for (int32 PartitionIndex = 0; PartitionIndex < Partitions.Num(); ++PartitionIndex)
			{
				FScriptArrayHelper Setups(
					Reflection.HLODSetups,
					Reflection.HLODSetups->ContainerPtrToValuePtr<void>(Partitions.GetRawPtr(PartitionIndex)));
				for (int32 SetupIndex = 0; SetupIndex < Setups.Num(); ++SetupIndex)
				{
					FScriptArrayHelper Layers(
						Reflection.HLODLayers,
						Reflection.HLODLayers->ContainerPtrToValuePtr<void>(Setups.GetRawPtr(SetupIndex)));
					Count += Layers.Num();
				}
			}
			return Count;
		}
	}

	int32 DisableGeneratedActorHLOD(UWorld* World)
	{
		const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));
		int32 ChangedActorCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(GeneratedTag))
			{
				continue;
			}
			if (It->bEnableAutoLODGeneration || It->GetHLODLayer() != nullptr)
			{
				It->Modify();
				It->bEnableAutoLODGeneration = false;
				It->SetHLODLayer(nullptr);
				It->MarkPackageDirty();
				++ChangedActorCount;
			}
		}
		return ChangedActorCount;
	}

	int32 CountHLODLayerReferences(UWorld* World)
	{
		UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
		UWorldPartitionRuntimeHashSet* RuntimeHash = WorldPartition != nullptr
			? Cast<UWorldPartitionRuntimeHashSet>(WorldPartition->RuntimeHash)
			: nullptr;
		if (RuntimeHash == nullptr)
		{
			return INDEX_NONE;
		}
		FHLODReflection Reflection;
		if (!Reflection.Resolve(RuntimeHash))
		{
			return INDEX_NONE;
		}
		return (WorldPartition->GetDefaultHLODLayer() != nullptr ? 1 : 0) +
			CountRuntimeHLODLayerReferences(RuntimeHash, Reflection);
	}

	bool DisableHLOD(UWorld* World, FString& OutError)
	{
		UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
		UWorldPartitionRuntimeHashSet* RuntimeHash = WorldPartition != nullptr
			? Cast<UWorldPartitionRuntimeHashSet>(WorldPartition->RuntimeHash)
			: nullptr;
		if (RuntimeHash == nullptr)
		{
			OutError = TEXT("Generated world must use the supported World Partition Runtime Hash Set.");
			return false;
		}

		FHLODReflection Reflection;
		if (!Reflection.Resolve(RuntimeHash))
		{
			OutError = TEXT("Cannot access the Runtime Hash Set HLOD setup contract.");
			return false;
		}
		if (CountHLODLayerReferences(World) == 0)
		{
			return true;
		}

		WorldPartition->Modify();
		RuntimeHash->Modify();
		WorldPartition->SetDefaultHLODLayer(nullptr);
		FScriptArrayHelper Partitions(
			Reflection.RuntimePartitions,
			Reflection.RuntimePartitions->ContainerPtrToValuePtr<void>(RuntimeHash));
		for (int32 PartitionIndex = 0; PartitionIndex < Partitions.Num(); ++PartitionIndex)
		{
			FScriptArrayHelper Setups(
				Reflection.HLODSetups,
				Reflection.HLODSetups->ContainerPtrToValuePtr<void>(Partitions.GetRawPtr(PartitionIndex)));
			for (int32 SetupIndex = 0; SetupIndex < Setups.Num(); ++SetupIndex)
			{
				FScriptArrayHelper Layers(
					Reflection.HLODLayers,
					Reflection.HLODLayers->ContainerPtrToValuePtr<void>(Setups.GetRawPtr(SetupIndex)));
				Layers.EmptyValues();
			}
		}
		if (CountHLODLayerReferences(World) != 0)
		{
			OutError = TEXT("Generated world retained an HLOD layer reference.");
			return false;
		}
		return true;
	}
}
