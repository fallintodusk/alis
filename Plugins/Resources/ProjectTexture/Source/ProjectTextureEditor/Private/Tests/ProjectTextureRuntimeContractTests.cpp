// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternRecipe.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeGraphContractTest,
	"Project.Texture.Runtime.GraphContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeGraphContractTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/texture/generation/runtime_graph_contract"));
	const FString Source = FPaths::Combine(
		Root, TEXT("Terrain/project_terrain_structure.pattern.json"));
	const FString RecipeJson = TEXT(R"JSON({
  "$schema": "../../Schemas/pattern-recipe.schema.json",
  "schema_version": "3",
  "pattern_id": "project_terrain_structure",
  "catalog_id": "DA_ProjectTerrainStructureCatalog",
  "family": "surface_structure",
  "compiler_version": "5",
  "output_contract": "surface_structure_rgb_v1",
  "seed": 1847,
  "resolution_class": "medium",
  "nodes": [
    {
      "node_id": "natural_noise_basis",
      "algorithm": "natural_noise_basis_native",
      "parents": [],
      "output_layout": "natural_noise_basis_rgb_v1"
    },
    {
      "node_id": "natural_ground_structure",
      "algorithm": "natural_ground_structure_native",
      "parents": ["natural_noise_basis"],
      "output_layout": "surface_structure_rgb_v1"
    }
  ],
  "exports": [
    {
      "slot": "Structure",
      "node_id": "natural_ground_structure",
      "descriptor_id": "RT_ProjectTerrainStructure"
    }
  ]
})JSON");
	FProjectTexturePatternRecipe Recipe;
	FString Error;
	TestTrue(TEXT("The closed v3 runtime DAG recipe parses."),
		FProjectTexturePatternRecipeContract::Parse(RecipeJson, Source, Root, Recipe, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeStructuralIdentityTest,
	"Project.Texture.Runtime.StructuralIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeStructuralIdentityTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/texture/generation/runtime_identity"));
	const FString Source = FPaths::Combine(
		Root, TEXT("Terrain/project_terrain_structure.pattern.json"));
	const FString Base = TEXT(R"JSON({
  "$schema": "../../Schemas/pattern-recipe.schema.json",
  "schema_version": "3",
  "pattern_id": "project_terrain_structure",
  "catalog_id": "DA_ProjectTerrainStructureCatalog",
  "family": "surface_structure",
  "compiler_version": "5",
  "output_contract": "surface_structure_rgb_v1",
  "seed": 1847,
  "resolution_class": "medium",
  "nodes": [
    { "node_id": "natural_noise_basis", "algorithm": "natural_noise_basis_native", "parents": [], "output_layout": "natural_noise_basis_rgb_v1" },
    { "node_id": "natural_ground_structure", "algorithm": "natural_ground_structure_native", "parents": ["natural_noise_basis"], "output_layout": "surface_structure_rgb_v1" }
  ],
  "exports": [
    { "slot": "Structure", "node_id": "natural_ground_structure", "descriptor_id": "RT_ProjectTerrainStructure" }
  ]
})JSON");
	FProjectTexturePatternRecipe First;
	FProjectTexturePatternRecipe Same;
	FProjectTexturePatternRecipe Changed;
	FString Error;
	TestTrue(TEXT("The base structural identity parses."),
		FProjectTexturePatternRecipeContract::Parse(Base, Source, Root, First, Error));
	TestTrue(TEXT("The normalized no-op parses."),
		FProjectTexturePatternRecipeContract::Parse(Base, Source, Root, Same, Error));
	TestTrue(TEXT("The changed parent authority parses."),
		FProjectTexturePatternRecipeContract::Parse(
			Base.Replace(TEXT("1847"), TEXT("1848")), Source, Root, Changed, Error));
	TestEqual(TEXT("A normalized no-op preserves the basis identity."),
		First.Nodes[0].StructuralIdentity, Same.Nodes[0].StructuralIdentity);
	TestEqual(TEXT("A normalized no-op preserves the descendant identity."),
		First.Nodes[1].StructuralIdentity, Same.Nodes[1].StructuralIdentity);
	TestNotEqual(TEXT("A parent structural change invalidates the parent."),
		First.Nodes[0].StructuralIdentity, Changed.Nodes[0].StructuralIdentity);
	TestNotEqual(TEXT("A parent structural change invalidates its descendant."),
		First.Nodes[1].StructuralIdentity, Changed.Nodes[1].StructuralIdentity);
	return true;
}

#endif
