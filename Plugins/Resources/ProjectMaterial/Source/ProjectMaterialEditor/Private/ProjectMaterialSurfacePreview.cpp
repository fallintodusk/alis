// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfacePreview.h"
#include "ProjectMaterialPreviewAuthority.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialLivePreview, Log, All);

FProjectMaterialSurfacePreview::~FProjectMaterialSurfacePreview()
{
	FString Error;
	if (!Revert(Error)) UE_LOG(LogProjectMaterialLivePreview, Warning, TEXT("Preview recovery: %s"), *Error);
}

bool FProjectMaterialSurfacePreview::CheckTarget(UMeshComponent* Component, int32 Slot, FString& OutError) const
{
	OutError.Reset();
	if (!IsInGameThread() || IsRunningCommandlet() || !IsValid(Component) || Component->IsTemplate() ||
		Slot < 0 || Slot >= Component->GetNumMaterials())
	{
		OutError = TEXT("Preview requires a live mesh material slot on the Editor game thread.");
		return false;
	}
	if (IsActive() && (Target.Get() != Component || MaterialSlot != Slot || Component->GetMaterial(Slot) != Preview.Get()))
	{
		OutError = TEXT("Revert the active preview first; its binding may have changed externally.");
		return false;
	}
	return true;
}

bool FProjectMaterialSurfacePreview::ApplyRecipeFile(const FString& Path, const FString& RecipeRoot,
	const FString& LayoutReceipt, UMeshComponent* Component, int32 Slot, FString& OutError)
{
	if (!CheckTarget(Component, Slot, OutError)) return false;
	FProjectMaterialSurfaceRecipe Recipe;
	UMaterialInstanceConstant* Baseline = nullptr;
	if (!ProjectMaterialPreviewAuthority::Load(Path, RecipeRoot, LayoutReceipt, Recipe, Baseline, OutError)) return false;
	return ApplyValidated(Recipe, Baseline, Baseline, Component, Slot, OutError);
}

bool FProjectMaterialSurfacePreview::ApplyFixtureRecipeFile(const FString& Path, const FString& RecipeRoot,
	const FString& FixtureRoot, const FString& LayoutReceipt, UMeshComponent* Component, int32 Slot, FString& OutError)
{
	if (!CheckTarget(Component, Slot, OutError)) return false;
	FProjectMaterialSurfaceRecipe Recipe, Accepted;
	UMaterialInstanceConstant* Fixture = nullptr;
	UMaterialInstanceConstant* Baseline = nullptr;
	if (FixtureRoot.IsEmpty())
	{
		OutError = TEXT("Fixture preview requires an explicit isolated authority root.");
		return false;
	}
	if (!ProjectMaterialPreviewAuthority::Load(
		Path, RecipeRoot, LayoutReceipt, Recipe, Fixture, OutError, FixtureRoot)) return false;
	const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
	if (!Plugin.IsValid())
	{
		OutError = TEXT("Accepted material owner is unavailable.");
		return false;
	}
	const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data/SurfaceRecipes"));
	if (!ProjectMaterialPreviewAuthority::Load(FPaths::Combine(Root, TEXT("Terrain/MI_ProjectTerrain_Default.surface.json")),
		Root, LayoutReceipt, Accepted, Baseline, OutError)) return false;
	if (!FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(Recipe.Archetype) ||
		Recipe.BindingId != Accepted.BindingId || Recipe.PatternId != Accepted.PatternId ||
		Recipe.SemanticBindings != Accepted.SemanticBindings)
	{
		OutError = TEXT("Fixture substitution must preserve the accepted terrain binding and producer contracts.");
		return false;
	}
	return ApplyValidated(Recipe, Fixture, Baseline, Component, Slot, OutError);
}

bool FProjectMaterialSurfacePreview::ApplyValidated(const FProjectMaterialSurfaceRecipe& Recipe,
	UMaterialInstanceConstant* CandidateParent, UMaterialInstanceConstant* RequiredBinding,
	UMeshComponent* Component, int32 Slot, FString& OutError)
{
	UMaterialInterface* Binding = IsActive() ? Original.Get() : Component->GetMaterial(Slot);
	UMaterialInstanceDynamic* RenderInstance = Cast<UMaterialInstanceDynamic>(Binding);
	if (Binding != RequiredBinding && (!RenderInstance || RenderInstance->Parent != RequiredBinding))
	{
		OutError = TEXT("Selected renderer does not consume this accepted material instance.");
		return false;
	}
	UMaterialInstanceDynamic* Candidate = UMaterialInstanceDynamic::Create(CandidateParent, GetTransientPackage());
	if (!Candidate)
	{
		OutError = TEXT("Transient material instance creation failed.");
		return false;
	}
	if (RenderInstance) Candidate->CopyParameterOverrides(RenderInstance);
	for (const auto& Value : Recipe.Scalars) Candidate->SetScalarParameterValue(Value.Key, Value.Value);
	for (const auto& Value : Recipe.Vectors) Candidate->SetVectorParameterValue(Value.Key, Value.Value);
	if (!IsActive())
	{
		Original.Reset(Binding);
		OriginalOverrideCount = Component->OverrideMaterials.Num();
		bOriginalSlotOverridden = Component->OverrideMaterials.IsValidIndex(Slot) && Component->OverrideMaterials[Slot] != nullptr;
		PreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddLambda(
			[this](UObject*, FObjectPreSaveContext)
			{
				FString Error;
				if (!Revert(Error)) UE_LOG(LogProjectMaterialLivePreview, Warning, TEXT("Preview save recovery: %s"), *Error);
			});
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda(
			[this](UWorld*, bool, bool)
			{
				FString Error;
				if (!Revert(Error)) UE_LOG(LogProjectMaterialLivePreview, Warning, TEXT("Preview world recovery: %s"), *Error);
			});
	}
	Target = Component;
	MaterialSlot = Slot;
	Preview.Reset(Candidate);
	Component->SetMaterial(Slot, Candidate);
	return true;
}

bool FProjectMaterialSurfacePreview::Revert(FString& OutError)
{
	OutError.Reset();
	if (!IsActive()) return true;
	UMeshComponent* Component = Target.Get();
	const bool bConflict = Component && Component->GetMaterial(MaterialSlot) != Preview.Get();
	if (bConflict)
	{
		OutError = TEXT("Preview binding changed externally; recovery will not overwrite it.");
	}
	else if (Component)
	{
		Component->SetMaterial(MaterialSlot, bOriginalSlotOverridden ? Original.Get() : nullptr);
		bool bAddedTailEmpty = true;
		for (int32 Index = OriginalOverrideCount; Index < Component->OverrideMaterials.Num(); ++Index)
		{
			bAddedTailEmpty &= Component->OverrideMaterials[Index] == nullptr;
		}
		// Remove only preview padding; retain any external overrides in other slots.
		if (bAddedTailEmpty && Component->OverrideMaterials.Num() > OriginalOverrideCount)
		{
			Component->OverrideMaterials.SetNum(OriginalOverrideCount);
		}
	}
	else UE_LOG(LogProjectMaterialLivePreview, Display, TEXT("Preview target was destroyed; transient state released."));
	FCoreUObjectDelegates::OnObjectPreSave.Remove(PreSaveHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	PreSaveHandle.Reset();
	WorldCleanupHandle.Reset();
	Preview.Reset();
	Original.Reset();
	Target.Reset();
	MaterialSlot = INDEX_NONE;
	OriginalOverrideCount = 0;
	bOriginalSlotOverridden = false;
	return !bConflict;
}

bool FProjectMaterialSurfacePreview::IsActive() const
{
	return Preview.IsValid();
}
