// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectTexturePatternManifestRecord
{
	FString RecipePath;
	FString RecipeSha256;
	FString RecipeSourceSha256;
	FString PatternId;
	FString Family;
	FString Algorithm;
	FString CompilerVersion;
	FString OutputContract;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
	FString CompilerFingerprint;
	FString EngineCompatibility;
};

namespace ProjectTexturePatternManifest
{
	bool Load(
		const FString& ManifestPath,
		TMap<FString, FProjectTexturePatternManifestRecord>& OutRecords,
		FString& OutError);

	FString Serialize(const TArray<FProjectTexturePatternManifestRecord>& Records);

	bool Save(
		const FString& ManifestPath,
		const TArray<FProjectTexturePatternManifestRecord>& Records,
		FString& OutManifestSha256,
		FString& OutError);
}
