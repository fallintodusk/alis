// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialInstanceConstant;
struct FProjectMaterialSurfaceRecipe;

// One explicitly selected live binding; never saves or modifies generated authority.
class PROJECTMATERIALEDITOR_API FProjectMaterialSurfacePreview final
{
public:
	FProjectMaterialSurfacePreview() = default;
	FProjectMaterialSurfacePreview(const FProjectMaterialSurfacePreview&) = delete;
	FProjectMaterialSurfacePreview& operator=(const FProjectMaterialSurfacePreview&) = delete;
	~FProjectMaterialSurfacePreview();
	bool ApplyRecipeFile(const FString& Path, const FString& RecipeRoot,
		const FString& LayoutReceipt, UMeshComponent* Component, int32 Slot, FString& OutError);
	bool ApplyFixtureRecipeFile(const FString& Path, const FString& RecipeRoot, const FString& FixtureRoot,
		const FString& LayoutReceipt, UMeshComponent* Component, int32 Slot, FString& OutError,
		const FString& PatternTestRoot = FString());
	bool Revert(FString& OutError);
	bool IsActive() const;

private:
	bool CheckTarget(UMeshComponent* Component, int32 Slot, FString& OutError) const;
	bool ApplyValidated(const FProjectMaterialSurfaceRecipe& Recipe, UMaterialInstanceConstant* CandidateParent,
		UMaterialInstanceConstant* RequiredBinding, UMeshComponent* Component, int32 Slot, FString& OutError);
	TWeakObjectPtr<UMeshComponent> Target;
	TStrongObjectPtr<UMaterialInterface> Original;
	TStrongObjectPtr<UMaterialInstanceDynamic> Preview;
	int32 MaterialSlot = INDEX_NONE;
	int32 OriginalOverrideCount = 0;
	bool bOriginalSlotOverridden = false;
	FDelegateHandle PreSaveHandle;
	FDelegateHandle WorldCleanupHandle;
};
