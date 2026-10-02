// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectTexturePatternNode
{
	FString NodeId;
	FString Algorithm;
	TArray<FString> Parents;
	FString OutputLayout;
	FString StructuralIdentity;
};

struct FProjectTexturePatternExport
{
	FString Slot;
	FString NodeId;
	FString DescriptorId;
};

struct FProjectTexturePatternRecipe
{
	FString SourcePath;
	FString RelativeFolder;
	FString PatternId;
	FString CatalogId;
	FString Family;
	FString Algorithm;
	FString CompilerVersion;
	FString OutputContract;
	int32 Seed = 0;
	FString ResolutionClass;
	TArray<FProjectTexturePatternNode> Nodes;
	TArray<FProjectTexturePatternExport> Exports;
	FString NormalizedSemantics;
	FString RecipeSha256;
	FString RecipeSourceSha256;
};

class PROJECTTEXTUREEDITOR_API FProjectTexturePatternRecipeContract final
{
public:
	static bool Parse(
		const FString& Json,
		const FString& SourcePath,
		const FString& RecipeRoot,
		FProjectTexturePatternRecipe& OutRecipe,
		FString& OutError);

	static bool ResolveOutputIdentity(
		const FProjectTexturePatternRecipe& Recipe,
		const FString& OutputPackageRoot,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutError);

	static FString ComputeArtifactSemanticIdentity(
		const FProjectTexturePatternRecipe& Recipe,
		const FString& OutputObjectPath);

	static FString ComputeStringSha256(const FString& Value);
	static FString GetEngineCompatibilityIdentity();
	static FString GetCompilerFingerprint();
};
