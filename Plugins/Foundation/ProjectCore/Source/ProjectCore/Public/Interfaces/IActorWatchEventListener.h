// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IActorWatchEventListener.generated.h"

class AActor;

/**
 * Generic watch event payload emitted by actor watcher capabilities.
 *
 * EventName is the stable semantic signal (for example "lock.access_denied").
 * EventTag and EventText provide optional tag/string filters without forcing
 * every consumer to know source-specific component types.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FActorWatchEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatch")
	FName EventName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatch")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatch")
	FString EventText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatch")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatch")
	TObjectPtr<AActor> Instigator = nullptr;
};

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class PROJECTCORE_API UActorWatchEventListener : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IActorWatchEventListener
{
	GENERATED_BODY()

public:
	virtual void HandleActorWatchEvent(const FActorWatchEvent& Event) = 0;
};
