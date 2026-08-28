// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialGenerationService.h"
#include "ProjectMaterialRecipe.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectMaterialGenerationTests
{
void CleanupEmptyOwnerDirectories()
{
	const FString GenerationRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation"));
	IFileManager::Get().DeleteDirectory(*FPaths::Combine(GenerationRoot, TEXT("automation")), false, false);
	IFileManager::Get().DeleteDirectory(*GenerationRoot, false, false);
	IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material")), false, false);
}

FString ParentRecipe(double Roughness = 0.9)
{
	return FString::Printf(TEXT(R"JSON({
  "$schema": "../../Schemas/material-recipe.schema.json",
  "schema_version": "1",
  "material_id": "M_ProjectTerrain",
  "artifact_kind": "parent",
  "family": "surface_opaque",
  "archetype": "landscape_basic_v1",
  "compiler_version": "1",
  "scalars": { "Roughness": %.3f, "SlopeContrast": 1.5 },
  "vectors": {
    "LowSlopeColor": [0.08, 0.18, 0.04, 1.0],
    "SteepSlopeColor": [0.16, 0.12, 0.08, 1.0]
  }
})JSON"), Roughness);
}

FString InstanceRecipe(double Roughness = 0.82)
{
	return FString::Printf(TEXT(R"JSON({
  "$schema": "../../Schemas/material-recipe.schema.json",
  "schema_version": "1",
  "material_id": "MI_ProjectTerrain_Default",
  "artifact_kind": "instance",
  "family": "surface_opaque",
  "archetype": "landscape_basic_v1",
  "compiler_version": "1",
  "parent": "/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain.M_ProjectTerrain",
  "scalars": { "Roughness": %.3f, "SlopeContrast": 1.25 },
  "vectors": {
    "LowSlopeColor": [0.07, 0.20, 0.05, 1.0],
    "SteepSlopeColor": [0.19, 0.14, 0.09, 1.0]
  }
})JSON"), Roughness);
}

class FFixture
{
public:
	explicit FFixture(const FString& TestName)
	{
		Root = FPaths::Combine(
			FPaths::ProjectDir(), TEXT("tmp/material/generation/automation"), TestName);
		IFileManager::Get().DeleteDirectory(*Root, false, true);
		ContentRoot = FPaths::Combine(Root, TEXT("content")) + TEXT("/");
		FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"), ContentRoot);
		Request.RecipeRoot = FPaths::Combine(Root, TEXT("recipes"));
		Request.OutputPackageRoot = TEXT("/ProjectMaterialTest/Generated");
		Request.OutputContentRoot = FPaths::Combine(ContentRoot, TEXT("Generated"));
		Request.ManifestRoot = FPaths::Combine(Root, TEXT("manifests"));
	}

	~FFixture()
	{
		TArray<UPackage*> Packages;
		for (const TCHAR* PackageName : {
			TEXT("/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain"),
			TEXT("/ProjectMaterialTest/Generated/Terrain/MI_ProjectTerrain_Default"),
			TEXT("/ProjectMaterialTest/Generated/Terrain/M_Orphan")})
		{
			if (UPackage* Package = FindPackage(nullptr, PackageName))
			{
				Packages.Add(Package);
			}
		}
		if (!Packages.IsEmpty())
		{
			UPackageTools::FlushAsyncCompilation(Packages);
			UPackageTools::UnloadPackages(Packages);
		}
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"), ContentRoot);
		IFileManager::Get().DeleteDirectory(*Root, false, true);
		CleanupEmptyOwnerDirectories();
	}

	bool WriteRecipes(double ParentRoughness = 0.9, double InstanceRoughness = 0.82) const
	{
		return WriteRecipe(TEXT("M_ProjectTerrain.material.json"), ParentRecipe(ParentRoughness)) &&
			WriteRecipe(TEXT("MI_ProjectTerrain_Default.material.json"), InstanceRecipe(InstanceRoughness));
	}

	bool WriteRecipe(const FString& Name, const FString& Json) const
	{
		const FString Directory = FPaths::Combine(Request.RecipeRoot, TEXT("Terrain"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		return FFileHelper::SaveStringToFile(
			Json,
			*FPaths::Combine(Directory, Name),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FString PackageFile(const FString& AssetName) const
	{
		return FPaths::Combine(Request.OutputContentRoot, TEXT("Terrain"), AssetName + TEXT(".uasset"));
	}

	FString ManifestFile() const
	{
		return FPaths::Combine(Request.ManifestRoot, TEXT("accepted.material-manifest.json"));
	}

	FString Root;
	FString ContentRoot;
	FProjectMaterialGenerationRequest Request;
};

bool HashFile(const FString& Path, FString& OutHash)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		return false;
	}
	OutHash = FProjectMaterialRecipeContract::ComputeSha256(Bytes);
	return OutHash.Len() == 64;
}

bool CreateOrphan(const FFixture& Fixture)
{
	const FString PackageName = TEXT("/ProjectMaterialTest/Generated/Terrain/M_Orphan");
	UPackage* Package = CreatePackage(*PackageName);
	UMaterial* Material = NewObject<UMaterial>(
		Package, UMaterial::StaticClass(), TEXT("M_Orphan"), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Material);
	const FString Filename = Fixture.PackageFile(TEXT("M_Orphan"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Args.bSlowTask = false;
	return UPackage::SavePackage(Package, Material, *Filename, Args);
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSelectedArchetypeGraphTest,
	"Project.Material.Generation.SelectedArchetypeGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSelectedArchetypeGraphTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("selected_archetype"));
	TestTrue(TEXT("Synthetic parent and instance recipes are written."), Fixture.WriteRecipes());
	FProjectMaterialGenerationResult Result;
	TestTrue(TEXT("The closed terrain graph generates and reloads."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Result));
	if (!Result.Error.IsEmpty())
	{
		AddError(Result.Error);
	}
	TestEqual(TEXT("Both synthetic assets are generated."), Result.Generated, 2);
	TestEqual(TEXT("Only the parent triggers a shader compile."), Result.ShaderCompiles, 1);
	TestTrue(TEXT("The manifest is promoted last."), FPaths::FileExists(Fixture.ManifestFile()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialInstanceParametersTest,
	"Project.Material.Generation.InstanceParameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialInstanceParametersTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("instance_parameters"));
	Fixture.WriteRecipes();
	FProjectMaterialGenerationResult Result;
	TestTrue(TEXT("Synthetic material family generates."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Result));
	auto* Instance = LoadObject<UMaterialInstanceConstant>(nullptr,
		TEXT("/ProjectMaterialTest/Generated/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	TestNotNull(TEXT("The generated instance reloads."), Instance);
	if (Instance != nullptr)
	{
		float Roughness = 0.0f;
		TestTrue(TEXT("The instance has an explicit Roughness override."),
			Instance->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Roughness")), Roughness, true));
		TestTrue(TEXT("The typed Roughness value survives reload."), FMath::IsNearlyEqual(Roughness, 0.82f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialIdempotentNoWriteTest,
	"Project.Material.Generation.IdempotentNoWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialIdempotentNoWriteTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("idempotent"));
	Fixture.WriteRecipes();
	FProjectMaterialGenerationResult First;
	TestTrue(TEXT("Initial generation succeeds."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, First));
	const FString ParentFile = Fixture.PackageFile(TEXT("M_ProjectTerrain"));
	const FDateTime ParentTime = IFileManager::Get().GetTimeStamp(*ParentFile);
	const FDateTime ManifestTime = IFileManager::Get().GetTimeStamp(*Fixture.ManifestFile());
	FString ParentHash;
	ProjectMaterialGenerationTests::HashFile(ParentFile, ParentHash);
	FProjectMaterialGenerationResult Second;
	TestTrue(TEXT("Unchanged generation succeeds."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Second));
	TestEqual(TEXT("The second run performs zero writes."), Second.Generated, 0);
	TestEqual(TEXT("The second run performs zero shader compiles."), Second.ShaderCompiles, 0);
	TestEqual(TEXT("Both outputs are skipped."), Second.Skipped, 2);
	TestEqual(TEXT("Parent timestamp is unchanged."),
		IFileManager::Get().GetTimeStamp(*ParentFile), ParentTime);
	TestEqual(TEXT("Manifest timestamp is unchanged."),
		IFileManager::Get().GetTimeStamp(*Fixture.ManifestFile()), ManifestTime);
	FString SecondHash;
	ProjectMaterialGenerationTests::HashFile(ParentFile, SecondHash);
	TestEqual(TEXT("Parent bytes are unchanged."), SecondHash, ParentHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialOutputIntegrityTest,
	"Project.Material.Generation.OutputIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialOutputIntegrityTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/material/generation/automation/output_integrity"));
	const FString File = FPaths::Combine(Root, TEXT("candidate.uasset"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	IFileManager::Get().MakeDirectory(*Root, true);
	TestTrue(TEXT("Synthetic accepted output bytes are written."),
		FFileHelper::SaveStringToFile(TEXT("accepted-bytes"), *File));
	FString AcceptedHash;
	TestTrue(TEXT("Accepted output bytes hash successfully."),
		ProjectMaterialGenerationTests::HashFile(File, AcceptedHash));
	TestTrue(TEXT("Exact accepted bytes pass the skip integrity gate."),
		FProjectMaterialGenerationService::HasAcceptedOutputIntegrity(File, AcceptedHash));
	TestTrue(TEXT("The output can be corrupted for the guard proof."),
		FFileHelper::SaveStringToFile(TEXT("corrupt"), *File));
	TestFalse(TEXT("Corrupt output bytes cannot pass the skip integrity gate."),
		FProjectMaterialGenerationService::HasAcceptedOutputIntegrity(File, AcceptedHash));
	IFileManager::Get().Delete(*File, false, true, true);
	TestFalse(TEXT("A missing output cannot pass the skip integrity gate."),
		FProjectMaterialGenerationService::HasAcceptedOutputIntegrity(File, AcceptedHash));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	ProjectMaterialGenerationTests::CleanupEmptyOwnerDirectories();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialDependencyLocalityTest,
	"Project.Material.Generation.DependencyLocality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialDependencyLocalityTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation/automation/dependency_locality/recipes"));
	FProjectMaterialRecipe ParentA;
	FProjectMaterialRecipe ParentB;
	FProjectMaterialRecipe InstanceA;
	FProjectMaterialRecipe InstanceB;
	FString Error;
	TestTrue(TEXT("Baseline parent parses."), FProjectMaterialRecipeContract::Parse(
		ProjectMaterialGenerationTests::ParentRecipe(0.9), FPaths::Combine(Root, TEXT("Terrain/M_ProjectTerrain.material.json")), Root, ParentA, Error));
	TestTrue(TEXT("Changed parent parses."), FProjectMaterialRecipeContract::Parse(
		ProjectMaterialGenerationTests::ParentRecipe(0.7), FPaths::Combine(Root, TEXT("Terrain/M_ProjectTerrain.material.json")), Root, ParentB, Error));
	TestTrue(TEXT("Baseline instance parses."), FProjectMaterialRecipeContract::Parse(
		ProjectMaterialGenerationTests::InstanceRecipe(0.82), FPaths::Combine(Root, TEXT("Terrain/MI_ProjectTerrain_Default.material.json")), Root, InstanceA, Error));
	TestTrue(TEXT("Changed instance parses."), FProjectMaterialRecipeContract::Parse(
		ProjectMaterialGenerationTests::InstanceRecipe(0.74), FPaths::Combine(Root, TEXT("Terrain/MI_ProjectTerrain_Default.material.json")), Root, InstanceB, Error));
	const FString ParentPath = TEXT("/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain.M_ProjectTerrain");
	const FString InstancePath = TEXT("/ProjectMaterialTest/Generated/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default");
	const FString ParentIdentityA = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(ParentA, ParentPath, TEXT(""));
	const FString ParentIdentityB = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(ParentB, ParentPath, TEXT(""));
	const FString ParentPackageA = FString::ChrN(64, TEXT('a'));
	const FString ParentPackageB = FString::ChrN(64, TEXT('b'));
	const FString InstanceIdentityA = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(InstanceA, InstancePath, ParentPackageA);
	const FString InstanceRecipeChanged = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(InstanceB, InstancePath, ParentPackageA);
	const FString InstanceDependencyChanged = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(InstanceA, InstancePath, ParentPackageB);
	TestNotEqual(TEXT("A direct parent recipe change invalidates the parent."), ParentIdentityA, ParentIdentityB);
	TestNotEqual(TEXT("A direct instance recipe change invalidates only that instance identity."), InstanceIdentityA, InstanceRecipeChanged);
	TestNotEqual(TEXT("A parent package change invalidates its exact dependent instance."), InstanceIdentityA, InstanceDependencyChanged);
	TestEqual(TEXT("An unchanged recipe and dependency retain stable identity."), InstanceIdentityA,
		FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(InstanceA, InstancePath, ParentPackageA));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialParentRecipeAuthorityTest,
	"Project.Material.Generation.ParentRecipeAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialParentRecipeAuthorityTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("parent_recipe_authority"));
	TestTrue(TEXT("The complete declared recipe set is written."), Fixture.WriteRecipes());
	FProjectMaterialGenerationResult First;
	TestTrue(TEXT("The complete recipe set generates."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, First));
	const FString ParentFile = Fixture.PackageFile(TEXT("M_ProjectTerrain"));
	TestTrue(TEXT("The generated parent bytes exist."), FPaths::FileExists(ParentFile));
	const FString ParentRecipe = FPaths::Combine(
		Fixture.Request.RecipeRoot, TEXT("Terrain/M_ProjectTerrain.material.json"));
	TestTrue(TEXT("The parent recipe is removed while its generated bytes remain."),
		IFileManager::Get().Delete(*ParentRecipe, false, true, true));

	FProjectMaterialGenerationResult Validation;
	TestFalse(TEXT("Validation rejects an instance without a current parent recipe."),
		FProjectMaterialGenerationService::Validate(Fixture.Request, Validation));
	TestTrue(TEXT("The rejection names the declared parent authority."),
		Validation.Error.Contains(TEXT("current declared parent recipe")));
	FProjectMaterialGenerationResult Regeneration;
	TestFalse(TEXT("Regeneration cannot adopt leftover parent package bytes."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Regeneration));
	TestTrue(TEXT("The leftover package still exists for the authority proof."),
		FPaths::FileExists(ParentFile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialProcessOwnershipTest,
	"Project.Material.Generation.ProcessOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialProcessOwnershipTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("process_ownership"));
	Fixture.WriteRecipes();
	FProjectMaterialGenerationRequest Production = Fixture.Request;
	Production.OutputPackageRoot = TEXT("/ProjectMaterial/Generated");
	FProjectMaterialGenerationResult Result;
	TestFalse(TEXT("An in-process test cannot claim production mutation ownership."),
		FProjectMaterialGenerationService::RegenerateTestMount(Production, Result));
	TestTrue(TEXT("The rejection names the commandlet boundary."),
		Result.Error.Contains(TEXT("dedicated one-shot commandlet")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialOrphanSafetyTest,
	"Project.Material.Generation.OrphanSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialOrphanSafetyTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("orphan_safety"));
	Fixture.WriteRecipes();
	FProjectMaterialGenerationResult First;
	FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, First);
	TestTrue(TEXT("A valid unowned package is created for the orphan proof."),
		ProjectMaterialGenerationTests::CreateOrphan(Fixture));
	FProjectMaterialGenerationResult Report;
	TestTrue(TEXT("Default generation reports the orphan."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Report));
	TestTrue(TEXT("Default mode retains the orphan."),
		FPaths::FileExists(Fixture.PackageFile(TEXT("M_Orphan"))));
	TestTrue(TEXT("The orphan package is listed."),
		Report.OrphanPackageNames.Contains(TEXT("/ProjectMaterialTest/Generated/Terrain/M_Orphan")));
	Fixture.Request.bCleanupOrphans = true;
	FProjectMaterialGenerationResult Cleanup;
	TestTrue(TEXT("Explicit unreferenced cleanup succeeds."),
		FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, Cleanup));
	TestFalse(TEXT("Explicit cleanup removes the unreferenced orphan."),
		FPaths::FileExists(Fixture.PackageFile(TEXT("M_Orphan"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialReloadAfterRollbackTest,
	"Project.Material.Generation.ReloadAfterRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialReloadAfterRollbackTest::RunTest(const FString& Parameters)
{
	ProjectMaterialGenerationTests::FFixture Fixture(TEXT("reload_after_rollback"));
	Fixture.WriteRecipes(0.9, 0.82);
	FProjectMaterialGenerationResult First;
	FProjectMaterialGenerationService::RegenerateTestMount(Fixture.Request, First);
	const FString ParentFile = Fixture.PackageFile(TEXT("M_ProjectTerrain"));
	TArray<uint8> PriorBytes;
	TestTrue(TEXT("The accepted -1 package is captured."),
		FFileHelper::LoadFileToArray(PriorBytes, *ParentFile));
	UPackage* Package = FindPackage(nullptr, TEXT("/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain"));
	TestNotNull(TEXT("The accepted package is loaded before rollback simulation."), Package);
	if (Package == nullptr)
	{
		return false;
	}
	UPackageTools::FlushAsyncCompilation({Package});
	TestTrue(TEXT("The child-process boundary releases the generated package."),
		UPackageTools::UnloadPackages({Package}));
	TestTrue(TEXT("A failed child may leave different candidate bytes."),
		FFileHelper::SaveStringToFile(TEXT("failed-candidate"), *ParentFile));
	TestTrue(TEXT("The exact -1 package bytes are restored."),
		FFileHelper::SaveArrayToFile(PriorBytes, *ParentFile));
	UMaterial* Reloaded = LoadObject<UMaterial>(nullptr,
		TEXT("/ProjectMaterialTest/Generated/Terrain/M_ProjectTerrain.M_ProjectTerrain"));
	TestNotNull(TEXT("A fresh material pointer is reacquired after rollback."), Reloaded);
	if (Reloaded != nullptr)
	{
		TestTrue(TEXT("Readback comes from the restored -1 package."), FMath::IsNearlyEqual(
			UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Reloaded, TEXT("Roughness")), 0.9f));
	}
	return true;
}

#endif
