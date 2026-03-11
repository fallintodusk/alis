// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractionService.h"
#include "Engine/PostProcessVolume.h"
#include "InteractionComponent.generated.h"

class UMaterialInterface;

/**
 * UInteractionComponent
 *
 * Attach to player pawn to enable interaction detection.
 * Performs traces to find interactable actors, tracks focus, triggers interactions.
 * Implements IInteractionComponentInterface for decoupled access from Character.
 *
 * Usage:
 *   1. Attached to default pawn class (via Blueprint or C++)
 *   2. Character calls TryInteract() via IInteractionComponentInterface
 *   3. Component broadcasts focus changes via IInteractionService (SOLID decoupling)
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class PROJECTINTERACTION_API UInteractionComponent : public UActorComponent, public IInteractionComponentInterface
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//~ IInteractionComponentInterface
	virtual bool TryInteract_Implementation() override;
	virtual AActor* GetFocusedActor_Implementation() const override { return FocusedActor.Get(); }
	virtual bool HasFocusedActor_Implementation() const override { return FocusedActor.IsValid(); }
	virtual UPrimitiveComponent* GetFocusedComponent_Implementation() const override { return FocusedComponent.Get(); }
	virtual FText GetFocusedLabel_Implementation() const override { return FocusedLabel; }

	// -------------------------------------------------------------------------
	// Server-Authoritative Interaction
	// -------------------------------------------------------------------------

	/**
	 * Server RPC for interaction. Re-traces on server (doesn't trust client's Target).
	 * Called automatically by TryInteract() when not authority.
	 */
	UFUNCTION(Server, Reliable)
	void Server_TryInteract();

protected:
	virtual void BeginPlay() override;

	/** Perform trace to find interactable actors */
	void UpdateTrace();

	/** Set new focused actor/component (handles focus/unfocus events, part-level filtering) */
	void SetFocusedActor(AActor* NewFocus, UPrimitiveComponent* HitComponent);

#if WITH_DEV_AUTOMATION_TESTS
public:
	/** Test hook for deterministic focus-selection coverage without trace/camera setup. */
	void TestOnly_SetFocusedActor(AActor* NewFocus, UPrimitiveComponent* HitComponent)
	{
		SetFocusedActor(NewFocus, HitComponent);
	}

	/**
	 * Test hook for deterministic interaction entry coverage.
	 * Executes the same actor-interface -> component-fallback routing used at runtime.
	 */
	bool TestOnly_ExecuteInteraction(AActor* Target, UPrimitiveComponent* HitComponent, AActor* OverrideInstigator = nullptr);
#endif

public:
	/** Max distance for interaction detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float TraceDistance = 300.0f;

	/** Trace channel for interaction detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Sphere trace radius (0 = line trace) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float TraceRadius = 20.0f;

	/** Debug draw traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = true;

	/** Trace every N frames (6 = ~10 traces/sec at 60fps) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Performance", meta = (ClampMin = "1", ClampMax = "60"))
	int32 TraceFrameInterval = 6;

	/** Enable outline highlight on focused actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Highlight")
	bool bEnableHighlight = true;

	/** Post-process material for outline effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Highlight")
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial;

private:
	/** Broadcast focus change through IInteractionService (for HUD prompts). */
	void BroadcastFocusChangedToService();
	/**
	 * Server-authoritative interaction execution.
	 * Re-traces to find target, validates, then broadcasts.
	 * Called by both local authority path and Server_TryInteract RPC.
	 */
	void ExecuteInteraction_ServerAuth();

	/** Currently focused interactable actor */
	TWeakObjectPtr<AActor> FocusedActor;

	/** Currently focused component (mesh to highlight) */
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;

	/** Current interaction label (e.g., "Open", "Close", "Interact") */
	FText FocusedLabel;

	/** Cached trace start (updated each tick) */
	FVector TraceStart;

	/** Cached trace end (updated each tick) */
	FVector TraceEnd;

	/** Frame counter for trace interval */
	int32 FrameCounter = 0;

	/** Setup post-process material on camera */
	void SetupPostProcess();

	/** Enable/disable custom depth rendering on component */
	void SetComponentCustomDepth(UPrimitiveComponent* Component, bool bEnable);

	/** Cached camera component for PP settings */
	TWeakObjectPtr<class UCameraComponent> CachedCamera;

	/** Loaded outline material instance */
	UPROPERTY()
	UMaterialInterface* LoadedOutlineMaterial = nullptr;

	/** Whether PP is currently set up */
	bool bPostProcessReady = false;
};
