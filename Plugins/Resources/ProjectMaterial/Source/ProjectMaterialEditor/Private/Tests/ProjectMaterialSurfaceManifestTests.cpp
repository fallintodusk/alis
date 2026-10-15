// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceManifest.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
	const FString DataRoot=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectMaterial/Data"));
	FString SchemaJson;
	TSharedPtr<FJsonObject> Schema;
	if (!FFileHelper::LoadFileToString(SchemaJson,*FPaths::Combine(DataRoot,TEXT("Schemas/surface-manifest.schema.json"))) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SchemaJson),Schema)) return false;
	const auto Properties=Schema->GetObjectField(TEXT("$defs"))->GetObjectField(TEXT("record"))->GetObjectField(TEXT("properties"));
	for (const TCHAR* Folder : {TEXT("SurfaceRecipes"),TEXT("TestFixtures")})
	{
		const FString RecipeRoot=FPaths::Combine(DataRoot,Folder); TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files,*RecipeRoot,TEXT("*.surface.json"),true,false,false);
		for (const FString& File : Files)
		{
			FString Json,Error; FProjectMaterialSurfaceRecipe Recipe;
			if (!FFileHelper::LoadFileToString(Json,*File) || !FProjectMaterialSurfaceRecipeContract::Parse(Json,File,RecipeRoot,Recipe,Error))
			{ AddError(Error); return false; }
			for (const auto& Field : TMap<FString,FString>{{TEXT("archetype"),Recipe.Archetype},{TEXT("compiler_version"),Recipe.CompilerVersion}})
			{
				const auto& Values=Properties->GetObjectField(Field.Key)->GetArrayField(TEXT("enum"));
				TestTrue(TEXT("Every compiled recipe fits the public manifest transport contract"),
					Values.ContainsByPredicate([&](const auto& V){return V->AsString()==Field.Value;}));
			}
		}
	}
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
