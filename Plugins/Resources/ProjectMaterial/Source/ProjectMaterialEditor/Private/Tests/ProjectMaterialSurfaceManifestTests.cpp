// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceManifest.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialSurfaceManifestTests
{
	FProjectMaterialSurfaceManifestRecord Record(
		const FString& OutputObjectPath,
		const FString& BindingId)
	{
		const FString Hash = FString::ChrN(64, TEXT('a'));
		FProjectMaterialSurfaceManifestRecord Result;
		Result.RecipePath = TEXT("Terrain/MI_ProjectTerrain_Default.surface.json");
		Result.RecipeSha256 = Hash;
		Result.RecipeSourceSha256 = Hash;
		Result.BindingId = BindingId;
		Result.Family = TEXT("surface_substrate");
		Result.Archetype = TEXT("terrain_metric");
		Result.CompilerVersion = TEXT("5");
		Result.PatternId = TEXT("project_terrain_structure");
		Result.PatternObjectPath = TEXT("/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure");
		Result.PatternOutputContract = TEXT("surface_structure_rgb_v1");
		Result.PatternSemanticIdentity = Hash;
		Result.PatternPackageSha256 = Hash;
		Result.DependencyObjectPath = TEXT("/ProjectMaterial/Surfaces/Terrain/M_ProjectTerrain.M_ProjectTerrain");
		Result.DependencyPackageSha256 = Hash;
		Result.TerrainLayoutReceiptSha256 = Hash;
		Result.OutputObjectPath = OutputObjectPath;
		Result.SemanticIdentity = Hash;
		Result.PackageSha256 = Hash;
		Result.CompilerFingerprint = Hash;
		Result.EngineCompatibility = TEXT("5.8");
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSurfaceManifestContractTest,
	"Project.Material.Generation.SurfaceManifestContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSurfaceManifestContractTest::RunTest(const FString& Parameters)
{
	const auto B = ProjectMaterialSurfaceManifestTests::Record(
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_B.MI_B"), TEXT("terrain.secondary"));
	const auto A = ProjectMaterialSurfaceManifestTests::Record(
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_A.MI_A"), TEXT("terrain.default"));
	const FString Json = ProjectMaterialSurfaceManifest::Serialize({B, A});
	TestFalse(TEXT("The surface manifest contains no CRLF bytes."), Json.Contains(TEXT("\r")));
	TestTrue(TEXT("The surface manifest has one final LF."), Json.EndsWith(TEXT("\n")));
	TestTrue(TEXT("Surface records are sorted by output identity."),
		Json.Find(A.OutputObjectPath) < Json.Find(B.OutputObjectPath));

	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/material/generation/surface_manifest"));
	const FString Path = FPaths::Combine(Root, TEXT("accepted.surface-manifest.json"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	IFileManager::Get().MakeDirectory(*Root, true);
	TestTrue(TEXT("The synthetic manifest is written."), FFileHelper::SaveStringToFile(Json, *Path));
	TMap<FString, FProjectMaterialSurfaceManifestRecord> Records;
	FString Error;
	TestTrue(TEXT("The synthetic manifest reloads."),
		ProjectMaterialSurfaceManifest::Load(Path, Records, Error));
	TestEqual(TEXT("Both records reload."), Records.Num(), 2);

	auto DuplicateBindingRecord = B;
	DuplicateBindingRecord.BindingId = A.BindingId;
	const FString DuplicateBinding = ProjectMaterialSurfaceManifest::Serialize({A, DuplicateBindingRecord});
	TestTrue(TEXT("The known-bad manifest is written."),
		FFileHelper::SaveStringToFile(DuplicateBinding, *Path));
	TestFalse(TEXT("Duplicate consumer bindings fail closed."),
		ProjectMaterialSurfaceManifest::Load(Path, Records, Error));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

#endif
