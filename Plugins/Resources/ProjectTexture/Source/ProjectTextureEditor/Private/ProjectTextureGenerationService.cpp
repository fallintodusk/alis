// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureGenerationService.h"

#include "ProjectTexturePatternBuilder.h"
#include "ProjectTexturePatternManifest.h"
#include "ProjectTexturePatternRecipe.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogProjectTextureGeneration, Log, All);

namespace ProjectTextureGenerationPrivate
{
constexpr TCHAR ProductionPackageRoot[] = TEXT("/ProjectTexture/Patterns");
constexpr TCHAR TestPackageRoot[] = TEXT("/ProjectTextureTest/Patterns");
constexpr TCHAR ManifestName[] = TEXT("accepted.pattern-manifest.json");

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
	const FProjectTextureGenerationRequest& Request,
	bool bMutating,
	bool bCommandletOwner,
	FString& OutError)
{
	if (Request.RecipeRoot.IsEmpty() || Request.OutputPackageRoot.IsEmpty() ||
		Request.OutputContentRoot.IsEmpty() || Request.ManifestRoot.IsEmpty())
	{
		OutError = TEXT("Pattern recipe, output-content, and manifest roots are required.");
		return false;
	}
	const bool bProduction = Request.OutputPackageRoot == ProductionPackageRoot;
	const bool bTest = Request.OutputPackageRoot == TestPackageRoot;
	if (!bProduction && !bTest)
	{
		OutError = TEXT("Output package root is not an admitted ProjectTexture owner mount.");
		return false;
	}
	if (bMutating && bProduction && (!bCommandletOwner || !IsRunningCommandlet()))
	{
		OutError = TEXT("Production ProjectTexture mutation requires the dedicated one-shot commandlet.");
		return false;
	}
	if (bMutating && bTest)
	{
		const FString AllowedRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/texture/generation"));
		if (!IsUnderDirectory(Request.RecipeRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.OutputContentRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.ManifestRoot, AllowedRoot))
		{
			OutError = TEXT("Test pattern mutation is confined to project tmp/texture/generation.");
			return false;
		}
	}
	return true;
}

bool HashFile(const FString& Path, FString& OutHash, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read generated pattern package: %s"), *Path);
		return false;
	}
	FProjectSha256::HashBuffer(Bytes, OutHash);
	if (OutHash.Len() != 64)
	{
		OutError = FString::Printf(TEXT("Could not hash generated pattern package: %s"), *Path);
		return false;
	}
	return true;
}

bool DiscoverRecipes(
	const FProjectTextureGenerationRequest& Request,
	TArray<FProjectTexturePatternRecipe>& OutRecipes,
	FString& OutError)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(
		Files, *Request.RecipeRoot, TEXT("*.pattern.json"), true, false, false);
	Files.Sort();
	TSet<FString> PatternIds;
	TSet<FString> OutputPaths;
	for (const FString& File : Files)
	{
		FString Json;
		FProjectTexturePatternRecipe Recipe;
		if (!FFileHelper::LoadFileToString(Json, *File) ||
			!FProjectTexturePatternRecipeContract::Parse(Json, File, Request.RecipeRoot, Recipe, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Could not read pattern recipe: %s"), *File);
			}
			return false;
		}
		FString PackageName;
		FString ObjectPath;
		if (!FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutError))
		{
			return false;
		}
		if (PatternIds.Contains(Recipe.PatternId) || OutputPaths.Contains(ObjectPath))
		{
			OutError = FString::Printf(TEXT("Duplicate pattern identity or output: %s"), *ObjectPath);
			return false;
		}
		PatternIds.Add(Recipe.PatternId);
		OutputPaths.Add(ObjectPath);
		OutRecipes.Add(MoveTemp(Recipe));
	}
	return true;
}

bool ResolvePackageFile(const FString& PackageName, FString& OutFilename, FString& OutError)
{
	OutFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	if (OutFilename.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not resolve pattern package file: %s"), *PackageName);
		return false;
	}
	return true;
}

bool SaveAndReload(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& PackageName,
	const FString& ObjectPath,
	UObject* Asset,
	FString& OutPackageHash,
	FString& OutError)
{
	UPackage* Package = Asset != nullptr ? Asset->GetOutermost() : nullptr;
	if (Package == nullptr)
	{
		OutError = TEXT("Generated pattern asset has no package.");
		return false;
	}
	FString Filename;
	if (!ResolvePackageFile(PackageName, Filename, OutError))
	{
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
		OutError = FString::Printf(TEXT("Could not save generated pattern: %s"), *Filename);
		return false;
	}
	Package->SetDirtyFlag(false);
	UPackageTools::FlushAsyncCompilation({Package});
	if (!UPackageTools::UnloadPackages({Package}))
	{
		OutError = FString::Printf(TEXT("Could not release generated pattern: %s"), *PackageName);
		return false;
	}
	CollectGarbage(RF_NoFlags);
	UObject* Reloaded = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (Reloaded == nullptr || !ProjectTexturePatternBuilder::Verify(Recipe, Reloaded, OutError))
	{
		return false;
	}
	Reloaded->GetOutermost()->SetDirtyFlag(false);
	return HashFile(Filename, OutPackageHash, OutError);
}

FProjectTexturePatternManifestRecord BuildRecord(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& ObjectPath,
	const FString& PackageHash)
{
	FProjectTexturePatternManifestRecord Record;
	Record.RecipePath = Recipe.SourcePath;
	Record.RecipeSha256 = Recipe.RecipeSha256;
	Record.RecipeSourceSha256 = Recipe.RecipeSourceSha256;
	Record.PatternId = Recipe.PatternId;
	Record.Family = Recipe.Family;
	Record.Algorithm = Recipe.Algorithm;
	Record.CompilerVersion = Recipe.CompilerVersion;
	Record.OutputContract = Recipe.OutputContract;
	Record.OutputObjectPath = ObjectPath;
	Record.SemanticIdentity = FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(
		Recipe, ObjectPath);
	Record.PackageSha256 = PackageHash;
	Record.CompilerFingerprint = FProjectTexturePatternRecipeContract::GetCompilerFingerprint();
	Record.EngineCompatibility = FProjectTexturePatternRecipeContract::GetEngineCompatibilityIdentity();
	return Record;
}

bool IsAcceptedSkip(
	const FProjectTexturePatternManifestRecord* Existing,
	const FProjectTexturePatternManifestRecord& Candidate,
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
	const FProjectTextureGenerationRequest& Request,
	const TMap<FString, FProjectTexturePatternManifestRecord>& Existing,
	const TSet<FString>& AcceptedObjectPaths,
	FProjectTextureGenerationResult& OutResult)
{
	TArray<FString> Candidates;
	for (const TPair<FString, FProjectTexturePatternManifestRecord>& Pair : Existing)
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
		OutResult.Error = TEXT("Asset Registry gathering did not complete; pattern orphan cleanup refused.");
		return false;
	}
	UE_LOG(LogProjectTextureGeneration, Display,
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
			UE_LOG(LogProjectTextureGeneration, Warning,
				TEXT("[GenerationService::Orphans] Referenced pattern orphan retained - package=%s referencers=%s"),
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
				TEXT("Pattern orphan resolves outside the output root: %s"), *PackageName);
			return false;
		}
		if (!IFileManager::Get().Delete(*PackageFile, false, true, true))
		{
			OutResult.Error = FString::Printf(
				TEXT("Could not delete unreferenced pattern orphan: %s"), *PackageFile);
			return false;
		}
		UE_LOG(LogProjectTextureGeneration, Display,
			TEXT("[GenerationService::Orphans] Unreferenced pattern orphan deleted - package=%s"), *PackageName);
	}
	return true;
}
}

bool FProjectTextureGenerationService::Validate(
	const FProjectTextureGenerationRequest& Request,
	FProjectTextureGenerationResult& OutResult)
{
	return RegenerateInternal(Request, false, true, OutResult);
}

bool FProjectTextureGenerationService::RegenerateTestMount(
	const FProjectTextureGenerationRequest& Request,
	FProjectTextureGenerationResult& OutResult)
{
	return RegenerateInternal(Request, false, false, OutResult);
}

bool FProjectTextureGenerationService::RegenerateForCommandlet(
	const FProjectTextureGenerationRequest& Request,
	FProjectTextureGenerationResult& OutResult)
{
	return RegenerateInternal(Request, true, false, OutResult);
}

// Validate walks the same path as Regenerate and stops before any mutation, so it accepts only
// when a regeneration would skip every output and leave the manifest bytes unchanged.
bool FProjectTextureGenerationService::RegenerateInternal(
	const FProjectTextureGenerationRequest& Request,
	bool bCommandletOwner,
	bool bValidateOnly,
	FProjectTextureGenerationResult& OutResult)
{
	using namespace ProjectTextureGenerationPrivate;
	OutResult = {};
	if (!ValidateRequest(Request, !bValidateOnly, bCommandletOwner, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectTexturePatternRecipe> Recipes;
	if (!DiscoverRecipes(Request, Recipes, OutResult.Error))
	{
		return false;
	}
	OutResult.Validated = Recipes.Num();
	const FString ManifestPath = FPaths::Combine(Request.ManifestRoot, ManifestName);
	TMap<FString, FProjectTexturePatternManifestRecord> Existing;
	if (!ProjectTexturePatternManifest::Load(ManifestPath, Existing, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectTexturePatternManifestRecord> Accepted;
	TSet<FString> AcceptedObjectPaths;
	for (const FProjectTexturePatternRecipe& Recipe : Recipes)
	{
		FString PackageName;
		FString ObjectPath;
		if (!FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutResult.Error))
		{
			return false;
		}
		FString PackageFile;
		if (!ResolvePackageFile(PackageName, PackageFile, OutResult.Error))
		{
			return false;
		}
		FProjectTexturePatternManifestRecord Candidate = BuildRecord(Recipe, ObjectPath, TEXT("pending"));
		if (IsAcceptedSkip(Existing.Find(ObjectPath), Candidate, PackageFile))
		{
			Candidate.PackageSha256 = Existing[ObjectPath].PackageSha256;
			Accepted.Add(Candidate);
			AcceptedObjectPaths.Add(ObjectPath);
			OutResult.OutputObjectPaths.Add(ObjectPath);
			++OutResult.Skipped;
			continue;
		}
		if (bValidateOnly)
		{
			OutResult.Error = FString::Printf(TEXT("Accepted pattern is stale: %s"), *ObjectPath);
			return false;
		}
		if (FPaths::FileExists(PackageFile))
		{
			if (!Request.bHostTransactionOwnsReplacement)
			{
				OutResult.Error = FString::Printf(
					TEXT("Existing pattern replacement requires the host transaction owner: %s"),
					*ObjectPath);
				return false;
			}
			if (!IFileManager::Get().Delete(*PackageFile, false, true, true))
			{
				OutResult.Error = FString::Printf(TEXT("Could not clear existing pattern: %s"), *PackageFile);
				return false;
			}
		}
		UObject* Asset = nullptr;
		if (!ProjectTexturePatternBuilder::Build(Recipe, PackageName, Asset, OutResult.Error))
		{
			return false;
		}
		FString PackageHash;
		if (!SaveAndReload(Recipe, PackageName, ObjectPath, Asset, PackageHash, OutResult.Error))
		{
			return false;
		}
		if (Request.FailureInjection == TEXT("post-save"))
		{
			OutResult.Error = TEXT("Injected failure after pattern save and before manifest promotion.");
			return false;
		}
		Candidate = BuildRecord(Recipe, ObjectPath, PackageHash);
		Accepted.Add(Candidate);
		AcceptedObjectPaths.Add(ObjectPath);
		OutResult.OutputObjectPaths.Add(ObjectPath);
		++OutResult.Generated;
	}
	// Any accepted record change is persisted, including a source digest refresh for which every
	// asset was skipped.
	FString ExistingManifest;
	const bool bManifestCurrent = FFileHelper::LoadFileToString(ExistingManifest, *ManifestPath) &&
		ExistingManifest == ProjectTexturePatternManifest::Serialize(Accepted);
	if (bValidateOnly && !bManifestCurrent)
	{
		OutResult.Error = FString::Printf(TEXT("Accepted pattern manifest is stale: %s"), *ManifestPath);
		return false;
	}
	if (!bValidateOnly && !HandleOrphans(Request, Existing, AcceptedObjectPaths, OutResult))
	{
		return false;
	}
	if (!bManifestCurrent)
	{
		if (!ProjectTexturePatternManifest::Save(
			ManifestPath, Accepted, OutResult.ManifestSha256, OutResult.Error))
		{
			return false;
		}
	}
	else if (!HashFile(ManifestPath, OutResult.ManifestSha256, OutResult.Error))
	{
		return false;
	}
	UE_LOG(LogProjectTextureGeneration, Display,
		TEXT("[GenerationService::%s] Accepted - validated=%d generated=%d skipped=%d orphans=%d"),
		bValidateOnly ? TEXT("Validate") : TEXT("Regenerate"),
		OutResult.Validated, OutResult.Generated, OutResult.Skipped, OutResult.OrphanPackageNames.Num());
	return true;
}
