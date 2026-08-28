// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialRecipe.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialRecipeTests
{
FString ValidRecipe(const FString& ExtraField = {})
{
	return FString::Printf(TEXT(R"JSON({
  "$schema": "../../Schemas/material-recipe.schema.json",
  "schema_version": "1",
  "material_id": "M_ProjectTerrain",
  "artifact_kind": "parent",
  "family": "surface_opaque",
  "archetype": "landscape_basic_v1",
  "compiler_version": "1",
  "scalars": { "Roughness": 0.9, "SlopeContrast": 1.5 },
  "vectors": {
    "LowSlopeColor": [0.08, 0.18, 0.04, 1.0],
    "SteepSlopeColor": [0.16, 0.12, 0.08, 1.0]
  }%s
})JSON"), *ExtraField);
}

FString RecipeWithSlopeContrast(const FString& Value)
{
	return ValidRecipe().Replace(
		TEXT("\"SlopeContrast\": 1.5"),
		*(FString(TEXT("\"SlopeContrast\": ")) + Value));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialRecipeContractTest,
	"Project.Material.Generation.RecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialRecipeContractTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation/recipe_contract"));
	const FString Source = FPaths::Combine(Root, TEXT("Terrain/M_ProjectTerrain.material.json"));
	FProjectMaterialRecipe Recipe;
	FString Error;
	TestTrue(TEXT("The selected closed recipe parses."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::ValidRecipe(), Source, Root, Recipe, Error));
	TestEqual(TEXT("The stable recipe identity is SHA-256."), Recipe.RecipeSha256.Len(), 64);
	TestFalse(TEXT("An unknown root field fails closed."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::ValidRecipe(TEXT(", \"nodes\": []")), Source, Root, Recipe, Error));
	TestTrue(TEXT("The rejection identifies the unknown field."), Error.Contains(TEXT("Unknown recipe field")));
	const FString WrongSchemaPath = ProjectMaterialRecipeTests::ValidRecipe().Replace(
		TEXT("../../Schemas/material-recipe.schema.json"), TEXT("material-recipe.schema.json"));
	TestFalse(TEXT("A non-canonical schema reference fails closed."),
		FProjectMaterialRecipeContract::Parse(WrongSchemaPath, Source, Root, Recipe, Error));
	const FString WrongFamily = ProjectMaterialRecipeTests::ValidRecipe().Replace(
		TEXT("surface_opaque"), TEXT("water"));
	TestFalse(TEXT("An unselected family fails closed."),
		FProjectMaterialRecipeContract::Parse(WrongFamily, Source, Root, Recipe, Error));
	const FString WrongParameter = ProjectMaterialRecipeTests::ValidRecipe().Replace(
		TEXT("\"Roughness\": 0.9"), TEXT("\"Metallic\": 0.0"));
	TestFalse(TEXT("An unknown parameter fails closed."),
		FProjectMaterialRecipeContract::Parse(WrongParameter, Source, Root, Recipe, Error));
	TestFalse(TEXT("SlopeContrast below the closed lower bound fails."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::RecipeWithSlopeContrast(TEXT("0")),
			Source, Root, Recipe, Error));
	TestTrue(TEXT("SlopeContrast at the closed lower bound passes."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::RecipeWithSlopeContrast(TEXT("0.01")),
			Source, Root, Recipe, Error));
	TestTrue(TEXT("SlopeContrast at the closed upper bound passes."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::RecipeWithSlopeContrast(TEXT("16")),
			Source, Root, Recipe, Error));
	TestFalse(TEXT("SlopeContrast above the closed upper bound fails."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::RecipeWithSlopeContrast(TEXT("16.01")),
			Source, Root, Recipe, Error));

	const FString SchemaPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Resources/ProjectMaterial/Data/Schemas/material-recipe.schema.json"));
	FString SchemaJson;
	TestTrue(TEXT("The tracked material recipe schema is readable."),
		FFileHelper::LoadFileToString(SchemaJson, *SchemaPath));
	TSharedPtr<FJsonObject> SchemaRoot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemaJson);
	TestTrue(TEXT("The tracked material recipe schema parses."),
		FJsonSerializer::Deserialize(Reader, SchemaRoot) && SchemaRoot.IsValid());
	if (SchemaRoot.IsValid())
	{
		const TSharedPtr<FJsonObject>* RootProperties = nullptr;
		const TSharedPtr<FJsonObject>* Scalars = nullptr;
		const TSharedPtr<FJsonObject>* ScalarProperties = nullptr;
		const TSharedPtr<FJsonObject>* SlopeContrast = nullptr;
		const bool bFoundSlopeContract =
			SchemaRoot->TryGetObjectField(TEXT("properties"), RootProperties) &&
			(*RootProperties)->TryGetObjectField(TEXT("scalars"), Scalars) &&
			(*Scalars)->TryGetObjectField(TEXT("properties"), ScalarProperties) &&
			(*ScalarProperties)->TryGetObjectField(TEXT("SlopeContrast"), SlopeContrast);
		TestTrue(TEXT("The schema declares the SlopeContrast contract."), bFoundSlopeContract);
		if (bFoundSlopeContract)
		{
			TestEqual(TEXT("Schema lower bound matches the compiler."),
				(*SlopeContrast)->GetNumberField(TEXT("minimum")), 0.01);
			TestEqual(TEXT("Schema upper bound matches the compiler."),
				(*SlopeContrast)->GetNumberField(TEXT("maximum")), 16.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialOwnerConfinementTest,
	"Project.Material.Generation.OwnerConfinement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialOwnerConfinementTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation/confinement"));
	FProjectMaterialRecipe Recipe;
	FString Error;
	TestFalse(TEXT("A recipe outside its owner root is rejected."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::ValidRecipe(),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/escaped.material.json")),
			Root,
			Recipe,
			Error));
	const FString Source = FPaths::Combine(Root, TEXT("Terrain/M_ProjectTerrain.material.json"));
	TestTrue(TEXT("The confined recipe parses."),
		FProjectMaterialRecipeContract::Parse(
			ProjectMaterialRecipeTests::ValidRecipe(), Source, Root, Recipe, Error));
	FString PackageName;
	FString ObjectPath;
	TestTrue(TEXT("A confined package identity is derived."),
		FProjectMaterialRecipeContract::ResolveOutputIdentity(
			Recipe, TEXT("/ProjectMaterialTest/Generated"), PackageName, ObjectPath, Error));
	TestEqual(TEXT("The final object identity is deterministic."), ObjectPath,
		FString(TEXT("/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain.M_ProjectTerrain")));
	TestFalse(TEXT("A cross-mount output root fails closed."),
		FProjectMaterialRecipeContract::ResolveOutputIdentity(
			Recipe, TEXT("ProjectWorld/Generated"), PackageName, ObjectPath, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialEngineIdentityTest,
	"Project.Material.Generation.EngineIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialEngineIdentityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("SHA-256 matches its published empty-input vector."),
		FProjectMaterialRecipeContract::ComputeStringSha256(TEXT("")),
		FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
	TestEqual(TEXT("SHA-256 matches its published abc vector."),
		FProjectMaterialRecipeContract::ComputeStringSha256(TEXT("abc")),
		FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	const FString MultiBlock =
		TEXT("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn")
		TEXT("hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu");
	TestEqual(TEXT("SHA-256 matches its published multi-block vector."),
		FProjectMaterialRecipeContract::ComputeStringSha256(MultiBlock),
		FString(TEXT("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1")));
	const FString Identity = FProjectMaterialRecipeContract::GetEngineIdentity();
	TestTrue(TEXT("Engine identity records the exact changelist."), Identity.Contains(TEXT("changelist=")));
	TestEqual(TEXT("Compiler fingerprint is SHA-256."),
		FProjectMaterialRecipeContract::GetCompilerFingerprint().Len(), 64);
	TestNotEqual(TEXT("Compiler fingerprint is not the legacy hand-maintained declaration."),
		FProjectMaterialRecipeContract::GetCompilerFingerprint(),
		FProjectMaterialRecipeContract::ComputeStringSha256(
			TEXT("project_material_editor|surface_opaque|landscape_basic_v1|compiler=1")));
	return true;
}

#endif
