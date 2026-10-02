// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UProjectMaterialSurfaceGenerateCommandlet;

struct FProjectMaterialSurfaceGenerationRequest
{
	FString RecipeRoot;
	FString OutputPackageRoot;
	FString OutputContentRoot;
	FString ManifestRoot;
	FString PatternManifestPath;
	FString PatternPackageRoot = TEXT("/ProjectTexture");
	FString PatternContentRoot;
	FString TerrainLayoutReceiptPath;
	bool bCleanupOrphans = false;
	bool bHostTransactionOwnsReplacement = false;
	FString FailureInjection;
};

struct FProjectMaterialSurfaceGenerationResult
{
	int32 Validated = 0;
	int32 Generated = 0;
	int32 Skipped = 0;
	int32 ShaderCompiles = 0;
	TArray<FString> OutputObjectPaths;
	TArray<FString> OrphanPackageNames;
	TArray<FString> RetainedOrphanPackageNames;
	FString ManifestSha256;
	FString Error;
};

class PROJECTMATERIALEDITOR_API FProjectMaterialSurfaceGenerationService final
{
public:
	static bool Validate(
		const FProjectMaterialSurfaceGenerationRequest& Request,
		FProjectMaterialSurfaceGenerationResult& OutResult);

	static bool RegenerateTestMount(
		const FProjectMaterialSurfaceGenerationRequest& Request,
		FProjectMaterialSurfaceGenerationResult& OutResult);

private:
	friend class UProjectMaterialSurfaceGenerateCommandlet;

	static bool RegenerateForCommandlet(
		const FProjectMaterialSurfaceGenerationRequest& Request,
		FProjectMaterialSurfaceGenerationResult& OutResult);

	static bool RegenerateInternal(
		const FProjectMaterialSurfaceGenerationRequest& Request,
		bool bCommandletOwner,
		bool bValidateOnly,
		FProjectMaterialSurfaceGenerationResult& OutResult);
};
