// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldContainerKey.generated.h"

/**
 * Stable identity for meaningful world storage.
 *
 * Persistence and session code should use this instead of actor pointers,
 * labels, or other transient runtime identity.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FWorldContainerKey
{
	GENERATED_BODY()

	/** Stable per-instance identifier for the world item holder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FGuid InstanceId;

	/** Optional world, level, or partition scope identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FName WorldScopeId;

	/** Logical container slot on the owner actor (for multi-container actors). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FName ContainerSlotId = FName(TEXT("Primary"));

	bool IsValid() const
	{
		return InstanceId.IsValid() && !ContainerSlotId.IsNone();
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT("WorldContainerKey(InstanceId=%s, WorldScopeId=%s, ContainerSlotId=%s)"),
			*InstanceId.ToString(EGuidFormats::DigitsWithHyphensLower),
			*WorldScopeId.ToString(),
			*ContainerSlotId.ToString());
	}
};
