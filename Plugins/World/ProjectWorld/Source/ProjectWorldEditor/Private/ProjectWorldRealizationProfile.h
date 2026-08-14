// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class EProjectWorldLayerKind : uint8
{
	GeneratedGeography,
	GeneratedGameplayPlacement,
	ProtectedAuthoredOverlay,
	RuntimeStateExclusion
};

enum class EProjectWorldDirtyGranularity : uint8
{
	WholeLayer,
	CanonicalCell,
	SourceTile,
	ObjectId,
	Never
};

struct FProjectWorldRealizationLayer
{
	FString LayerId;
	EProjectWorldLayerKind LayerKind = EProjectWorldLayerKind::GeneratedGeography;
	FString GeneratorId;
	int32 GeneratorVersion = 0;
	TArray<FString> DependsOn;
	TArray<FString> CanonicalSelectors;
	FString ArtifactRoot;
	FString SpatialOwnership;
	EProjectWorldDirtyGranularity DirtyGranularity = EProjectWorldDirtyGranularity::WholeLayer;
	int32 DependencyHaloCells = 0;
	FString RuntimeMapping;
	FString NormalizedSettings;
	FString ContractHash;

	bool IsGenerated() const
	{
		return LayerKind == EProjectWorldLayerKind::GeneratedGeography ||
			LayerKind == EProjectWorldLayerKind::GeneratedGameplayPlacement;
	}
};

struct FProjectWorldRealizationProfile
{
	FString ProfileId;
	FString ProfileHash;
	FString ExecutionHash;
	FString WorldDataPluginName;
	FString CanonicalProfileId;
	FString MapPackagePath;
	FString RuntimeProfileId;
	FString LogicalLandscapeId;
	int32 ComponentsPerProxy = 0;
	TArray<FString> ProtectedAuthoredRoots;
	TArray<FString> ExcludedRuntimeStateRoots;
	TArray<FProjectWorldRealizationLayer> Layers;
	TArray<FString> TopologicalLayerIds;
};

struct FProjectWorldLayerDirtyPlan
{
	FString LayerId;
	TArray<FString> DirtyUnits;
};

struct FProjectWorldDirtyInputs
{
	bool bFirstApply = false;
	TMap<FString, TSet<FString>> ComputedUnits;
	TMap<FString, TSet<FString>> OperatorAdditions;
	TMap<FString, TSet<FString>> ValidUnits;
	TMap<FString, TSet<FString>> OperatorValidUnits;
};

namespace ProjectWorldRealizationProfile
{
	bool Load(
		const FString& Path,
		FProjectWorldRealizationProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError);

	bool ValidateAndFinalize(
		FProjectWorldRealizationProfile& Profile,
		FString& OutError);

	bool IsGeneratorRegistered(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		EProjectWorldLayerKind LayerKind);

	bool BuildDirtyPlan(
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldDirtyInputs& Inputs,
		TArray<FProjectWorldLayerDirtyPlan>& OutPlan,
		FString& OutError);
}
