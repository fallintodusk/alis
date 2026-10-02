// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectMaterialPatternDependency;
struct FProjectMaterialTerrainLayoutContract;

enum class EProjectMaterialSurfaceArtifactKind : uint8
{
	Parent,
	Instance
};

enum class EProjectMaterialProjectionFrame : uint8
{
	WorldMetricTriplanar2D,
	ObjectMetricTriplanar2D
};

struct FProjectMaterialSurfaceRecipe
{
	FString SourcePath;
	FString RelativeFolder;
	FString MaterialId;
	EProjectMaterialSurfaceArtifactKind ArtifactKind = EProjectMaterialSurfaceArtifactKind::Parent;
	FString BindingId;
	FString Family;
	FString Archetype;
	FString CompilerVersion;
	FString ParentObjectPath;
	EProjectMaterialProjectionFrame ProjectionFrame = EProjectMaterialProjectionFrame::WorldMetricTriplanar2D;
	FString PatternId;
	TArray<FName> SemanticBindings;
	TMap<FName, double> Scalars;
	TMap<FName, FLinearColor> Vectors;
	FString NormalizedSemantics;
	FString RecipeSha256;
	FString RecipeSourceSha256;
};

class PROJECTMATERIALEDITOR_API FProjectMaterialSurfaceRecipeContract final
{
public:
	static bool Parse(
		const FString& Json,
		const FString& SourcePath,
		const FString& RecipeRoot,
		FProjectMaterialSurfaceRecipe& OutRecipe,
		FString& OutError);

	static bool ResolveOutputIdentity(
		const FProjectMaterialSurfaceRecipe& Recipe,
		const FString& OutputPackageRoot,
		FString& OutPackageName,
		FString& OutObjectPath,
		FString& OutError);

	static FString ComputeArtifactSemanticIdentity(
		const FProjectMaterialSurfaceRecipe& Recipe,
		const FString& OutputObjectPath,
		const FProjectMaterialPatternDependency& Pattern,
		const FString& ParentPackageSha256,
		const FProjectMaterialTerrainLayoutContract* TerrainLayout);

	static FString GetEngineCompatibilityIdentity();
};
