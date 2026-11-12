#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"

class FAutomationTestBase;
class FJsonObject;
class UWorld;
namespace UE::MeshPartition
{
	class UMeshPartitionDefinition;
	class UMeshPartitionEditorComponent;
	class ACompiledSection;
	struct FCompiledSectionBuildInfo;
	struct FCompiledSectionBuildVariant;
	class FMeshData;
}

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainStorageFixture
{
	bool PrepareDefinition(FAutomationTestBase& Test, UE::MeshPartition::UMeshPartitionDefinition& Definition,
		const UE::MeshPartition::UMeshPartitionDefinition& Shared);
	struct FSplitSections
	{
		TArray<UE::MeshPartition::ACompiledSection*> Sections;
		TArray<TSharedPtr<const UE::MeshPartition::FMeshData>> Parts;
	};
	bool PrepareSplit(FAutomationTestBase& Test, UE::MeshPartition::UMeshPartitionEditorComponent& Editor,
		const UE::MeshPartition::FCompiledSectionBuildInfo& Info, const UE::MeshPartition::FCompiledSectionBuildVariant& Variant,
		const UE::MeshPartition::FMeshData& Native, const FTransform& ParentToWorld, const FVector& GridOrigin,
		FSplitSections& Out);
	bool CheckShared(FAutomationTestBase& Test, const FProjectWorldCanonicalBundle& Bundle, const FSplitSections& Split,
		const TArray<TArray<FProjectWorldSurfaceTriangle>>& Actual, FJsonObject& Receipt);
	bool ReplaySaved(FAutomationTestBase& Test, UWorld& OriginalWorld, const FString& Filename,
		const FProjectWorldCanonicalBundle& Bundle, const TMap<FName, FSplitSections>& Inputs,
		TFunctionRef<void()> ReleaseOriginalWorld, FJsonObject& Receipt,TConstArrayView<FVector> RequiredPoints = {},
		bool CompareStorageFrame = true,TFunction<bool(UWorld&,FJsonObject&)> ReloadObserver = {},
		TConstArrayView<FString> ExtraPackages = {},bool CompareBase = false);
}
#endif
