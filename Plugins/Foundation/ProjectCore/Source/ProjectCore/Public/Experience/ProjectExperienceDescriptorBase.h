// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "ProjectExperienceDescriptorBase.generated.h"

struct FLoadRequest;

/** Declarative scan spec for AssetManager registration (data-only). */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FExperienceAssetScanSpec
{
	GENERATED_BODY()

	/** Primary asset type name (e.g., "Map"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	FName PrimaryAssetType;

	/** Base class filter for the asset type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TSubclassOf<UObject> BaseClass;

	/** Directories to scan (e.g., "/MainMenuWorld/Maps"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<FString> Directories;

	/** Whether assets may have blueprint classes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	bool bHasBlueprintClasses = false;

	/** Whether assets are editor-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	bool bIsEditorOnly = false;

	/** Whether to force synchronous scan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	bool bForceSynchronousScan = true;
};

/**
 * Soft-reference sets for an experience/world load.
 * Data-only: ProjectLoading is responsible for resolving these into primary asset IDs.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FExperienceLoadAssets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<FSoftObjectPath> CriticalAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<FSoftObjectPath> WarmupAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<FSoftObjectPath> BackgroundAssets;
};

/**
 * Base descriptor for experience/world loading.
 * Lives in ProjectCore so plugins can declare data without depending on ProjectLoading.
 * ProjectLoading consumes these descriptors and performs AssetManager resolution.
 */
UCLASS(Abstract, BlueprintType)
class PROJECTCORE_API UProjectExperienceDescriptorBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience")
	FName ExperienceName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience")
	FExperienceLoadAssets LoadAssets;

	/**
	 * Populate an FLoadRequest with descriptor data (soft paths only).
	 * AssetManager resolution is performed by ProjectLoading.
	 */
	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const;

	/** Provide AssetManager scan specs (data-only). Default: none. */
	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const {}
};
