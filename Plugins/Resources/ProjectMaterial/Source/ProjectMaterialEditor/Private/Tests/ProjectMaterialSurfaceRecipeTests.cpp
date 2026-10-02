// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceRecipe.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialSurfaceRecipeTests
{
	FString RecipeRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(), TEXT("Resources/ProjectMaterial/Data/SurfaceRecipes"));
	}

	bool LoadRecipe(
		const TCHAR* RelativePath,
		FProjectMaterialSurfaceRecipe& OutRecipe,
		FString& OutError)
	{
		const FString Root = RecipeRoot();
		const FString Path = FPaths::Combine(Root, RelativePath);
		FString Json;
		return FFileHelper::LoadFileToString(Json, *Path) &&
			FProjectMaterialSurfaceRecipeContract::Parse(Json, Path, Root, OutRecipe, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSurfaceRecipeContractTest,
	"Project.Material.Generation.SurfaceRecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSurfaceRecipeContractTest::RunTest(const FString& Parameters)
{
	FString Error;
	FProjectMaterialSurfaceRecipe Terrain;
	TestTrue(TEXT("The terrain surface parent recipe parses."),
		ProjectMaterialSurfaceRecipeTests::LoadRecipe(
			TEXT("Terrain/M_ProjectTerrain.surface.json"), Terrain, Error));
	TestEqual(TEXT("Terrain uses world metric 3D coordinates."),
		Terrain.ProjectionFrame, EProjectMaterialProjectionFrame::WorldMetricTriplanar2D);
	TestEqual(TEXT("Terrain consumes exactly two honest semantic bindings."),
		Terrain.SemanticBindings.Num(), 2);
	TestTrue(TEXT("Terrain consumes ground."), Terrain.SemanticBindings.Contains(TEXT("ground")));
	TestTrue(TEXT("Terrain consumes hydro_transition."),
		Terrain.SemanticBindings.Contains(TEXT("hydro_transition")));
	TestFalse(TEXT("Slope never becomes a factual rock binding."),
		Terrain.SemanticBindings.Contains(TEXT("rock")));
	TestEqual(TEXT("The current surface compiler version is explicit."),
		Terrain.CompilerVersion, FString(TEXT("5")));
	TestFalse(TEXT("The removed scalar normal strength is not part of the recipe."),
		Terrain.Scalars.Contains(TEXT("StructureStrength")));
	TestEqual(TEXT("The recipe source identity is SHA-256."), Terrain.RecipeSourceSha256.Len(), 64);
	const FString FormattingPath = FPaths::Combine(
		ProjectMaterialSurfaceRecipeTests::RecipeRoot(), TEXT("Terrain/M_ProjectTerrain.surface.json"));
	FString FormattingJson;
	FProjectMaterialSurfaceRecipe Reformatted;
	TestTrue(TEXT("A formatting-only edit parses."),
		FFileHelper::LoadFileToString(FormattingJson, *FormattingPath) &&
		FProjectMaterialSurfaceRecipeContract::Parse(FormattingJson + TEXT("\n"), FormattingPath,
			ProjectMaterialSurfaceRecipeTests::RecipeRoot(), Reformatted, Error));
	TestEqual(TEXT("A formatting-only edit keeps the semantic identity."),
		Reformatted.RecipeSha256, Terrain.RecipeSha256);
	TestNotEqual(TEXT("A formatting-only edit changes the source identity."),
		Reformatted.RecipeSourceSha256, Terrain.RecipeSourceSha256);

	FProjectMaterialSurfaceRecipe TerrainInstance;
	TestTrue(TEXT("The terrain instance recipe parses."),
		ProjectMaterialSurfaceRecipeTests::LoadRecipe(
			TEXT("Terrain/MI_ProjectTerrain_Default.surface.json"), TerrainInstance, Error));
	TestEqual(TEXT("The consumer binding is data."),
		TerrainInstance.BindingId, FString(TEXT("terrain.default")));

	FProjectMaterialSurfaceRecipe Object;
	TestTrue(TEXT("The movable-object recipe parses."),
		ProjectMaterialSurfaceRecipeTests::LoadRecipe(
			TEXT("Object/M_ProjectMetricSurface.surface.json"), Object, Error));
	TestEqual(TEXT("Movable objects use object-local projection."),
		Object.ProjectionFrame, EProjectMaterialProjectionFrame::ObjectMetricTriplanar2D);
	TestEqual(TEXT("The object proof invents no canonical semantic binding."),
		Object.SemanticBindings.Num(), 0);
	TestEqual(TEXT("Both archetypes reuse the same structural pattern ID."),
		Object.PatternId, Terrain.PatternId);

	FString TerrainJson;
	const FString TerrainPath = FPaths::Combine(
		ProjectMaterialSurfaceRecipeTests::RecipeRoot(),
		TEXT("Terrain/M_ProjectTerrain.surface.json"));
	TestTrue(TEXT("The tracked terrain recipe is readable for the replaceability control."),
		FFileHelper::LoadFileToString(TerrainJson, *TerrainPath));
	const FString AlternatePattern = TerrainJson.Replace(
		TEXT("\"pattern_id\": \"project_terrain_structure\""),
		TEXT("\"pattern_id\": \"wood_grain\""));
	FProjectMaterialSurfaceRecipe AlternateRecipe;
	TestTrue(TEXT("The recipe contract accepts another safe pattern ID without knowing its implementation."),
		FProjectMaterialSurfaceRecipeContract::Parse(
			AlternatePattern,
			TerrainPath,
			ProjectMaterialSurfaceRecipeTests::RecipeRoot(),
			AlternateRecipe,
			Error));
	TestEqual(TEXT("The alternate stable pattern identity survives parsing."),
		AlternateRecipe.PatternId, FString(TEXT("wood_grain")));

	FString PackageName;
	FString ObjectPath;
	TestTrue(TEXT("The concern-named terrain output is derived."),
		FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
			Terrain, TEXT("/ProjectMaterial/Surfaces"), PackageName, ObjectPath, Error));
	TestEqual(TEXT("The final path has no Generated segment."), ObjectPath,
		FString(TEXT("/ProjectMaterial/Surfaces/Terrain/M_ProjectTerrain.M_ProjectTerrain")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSurfaceRecipeKnownBadTest,
	"Project.Material.Generation.SurfaceRecipeKnownBad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSurfaceRecipeKnownBadTest::RunTest(const FString& Parameters)
{
	const FString Root = ProjectMaterialSurfaceRecipeTests::RecipeRoot();
	const FString Path = FPaths::Combine(Root, TEXT("Terrain/M_ProjectTerrain.surface.json"));
	FString Json;
	TestTrue(TEXT("The tracked terrain recipe is readable."), FFileHelper::LoadFileToString(Json, *Path));
	FProjectMaterialSurfaceRecipe Recipe;
	FString Error;

	const FString Rock = Json.Replace(
		TEXT("\"ground\", \"hydro_transition\""), TEXT("\"rock\", \"hydro_transition\""));
	TestFalse(TEXT("An unjustified rock semantic is rejected."),
		FProjectMaterialSurfaceRecipeContract::Parse(Rock, Path, Root, Recipe, Error));

	const FString WrongProjection = Json.Replace(
		TEXT("\"world_metric_triplanar_2d\""), TEXT("\"object_metric_triplanar_2d\""));
	TestFalse(TEXT("Terrain cannot select movable-object projection."),
		FProjectMaterialSurfaceRecipeContract::Parse(WrongProjection, Path, Root, Recipe, Error));

	const FString Nodes = Json.Replace(
		TEXT("\n  }\n}"), TEXT("\n  },\n  \"nodes\": []\n}"));
	TestFalse(TEXT("An arbitrary material graph fails closed."),
		FProjectMaterialSurfaceRecipeContract::Parse(Nodes, Path, Root, Recipe, Error));

	const FString RemovedNormalStrength = Json.Replace(
		TEXT("\"DetailStrength\": 0.12"),
		TEXT("\"DetailStrength\": 0.12,\n    \"StructureStrength\": 0.32"));
	TestFalse(TEXT("A removed scalar normal-strength field is rejected."),
		FProjectMaterialSurfaceRecipeContract::Parse(RemovedNormalStrength, Path, Root, Recipe, Error));
	return true;
}

#endif
