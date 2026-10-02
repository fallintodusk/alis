// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureGenerationService.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectTextureGenerationTests
{
FString RecipeJson()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureGenerationContractTest,
	"Project.Texture.Generation.AssetCompilerContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureGenerationContractTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/texture/generation/asset_compiler_contract"));
	const FString RecipeRoot = FPaths::Combine(Root, TEXT("recipes"));
	const FString RecipePath = FPaths::Combine(
		RecipeRoot, TEXT("Terrain/project_terrain_structure.pattern.json"));
	const FString ContentRoot = FPaths::Combine(Root, TEXT("content"));
	const FString OutputRoot = FPaths::Combine(ContentRoot, TEXT("Patterns"));
	const FString ManifestRoot = FPaths::Combine(Root, TEXT("manifests"));
	const FString ManifestPath = FPaths::Combine(
		ManifestRoot, TEXT("accepted.pattern-manifest.json"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(RecipePath), true);
	TestTrue(TEXT("The test recipe is written."), FFileHelper::SaveStringToFile(
		ProjectTextureGenerationTests::RecipeJson(), *RecipePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"), ContentRoot + TEXT("/"));

	FProjectTextureGenerationRequest Request;
	Request.RecipeRoot = RecipeRoot;
	Request.OutputPackageRoot = TEXT("/ProjectTextureTest/Patterns");
	Request.OutputContentRoot = OutputRoot;
	Request.ManifestRoot = ManifestRoot;
	FProjectTextureGenerationResult First;
	TestTrue(TEXT("The first generation is accepted."),
		FProjectTextureGenerationService::RegenerateTestMount(Request, First));
	TestEqual(TEXT("One runtime pattern bundle is generated."), First.Generated, 1);
	TestEqual(TEXT("No runtime pattern bundle is skipped initially."), First.Skipped, 0);
	TestEqual(TEXT("The manifest hash is SHA-256."), First.ManifestSha256.Len(), 64);

	FString Manifest;
	TestTrue(TEXT("The accepted manifest is readable."),
		FFileHelper::LoadFileToString(Manifest, *ManifestPath));
	TestFalse(TEXT("The accepted manifest contains no CRLF."), Manifest.Contains(TEXT("\r")));
	const FString PackagePath = FPaths::Combine(
		OutputRoot, TEXT("Terrain/DA_ProjectTerrainStructureCatalog.uasset"));
	const FDateTime PackageTime = IFileManager::Get().GetTimeStamp(*PackagePath);
	const FDateTime ManifestTime = IFileManager::Get().GetTimeStamp(*ManifestPath);

	FProjectTextureGenerationResult Second;
	TestTrue(TEXT("The unchanged regeneration is accepted."),
		FProjectTextureGenerationService::RegenerateTestMount(Request, Second));
	TestEqual(TEXT("The unchanged bundle is not regenerated."), Second.Generated, 0);
	TestEqual(TEXT("The unchanged bundle is skipped."), Second.Skipped, 1);
	TestEqual(TEXT("The package timestamp is unchanged."),
		IFileManager::Get().GetTimeStamp(*PackagePath), PackageTime);
	TestEqual(TEXT("The manifest timestamp is unchanged."),
		IFileManager::Get().GetTimeStamp(*ManifestPath), ManifestTime);

	FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"), ContentRoot + TEXT("/"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

#endif
