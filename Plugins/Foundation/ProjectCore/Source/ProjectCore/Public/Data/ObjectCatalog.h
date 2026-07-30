// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectCatalog.generated.h"

/**
 * Catalog of ObjectDefinitions used by a world/mode.
 * Lives in ProjectCore so world plugins can define catalogs without depending on Inventory.
 */
UCLASS(BlueprintType)
class PROJECTCORE_API UObjectCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** ObjectDefinitions to preload for this mode/world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog", meta = (AllowedTypes = "ObjectDefinition"))
	TArray<FPrimaryAssetId> Objects;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
