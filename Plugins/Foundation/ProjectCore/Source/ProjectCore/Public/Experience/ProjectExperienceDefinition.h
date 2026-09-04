// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPtr.h"
#include "ProjectExperienceDefinition.generated.h"

class UWorld;

/**
 * Concrete, configured experience record generated from its JSON source of truth by
 * ProjectDefinitionGenerator.
 *
 * This is the DATA CONTRACT only. ProjectCore performs no AssetManager work and no
 * content loading for it: ProjectLoading discovers these assets and projects them onto
 * the generic descriptor/registry it already owns.
 *
 * Adding a configured experience (another reconstructed territory, another showcase)
 * must stay a data change - one JSON file plus its generated asset - with no new C++
 * class and no registration line anywhere.
 *
 * Every field below is deliberately expressible by ProjectDefinitionGenerator's existing
 * field types, so no generator extension is required. A single scan directory and a
 * single traversal token cover the configured experiences; widen the contract only when
 * a real experience needs more, not in advance.
 */
UCLASS(BlueprintType)
class PROJECTCORE_API UProjectExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable experience identity. Matches the id the menu and registry look up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	FName ExperienceName;

	/** World this experience travels to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TSoftObjectPtr<UWorld> Map;

	/**
	 * Generic traversal policy token forwarded as the "Traversal" load-request option
	 * (for example PreviewFlight). ProjectSinglePlay owns what the token means; nothing
	 * here knows which experience selected it. Empty means the mode default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	FString TraversalMode;

	/**
	 * Package directory scanned so the AssetManager can resolve this experience's map in
	 * cooked builds. Empty means the experience needs no descriptor-specific scan.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	FString AssetScanDirectory;

	/** Assets that must be resolved before the experience is considered loadable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<TSoftObjectPtr<UObject>> CriticalAssets;

	/** Assets warmed up alongside the experience. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	TArray<TSoftObjectPtr<UObject>> WarmupAssets;

	/** True if this asset was generated from JSON (not hand-created). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	bool bGenerated = false;

	/** Generator version that created this asset. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	int32 GeneratorVersion = 0;

	/** Source JSON file path (relative to plugin). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonPath;

	/** Hash of source JSON for incremental regeneration. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonHash;

	/** A definition is usable only with a stable identity and a real map. */
	bool IsValidDefinition() const { return !ExperienceName.IsNone() && !Map.IsNull(); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
