// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectMaterialManifestRecord
{
	FString RecipePath;
	FString RecipeSha256;
	FString Family;
	FString Archetype;
	FString CompilerVersion;
	FString DependencyObjectPath;
	FString DependencyPackageSha256;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
	FString CompilerFingerprint;
	FString EngineIdentity;
};

namespace ProjectMaterialManifest
{
bool Load(
	const FString& ManifestPath,
	TMap<FString, FProjectMaterialManifestRecord>& OutRecords,
	FString& OutError);

bool Save(
	const FString& ManifestPath,
	const TArray<FProjectMaterialManifestRecord>& Records,
	FString& OutSha256,
	FString& OutError);

FString Serialize(const TArray<FProjectMaterialManifestRecord>& Records);
}
