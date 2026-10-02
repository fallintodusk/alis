// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialTerrainLayoutContract.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialSurfaceIdentityTests
{
const TCHAR* PatternPath =
	TEXT("/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure");

FProjectMaterialSurfaceRecipe TerrainRecipe()
{
	FProjectMaterialSurfaceRecipe Recipe;
	Recipe.MaterialId = TEXT("M_ProjectTerrain");
	Recipe.Family = TEXT("surface_substrate");
	Recipe.Archetype = TEXT("terrain_metric");
	Recipe.CompilerVersion = TEXT("5");
	Recipe.PatternId = TEXT("project_terrain_structure");
	Recipe.RecipeSha256 = FString::ChrN(64, TEXT('a'));
	return Recipe;
}

// Provenance stands for everything a producer records about how it made its output: its semantic
// identity (which carries its compiler fingerprint) and the exact package bytes.
FProjectMaterialPatternDependency Pattern(const TCHAR* Contract, const TCHAR* ObjectPath, TCHAR Provenance)
{
	FProjectMaterialPatternDependency Dependency;
	Dependency.PatternId = TEXT("project_terrain_structure");
	Dependency.OutputContract = Contract;
	Dependency.OutputObjectPath = ObjectPath;
	Dependency.SemanticIdentity = FString::ChrN(64, Provenance);
	Dependency.PackageSha256 = FString::ChrN(64, static_cast<TCHAR>(Provenance + 1));
	return Dependency;
}

// The receipt identity covers the canonical surface contract hash, adapter fingerprint, engine
// identity, MPD bytes, build policy, and the channel layout, so it changes with any of them.
FProjectMaterialTerrainLayoutContract Layout(TCHAR LayoutHash, TCHAR Provenance)
{
	FProjectMaterialTerrainLayoutContract Contract;
	Contract.LayoutId = TEXT("project_mesh_terrain_channels");
	Contract.LayoutVersion = 1;
	Contract.LayoutSha256 = FString::ChrN(64, LayoutHash);
	Contract.AdapterCompilerSha256 = FString::ChrN(64, Provenance);
	Contract.SharedDefinitionPackageSha256 = FString::ChrN(64, Provenance);
	Contract.BuildPolicySha256 = FString::ChrN(64, Provenance);
	Contract.EngineIdentity = FString::Printf(TEXT("5.8|changelist=%c"), Provenance);
	Contract.ReceiptSha256 = FString::ChrN(32, LayoutHash) + FString::ChrN(32, Provenance);
	Contract.SemanticChannels = {{TEXT("ground"), 0}, {TEXT("hydro_transition"), 1}};
	return Contract;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSurfaceIdentityFirewallTest,
	"Project.Material.Generation.SurfaceIdentityFirewall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSurfaceIdentityFirewallTest::RunTest(const FString& Parameters)
{
	using namespace ProjectMaterialSurfaceIdentityTests;
	const FProjectMaterialSurfaceRecipe Recipe = TerrainRecipe();
	const FString Output = TEXT("/ProjectMaterial/Surfaces/Terrain/M_ProjectTerrain.M_ProjectTerrain");
	const TCHAR* Contract = TEXT("surface_structure_rgb_v1");
	auto Identity = [&](const FProjectMaterialPatternDependency& PatternDependency,
		const FProjectMaterialTerrainLayoutContract& TerrainLayout)
	{
		return FProjectMaterialSurfaceRecipeContract::ComputeArtifactSemanticIdentity(
			Recipe, Output, PatternDependency, FString(), &TerrainLayout);
	};
	const FString Accepted = Identity(Pattern(Contract, PatternPath, TEXT('b')), Layout(TEXT('1'), TEXT('e')));

	TestEqual(TEXT("Pattern provenance alone leaves the material identity unchanged."),
		Identity(Pattern(Contract, PatternPath, TEXT('f')), Layout(TEXT('1'), TEXT('e'))), Accepted);
	TestFalse(TEXT("A changed pattern output contract changes the material identity."),
		Identity(Pattern(TEXT("surface_structure_rgb_v2"), PatternPath, TEXT('b')), Layout(TEXT('1'), TEXT('e'))) ==
			Accepted);
	TestFalse(TEXT("A changed pattern object path changes the material identity."),
		Identity(Pattern(Contract, TEXT("/ProjectTexture/Patterns/Terrain/DA_Other.RT_Other"), TEXT('b')),
			Layout(TEXT('1'), TEXT('e'))) == Accepted);
	TestEqual(TEXT("Adapter, engine, MPD, or build-policy provenance alone leaves the terrain identity unchanged."),
		Identity(Pattern(Contract, PatternPath, TEXT('b')), Layout(TEXT('1'), TEXT('9'))), Accepted);
	TestFalse(TEXT("A changed channel layout changes the terrain material identity."),
		Identity(Pattern(Contract, PatternPath, TEXT('b')), Layout(TEXT('2'), TEXT('e'))) == Accepted);
	return true;
}

#endif
