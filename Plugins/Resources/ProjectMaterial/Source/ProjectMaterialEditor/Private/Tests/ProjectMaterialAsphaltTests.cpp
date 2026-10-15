// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialAggregateSurface.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialFixtureDomain.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialPreviewAuthority.h"
#include "ProjectMaterialSurfacePreview.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProjectTextureGenerationService.h"
#include "ProjectTextureFixtureDomain.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "AssetReferencingPolicySettings.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialAsphaltTest,
	"Project.Material.Asphalt.AuthenticatedSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialAsphaltTest::RunTest(const FString&)
{
	const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TextureRoot=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/texture/generation/asphalt_"))+Id;
	const FString MaterialRoot=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/material/generation/asphalt_"))+Id;
	auto* Settings=GetMutableDefault<UAssetReferencingPolicySettings>(); const auto Before=Settings->AdditionalDomains;
	const bool HadTexture=Before.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectTextureFixture");});
	const bool HadMaterial=Before.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectMaterialFixture");});
	TArray<FString> Packages;
	ON_SCOPE_EXIT
	{
		TArray<UPackage*> Loaded;
		for (const FString& Name : Packages) if (auto* P=FindPackage(nullptr,*Name)) {P->SetDirtyFlag(false); Loaded.AddUnique(P);}
		if (!Loaded.IsEmpty()) UPackageTools::UnloadPackages(Loaded);
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"),FPaths::Combine(MaterialRoot,TEXT("content/")));
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"),FPaths::Combine(TextureRoot,TEXT("content/")));
		IFileManager::Get().DeleteDirectory(*MaterialRoot,false,true); IFileManager::Get().DeleteDirectory(*TextureRoot,false,true);
		const uint64 AfterFrame=GFrameCounter+2;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Settings,Before,HadTexture,HadMaterial,AfterFrame]()
		{
			if (GFrameCounter<AfterFrame) return false;
			if (!HadMaterial) ProjectMaterialFixtureDomain::Shutdown(); if (!HadTexture) ProjectTextureFixtureDomain::Shutdown();
			bool Same=Before.Num()==Settings->AdditionalDomains.Num();
			for (int32 I=0; Same && I<Before.Num(); ++I) Same=FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Before[I],&Settings->AdditionalDomains[I],0);
			TestTrue(TEXT("Fixture reference policy is restored exactly"),Same); return true;
		}));
	};
	FProjectTextureGenerationRequest Texture;
	Texture.RecipeRoot=FPaths::Combine(TextureRoot,TEXT("recipes")); Texture.OutputPackageRoot=TEXT("/ProjectTextureTest/Patterns");
	Texture.OutputContentRoot=FPaths::Combine(TextureRoot,TEXT("content/Patterns")); Texture.ManifestRoot=FPaths::Combine(TextureRoot,TEXT("manifests"));
	FString Json,Error;
	const FString Source=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectTexture/Data/TestFixtures/Aggregate/project_asphalt_fixture.pattern.json"));
	const FString Dest=FPaths::Combine(Texture.RecipeRoot,TEXT("Aggregate/project_asphalt_fixture.pattern.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
	if (!FFileHelper::LoadFileToString(Json,*Source) || !FFileHelper::SaveStringToFile(Json,*Dest)) return false;
	const FString SecondJson=Json.Replace(TEXT("project_asphalt_fixture"),TEXT("project_aggregate_second"))
		.Replace(TEXT("DA_ProjectAsphaltFixture"),TEXT("DA_ProjectAggregateSecond"))
		.Replace(TEXT("RT_ProjectAsphaltSignals"),TEXT("RT_ProjectAggregateSecondSignals"))
		.Replace(TEXT("RT_ProjectAsphaltNormal"),TEXT("RT_ProjectAggregateSecondNormal"));
	if (!FFileHelper::SaveStringToFile(SecondJson,*FPaths::Combine(Texture.RecipeRoot,TEXT("Aggregate/project_aggregate_second.pattern.json")))) return false;
	FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"),FPaths::Combine(TextureRoot,TEXT("content/")));
	FProjectTextureGenerationResult Generated;
	if (!TestTrue(TEXT("The existing texture compiler saves and reloads the exact source"),FProjectTextureGenerationService::RegenerateTestMount(Texture,Generated))) { AddError(Generated.Error); return false; }
	for (const FString& Path : Generated.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Path));
	FProjectMaterialSurfaceGenerationRequest Material;
	if (!FProjectMaterialSurfaceGenerationService::MakeTestRequest(MaterialRoot,FString(),Material,Error)) return false;
	Material.PatternPackageRoot=TEXT("/ProjectTextureTest"); Material.PatternContentRoot=FPaths::Combine(TextureRoot,TEXT("content"));
	Material.PatternManifestPath=FPaths::Combine(Texture.ManifestRoot,TEXT("accepted.pattern-manifest.json"));
	FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"),FPaths::Combine(MaterialRoot,TEXT("content/")));
	for (const TCHAR* Name : {TEXT("M_ProjectAsphaltFixture"),TEXT("MI_ProjectAsphaltFixture")})
	{
		const FString File=FPaths::Combine(Material.RecipeRoot,TEXT("Asphalt"),FString(Name)+TEXT(".surface.json"));
		const FString From=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectMaterial/Data/TestFixtures/Asphalt"),FString(Name)+TEXT(".surface.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
		if (!FFileHelper::LoadFileToString(Json,*From)) return false;
		if (FString(Name).StartsWith(TEXT("MI_"))) Json=Json.Replace(TEXT("project_asphalt_fixture"),TEXT("project_aggregate_second"));
		if (!FFileHelper::SaveStringToFile(Json,*File)) return false;
		if (FString(Name).StartsWith(TEXT("MI_")))
		{
			const FString Variant=Json.Replace(TEXT("MI_ProjectAsphaltFixture"),TEXT("MI_ProjectAggregateVariant"))
				.Replace(TEXT("road.default_asphalt"),TEXT("object.aggregate_variant"))
				.Replace(TEXT("\"TileMeters\": 0.32"),TEXT("\"TileMeters\": 0.61"));
			if (!FFileHelper::SaveStringToFile(Variant,*FPaths::Combine(Material.RecipeRoot,TEXT("Asphalt/MI_ProjectAggregateVariant.surface.json")))) return false;
		}
	}
	auto* LaterSource=LoadObject<UTextureRenderTarget2D>(nullptr,TEXT("/ProjectTextureTest/Patterns/Aggregate/DA_ProjectAggregateSecond.RT_ProjectAggregateSecondSignals"));
	if (!TestNotNull(TEXT("Later instance source loads before material generation"),LaterSource)) return false;
	LaterSource->GetOutermost()->MarkPackageDirty();
	FProjectMaterialSurfaceGenerationResult Refused;
	TestFalse(TEXT("A later dirty source refuses the whole request"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,Refused));
	TestEqual(TEXT("Whole-request refusal writes no earlier parent"),Refused.Generated,0);
	TArray<FString> RefusedFiles;
	IFileManager::Get().FindFilesRecursive(RefusedFiles,*Material.OutputContentRoot,TEXT("*.uasset"),true,false,false);
	TestEqual(TEXT("Refusal creates no material packages"),RefusedFiles.Num(),0);
	TestFalse(TEXT("Refusal creates no accepted manifest"),FPaths::FileExists(FPaths::Combine(Material.ManifestRoot,TEXT("accepted.surface-manifest.json"))));
	LaterSource->GetOutermost()->SetDirtyFlag(false);
	for (const FString& Path : Refused.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Path));
	if (Refused.Generated!=0 || !RefusedFiles.IsEmpty()) return false;
	FProjectMaterialSurfaceGenerationResult First;
	if (!TestTrue(TEXT("Save/reload authenticated asphalt parent and instance"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,First))) { AddError(First.Error); return false; }
	TestEqual(TEXT("A shared parent and two data-authored instances"),First.Generated,3); TestEqual(TEXT("One graph compile"),First.ShaderCompiles,1);
	for (const FString& Path : First.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Path));
	FProjectMaterialSurfaceGenerationResult Warm;
	TestTrue(TEXT("Unchanged recipe reuses its artifacts"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,Warm));
	TestEqual(TEXT("Warm material writes are zero"),Warm.Generated,0); TestEqual(TEXT("All artifacts skipped"),Warm.Skipped,3);
	const FString ManifestFile=FPaths::Combine(Material.ManifestRoot,TEXT("accepted.surface-manifest.json"));
	TArray<uint8> ManifestBefore,ManifestAfter;
	if (!FFileHelper::LoadFileToArray(ManifestBefore,*ManifestFile)) return false;
	LaterSource=LoadObject<UTextureRenderTarget2D>(nullptr,TEXT("/ProjectTextureTest/Patterns/Aggregate/DA_ProjectAggregateSecond.RT_ProjectAggregateSecondSignals"));
	LaterSource->GetOutermost()->MarkPackageDirty();
	TestFalse(TEXT("Validation cannot skip a dirty selected source"),FProjectMaterialSurfaceGenerationService::Validate(Material,Refused));
	TestFalse(TEXT("Regeneration cannot skip a dirty selected source"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,Refused));
	LaterSource->GetOutermost()->SetDirtyFlag(false);
	TestEqual(TEXT("Dirty-source rejection performs no writes or skips"),Refused.Generated+Refused.Skipped,0);
	TestTrue(TEXT("Rejected request preserves accepted manifest bytes"),FFileHelper::LoadFileToArray(ManifestAfter,*ManifestFile) && ManifestAfter==ManifestBefore);
	FProjectMaterialSurfaceDependency P;
	if (!FProjectMaterialPatternAuthority::Resolve(TEXT("project_asphalt_fixture"),Material.PatternManifestPath,Material.PatternPackageRoot,Material.PatternContentRoot,P,Error)) return false;
	const FString ParentFile=FPaths::Combine(Material.RecipeRoot,TEXT("Asphalt/M_ProjectAsphaltFixture.surface.json"));
	FProjectMaterialSurfaceRecipe Recipe;
	if (!FFileHelper::LoadFileToString(Json,*ParentFile) || !FProjectMaterialSurfaceRecipeContract::Parse(Json,ParentFile,Material.RecipeRoot,Recipe,Error)) return false;
	TestEqual(TEXT("World projection survives recipe parsing"),Recipe.ProjectionFrame,EProjectMaterialProjectionFrame::WorldMetricXY2D);
	FProjectMaterialSurfaceDependency Second;
	if (!FProjectMaterialPatternAuthority::Resolve(TEXT("project_aggregate_second"),Material.PatternManifestPath,Material.PatternPackageRoot,Material.PatternContentRoot,Second,Error)) return false;
	auto* Instance=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/Asphalt/MI_ProjectAsphaltFixture.MI_ProjectAsphaltFixture"));
    TestTrue(TEXT("Saved geometry parent declares native Nanite usage"),Instance && Instance->Parent && Instance->Parent->GetUsageByFlag(MATUSAGE_Nanite));
    TestTrue(TEXT("Saved instance inherits Nanite usage before its first city consumer"),Instance && Instance->GetUsageByFlag(MATUSAGE_Nanite));
	UTexture* Actual=nullptr;
	TestTrue(TEXT("The recipe-selected source is bound on the reloaded instance"),Instance &&
		Instance->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceSignals")),Actual) &&
		Actual==LoadObject<UTexture>(nullptr,*Second.OutputObjectPath));
	auto* Variant=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/Asphalt/MI_ProjectAggregateVariant.MI_ProjectAggregateVariant"));
	UTexture* VariantSource=nullptr;
	TestTrue(TEXT("Different appearance recipes share one shader parent and source bundle"),Variant && Variant->Parent==Instance->Parent &&
		Variant->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceSignals")),VariantSource) && VariantSource==Actual);
	const FString InstanceFile=FPaths::Combine(Material.RecipeRoot,TEXT("Asphalt/MI_ProjectAsphaltFixture.surface.json"));
	FProjectMaterialSurfaceRecipe InstanceRecipe;
	if (!FFileHelper::LoadFileToString(Json,*InstanceFile) || !FProjectMaterialSurfaceRecipeContract::Parse(Json,InstanceFile,Material.RecipeRoot,InstanceRecipe,Error)) return false;
	InstanceRecipe.ParentObjectPath.ReplaceInline(TEXT("/ProjectMaterial/Surfaces/"),TEXT("/ProjectMaterialTest/Surfaces/"));
	const FString PreviewRoot=FPaths::Combine(MaterialRoot,TEXT("preview"));
	const FString PreviewFile=FPaths::Combine(PreviewRoot,TEXT("Asphalt/MI_ProjectAsphaltFixture.surface.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreviewFile),true);
	if (!FFileHelper::SaveStringToFile(Json.Replace(TEXT("\"TileMeters\": 0.32"),TEXT("\"TileMeters\": 0.47")),*PreviewFile)) return false;
	UMaterialInstanceConstant* PreviewBaseline=nullptr; FProjectMaterialSurfaceRecipe PreviewRecipe;
	TestTrue(TEXT("A changed parameter previews on a shared parent with an independently selected pattern"),
		ProjectMaterialPreviewAuthority::Load(PreviewFile,PreviewRoot,FString(),PreviewRecipe,PreviewBaseline,Error,MaterialRoot,TextureRoot));
	TestEqual(TEXT("Preview authority preserves the accepted baseline"),PreviewBaseline,Instance);
	auto* Mesh=NewObject<UStaticMesh>(GetTransientPackage()); Mesh->GetStaticMaterials().Add(FStaticMaterial(Instance));
	auto* Component=NewObject<UStaticMeshComponent>(GetTransientPackage()); Component->SetStaticMesh(Mesh);
	FProjectMaterialSurfacePreview Session;
	if (!TestTrue(TEXT("The public fixture operation previews its independently bound pattern"),
		Session.ApplyFixtureRecipeFile(PreviewFile,PreviewRoot,MaterialRoot,FString(),Component,0,Error,TextureRoot))) { AddError(Error); return false; }
	auto* Live=Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)); float LiveScale=0; UTexture* LiveSource=nullptr;
	TestTrue(TEXT("Live JSON parameters and selected source reach the same MID"),Live &&
		Live->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TileMeters")),LiveScale) && FMath::IsNearlyEqual(LiveScale,.47f) &&
		Live->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceSignals")),LiveSource) && LiveSource==Actual);
	TestTrue(TEXT("Fixture parameter updates reuse the same operation"),
		Session.ApplyFixtureRecipeFile(PreviewFile,PreviewRoot,MaterialRoot,FString(),Component,0,Error,TextureRoot));
	TestTrue(TEXT("Fixture preview restores its exact accepted binding"),Session.Revert(Error) && Component->GetMaterial(0)==Instance);
	auto* OriginalMID=UMaterialInstanceDynamic::Create(Instance,GetTransientPackage());
	UTexture* WrongSource=LoadObject<UTexture>(nullptr,*P.OutputObjectPath);
	OriginalMID->SetTextureParameterValue(TEXT("SurfaceSignals"),WrongSource); Component->SetMaterial(0,OriginalMID);
	if (!TestTrue(TEXT("Authenticated preview corrects copied renderer texture overrides"),
		Session.ApplyFixtureRecipeFile(PreviewFile,PreviewRoot,MaterialRoot,FString(),Component,0,Error,TextureRoot))) return false;
	Live=Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)); LiveSource=nullptr;
	TestTrue(TEXT("An unauthenticated MID source cannot reach preview shading"),Live &&
		Live->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceSignals")),LiveSource) && LiveSource==Actual);
	TestTrue(TEXT("Revert preserves the exact original MID and its prior override"),Session.Revert(Error) &&
		Component->GetMaterial(0)==OriginalMID && OriginalMID->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceSignals")),LiveSource) && LiveSource==WrongSource);
	OriginalMID->SetTextureParameterValue(TEXT("SurfaceSignals"),Actual);
	TestTrue(TEXT("A valid renderer MID can also preview and revert exactly"),
		Session.ApplyFixtureRecipeFile(PreviewFile,PreviewRoot,MaterialRoot,FString(),Component,0,Error,TextureRoot) && Session.Revert(Error) && Component->GetMaterial(0)==OriginalMID);
	Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("SurfaceSignals")),LoadObject<UTexture>(nullptr,*P.OutputObjectPath));
	TestFalse(TEXT("Known-wrong effective source is refused"),ProjectMaterialPatternBindings::VerifyInstance(InstanceRecipe,Second,Instance,Error));
	Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("SurfaceSignals")),Actual);
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("TileMeters")),.91f);
	TestFalse(TEXT("Known-wrong saved recipe value is refused"),ProjectMaterialPatternBindings::VerifyInstance(InstanceRecipe,Second,Instance,Error));
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("TileMeters")),InstanceRecipe.Scalars.FindChecked(TEXT("TileMeters")));
	const auto UsageBefore=Instance->BasePropertyOverrides;
	Instance->BasePropertyOverrides.bOverride_UsageFlags |= 1u << MATUSAGE_Nanite;
	Instance->BasePropertyOverrides.UsageFlags &= ~(1u << MATUSAGE_Nanite);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	TestFalse(TEXT("An explicit false vertex-factory override refuses authentication"),ProjectMaterialPatternBindings::VerifyInstance(InstanceRecipe,Second,Instance,Error));
	Instance->BasePropertyOverrides=UsageBefore;
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	TestTrue(TEXT("Native inheritance restores the exact supported vertex factory"),Instance->GetUsageByFlag(MATUSAGE_Nanite));
	Instance->GetOutermost()->SetDirtyFlag(false);
	TestTrue(TEXT("Restored instance again matches the authenticated recipe"),ProjectMaterialPatternBindings::VerifyInstance(InstanceRecipe,Second,Instance,Error));
	auto* Parent=LoadObject<UMaterial>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/Asphalt/M_ProjectAsphaltFixture.M_ProjectAsphaltFixture"));
	if (!TestNotNull(TEXT("Saved parent loads"),Parent)) return false;
	auto* Signals=LoadObject<UTextureRenderTarget2D>(nullptr,*P.OutputObjectPath);
	const FString SourcePackage=FPackageName::LongPackageNameToFilename(Signals->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
	TArray<uint8> SourceBefore,SourceAfter; TestTrue(TEXT("Record accepted source bytes"),FFileHelper::LoadFileToArray(SourceBefore,*SourcePackage));
	Signals->GetOutermost()->MarkPackageDirty();
	TestFalse(TEXT("Unsaved source cannot authenticate under disk-only hashes"),ProjectMaterialAggregateSurface::Verify(Recipe,P,Parent,Error));
	Signals->GetOutermost()->SetDirtyFlag(false);
	auto* Slot=CastChecked<UProjectTextureOutputSlot>(Signals->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()));
	auto* NormalTarget=Slot->Catalog->OutputSlots[1]->GetRenderTarget();
	const auto OriginalFormat=NormalTarget->RenderTargetFormat; NormalTarget->RenderTargetFormat=RTF_RGBA16f;
	TestFalse(TEXT("An unauthenticated output format cannot pass"),ProjectMaterialAggregateSurface::Verify(Recipe,P,Parent,Error));
	NormalTarget->RenderTargetFormat=OriginalFormat;
	TestTrue(TEXT("Refusal leaves accepted source bytes unchanged"),FFileHelper::LoadFileToArray(SourceAfter,*SourcePackage) && SourceBefore==SourceAfter);
	for (auto* N : UMaterialEditingLibrary::GetMaterialExpressions(Parent)) if (auto* Slab=Cast<UMaterialExpressionSubstrateSlabBSDF>(N))
	{
		const auto Normal=Slab->Normal; Slab->Normal.Expression=nullptr;
		TestFalse(TEXT("Disconnected world normal is rejected"),ProjectMaterialAggregateSurface::Verify(Recipe,P,Parent,Error)); Slab->Normal=Normal;
		const auto Roughness=Slab->Roughness; Slab->Roughness=Slab->DiffuseAlbedo;
		TestFalse(TEXT("A wrong roughness route is rejected"),ProjectMaterialAggregateSurface::Verify(Recipe,P,Parent,Error)); Slab->Roughness=Roughness;
	}
	FProjectMaterialSurfaceGenerationResult Validate;
	TestTrue(TEXT("Read-only validation of exact saved authority"),FProjectMaterialSurfaceGenerationService::Validate(Material,Validate));
	return true;
}
#endif
