// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UProjectTextureGenerateCommandlet;

struct FProjectTextureGenerationRequest
{
	FString RecipeRoot;
	FString OutputPackageRoot;
	FString OutputContentRoot;
	FString ManifestRoot;
	bool bCleanupOrphans = false;
	bool bHostTransactionOwnsReplacement = false;
	FString FailureInjection;
};

struct FProjectTextureGenerationResult
{
	int32 Validated = 0;
	int32 Generated = 0;
	int32 Skipped = 0;
	TArray<FString> OutputObjectPaths;
	TArray<FString> OrphanPackageNames;
	TArray<FString> RetainedOrphanPackageNames;
	FString ManifestSha256;
	FString Error;
};

class PROJECTTEXTUREEDITOR_API FProjectTextureGenerationService final
{
public:
	static bool Validate(
		const FProjectTextureGenerationRequest& Request,
		FProjectTextureGenerationResult& OutResult);

	static bool RegenerateTestMount(
		const FProjectTextureGenerationRequest& Request,
		FProjectTextureGenerationResult& OutResult);

private:
	friend class UProjectTextureGenerateCommandlet;

	static bool RegenerateForCommandlet(
		const FProjectTextureGenerationRequest& Request,
		FProjectTextureGenerationResult& OutResult);

	static bool RegenerateInternal(
		const FProjectTextureGenerationRequest& Request,
		bool bCommandletOwner,
		bool bValidateOnly,
		FProjectTextureGenerationResult& OutResult);
};
