// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternManifest.h"
#include "ProjectTexturePatternRecipe.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectTexturePatternRecipeTests
{
	FString ValidRecipe()
	{
		return TEXT(R"JSON({
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
	}

	FProjectTexturePatternManifestRecord Record(const FString& Output, const FString& Pattern)
	{
		const FString Hash = FString::ChrN(64, TEXT('a'));
		FProjectTexturePatternManifestRecord Result;
		Result.RecipePath = TEXT("Terrain/project_terrain_structure.pattern.json");
		Result.RecipeSha256 = Hash;
		Result.RecipeSourceSha256 = Hash;
		Result.PatternId = Pattern;
		Result.Family = TEXT("surface_structure");
		Result.Algorithm = TEXT("runtime_dag_native");
		Result.CompilerVersion = TEXT("5");
		Result.OutputContract = TEXT("surface_structure_rgb_v1");
		Result.OutputObjectPath = Output;
		Result.SemanticIdentity = Hash;
		Result.PackageSha256 = Hash;
		Result.CompilerFingerprint = Hash;
		Result.EngineCompatibility = TEXT("5.8");
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTexturePatternRecipeContractTest,
	"Project.Texture.Generation.PatternRecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTexturePatternRecipeContractTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/texture/generation/recipe_contract"));
	const FString Source = FPaths::Combine(Root, TEXT("Terrain/project_terrain_structure.pattern.json"));
	FProjectTexturePatternRecipe Recipe;
	FString Error;
	TestTrue(TEXT("The selected closed pattern recipe parses."),
		FProjectTexturePatternRecipeContract::Parse(
			ProjectTexturePatternRecipeTests::ValidRecipe(), Source, Root, Recipe, Error));
	TestEqual(TEXT("The recipe identity is SHA-256."), Recipe.RecipeSha256.Len(), 64);
	TestEqual(TEXT("The recipe source identity is SHA-256."), Recipe.RecipeSourceSha256.Len(), 64);
	const FString SemanticIdentity = Recipe.RecipeSha256;
	const FString SourceIdentity = Recipe.RecipeSourceSha256;
	TestTrue(TEXT("A formatting-only edit parses."),
		FProjectTexturePatternRecipeContract::Parse(
			ProjectTexturePatternRecipeTests::ValidRecipe() + TEXT("\n"), Source, Root, Recipe, Error));
	TestEqual(TEXT("A formatting-only edit keeps the semantic identity."), Recipe.RecipeSha256, SemanticIdentity);
	TestNotEqual(TEXT("A formatting-only edit changes the source identity."),
		Recipe.RecipeSourceSha256, SourceIdentity);
	TestEqual(TEXT("The engine compatibility identity is Major.Minor."),
		FProjectTexturePatternRecipeContract::GetEngineCompatibilityIdentity(), FString(TEXT("5.8")));
	TestEqual(TEXT("The compiler fingerprint is SHA-256."),
		FProjectTexturePatternRecipeContract::GetCompilerFingerprint().Len(), 64);

	FString UnknownGraph = ProjectTexturePatternRecipeTests::ValidRecipe().Replace(
		TEXT("\n}") , TEXT(",\n  \"unknown\": true\n}"));
	TestFalse(TEXT("An arbitrary node graph fails closed."),
		FProjectTexturePatternRecipeContract::Parse(UnknownGraph, Source, Root, Recipe, Error));
	TestTrue(TEXT("The arbitrary graph rejection names the unknown field."),
		Error.Contains(TEXT("Unknown pattern recipe field")));

	const FString MissingParent = ProjectTexturePatternRecipeTests::ValidRecipe().Replace(
		TEXT("[\"natural_noise_basis\"]"), TEXT("[\"missing_basis\"]"));
	TestFalse(TEXT("A missing DAG parent fails closed."),
		FProjectTexturePatternRecipeContract::Parse(MissingParent, Source, Root, Recipe, Error));

	TestTrue(TEXT("The selected recipe reparses for identity checks."),
		FProjectTexturePatternRecipeContract::Parse(
			ProjectTexturePatternRecipeTests::ValidRecipe(), Source, Root, Recipe, Error));
	FString PackageName;
	FString ObjectPath;
	TestTrue(TEXT("A concern-named output identity is derived."),
		FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
			Recipe, TEXT("/ProjectTexture/Patterns"), PackageName, ObjectPath, Error));
	TestEqual(TEXT("The output has no provenance folder segment."), ObjectPath,
		FString(TEXT("/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure")));
	TestFalse(TEXT("A relative output root fails closed."),
		FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
			Recipe, TEXT("ProjectTexture/Patterns"), PackageName, ObjectPath, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTexturePatternManifestContractTest,
	"Project.Texture.Generation.PatternManifestContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTexturePatternManifestContractTest::RunTest(const FString& Parameters)
{
	const auto B = ProjectTexturePatternRecipeTests::Record(
		TEXT("/ProjectTexture/Patterns/Terrain/MF_B.MF_B"), TEXT("b"));
	const auto A = ProjectTexturePatternRecipeTests::Record(
		TEXT("/ProjectTexture/Patterns/Terrain/MF_A.MF_A"), TEXT("a"));
	const FString Json = ProjectTexturePatternManifest::Serialize({B, A});
	TestFalse(TEXT("The manifest contains no CRLF bytes."), Json.Contains(TEXT("\r")));
	TestTrue(TEXT("The manifest has one final LF."), Json.EndsWith(TEXT("\n")));
	TestTrue(TEXT("Manifest records are sorted by output identity."),
		Json.Find(A.OutputObjectPath) < Json.Find(B.OutputObjectPath));
	return true;
}

#endif
