// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/WorldContainerKey.h"
#include "ContainerSessionTypes.generated.h"

/**
 * Interaction mode for a world-container session.
 */
UENUM(BlueprintType)
enum class EContainerSessionMode : uint8
{
	QuickLoot UMETA(DisplayName = "Quick Loot"),
	FullOpen UMETA(DisplayName = "Full Open")
};

/**
 * Optional side enum for dual-owner container routing.
 */
UENUM(BlueprintType)
enum class EContainerSessionSide : uint8
{
	Player UMETA(DisplayName = "Player"),
	World UMETA(DisplayName = "World")
};

/**
 * Lightweight handle for an opened world-container session.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FContainerSessionHandle
{
	GENERATED_BODY()

	/** Unique runtime session id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContainerSession")
	FGuid SessionId;

	/** Stable identity of the world-side container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContainerSession")
	FWorldContainerKey ContainerKey;

	/** Interaction mode used to open this session. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ContainerSession")
	EContainerSessionMode Mode = EContainerSessionMode::QuickLoot;

	bool IsValid() const
	{
		return SessionId.IsValid() && ContainerKey.IsValid();
	}

	void Reset()
	{
		SessionId.Invalidate();
		ContainerKey = FWorldContainerKey();
		Mode = EContainerSessionMode::QuickLoot;
	}
};

FORCEINLINE bool operator==(const FContainerSessionHandle& Left, const FContainerSessionHandle& Right)
{
	return Left.SessionId == Right.SessionId;
}
