// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectMaterialSurfaceManifestRecord
{
	FString RecipePath;
	FString RecipeSha256;
	FString RecipeSourceSha256;
	FString BindingId;
	FString Family;
	FString Archetype;
	FString CompilerVersion;
	FString PatternId;
	FString PatternObjectPath;
	FString PatternOutputContract;
	FString PatternSemanticIdentity;
	FString PatternPackageSha256;
	FString DependencyObjectPath;
	FString DependencyPackageSha256;
	FString TerrainLayoutReceiptSha256;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
	FString CompilerFingerprint;
	FString EngineCompatibility;
};

namespace ProjectMaterialSurfaceManifest
{
	bool Load(
		const FString& ManifestPath,
		TMap<FString, FProjectMaterialSurfaceManifestRecord>& OutRecords,
		FString& OutError);

	FString Serialize(const TArray<FProjectMaterialSurfaceManifestRecord>& Records);

	bool Save(
		const FString& ManifestPath,
		const TArray<FProjectMaterialSurfaceManifestRecord>& Records,
		FString& OutManifestSha256,
		FString& OutError);
}
