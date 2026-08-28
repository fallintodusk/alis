// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class EProjectMaterialArtifactKind : uint8
{
	Parent,
	Instance
};

struct FProjectMaterialRecipe
{
	FString SourcePath;
	FString RelativeFolder;
	FString MaterialId;
	EProjectMaterialArtifactKind ArtifactKind = EProjectMaterialArtifactKind::Parent;
	FString Family;
	FString Archetype;
	FString CompilerVersion;
	FString ParentObjectPath;
	TMap<FName, double> Scalars;
	TMap<FName, FLinearColor> Vectors;
	FString NormalizedSemantics;
	FString RecipeSha256;
};

class PROJECTMATERIALEDITOR_API FProjectMaterialRecipeContract
{
public:
	static bool Parse(
		const FString& Json,
		const FString& SourcePath,
		const FString& RecipeRoot,
		FProjectMaterialRecipe& OutRecipe,
		FString& OutError);

	static bool ResolveOutputIdentity(
		const FProjectMaterialRecipe& Recipe,
		const FString& OutputPackageRoot,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutError);

	static FString ComputeArtifactSemanticIdentity(
		const FProjectMaterialRecipe& Recipe,
		const FString& OutputObjectPath,
		const FString& DependencyPackageSha256);

	static FString ComputeSha256(const TArrayView<const uint8> Bytes);
	static FString ComputeStringSha256(const FString& Value);
	static FString GetEngineIdentity();
	static FString GetCompilerFingerprint();
};
