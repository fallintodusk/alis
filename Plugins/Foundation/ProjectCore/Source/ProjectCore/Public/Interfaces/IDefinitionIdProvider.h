// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "IDefinitionIdProvider.generated.h"

/**
 * IDefinitionIdProvider
 *
 * Interface for actors that expose a data-driven ObjectDefinition id.
 * Used to decouple capability components from concrete actor classes.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UDefinitionIdProvider : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IDefinitionIdProvider
{
	GENERATED_BODY()

public:
	/** ObjectDefinition asset id for this actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition")
	FPrimaryAssetId GetObjectDefinitionId() const;
};
