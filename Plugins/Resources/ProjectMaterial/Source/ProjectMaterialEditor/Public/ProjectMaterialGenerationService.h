// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UProjectMaterialGenerateCommandlet;

struct FProjectMaterialGenerationRequest
{
	FString RecipeRoot;
	FString OutputPackageRoot;
	FString OutputContentRoot;
	FString ManifestRoot;
	bool bCleanupOrphans = false;
	bool bHostTransactionOwnsReplacement = false;
	FString FailureInjection;
};

struct FProjectMaterialGenerationResult
{
	int32 Validated = 0;
	int32 Generated = 0;
	int32 Skipped = 0;
	int32 ShaderCompiles = 0;
	TArray<FString> OutputObjectPaths;
	TArray<FString> OrphanPackageNames;
	FString ManifestSha256;
	FString Error;
};

class PROJECTMATERIALEDITOR_API FProjectMaterialGenerationService
{
public:
	static bool Validate(
		const FProjectMaterialGenerationRequest& Request,
		FProjectMaterialGenerationResult& OutResult);

	static bool RegenerateTestMount(
		const FProjectMaterialGenerationRequest& Request,
		FProjectMaterialGenerationResult& OutResult);

	static bool HasAcceptedOutputIntegrity(
		const FString& PackageFile,
		const FString& AcceptedSha256);

private:
	friend class UProjectMaterialGenerateCommandlet;

	static bool RegenerateForCommandlet(
		const FProjectMaterialGenerationRequest& Request,
		FProjectMaterialGenerationResult& OutResult);

	static bool RegenerateInternal(
		const FProjectMaterialGenerationRequest& Request,
		bool bCommandletOwner,
		FProjectMaterialGenerationResult& OutResult);
};
