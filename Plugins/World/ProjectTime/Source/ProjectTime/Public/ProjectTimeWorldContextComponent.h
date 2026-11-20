#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectTimeWorldContextComponent.generated.h"

/** Cooked projection of validated territory time context; no clock state is saved. */
UCLASS()
class PROJECTTIME_API UProjectTimeWorldContextComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FString ZoneId;
	UPROPERTY()
	FString ProfileId;
	UPROPERTY()
	FString ProfileHash;

	bool IsValidContext() const;
	FString IntegrityRecord() const;
};
