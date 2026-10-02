// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceGenerationService.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSurfaceGenerationContractTest,
	"Project.Material.Generation.SurfaceAssetCompilerContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSurfaceGenerationContractTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> MaterialPlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
	const TSharedPtr<IPlugin> TexturePlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectTexture"));
	const TSharedPtr<IPlugin> MeshTerrainPlugin =
		IPluginManager::Get().FindPlugin(TEXT("ProjectWorldMeshTerrain"));
	TestTrue(TEXT("Material, texture, and Mesh Terrain owner plugins are available."),
		MaterialPlugin.IsValid() && TexturePlugin.IsValid() && MeshTerrainPlugin.IsValid());
	if (!MaterialPlugin.IsValid() || !TexturePlugin.IsValid() || !MeshTerrainPlugin.IsValid())
	{
		return false;
	}
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/material/generation/surface_asset_compiler"));
	const FString RecipeRoot = FPaths::Combine(Root, TEXT("recipes"));
	const FString ContentRoot = FPaths::Combine(Root, TEXT("content"));
	const FString OutputRoot = FPaths::Combine(ContentRoot, TEXT("Surfaces"));
	const FString ManifestRoot = FPaths::Combine(Root, TEXT("manifests"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	const FString SourceRecipes = FPaths::Combine(MaterialPlugin->GetBaseDir(), TEXT("Data/SurfaceRecipes"));
	TArray<FString> SourceFiles;
	IFileManager::Get().FindFilesRecursive(
		SourceFiles, *SourceRecipes, TEXT("*.surface.json"), true, false);
	bool bCopiedRecipes = !SourceFiles.IsEmpty();
	for (const FString& SourceFile : SourceFiles)
	{
		FString Relative = SourceFile;
		FPaths::MakePathRelativeTo(Relative, *(SourceRecipes + TEXT("/")));
		const FString Destination = FPaths::Combine(RecipeRoot, Relative);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
		bCopiedRecipes &= IFileManager::Get().Copy(*Destination, *SourceFile) == COPY_OK;
	}
	TestTrue(TEXT("The accepted recipes copy into the disposable test owner."), bCopiedRecipes);
	FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"), ContentRoot + TEXT("/"));

	FProjectMaterialSurfaceGenerationRequest Request;
	Request.RecipeRoot = RecipeRoot;
	Request.OutputPackageRoot = TEXT("/ProjectMaterialTest/Surfaces");
	Request.OutputContentRoot = OutputRoot;
	Request.ManifestRoot = ManifestRoot;
	Request.PatternManifestPath = FPaths::Combine(
		TexturePlugin->GetBaseDir(), TEXT("Data/Manifests/Patterns/accepted.pattern-manifest.json"));
	Request.PatternContentRoot = TexturePlugin->GetContentDir();
	Request.TerrainLayoutReceiptPath = FPaths::Combine(
		MeshTerrainPlugin->GetBaseDir(),
		TEXT("Data/TestFixtures/mesh-terrain-layout-receipt.json"));

	FProjectMaterialSurfaceGenerationResult First;
	const bool bFirstGenerationAccepted =
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request, First);
	TestTrue(*FString::Printf(TEXT("The first closed surface generation is accepted: %s"), *First.Error),
		bFirstGenerationAccepted);
	TestEqual(TEXT("Two parents and two instances are generated."), First.Generated, 4);
	TestEqual(TEXT("Only the two structural parents compile shaders."), First.ShaderCompiles, 2);
	for (const TCHAR* ParentObjectPath : {
		TEXT("/ProjectMaterialTest/Surfaces/Terrain/M_ProjectTerrain.M_ProjectTerrain"),
		TEXT("/ProjectMaterialTest/Surfaces/Object/M_ProjectMetricSurface.M_ProjectMetricSurface")})
	{
		UMaterial* Parent = LoadObject<UMaterial>(nullptr, ParentObjectPath);
		if (!TestNotNull(TEXT("The generated surface parent loads for graph inspection."), Parent))
		{
			return false;
		}
		int32 TextureSamples = 0;
		int32 TextureDerivedNormals = 0;
		for (UMaterialExpression* Expression : UMaterialEditingLibrary::GetMaterialExpressions(Parent))
		{
			if (const UMaterialExpressionTextureSample* Sample =
				Cast<UMaterialExpressionTextureSample>(Expression))
			{
				++TextureSamples;
				TestTrue(TEXT("ProjectMaterial samples structural RGB as linear data."),
					Sample->SamplerType == SAMPLERTYPE_LinearColor);
				const UTextureRenderTarget2D* Pattern = Cast<UTextureRenderTarget2D>(Sample->Texture);
				TestNotNull(TEXT("Structural samples reference a render target."), Pattern);
				if (Pattern != nullptr)
				{
					TestFalse(TEXT("Structural samples do not carry sRGB metadata."), Pattern->SRGB);
					TestFalse(TEXT("Structural samples use a non-sRGB resource."), Pattern->IsSRGB());
				}
			}
			TextureDerivedNormals += Cast<UMaterialExpressionDeriveNormalZ>(Expression) != nullptr ? 1 : 0;
		}
		TestEqual(TEXT("A triplanar parent samples the three projection planes."), TextureSamples, 3);
		TestEqual(TEXT("A surface parent does not derive a fake normal from the scalar signal."),
			TextureDerivedNormals, 0);
	}
	const FString ManifestPath = FPaths::Combine(
		ManifestRoot, TEXT("accepted.surface-manifest.json"));
	FString Manifest;
	TestTrue(TEXT("The surface manifest is readable."),
		FFileHelper::LoadFileToString(Manifest, *ManifestPath));
	TestFalse(TEXT("The surface manifest contains no CRLF."), Manifest.Contains(TEXT("\r")));

	TArray<FString> PackageFiles;
	IFileManager::Get().FindFilesRecursive(PackageFiles, *OutputRoot, TEXT("*.uasset"), true, false);
	PackageFiles.Sort();
	TArray<FDateTime> PackageTimes;
	for (const FString& PackageFile : PackageFiles)
	{
		PackageTimes.Add(IFileManager::Get().GetTimeStamp(*PackageFile));
	}
	const FDateTime ManifestTime = IFileManager::Get().GetTimeStamp(*ManifestPath);
	FProjectMaterialSurfaceGenerationResult Second;
	const bool bSecondGenerationAccepted =
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request, Second);
	TestTrue(*FString::Printf(TEXT("The unchanged surface regeneration is accepted: %s"), *Second.Error),
		bSecondGenerationAccepted);
	TestEqual(TEXT("The unchanged surfaces are not regenerated."), Second.Generated, 0);
	TestEqual(TEXT("All unchanged surfaces are skipped."), Second.Skipped, 4);
	for (int32 Index = 0; Index < PackageFiles.Num(); ++Index)
	{
		TestEqual(TEXT("An accepted surface package timestamp remains unchanged."),
			IFileManager::Get().GetTimeStamp(*PackageFiles[Index]), PackageTimes[Index]);
	}
	TestEqual(TEXT("The accepted surface manifest timestamp remains unchanged."),
		IFileManager::Get().GetTimeStamp(*ManifestPath), ManifestTime);

	FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"), ContentRoot + TEXT("/"));
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

#endif
