// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "DefinitionActorSyncSubsystem.generated.h"

class UPrimaryDataAsset;
class UProjectObjectActorFactory;
class AActor;
class SNotificationItem;

/**
 * Syncs placed actors when definitions are regenerated.
 *
 * Subscribes to FDefinitionEvents::OnDefinitionRegenerated() from ProjectEditorCore.
 * Shows 5-second countdown before updating, with Apply Now/Cancel buttons.
 * Uses two-tier approach: Reapply (in-place) or Replace (structural changes).
 *
 * @see docs/DefinitionActorSyncSubsystem.md for architecture details
 */
UCLASS()
class PROJECTPLACEMENTEDITOR_API UDefinitionActorSyncSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEditorSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem Interface

#if WITH_DEV_AUTOMATION_TESTS
	/** Test hook: route passive-load actor processing through the real subsystem path. */
	void TestOnly_OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors)
	{
		OnActorsLoadedIntoLevel(Actors);
	}

	/** Test hook: route apply decision through the real subsystem path. */
	bool TestOnly_ApplyDefinitionToActor(AActor* Actor, UPrimaryDataAsset* Def)
	{
		return ApplyDefinitionToActor(Actor, Def);
	}

	/** Test hook: expose replace path for integration coverage. */
	AActor* TestOnly_ReplaceActorFromDefinition(AActor* OldActor, UPrimaryDataAsset* Def)
	{
		return ReplaceActorFromDefinition(OldActor, Def);
	}

	/** Test hook: register in-memory definitions for passive-load tests without AssetManager disk assets. */
	static void TestOnly_RegisterDefinitionOverride(const FPrimaryAssetId& DefinitionId, UPrimaryDataAsset* Definition);

	/** Test hook: clear in-memory definition overrides. */
	static void TestOnly_ClearDefinitionOverrides();
#endif

private:
	/** Called when a definition is regenerated */
	void OnDefinitionRegenerated(const FString& TypeName, UObject* RegeneratedAsset);

	/** Called when editor world is created - sets up level hooks */
	void OnWorldCreated(UWorld* World);

	/** Called when actors are loaded into level (World Partition / level streaming) */
	void OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors);

	/** Load definition by ID (generic - works with any definition type) */
	UPrimaryDataAsset* LoadDefinition(const FPrimaryAssetId& DefinitionId);

	/** Show notification for structural mismatch */
	void ShowStructuralMismatchNotification(AActor* Actor);

	// -------------------------------------------------------------------------
	// Actor Update Countdown Notification
	// -------------------------------------------------------------------------

	/** Show countdown notification before updating actors */
	void ShowActorUpdateCountdown(UPrimaryDataAsset* Def, int32 ActorCount);

	/** Timer tick - update countdown display */
	void OnActorUpdateCountdownTick();

	/** Cancel button clicked - abort pending actor update */
	void OnCancelActorUpdate();

	/** Apply Now button clicked - skip remaining countdown */
	void OnApplyActorUpdateNow();

	/** Execute the pending actor update */
	void ExecutePendingActorUpdate();

	/** Replace an actor with a fresh spawn from definition */
	AActor* ReplaceActorFromDefinition(AActor* OldActor, UPrimaryDataAsset* Def);

	// -------------------------------------------------------------------------
	// Update Strategy (uses IDefinitionApplicable interface)
	// -------------------------------------------------------------------------

	/**
	 * Apply definition to actor (via IDefinitionApplicable interface).
	 * Actor implements custom logic for its definition type.
	 * If actor doesn't implement interface or returns false, Replace is used.
	 *
	 * @param Actor The actor to update
	 * @param Def The updated definition to apply (UObjectDefinition)
	 * @return true if actor successfully applied definition
	 */
	bool ApplyDefinitionToActor(AActor* Actor, UPrimaryDataAsset* Def);

	/** Cached factory pointer to avoid repeated scans */
	TWeakObjectPtr<UProjectObjectActorFactory> CachedObjectFactory;

	/** Pending definition for countdown actor update */
	TWeakObjectPtr<UPrimaryDataAsset> PendingUpdateDefinition;

	/** Countdown timer for actor update */
	FTimerHandle ActorUpdateCountdownHandle;

	/** Current countdown seconds remaining */
	int32 ActorUpdateCountdownSeconds = 0;

	/** Number of actors to update (for notification display) */
	int32 PendingActorCount = 0;

	/** Actor update notification */
	TWeakPtr<SNotificationItem> ActorUpdateNotification;

	/** Delegate handle for definition regeneration */
	FDelegateHandle OnDefinitionRegeneratedHandle;

	/** Delegate handle for world creation (sets up level hooks) */
	FDelegateHandle OnWorldCreatedHandle;

	/** Delegate handle for actor load events (World Partition) */
	FDelegateHandle OnLoadedActorAddedHandle;
};
