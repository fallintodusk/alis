#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ProjectManifestTypes.generated.h"

/**
 * Enumerates the broad categories of world layers we expect when streaming an open-world environment.
 * This helps downstream systems decide when to mount optional content (e.g. interior cells vs. landscape tiles).
 */
UENUM(BlueprintType)
enum class EProjectWorldLayerType : uint8
{
	Landscape UMETA(DisplayName = "Landscape"),
	Interior UMETA(DisplayName = "Interior"),
	POI UMETA(DisplayName = "Point Of Interest"),
	Landmark UMETA(DisplayName = "Landmark"),
	RuntimeGenerated UMETA(DisplayName = "Runtime Generated"),
	Other UMETA(DisplayName = "Other")
};

/**
 * Descriptor for a world-region entry in the manifest.
 * Regions describe logical groupings of World Partition cells or runtime streaming chunks.
 */
USTRUCT(BlueprintType)
struct PROJECTWORLD_API FProjectWorldRegionDescriptor
{
	GENERATED_BODY()

	/** Canonical identifier used across tooling and telemetry (e.g. "Kazan_CBD"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	FName RegionId = NAME_None;

	/** User-facing label for menus/map UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	FText DisplayName;

	/** Optional partition grid name (UE5 World Partition) for chunk selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	FName PartitionGrid = NAME_None;

	/** Optional bounding box in world space to help with runtime analytics and editor tooling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	FBoxSphereBounds Bounds = FBoxSphereBounds(EForceInit::ForceInitToZero);

	/** Layer classification describing how the region should be treated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	EProjectWorldLayerType LayerType = EProjectWorldLayerType::Landscape;

	/** Optional soft references to key assets (e.g. data layers, runtime feature toggles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	TArray<TSoftObjectPtr<UObject>> SupportingAssets;
};
