#include "ProjectMaterialSingleLayerWater.h"
#include "ProjectMaterialSurfaceDependency.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialSurfaceManifest.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialSurfacePreview.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Engine/Texture.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "PackageTools.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialRiverTest,
	"Project.Material.River.AuthenticatedNativeSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialRiverTest::RunTest(const FString&)
{
	const FString Root=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/material/generation/native_river_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Source=FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins/Resources/ProjectMaterial/Data/TestFixtures/River"));
	FString Error;
	FProjectMaterialSurfaceGenerationRequest Request;
	if (!TestTrue(TEXT("Existing material fixture owner admits isolated root"),
		FProjectMaterialSurfaceGenerationService::MakeTestRequest(Root,FString(),Request,Error))) return false;
	const FString Content=FPaths::Combine(Root,TEXT("content/"));
	FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"),Content);
	ON_SCOPE_EXIT
	{
		TArray<UPackage*> Packages;
		for (const TCHAR* Name : {TEXT("M_ProjectRiverFixture"),TEXT("MI_ProjectRiverFixture")})
		{
			if (UPackage* P=FindPackage(nullptr,*(FString(TEXT("/ProjectMaterialTest/Surfaces/River/"))+Name))) { P->SetDirtyFlag(false); Packages.Add(P); }
		}
		if (!Packages.IsEmpty()) UPackageTools::UnloadPackages(Packages);
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"),Content);
		IFileManager::Get().DeleteDirectory(*Root,false,true);
	};
	FProjectMaterialSurfaceRecipe Recipe;
	for (const TCHAR* Name : {TEXT("M_ProjectRiverFixture"),TEXT("MI_ProjectRiverFixture")})
	{
		const FString Dest=FPaths::Combine(Request.RecipeRoot,TEXT("River"),FString(Name)+TEXT(".surface.json"));
		const FString Src=FPaths::Combine(Source,FString(Name)+TEXT(".surface.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
		FString Json;
		if (!TestTrue(TEXT("Copy declared native recipe"),FFileHelper::LoadFileToString(Json,*Src) && FFileHelper::SaveStringToFile(Json,*Dest))) return false;
		if (FString(Name).StartsWith(TEXT("M_")))
		{
			if (!TestTrue(TEXT("Native recipe parses without a pattern"),FProjectMaterialSurfaceRecipeContract::Parse(Json,Dest,Request.RecipeRoot,Recipe,Error))) return false;
			TestTrue(TEXT("No invented texture-cache pattern identity"),Recipe.PatternId.IsEmpty());
			FProjectMaterialSurfaceRecipe BadRecipe;
			TestFalse(TEXT("A fake pattern is rejected"),FProjectMaterialSurfaceRecipeContract::Parse(
				Json.Replace(TEXT("\"semantic_bindings\""),TEXT("\"pattern\":{\"pattern_id\":\"fake_water\"},\"semantic_bindings\"")),Dest,Request.RecipeRoot,BadRecipe,Error));
		}
	}
	// These paths deliberately do not exist: native-only compilation must not read
	// ProjectTexture authority or build a cache output that the river does not use.
	Request.PatternManifestPath=FPaths::Combine(Root,TEXT("no_pattern_manifest"));
	Request.PatternContentRoot=FPaths::Combine(Root,TEXT("no_pattern_content"));
	UTexture* NativeTexture=LoadObject<UTexture>(nullptr,TEXT("/Water/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened.T_Water_TilingNormal_With_Height_02_Softened"));
	if (!TestNotNull(TEXT("Native normal texture loads"),NativeTexture)) return false;
	NativeTexture->MarkPackageDirty();
	FProjectMaterialSurfaceDependency RefusedResources;
	TestFalse(TEXT("Dirty native resource refuses authentication"),ProjectMaterialSingleLayerWater::ResolveResources(RefusedResources,Error));
	TestFalse(TEXT("Refusal produces no material outputs"),FPaths::DirectoryExists(Request.OutputContentRoot));
	NativeTexture->GetOutermost()->SetDirtyFlag(false);
	FProjectMaterialSurfaceGenerationResult First;
	if (!TestTrue(*FString::Printf(TEXT("Generate, save and reload the native graph: %s"),*First.Error),
		FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request,First))) { AddError(First.Error); return false; }
	TestEqual(TEXT("One parent and one instance"),First.Generated,2);
	TestEqual(TEXT("Only one graph compile"),First.ShaderCompiles,1);
	TMap<FString,FProjectMaterialSurfaceManifestRecord> Records;
	const FString Manifest=FPaths::Combine(Request.ManifestRoot,TEXT("accepted.surface-manifest.json"));
	if (!TestTrue(TEXT("Saved authority loads"),ProjectMaterialSurfaceManifest::Load(Manifest,Records,Error))) return false;
	TestEqual(TEXT("Two authenticated artifacts"),Records.Num(),2);
	for (const auto& Record : Records)
	{
		TestTrue(TEXT("No pattern provenance is fabricated"),Record.Value.PatternId.IsEmpty() && Record.Value.PatternObjectPath.IsEmpty());
		TestEqual(TEXT("Four direct Epic resources authenticated"),Record.Value.NativeResourceHashes.Num(),4);
	}
	FProjectMaterialSurfaceGenerationResult Warm;
	TestTrue(TEXT("Unchanged native request skips"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request,Warm));
	TestEqual(TEXT("No second asset write"),Warm.Generated,0);
	TestEqual(TEXT("Two assets reused"),Warm.Skipped,2);
	FProjectMaterialSurfaceGenerationResult Validate;
	TestTrue(TEXT("Read-only authority verification succeeds"),FProjectMaterialSurfaceGenerationService::Validate(Request,Validate));
	UMaterial* Material=LoadObject<UMaterial>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/River/M_ProjectRiverFixture.M_ProjectRiverFixture"));
	if (TestNotNull(TEXT("Native parent loads"),Material))
	{
		TestTrue(TEXT("Explicit single-layer-water BSDF and resource shape"),ProjectMaterialSingleLayerWater::Verify(Recipe,Material,Error));
		for (auto* Expression : UMaterialEditingLibrary::GetMaterialExpressions(Material))
		{
			if (auto* Water=Cast<UMaterialExpressionSubstrateSingleLayerWaterBSDF>(Expression))
			{
				const auto Normal=Water->Normal;
				Water->Normal.Expression=nullptr;
				TestFalse(TEXT("Disconnected water normal refuses"),ProjectMaterialSingleLayerWater::Verify(Recipe,Material,Error));
				Water->Normal=Normal;
				const auto Extinction=Water->WaterExtinction;
				Water->WaterExtinction=Water->WaterAlbedo;
				TestFalse(TEXT("Wrong optical route refuses"),ProjectMaterialSingleLayerWater::Verify(Recipe,Material,Error));
				Water->WaterExtinction=Extinction;
			}
		}
		Material->bTangentSpaceNormal=true;
		TestFalse(TEXT("Known-bad normal frame refuses"),ProjectMaterialSingleLayerWater::Verify(Recipe,Material,Error));
		Material->bTangentSpaceNormal=false; Material->GetOutermost()->SetDirtyFlag(false);
	}
	auto* Instance=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/River/MI_ProjectRiverFixture.MI_ProjectRiverFixture"));
	auto* Mesh=NewObject<UStaticMesh>(GetTransientPackage()); Mesh->GetStaticMaterials().Add(FStaticMaterial(Instance));
	auto* Component=NewObject<UStaticMeshComponent>(GetTransientPackage()); Component->SetStaticMesh(Mesh);
	const FString PreviewRoot=FPaths::Combine(Root,TEXT("preview"));
	const FString PreviewPath=FPaths::Combine(PreviewRoot,TEXT("River/MI_ProjectRiverFixture.surface.json"));
	FString PreviewJson;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreviewPath),true);
	if (!FFileHelper::LoadFileToString(PreviewJson,*FPaths::Combine(Request.RecipeRoot,TEXT("River/MI_ProjectRiverFixture.surface.json"))) ||
		!FFileHelper::SaveStringToFile(PreviewJson.Replace(TEXT("\"RippleStrength\": 0.22"),TEXT("\"RippleStrength\": 0.15")),*PreviewPath)) return false;
	FProjectMaterialSurfacePreview Session;
	if (!TestTrue(TEXT("Existing preview operation accepts an authenticated water fixture binding"),
		Session.ApplyFixtureRecipeFile(PreviewPath,PreviewRoot,Root,FString(),Component,0,Error))) { AddError(Error); return false; }
	auto* Preview=Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)); float Strength=0;
	TestTrue(TEXT("The live MID consumes the changed JSON parameter"),Preview && Preview->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RippleStrength")),Strength) && FMath::IsNearlyEqual(Strength,.15f));
	TestTrue(TEXT("Water preview restores the exact binding"),Session.Revert(Error) && Component->GetMaterial(0)==Instance);
	auto* OriginalMID=UMaterialInstanceDynamic::Create(Instance,GetTransientPackage());
	UTexture* Wrong=LoadObject<UTexture>(nullptr,TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	if (!TestNotNull(TEXT("Known-wrong native texture control exists"),Wrong)) return false;
	OriginalMID->SetTextureParameterValue(TEXT("RippleNormal"),Wrong); Component->SetMaterial(0,OriginalMID);
	TestTrue(TEXT("Native fixture preview authenticates the copied MID texture"),Session.ApplyFixtureRecipeFile(PreviewPath,PreviewRoot,Root,FString(),Component,0,Error));
	Preview=Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)); UTexture* Effective=nullptr; UTexture* AuthenticatedNormal=nullptr;
	TestTrue(TEXT("Reloaded authority exposes its authenticated native normal"),Instance->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RippleNormal")),AuthenticatedNormal));
	TestTrue(TEXT("Preview uses the authenticated Epic texture"),Preview &&
		Preview->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RippleNormal")),Effective) && Effective==AuthenticatedNormal && Effective!=Wrong);
	TestTrue(TEXT("Native MID revert preserves the original override"),Session.Revert(Error) && Component->GetMaterial(0)==OriginalMID &&
		OriginalMID->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RippleNormal")),Effective) && Effective==Wrong);
	return true;
}
#endif
