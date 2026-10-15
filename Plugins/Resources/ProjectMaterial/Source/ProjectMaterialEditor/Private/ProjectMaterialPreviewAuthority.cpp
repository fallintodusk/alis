// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialPreviewAuthority.h"
#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialFixturePackage.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialSingleLayerWater.h"
#include "ProjectMaterialSurfaceBuilder.h"
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialSurfaceManifest.h"
#include "ProjectMaterialSurfaceParameters.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialTerrainLayoutContract.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace ProjectMaterialPreviewAuthority
{
bool AuthenticatePackage(const FString& ObjectPath, const FString& Expected, const FString& PackageRoot, FString& OutError)
{
	const FString Package = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (!Package.StartsWith(PackageRoot + TEXT("/")))
	{
		OutError = TEXT("Preview dependency escapes ProjectMaterial surface ownership.");
		return false;
	}
	TArray<uint8> Bytes;
	const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
	if (!FFileHelper::LoadFileToArray(Bytes, *File) ||
		FProjectMaterialCompilerIdentity::ComputeSha256(Bytes) != Expected)
	{
		OutError = FString::Printf(TEXT("Preview baseline package bytes fail authentication: %s"), *ObjectPath);
		return false;
	}
	return true;
}

bool Load(const FString& Path, const FString& Root, const FString& LayoutReceipt,
	FProjectMaterialSurfaceRecipe& OutRecipe, UMaterialInstanceConstant*& OutBaseline, FString& OutError,
	const FString& FixtureRoot, const FString& PatternTestRoot)
{
	OutBaseline = nullptr;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = TEXT("Preview recipe is unreadable.");
		return false;
	}
	if (!FProjectMaterialSurfaceRecipeContract::Parse(Json, Path, Root, OutRecipe, OutError)) return false;
	if (OutRecipe.ArtifactKind != EProjectMaterialSurfaceArtifactKind::Instance)
	{
		OutError = TEXT("Live preview admits instance parameters only; graph changes require the existing generator.");
		return false;
	}
	FString PackageRoot = TEXT("/ProjectMaterial/Surfaces");
	FString FixtureManifest;
	FString BaselineRecipeRoot;
	if (!PatternTestRoot.IsEmpty() && FixtureRoot.IsEmpty())
	{ OutError=TEXT("An isolated pattern root requires an isolated material authority."); return false; }
	FString PatternPackageRoot=TEXT("/ProjectTexture");
	FString PatternContentRoot;
	FString PatternManifestPath;
	if (!PatternTestRoot.IsEmpty())
	{
		const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/texture/generation")));
		const FString Selected=FPaths::ConvertRelativePathToFull(PatternTestRoot);
		if (!FPaths::IsUnderDirectory(Selected,Allowed) || FPaths::IsSamePath(Selected,Allowed))
		{ OutError=TEXT("Pattern preview root escapes the disposable texture generation owner."); return false; }
		PatternPackageRoot=TEXT("/ProjectTextureTest");
		PatternContentRoot=FPaths::Combine(Selected,TEXT("content"));
		PatternManifestPath=FPaths::Combine(Selected,TEXT("manifests/accepted.pattern-manifest.json"));
	}
	if (!FixtureRoot.IsEmpty())
	{
		FProjectMaterialSurfaceGenerationRequest Request;
		FProjectMaterialSurfaceGenerationResult Validated;
		if (!FProjectMaterialSurfaceGenerationService::MakeTestRequest(FixtureRoot, LayoutReceipt, Request, OutError)) return false;
		if (!PatternTestRoot.IsEmpty())
		{
			Request.PatternPackageRoot=PatternPackageRoot;
			Request.PatternContentRoot=PatternContentRoot;
			Request.PatternManifestPath=PatternManifestPath;
		}
		FString Mounted;
		FString Expected = FPaths::Combine(Request.OutputContentRoot, TEXT("__probe"));
		if (!FPackageName::TryConvertLongPackageNameToFilename(FString(TEXT("/ProjectMaterialTest/Surfaces/__probe")), Mounted) ||
			!FPaths::IsSamePath(Mounted, Expected))
		{
			OutError = TEXT("Fixture mount does not match the selected isolated content root.");
			return false;
		}
		if (!FProjectMaterialSurfaceGenerationService::Validate(Request, Validated))
		{
			OutError = Validated.Error;
			return false;
		}
		PackageRoot = Request.OutputPackageRoot;
		BaselineRecipeRoot = Request.RecipeRoot;
		FixtureManifest = FPaths::Combine(Request.ManifestRoot, TEXT("accepted.surface-manifest.json"));
		OutRecipe.ParentObjectPath.ReplaceInline(TEXT("/ProjectMaterial/Surfaces/"), *(PackageRoot + TEXT("/")));
	}
	FString Package, Object;
	if (!FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
		OutRecipe, PackageRoot, Package, Object, OutError)) return false;
	const auto MaterialPlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
	const auto TexturePlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectTexture"));
	if (!MaterialPlugin.IsValid() || !TexturePlugin.IsValid())
	{
		OutError = TEXT("Material or pattern owner plugin is unavailable.");
		return false;
	}
	TMap<FString, FProjectMaterialSurfaceManifestRecord> Records;
	if (!ProjectMaterialSurfaceManifest::Load(FixtureManifest.IsEmpty() ? FPaths::Combine(MaterialPlugin->GetBaseDir(),
		TEXT("Data/Manifests/Surfaces/accepted.surface-manifest.json")) : FixtureManifest, Records, OutError)) return false;
	const auto* Instance = Records.Find(Object);
	const auto* Parent = Records.Find(OutRecipe.ParentObjectPath);
	if (!Instance || !Parent || Instance->BindingId != OutRecipe.BindingId ||
		Instance->Family != OutRecipe.Family || Instance->Archetype != OutRecipe.Archetype ||
		Instance->CompilerVersion != OutRecipe.CompilerVersion || Instance->PatternId != OutRecipe.PatternId ||
		Instance->DependencyObjectPath.Replace(TEXT("/ProjectMaterial/Surfaces/"), *(PackageRoot + TEXT("/"))) != OutRecipe.ParentObjectPath ||
		Instance->DependencyPackageSha256 != Parent->PackageSha256 ||
		Instance->CompilerFingerprint != Parent->CompilerFingerprint ||
		Instance->EngineCompatibility != FProjectMaterialSurfaceRecipeContract::GetEngineCompatibilityIdentity() ||
		Parent->EngineCompatibility != Instance->EngineCompatibility)
	{
		OutError = TEXT("Preview candidate changes the accepted instance structure or has no compatible baseline.");
		return false;
	}
	if (!AuthenticatePackage(Object, Instance->PackageSha256, PackageRoot, OutError) ||
		!AuthenticatePackage(Parent->OutputObjectPath, Parent->PackageSha256, PackageRoot, OutError)) return false;
	FProjectMaterialSurfaceDependency Pattern;
	if (PatternManifestPath.IsEmpty())
	{
		PatternManifestPath=FPaths::Combine(TexturePlugin->GetBaseDir(),TEXT("Data/Manifests/Patterns/accepted.pattern-manifest.json"));
		PatternContentRoot=TexturePlugin->GetContentDir();
	}
	const bool bWater=OutRecipe.Archetype==TEXT("single_layer_water");
	const bool bResolved=bWater ? ProjectMaterialSingleLayerWater::ResolveResources(Pattern,OutError) : FProjectMaterialPatternAuthority::Resolve(OutRecipe.PatternId,
		PatternManifestPath,PatternPackageRoot,PatternContentRoot,Pattern,OutError);
	if (!bResolved) return false;
	bool bNativeMatches=Pattern.NativeResourceHashes.Num()==Parent->NativeResourceHashes.Num() && Pattern.NativeResourceHashes.Num()==Instance->NativeResourceHashes.Num();
	for (const auto& Pair : Pattern.NativeResourceHashes)
		bNativeMatches &= Parent->NativeResourceHashes.FindRef(Pair.Key)==Pair.Value && Instance->NativeResourceHashes.FindRef(Pair.Key)==Pair.Value;
	if (bWater && !bNativeMatches)
	{
		OutError=TEXT("Preview native resource identity differs from the accepted parent."); return false;
	}
	if (Pattern.OutputObjectPath != Instance->PatternObjectPath ||
		Pattern.OutputContract != Instance->PatternOutputContract ||
		Parent->PatternOutputContract != Pattern.OutputContract)
	{
		OutError = TEXT("Preview pattern contract differs from the accepted material graph.");
		return false;
	}
	FProjectMaterialTerrainLayoutContract Layout;
	if (FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(OutRecipe.Archetype) &&
		!FProjectMaterialTerrainLayoutCompiler::CompileFile(LayoutReceipt, FString(), Layout, OutError)) return false;
	OutBaseline = LoadObject<UMaterialInstanceConstant>(nullptr, *Object);
	if (!FixtureRoot.IsEmpty() && OutBaseline && OutBaseline->Parent)
	{
		for (const FString& LoadedPackage : {Package, FPackageName::ObjectPathToPackageName(Parent->OutputObjectPath)})
		{
			if (!ProjectMaterialFixturePackage::CheckLoadedOrigin(LoadedPackage,
				FPackageName::LongPackageNameToFilename(LoadedPackage, FPackageName::GetAssetPackageExtension()), OutError)) return false;
		}
	}
	if (!OutBaseline || !OutBaseline->Parent || OutBaseline->GetOutermost()->IsDirty() ||
		OutBaseline->Parent->GetOutermost()->IsDirty())
	{
		OutError = TEXT("Preview baseline is unavailable or has unsaved asset changes.");
		return false;
	}
	FProjectMaterialSurfaceRecipe ParentRecipe = OutRecipe;
	ParentRecipe.ArtifactKind = EProjectMaterialSurfaceArtifactKind::Parent;
	if (bWater || FProjectMaterialSurfaceRecipeContract::IsMetricHeightArchetype(OutRecipe.Archetype) ||
		!FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(OutRecipe.Archetype))
	{
		if (BaselineRecipeRoot.IsEmpty()) BaselineRecipeRoot=FPaths::Combine(MaterialPlugin->GetBaseDir(),TEXT("Data/SurfaceRecipes"));
		const FString ParentSource=FPaths::ConvertRelativePathToFull(FPaths::Combine(BaselineRecipeRoot,Parent->RecipePath));
		FString ParentJson;
		if (!FPaths::IsUnderDirectory(ParentSource,FPaths::ConvertRelativePathToFull(BaselineRecipeRoot)) ||
			!FFileHelper::LoadFileToString(ParentJson,*ParentSource) ||
			!FProjectMaterialSurfaceRecipeContract::Parse(ParentJson,ParentSource,BaselineRecipeRoot,ParentRecipe,OutError) ||
			ParentRecipe.ArtifactKind!=EProjectMaterialSurfaceArtifactKind::Parent || ParentRecipe.Archetype!=OutRecipe.Archetype || ParentRecipe.RecipeSourceSha256!=Parent->RecipeSourceSha256)
		{
			OutError=TEXT("Preview native parent source is not the accepted recipe."); return false;
		}
	}
	FProjectMaterialSurfaceDependency ParentPattern=Pattern;
	if (!bWater && Parent->PatternObjectPath!=Pattern.OutputObjectPath &&
		!FProjectMaterialPatternAuthority::Resolve(Parent->PatternId,PatternManifestPath,PatternPackageRoot,PatternContentRoot,ParentPattern,OutError)) return false;
	if (!ProjectMaterialPatternBindings::VerifyBindings(OutRecipe, Pattern, OutBaseline, OutError) ||
		!ProjectMaterialSurfaceBuilder::Verify(ParentRecipe, ParentPattern, Layout.SemanticChannels, OutBaseline->Parent, OutError) ||
		!ProjectMaterialSurfaceParameters::Validate(OutRecipe, OutBaseline, OutError)) return false;
	return true;
}
}
