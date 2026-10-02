// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialSurfaceRecipe.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialPatternAuthorityTests
{
	FString SurfaceRecipe()
	{
		return TEXT(R"JSON({
  "$schema": "../../Schemas/surface-recipe.schema.json",
  "schema_version": "4",
  "material_id": "M_ProjectTerrain",
  "artifact_kind": "parent",
  "family": "surface_substrate",
  "archetype": "terrain_metric",
  "compiler_version": "5",
  "projection_frame": "world_metric_triplanar_2d",
  "pattern": {
    "pattern_id": "project_terrain_structure"
  },
  "semantic_bindings": ["ground", "hydro_transition"],
  "scalars": {
    "BaseRoughness": 0.9,
    "PatternScaleMeters": 1.75,
    "MacroStrength": 0.06,
    "DetailStrength": 0.06,
    "HydroDarkening": 0.24,
    "SlopeContrast": 1.35
  },
  "vectors": {
    "GroundColor": [0.055, 0.125, 0.035, 1.0],
    "SteepGroundColor": [0.14, 0.085, 0.035, 1.0],
    "HydroTint": [0.035, 0.07, 0.055, 1.0]
  }
})JSON");
	}

	FString Manifest(
		const FString& SemanticIdentity,
		const FString& PackageSha256,
		const FString& EngineCompatibility,
		const FString& OutputContract = TEXT("surface_structure_rgb_v1"))
	{
		const FString Hash = FString::ChrN(64, TEXT('a'));
		return FString::Printf(TEXT(R"JSON({
  "$schema": "../../Schemas/pattern-manifest.schema.json",
  "schema_version": 3,
  "records": [
    {
      "recipe_path": "Terrain/project_terrain_structure.pattern.json",
      "recipe_sha256": "%s",
      "pattern_id": "project_terrain_structure",
      "family": "surface_structure",
      "algorithm": "native_noise_experiment_7",
      "compiler_version": "42",
      "output_contract": "%s",
      "output_object_path": "/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure",
      "semantic_identity": "%s",
      "package_sha256": "%s",
      "compiler_fingerprint": "%s",
      "engine_compatibility": "%s",
      "producer_only_future_field": "ignored-by-consumer"
    }
  ]
})JSON"), *Hash, *OutputContract, *SemanticIdentity, *PackageSha256, *Hash, *EngineCompatibility);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialPatternAuthorityTest,
	"Project.Material.Generation.PatternAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialPatternAuthorityTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/material/generation/pattern_authority"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	const FString RecipeRoot = FPaths::Combine(Root, TEXT("recipes"));
	const FString RecipePath = FPaths::Combine(RecipeRoot, TEXT("Terrain/M_ProjectTerrain.surface.json"));
	const FString ContentRoot = FPaths::Combine(Root, TEXT("content"));
	const FString PackagePath = FPaths::Combine(
		ContentRoot, TEXT("Patterns/Terrain/DA_ProjectTerrainStructureCatalog.uasset"));
	const FString ManifestPath = FPaths::Combine(Root, TEXT("accepted.pattern-manifest.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackagePath), true);
	const FString AcceptedBytes = TEXT("synthetic-pattern-package");
	TestTrue(TEXT("Synthetic pattern package bytes are written."),
		FFileHelper::SaveStringToFile(AcceptedBytes, *PackagePath));
	TArray<uint8> Bytes;
	FFileHelper::LoadFileToArray(Bytes, *PackagePath);
	const FString PackageSha256 = FProjectMaterialCompilerIdentity::ComputeSha256(Bytes);
	const FString SemanticIdentity = FString::ChrN(64, TEXT('b'));
	const FEngineVersion Engine = FEngineVersion::Current();
	const FString EngineCompatibility = FString::Printf(
		TEXT("%d.%d"), Engine.GetMajor(), Engine.GetMinor());
	IFileManager::Get().MakeDirectory(*RecipeRoot, true);
	FProjectMaterialSurfaceRecipe Recipe;
	FString Error;
	TestTrue(TEXT("Synthetic surface recipe parses."),
		FProjectMaterialSurfaceRecipeContract::Parse(
			ProjectMaterialPatternAuthorityTests::SurfaceRecipe(),
			RecipePath,
			RecipeRoot,
			Recipe,
			Error));
	TestTrue(TEXT("Synthetic accepted manifest is written."),
		FFileHelper::SaveStringToFile(
			ProjectMaterialPatternAuthorityTests::Manifest(
				SemanticIdentity, PackageSha256, EngineCompatibility),
			*ManifestPath));
	FProjectMaterialPatternDependency Dependency;
	TestTrue(TEXT("Pattern ID resolves through the accepted manifest and package bytes."),
		FProjectMaterialPatternAuthority::Resolve(
			Recipe.PatternId, ManifestPath, TEXT("/ProjectTexture"), ContentRoot, Dependency, Error));
	TestEqual(TEXT("The manifest owns the resolved output path."), Dependency.OutputObjectPath,
		FString(TEXT("/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure")));
	TestEqual(TEXT("The accepted package hash is returned."), Dependency.PackageSha256, PackageSha256);
	TestEqual(TEXT("The consumer binds only the stable output ABI."), Dependency.OutputContract,
		FString(TEXT("surface_structure_rgb_v1")));
	TestTrue(TEXT("A newer additive producer manifest does not break the consumer projection."),
		!Dependency.OutputObjectPath.IsEmpty());

	TestTrue(TEXT("A manifest with an incompatible output ABI can be written."),
		FFileHelper::SaveStringToFile(
			ProjectMaterialPatternAuthorityTests::Manifest(
				SemanticIdentity,
				PackageSha256,
				EngineCompatibility,
				TEXT("unrelated_output_v9")),
			*ManifestPath));
	TestFalse(TEXT("A resolved pattern with an incompatible output ABI fails closed."),
		FProjectMaterialPatternAuthority::Resolve(
			Recipe.PatternId, ManifestPath, TEXT("/ProjectTexture"), ContentRoot, Dependency, Error));
	TestTrue(TEXT("The accepted manifest can be restored after the ABI control."),
		FFileHelper::SaveStringToFile(
			ProjectMaterialPatternAuthorityTests::Manifest(
				SemanticIdentity, PackageSha256, EngineCompatibility),
			*ManifestPath));

	TestTrue(TEXT("Synthetic package bytes can be corrupted."),
		FFileHelper::SaveStringToFile(TEXT("corrupt"), *PackagePath));
	TestFalse(TEXT("Corrupt package bytes fail closed."),
		FProjectMaterialPatternAuthority::Resolve(
			Recipe.PatternId, ManifestPath, TEXT("/ProjectTexture"), ContentRoot, Dependency, Error));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

#endif
