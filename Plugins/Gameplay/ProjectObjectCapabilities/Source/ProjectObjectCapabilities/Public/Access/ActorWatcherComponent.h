// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IActorWatchEventListener.h"
#include "UObject/PrimaryAssetId.h"
#include "ActorWatcherComponent.generated.h"

class ULockableComponent;

UENUM(BlueprintType)
enum class EActorWatchTrigger : uint8
{
	LockAccessDenied UMETA(DisplayName = "Lock Access Denied")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorWatchEvent, const FActorWatchEvent&, Event);

/**
 * Generic watched-actor event relay capability.
 *
 * Resolves a target actor by explicit reference and/or query filters, binds to
 * a configured trigger source, then emits normalized events with optional
 * tag/string filtering. Consumers depend on IActorWatchEventListener instead of
 * source-specific component types.
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UActorWatcherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActorWatcherComponent();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Explicit actor to watch. If unset, query filters are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Target")
	TSoftObjectPtr<AActor> WatchedActor;

	/** Optional actor tag filter for resolving watched actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Target")
	FName WatchedActorTag;

	/** Optional actor-name substring filter for watched actor resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Target")
	FString WatchedActorNameContains;

	/** Trigger source type for watched actor notifications. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Trigger")
	EActorWatchTrigger Trigger = EActorWatchTrigger::LockAccessDenied;

	/** Optional exact event name filter. Empty = accept all event names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Filter")
	FName RequiredEventName;

	/** Optional exact gameplay-tag filter. Invalid = accept all tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Filter")
	FGameplayTag RequiredEventTag;

	/** Optional substring filter on EventText (case-insensitive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorWatcher|Filter")
	FString RequiredEventTextContains;

	/** Broadcast after filters pass. */
	UPROPERTY(BlueprintAssignable, Category = "ActorWatcher")
	FOnActorWatchEvent OnWatchEvent;

	/** Resolved watched actor (bound actor preferred, then query resolution). */
	UFUNCTION(BlueprintPure, Category = "ActorWatcher")
	AActor* GetCurrentWatchedActor() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	AActor* ResolveWatchedActor() const;
	bool MatchesResolveFilters(const AActor* Candidate) const;
	bool PassesEventFilters(const FActorWatchEvent& Event) const;
	void EmitWatchEvent(const FActorWatchEvent& Event);
	void NotifyOwnerListeners(const FActorWatchEvent& Event) const;

	void TryBindToWatchedActor();
	void ScheduleCheck(float Interval);

	UFUNCTION()
	void HandleLockAccessDenied(FGameplayTag RequiredKeyTag, AActor* Instigator);

	FTimerHandle RetryHandle;
	bool bBound = false;

	// Tracks the actor currently bound for trigger callbacks.
	TWeakObjectPtr<AActor> BoundActor;
};
