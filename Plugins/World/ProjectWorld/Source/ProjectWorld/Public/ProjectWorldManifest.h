#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectManifestTypes.h"

#include "ProjectWorldManifest.generated.h"


/**
 * Primary data asset describing an open-world root map.
 * Works hand-in-hand with UE5's World Partition workflows.
 *
 * TODO: DIP Pattern Implementation
 * - Define IWorldService interface in ProjectCore (world management: manifest queries, world lifecycle, etc.)
 * - Create UProjectWorldSubsystem (GameInstance subsystem) that:
 *   - Implements IWorldService
 *   - Manages world manifests (query available worlds, get current world, etc.)
 *   - Registers with FProjectServiceLocator::Register<IWorldService>() in Initialize()
 *   - Unregisters in Deinitialize()
 * - See ProjectLoadingSubsystem for reference implementation pattern
 */
UCLASS(BlueprintType)
class PROJECTWORLD_API UProjectWorldManifest : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectWorldManifest();

	/** Root world asset (e.g., /Game/Project/Worlds/AlisWorld.AlisWorld). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TSoftObjectPtr<UWorld> WorldRoot;

	/** High-level description for menus and telemetry dashboards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metadata")
	FText Description;

	/** Region descriptors representing streaming groups or design-centric slices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FProjectWorldRegionDescriptor> Regions;

	/** Optional data asset describing cell->region mappings (future automation hook). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TSoftObjectPtr<UPrimaryDataAsset> PartitionMetadata;

	// UObject interface
	virtual void PostLoad() override;

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Validates manifest configuration and returns detailed error messages. */
	UFUNCTION(BlueprintCallable, Category = "Validation")
	bool Validate(TArray<FText>& OutErrors) const;
};
