// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimePartitionPolicy.h"

#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/WorldPartition.h"

namespace ProjectWorldRuntimePartitionPolicy
{
	namespace
	{
		bool ResolveMainPartition(
			UWorld* World,
			UWorldPartitionRuntimeHashSet*& OutRuntimeHash,
			URuntimePartitionLHGrid*& OutPartition,
			FString& OutError)
		{
			UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
			OutRuntimeHash = WorldPartition != nullptr
				? Cast<UWorldPartitionRuntimeHashSet>(WorldPartition->RuntimeHash)
				: nullptr;
			FArrayProperty* RuntimePartitions = OutRuntimeHash != nullptr
				? FindFProperty<FArrayProperty>(OutRuntimeHash->GetClass(), TEXT("RuntimePartitions"))
				: nullptr;
			if (RuntimePartitions == nullptr)
			{
				OutError = TEXT("Generated world must use the supported Runtime Hash Set partition array.");
				return false;
			}
			FScriptArrayHelper Partitions(
				RuntimePartitions,
				RuntimePartitions->ContainerPtrToValuePtr<void>(OutRuntimeHash));
			if (Partitions.Num() != 1)
			{
				OutError = FString::Printf(TEXT("Runtime profile requires exactly one partition; found %d."), Partitions.Num());
				return false;
			}
			const FRuntimePartitionDesc* Desc = reinterpret_cast<const FRuntimePartitionDesc*>(Partitions.GetRawPtr(0));
			OutPartition = Desc != nullptr ? Cast<URuntimePartitionLHGrid>(Desc->MainLayer.Get()) : nullptr;
			if (OutPartition == nullptr)
			{
				OutError = TEXT("The one runtime partition must use UE 5.8 LHGrid.");
				return false;
			}
			return true;
		}

		bool ResolveProperties(
			URuntimePartitionLHGrid* Partition,
			FNumericProperty*& OutCellSize,
			FBoolProperty*& OutIs2D,
			FString& OutError)
		{
			OutCellSize = FindFProperty<FNumericProperty>(Partition->GetClass(), TEXT("CellSize"));
			OutIs2D = FindFProperty<FBoolProperty>(Partition->GetClass(), TEXT("bIs2D"));
			if (OutCellSize == nullptr || OutIs2D == nullptr)
			{
				OutError = TEXT("Cannot access the UE 5.8 LHGrid cell-size and 2D properties.");
				return false;
			}
			return true;
		}
	}

	bool Read(
		UWorld* World,
		FProjectWorldRuntimePartitionSettings& OutSettings,
		FString& OutError)
	{
		OutSettings = FProjectWorldRuntimePartitionSettings();
		UWorldPartitionRuntimeHashSet* RuntimeHash = nullptr;
		URuntimePartitionLHGrid* Partition = nullptr;
		FNumericProperty* CellSizeProperty = nullptr;
		FBoolProperty* Is2DProperty = nullptr;
		if (!ResolveMainPartition(World, RuntimeHash, Partition, OutError) ||
			!ResolveProperties(Partition, CellSizeProperty, Is2DProperty, OutError))
		{
			return false;
		}
		OutSettings.PartitionCount = 1;
		OutSettings.CellSizeCentimeters = static_cast<int32>(Partition->GetCellSize());
		OutSettings.LoadingRangeCentimeters = Partition->LoadingRange;
		OutSettings.bIs2D = Is2DProperty->GetPropertyValue_InContainer(Partition);
		OutSettings.bBlockOnSlowStreaming = Partition->bBlockOnSlowStreaming;
		return true;
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldRuntimePartitionSettings& Settings,
		bool& bOutChanged,
		FString& OutError)
	{
		bOutChanged = false;
		if (Settings.PartitionCount != 1 || Settings.CellSizeCentimeters < 1600 ||
			Settings.LoadingRangeCentimeters < Settings.CellSizeCentimeters || !Settings.bIs2D)
		{
			OutError = TEXT("Runtime partition settings violate the one-grid 2D LHGrid contract.");
			return false;
		}
		UWorldPartitionRuntimeHashSet* RuntimeHash = nullptr;
		URuntimePartitionLHGrid* Partition = nullptr;
		FNumericProperty* CellSizeProperty = nullptr;
		FBoolProperty* Is2DProperty = nullptr;
		if (!ResolveMainPartition(World, RuntimeHash, Partition, OutError) ||
			!ResolveProperties(Partition, CellSizeProperty, Is2DProperty, OutError))
		{
			return false;
		}
		bOutChanged = Partition->GetCellSize() != static_cast<uint32>(Settings.CellSizeCentimeters) ||
			Partition->LoadingRange != Settings.LoadingRangeCentimeters ||
			Is2DProperty->GetPropertyValue_InContainer(Partition) != Settings.bIs2D ||
			Partition->bBlockOnSlowStreaming != Settings.bBlockOnSlowStreaming;
		if (bOutChanged)
		{
			RuntimeHash->Modify();
			Partition->Modify();
			CellSizeProperty->SetIntPropertyValue(
				CellSizeProperty->ContainerPtrToValuePtr<void>(Partition),
				static_cast<uint64>(Settings.CellSizeCentimeters));
			Partition->LoadingRange = Settings.LoadingRangeCentimeters;
			Is2DProperty->SetPropertyValue_InContainer(Partition, Settings.bIs2D);
			Partition->bBlockOnSlowStreaming = Settings.bBlockOnSlowStreaming;
			Partition->MarkPackageDirty();
		}

		FProjectWorldRuntimePartitionSettings Actual;
		if (!Read(World, Actual, OutError) ||
			Actual.PartitionCount != Settings.PartitionCount ||
			Actual.CellSizeCentimeters != Settings.CellSizeCentimeters ||
			Actual.LoadingRangeCentimeters != Settings.LoadingRangeCentimeters ||
			Actual.bIs2D != Settings.bIs2D ||
			Actual.bBlockOnSlowStreaming != Settings.bBlockOnSlowStreaming)
		{
			OutError = TEXT("Runtime Hash Set read-back does not match the requested profile.");
			return false;
		}
		return true;
	}
}
