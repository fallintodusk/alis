// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "IObjectDefinitionHost.generated.h"

/**
 * Interface for objects that host ObjectDefinition synchronization metadata.
 *
 * Can be implemented by actor classes directly or by actor components.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UObjectDefinitionHostInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTWORLD_API IObjectDefinitionHostInterface
{
	GENERATED_BODY()

public:
	/** Stable ObjectDefinition id bound to this placed actor instance. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	FPrimaryAssetId GetHostedObjectDefinitionId() const;

	/** Set ObjectDefinition id bound to this placed actor instance. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	void SetHostedObjectDefinitionId(const FPrimaryAssetId& DefinitionId);

	/** Last applied structural hash (meshes/capabilities shape). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	FString GetHostedAppliedStructureHash() const;

	/** Set last applied structural hash. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	void SetHostedAppliedStructureHash(const FString& StructureHash);

	/** Last applied content hash (full definition payload). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	FString GetHostedAppliedContentHash() const;

	/** Set last applied content hash. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Definition Host")
	void SetHostedAppliedContentHash(const FString& ContentHash);
};

