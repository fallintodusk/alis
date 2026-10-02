// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceGenerationService.h"

#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialSurfaceBuilder.h"
#include "ProjectMaterialSurfaceManifest.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialTerrainLayoutContract.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialSurfaceGeneration, Log, All);

namespace ProjectMaterialSurfaceGenerationPrivate
{
constexpr TCHAR ProductionPackageRoot[] = TEXT("/ProjectMaterial/Surfaces");
constexpr TCHAR TestPackageRoot[] = TEXT("/ProjectMaterialTest/Surfaces");
constexpr TCHAR ManifestName[] = TEXT("accepted.surface-manifest.json");

bool IsUnderDirectory(const FString& Child, const FString& Parent)
{
	FString AbsoluteChild = FPaths::ConvertRelativePathToFull(Child);
	FString AbsoluteParent = FPaths::ConvertRelativePathToFull(Parent);
	FPaths::NormalizeDirectoryName(AbsoluteChild);
	FPaths::NormalizeDirectoryName(AbsoluteParent);
	return AbsoluteChild.Equals(AbsoluteParent, ESearchCase::IgnoreCase) ||
		FPaths::IsUnderDirectory(AbsoluteChild, AbsoluteParent);
}

bool ValidateRequest(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	bool bMutating,
	bool bCommandletOwner,
	FString& OutError)
{
	if (Request.RecipeRoot.IsEmpty() || Request.OutputContentRoot.IsEmpty() ||
		Request.ManifestRoot.IsEmpty() || Request.PatternManifestPath.IsEmpty() ||
		Request.PatternContentRoot.IsEmpty() || Request.TerrainLayoutReceiptPath.IsEmpty())
	{
		OutError = TEXT("Surface recipe, output, manifest, pattern, and layout inputs are required.");
		return false;
	}
	const bool bProduction = Request.OutputPackageRoot == ProductionPackageRoot;
	const bool bTest = Request.OutputPackageRoot == TestPackageRoot;
	if (!bProduction && !bTest)
	{
		OutError = TEXT("Output package root is not an admitted ProjectMaterial surface mount.");
		return false;
	}
	// A test output may bind a ProjectTexture test mount; production binds only ProjectTexture.
	if (Request.PatternPackageRoot != TEXT("/ProjectTexture") &&
		!(bTest && Request.PatternPackageRoot == TEXT("/ProjectTextureTest")))
	{
		OutError = TEXT("Pattern package root is not an admitted ProjectTexture mount for this output.");
		return false;
	}
	if (bMutating && bProduction && (!bCommandletOwner || !IsRunningCommandlet()))
	{
		OutError = TEXT("Production surface mutation requires the dedicated one-shot commandlet.");
		return false;
	}
	if (bMutating && bTest)
	{
		const FString AllowedRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation"));
		if (!IsUnderDirectory(Request.RecipeRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.OutputContentRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.ManifestRoot, AllowedRoot))
		{
			OutError = TEXT("Test surface mutation is confined to project tmp/material/generation.");
			return false;
		}
	}
	return true;
}

bool HashFile(const FString& Path, FString& OutHash, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || !FProjectSha256::HashBuffer(Bytes, OutHash) ||
		OutHash.Len() != 64)
	{
		OutError = FString::Printf(TEXT("Could not hash generated surface file: %s"), *Path);
		return false;
	}
	return true;
}

bool ResolvePackageFile(const FString& PackageName, FString& OutFilename, FString& OutError)
{
	OutFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	if (OutFilename.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not resolve surface package file: %s"), *PackageName);
		return false;
	}
	return true;
}

bool DiscoverRecipes(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	TArray<FProjectMaterialSurfaceRecipe>& OutRecipes,
	FString& OutError)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(
		Files, *Request.RecipeRoot, TEXT("*.surface.json"), true, false, false);
	Files.Sort();
	TSet<FString> MaterialIds;
	TSet<FString> BindingIds;
	TSet<FString> OutputPaths;
	for (const FString& File : Files)
	{
		FString Json;
		FProjectMaterialSurfaceRecipe Recipe;
		if (!FFileHelper::LoadFileToString(Json, *File) ||
			!FProjectMaterialSurfaceRecipeContract::Parse(Json, File, Request.RecipeRoot, Recipe, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Could not read surface recipe: %s"), *File);
			}
			return false;
		}
		FString PackageName;
		FString ObjectPath;
		if (!FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutError))
		{
			return false;
		}
		if (MaterialIds.Contains(Recipe.MaterialId) || OutputPaths.Contains(ObjectPath) ||
			(!Recipe.BindingId.IsEmpty() && BindingIds.Contains(Recipe.BindingId)))
		{
			OutError = FString::Printf(TEXT("Duplicate surface identity, binding, or output: %s"), *ObjectPath);
			return false;
		}
		MaterialIds.Add(Recipe.MaterialId);
		OutputPaths.Add(ObjectPath);
		if (!Recipe.BindingId.IsEmpty())
		{
			BindingIds.Add(Recipe.BindingId);
		}
		OutRecipes.Add(MoveTemp(Recipe));
	}
	OutRecipes.Sort([](const auto& Left, const auto& Right)
	{
		if (Left.ArtifactKind != Right.ArtifactKind)
		{
			return Left.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Parent;
		}
		return Left.MaterialId < Right.MaterialId;
	});
	TSet<FString> ParentPaths;
	for (const FProjectMaterialSurfaceRecipe& Recipe : OutRecipes)
	{
		if (Recipe.ArtifactKind != EProjectMaterialSurfaceArtifactKind::Parent)
		{
			continue;
		}
		FString PackageName;
		FString ObjectPath;
		FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutError);
		ParentPaths.Add(ObjectPath);
	}
	for (const FProjectMaterialSurfaceRecipe& Recipe : OutRecipes)
	{
		if (Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Instance &&
			!ParentPaths.Contains(Recipe.ParentObjectPath.Replace(
				TEXT("/ProjectMaterial/Surfaces/"), *(Request.OutputPackageRoot + TEXT("/")))))
		{
			OutError = FString::Printf(TEXT("Surface instance parent is not declared: %s"),
				*Recipe.ParentObjectPath);
			return false;
		}
	}
	return true;
}

bool SaveAndReload(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern,
	const TMap<FName, int32>& SemanticChannels,
	const FString& PackageName,
	const FString& ObjectPath,
	UObject* Asset,
	FString& OutPackageHash,
	FString& OutError)
{
	UPackage* Package = Asset != nullptr ? Asset->GetOutermost() : nullptr;
	FString Filename;
	if (Package == nullptr || !ResolvePackageFile(PackageName, Filename, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Generated surface asset has no package.");
		}
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.bWarnOfLongFilename = true;
	SaveArgs.bSlowTask = false;
	if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Could not save generated surface: %s"), *Filename);
		return false;
	}
	Package->SetDirtyFlag(false);
	UPackageTools::FlushAsyncCompilation({Package});
	if (!UPackageTools::UnloadPackages({Package}))
	{
		OutError = FString::Printf(TEXT("Could not release generated surface: %s"), *PackageName);
		return false;
	}
	CollectGarbage(RF_NoFlags);
	UObject* Reloaded = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (Reloaded == nullptr ||
		!ProjectMaterialSurfaceBuilder::Verify(Recipe, Pattern, SemanticChannels, Reloaded, OutError))
	{
		return false;
	}
	Reloaded->GetOutermost()->SetDirtyFlag(false);
	return HashFile(Filename, OutPackageHash, OutError);
}

FProjectMaterialSurfaceManifestRecord BuildRecord(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern,
	const FString& ObjectPath,
	const FString& ParentHash,
	const FProjectMaterialTerrainLayoutContract* Layout,
	const FString& PackageHash)
{
	FProjectMaterialSurfaceManifestRecord Record;
	Record.RecipePath = Recipe.SourcePath;
	Record.RecipeSha256 = Recipe.RecipeSha256;
	Record.RecipeSourceSha256 = Recipe.RecipeSourceSha256;
	Record.BindingId = Recipe.BindingId;
	Record.Family = Recipe.Family;
	Record.Archetype = Recipe.Archetype;
	Record.CompilerVersion = Recipe.CompilerVersion;
	Record.PatternId = Recipe.PatternId;
	Record.PatternObjectPath = Pattern.OutputObjectPath;
	Record.PatternOutputContract = Pattern.OutputContract;
	Record.PatternSemanticIdentity = Pattern.SemanticIdentity;
	Record.PatternPackageSha256 = Pattern.PackageSha256;
	Record.DependencyObjectPath = Recipe.ParentObjectPath;
	Record.DependencyPackageSha256 = ParentHash;
	Record.TerrainLayoutReceiptSha256 = Layout != nullptr ? Layout->ReceiptSha256 : FString();
	Record.OutputObjectPath = ObjectPath;
	Record.SemanticIdentity = FProjectMaterialSurfaceRecipeContract::ComputeArtifactSemanticIdentity(
		Recipe, ObjectPath, Pattern, ParentHash, Layout);
	Record.PackageSha256 = PackageHash;
	Record.CompilerFingerprint = FProjectMaterialCompilerIdentity::GetCompilerFingerprint();
	Record.EngineCompatibility = FProjectMaterialSurfaceRecipeContract::GetEngineCompatibilityIdentity();
	return Record;
}

bool IsAcceptedSkip(
	const FProjectMaterialSurfaceManifestRecord* Existing,
	const FProjectMaterialSurfaceManifestRecord& Candidate,
	const FString& PackageFile)
{
	if (Existing == nullptr || Existing->SemanticIdentity != Candidate.SemanticIdentity ||
		!FPaths::FileExists(PackageFile))
	{
		return false;
	}
	FString Actual;
	FString Error;
	return HashFile(PackageFile, Actual, Error) && Actual == Existing->PackageSha256;
}

bool HandleOrphans(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	const TMap<FString, FProjectMaterialSurfaceManifestRecord>& Existing,
	const TSet<FString>& AcceptedObjectPaths,
	FProjectMaterialSurfaceGenerationResult& OutResult)
{
	TArray<FString> Candidates;
	for (const TPair<FString, FProjectMaterialSurfaceManifestRecord>& Pair : Existing)
	{
		if (AcceptedObjectPaths.Contains(Pair.Key))
		{
			continue;
		}
		const FString PackageName = FPackageName::ObjectPathToPackageName(Pair.Key);
		OutResult.OrphanPackageNames.Add(PackageName);
		if (Request.bCleanupOrphans && PackageName.StartsWith(Request.OutputPackageRoot + TEXT("/")))
		{
			Candidates.Add(PackageName);
		}
	}
	OutResult.OrphanPackageNames.Sort();
	if (Candidates.IsEmpty())
	{
		return true;
	}

	// A commandlet does not gather the registry at startup, so references held by on-disk packages
	// this run never loaded are unknown until one full synchronous search completes.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	if (Registry.IsGathering())
	{
		OutResult.Error = TEXT("Asset Registry gathering did not complete; surface orphan cleanup refused.");
		return false;
	}
	UE_LOG(LogProjectMaterialSurfaceGeneration, Display,
		TEXT("[GenerationService::Orphans] Asset Registry gathered for orphan cleanup - candidates=%d"),
		Candidates.Num());
	for (const FString& PackageName : Candidates)
	{
		TArray<FName> Referencers;
		Registry.GetReferencers(FName(PackageName), Referencers);
		if (!Referencers.IsEmpty())
		{
			TArray<FString> Described;
			for (const FName Referencer : Referencers)
			{
				Described.Add(FString::Printf(TEXT("%s(loaded=%s)"), *Referencer.ToString(),
					FindPackage(nullptr, *Referencer.ToString()) != nullptr ? TEXT("true") : TEXT("false")));
			}
			Described.Sort();
			UE_LOG(LogProjectMaterialSurfaceGeneration, Warning,
				TEXT("[GenerationService::Orphans] Referenced surface orphan retained - package=%s referencers=%s"),
				*PackageName, *FString::Join(Described, TEXT(", ")));
			OutResult.RetainedOrphanPackageNames.Add(PackageName);
			continue;
		}
		// A deletion that cannot complete rejects the run, so the host restores the prior manifest and
		// outputs; an orphan whose file is already absent needs no deletion.
		FString PackageFile;
		if (!ResolvePackageFile(PackageName, PackageFile, OutResult.Error))
		{
			return false;
		}
		if (!IsUnderDirectory(PackageFile, Request.OutputContentRoot))
		{
			OutResult.Error = FString::Printf(
				TEXT("Surface orphan resolves outside the output root: %s"), *PackageName);
			return false;
		}
		if (!IFileManager::Get().Delete(*PackageFile, false, true, true))
		{
			OutResult.Error = FString::Printf(
				TEXT("Could not delete unreferenced surface orphan: %s"), *PackageFile);
			return false;
		}
		UE_LOG(LogProjectMaterialSurfaceGeneration, Display,
			TEXT("[GenerationService::Orphans] Unreferenced surface orphan deleted - package=%s"), *PackageName);
	}
	return true;
}

bool PrepareInputs(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	const TArray<FProjectMaterialSurfaceRecipe>& Recipes,
	TMap<FString, FProjectMaterialPatternDependency>& OutPatterns,
	FProjectMaterialTerrainLayoutContract& OutLayout,
	FString& OutError)
{
	bool bNeedsTerrainLayout = false;
	for (const FProjectMaterialSurfaceRecipe& Recipe : Recipes)
	{
		if (!OutPatterns.Contains(Recipe.PatternId))
		{
			FProjectMaterialPatternDependency Pattern;
			if (!FProjectMaterialPatternAuthority::Resolve(
				Recipe.PatternId, Request.PatternManifestPath, Request.PatternPackageRoot,
				Request.PatternContentRoot, Pattern, OutError))
			{
				return false;
			}
			OutPatterns.Add(Recipe.PatternId, MoveTemp(Pattern));
		}
		bNeedsTerrainLayout |= Recipe.Archetype == TEXT("terrain_metric");
	}
	return !bNeedsTerrainLayout || FProjectMaterialTerrainLayoutCompiler::CompileFile(
		Request.TerrainLayoutReceiptPath, FString(), OutLayout, OutError);
}
}

bool FProjectMaterialSurfaceGenerationService::Validate(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	FProjectMaterialSurfaceGenerationResult& OutResult)
{
	return RegenerateInternal(Request, false, true, OutResult);
}

bool FProjectMaterialSurfaceGenerationService::RegenerateTestMount(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	FProjectMaterialSurfaceGenerationResult& OutResult)
{
	return RegenerateInternal(Request, false, false, OutResult);
}

bool FProjectMaterialSurfaceGenerationService::RegenerateForCommandlet(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	FProjectMaterialSurfaceGenerationResult& OutResult)
{
	return RegenerateInternal(Request, true, false, OutResult);
}

// Validate walks the same path as Regenerate and stops before any mutation, so it accepts only
// when a regeneration would skip every output and leave the manifest bytes unchanged.
bool FProjectMaterialSurfaceGenerationService::RegenerateInternal(
	const FProjectMaterialSurfaceGenerationRequest& Request,
	bool bCommandletOwner,
	bool bValidateOnly,
	FProjectMaterialSurfaceGenerationResult& OutResult)
{
	using namespace ProjectMaterialSurfaceGenerationPrivate;
	OutResult = {};
	if (!ValidateRequest(Request, !bValidateOnly, bCommandletOwner, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectMaterialSurfaceRecipe> Recipes;
	if (!DiscoverRecipes(Request, Recipes, OutResult.Error))
	{
		return false;
	}
	OutResult.Validated = Recipes.Num();
	TMap<FString, FProjectMaterialPatternDependency> Patterns;
	FProjectMaterialTerrainLayoutContract Layout;
	if (!PrepareInputs(Request, Recipes, Patterns, Layout, OutResult.Error))
	{
		return false;
	}
	const FString ManifestPath = FPaths::Combine(Request.ManifestRoot, ManifestName);
	TMap<FString, FProjectMaterialSurfaceManifestRecord> Existing;
	if (!ProjectMaterialSurfaceManifest::Load(ManifestPath, Existing, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectMaterialSurfaceManifestRecord> Accepted;
	TMap<FString, FString> AcceptedPackageHashes;
	TSet<FString> AcceptedObjectPaths;
	for (const FProjectMaterialSurfaceRecipe& Recipe : Recipes)
	{
		const FProjectMaterialPatternDependency* Pattern = Patterns.Find(Recipe.PatternId);
		if (Pattern == nullptr)
		{
			OutResult.Error = FString::Printf(
				TEXT("Resolved pattern dependency is absent: %s"), *Recipe.PatternId);
			return false;
		}
		FString PackageName;
		FString ObjectPath;
		if (!FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutResult.Error))
		{
			return false;
		}
		FString PackageFile;
		if (!ResolvePackageFile(PackageName, PackageFile, OutResult.Error))
		{
			return false;
		}
		FString ParentHash;
		FProjectMaterialSurfaceRecipe BuildRecipe = Recipe;
		if (Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Instance)
		{
			const FString TestParentPath = Recipe.ParentObjectPath.Replace(
				TEXT("/ProjectMaterial/Surfaces/"), *(Request.OutputPackageRoot + TEXT("/")));
			const FString ParentPackage = FPackageName::ObjectPathToPackageName(TestParentPath);
			const FString* AcceptedHash = AcceptedPackageHashes.Find(ParentPackage);
			if (AcceptedHash == nullptr)
			{
				OutResult.Error = FString::Printf(TEXT("Surface parent was not accepted: %s"),
					*Recipe.ParentObjectPath);
				return false;
			}
			ParentHash = *AcceptedHash;
			BuildRecipe.ParentObjectPath = TestParentPath;
		}
		const FProjectMaterialTerrainLayoutContract* TerrainLayout =
			Recipe.Archetype == TEXT("terrain_metric") ? &Layout : nullptr;
		FProjectMaterialSurfaceManifestRecord Candidate = BuildRecord(
			Recipe, *Pattern, ObjectPath, ParentHash, TerrainLayout, TEXT("pending"));
		if (IsAcceptedSkip(Existing.Find(ObjectPath), Candidate, PackageFile))
		{
			Candidate.PackageSha256 = Existing[ObjectPath].PackageSha256;
			Accepted.Add(Candidate);
			AcceptedPackageHashes.Add(PackageName, Candidate.PackageSha256);
			AcceptedObjectPaths.Add(ObjectPath);
			OutResult.OutputObjectPaths.Add(ObjectPath);
			++OutResult.Skipped;
			continue;
		}
		if (bValidateOnly)
		{
			OutResult.Error = FString::Printf(TEXT("Accepted surface is stale: %s"), *ObjectPath);
			return false;
		}
		if (FPaths::FileExists(PackageFile))
		{
			if (!Request.bHostTransactionOwnsReplacement)
			{
				OutResult.Error = FString::Printf(
					TEXT("Existing surface replacement requires the host transaction owner: %s"),
					*ObjectPath);
				return false;
			}
			if (!IFileManager::Get().Delete(*PackageFile, false, true, true))
			{
				OutResult.Error = FString::Printf(TEXT("Could not clear existing surface: %s"), *PackageFile);
				return false;
			}
		}
		UObject* Asset = nullptr;
		TArray<FString> CompileErrors;
		const TMap<FName, int32> Channels = Recipe.Archetype == TEXT("terrain_metric")
			? Layout.SemanticChannels : TMap<FName, int32>();
		const bool bBuilt = Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Parent
			? ProjectMaterialSurfaceBuilder::BuildParent(
				BuildRecipe, *Pattern, PackageName, Channels, Asset, CompileErrors, OutResult.Error)
			: ProjectMaterialSurfaceBuilder::BuildInstance(
				BuildRecipe, PackageName, Asset, OutResult.Error);
		if (!bBuilt)
		{
			return false;
		}
		if (Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Parent)
		{
			++OutResult.ShaderCompiles;
		}
		FString PackageHash;
		if (!SaveAndReload(
			BuildRecipe, *Pattern, Channels, PackageName, ObjectPath, Asset, PackageHash, OutResult.Error))
		{
			return false;
		}
		if (Request.FailureInjection == TEXT("post-save"))
		{
			OutResult.Error = TEXT("Injected failure after surface save and before manifest promotion.");
			return false;
		}
		Candidate = BuildRecord(Recipe, *Pattern, ObjectPath, ParentHash, TerrainLayout, PackageHash);
		Accepted.Add(Candidate);
		AcceptedPackageHashes.Add(PackageName, PackageHash);
		AcceptedObjectPaths.Add(ObjectPath);
		OutResult.OutputObjectPaths.Add(ObjectPath);
		++OutResult.Generated;
	}
	// Any accepted record change is persisted, including a source digest refresh for which every
	// asset was skipped.
	FString ExistingManifest;
	const bool bManifestCurrent = FFileHelper::LoadFileToString(ExistingManifest, *ManifestPath) &&
		ExistingManifest == ProjectMaterialSurfaceManifest::Serialize(Accepted);
	if (bValidateOnly && !bManifestCurrent)
	{
		OutResult.Error = FString::Printf(TEXT("Accepted surface manifest is stale: %s"), *ManifestPath);
		return false;
	}
	if (!bValidateOnly && !HandleOrphans(Request, Existing, AcceptedObjectPaths, OutResult))
	{
		return false;
	}
	if (!bManifestCurrent)
	{
		if (!ProjectMaterialSurfaceManifest::Save(
			ManifestPath, Accepted, OutResult.ManifestSha256, OutResult.Error))
		{
			return false;
		}
	}
	else if (!HashFile(ManifestPath, OutResult.ManifestSha256, OutResult.Error))
	{
		return false;
	}
	UE_LOG(LogProjectMaterialSurfaceGeneration, Display,
		TEXT("[GenerationService::%s] Accepted - validated=%d generated=%d skipped=%d compiles=%d orphans=%d"),
		bValidateOnly ? TEXT("Validate") : TEXT("Regenerate"),
		OutResult.Validated, OutResult.Generated, OutResult.Skipped,
		OutResult.ShaderCompiles, OutResult.OrphanPackageNames.Num());
	return true;
}
