// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternBuilder.h"

#include "ProjectTextureRuntimeAssetBuilder.h"

bool ProjectTexturePatternBuilder::Build(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError)
{
	return ProjectTextureRuntimeAssetBuilder::Build(
		Recipe, PackageName, OutAsset, OutError);
}

bool ProjectTexturePatternBuilder::Verify(
	const FProjectTexturePatternRecipe& Recipe,
	UObject* Asset,
	FString& OutError)
{
	return ProjectTextureRuntimeAssetBuilder::Verify(Recipe, Asset, OutError);
}
