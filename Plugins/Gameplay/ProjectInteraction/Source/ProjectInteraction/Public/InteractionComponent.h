// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IInteractionService.h"
#include "Engine/PostProcessVolume.h"
#include "TimerManager.h"
#include "InteractionComponent.generated.h"

class APawn;
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
	virtual bool BeginInteractInput_Implementation() override;
	virtual void EndInteractInput_Implementation() override;
	virtual AActor* GetFocusedActor_Implementation() const override { return FocusedActor.Get(); }
	virtual bool HasFocusedActor_Implementation() const override { return FocusedActor.IsValid(); }
	virtual UPrimitiveComponent* GetFocusedComponent_Implementation() const override { return FocusedComponent.Get(); }
	virtual FText GetFocusedLabel_Implementation() const override { return FocusedLabel; }
	virtual FInteractionPromptState GetInteractionPromptState_Implementation() const override;

	/** Starts local-only presentation targeting/highlight once possession/local control is valid. */
	void ActivateLocalPresentationIfNeeded();

	// -------------------------------------------------------------------------
	// Server-Authoritative Interaction
	// -------------------------------------------------------------------------

	/**
	 * Server RPC for interaction. Carries the client's currently focused
	 * (Actor, Component) - the same target that was highlighted by the local
	 * targeting resolver. The server validates plausibility (interactable,
	 * within range) but does NOT re-resolve a different target: highlight
	 * and interaction share a single source of truth on the client.
	 *
	 * Called automatically by TryInteract() when not authority.
	 */
	UFUNCTION(Server, Reliable)
	void Server_TryInteract(AActor* TargetActor, UPrimitiveComponent* TargetComponent);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Perform trace to find interactable actors */
	void UpdateTrace();

	/** Resolve from an explicit view and apply focus/hysteresis side effects. */
	bool UpdateFocusFromView(const FVector& ViewOrigin, const FVector& ViewForward);

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

	/**
	 * Test hook for deterministic targeting coverage using an explicit view.
	 * Runs the same slice-1 resolver used by UpdateTrace without touching focus state.
	 */
	bool TestOnly_ResolveBestInteractionTarget(
		const FVector& ViewOrigin,
		const FVector& ViewForward,
		AActor*& OutActor,
		UPrimitiveComponent*& OutHitComponent) const;

	/** Test hook for resolver-to-focus coverage with hysteresis applied. */
	bool TestOnly_UpdateFocusFromView(const FVector& ViewOrigin, const FVector& ViewForward);
#endif

public:
	/**
	 * Default tuning values - single source of truth for both the per-component
	 * UPROPERTY defaults below and the resolver-side `FInteractionTargetingWeights`
	 * struct in `InteractionTargetResolver.h`. Designers override per-pawn via
	 * Blueprint; runtime code MUST NOT bake new defaults elsewhere.
	 */
	static constexpr float DefaultInteractionRadius = 200.0f;
	static constexpr float DefaultShortCircuitRadius = 60.0f;
	static constexpr float DefaultMinAimDot = 0.85f;
	static constexpr float DefaultFocusSwitchHysteresis = 0.10f;

	/** Sphere radius for overlap-based interaction candidate gathering. ~2 m: arm's
	 *  reach plus enough margin for floor pickups when the player bends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractionRadius = DefaultInteractionRadius;

	/** Within this distance LOS is bypassed, but the aim gate still applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float ShortCircuitRadius = DefaultShortCircuitRadius;

	/** Minimum forward-dot required for a candidate to remain focusable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "-1.0", ClampMax = "0.999"))
	float MinAimDot = DefaultMinAimDot;

	/**
	 * Anti-flicker hysteresis applied within a single bucket - both view-ray-hit
	 * (collision or bounds intersection) or both fallback (closest-point only).
	 * Within a bucket the incumbent keeps focus unless the challenger beats it on
	 * the relevant discriminator (`ViewRayHitDistance` for view-ray-hit, `AimDot`
	 * for fallback) by more than this fraction. Cross-bucket switches are deliberate
	 * aim transitions and are never smoothed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float FocusSwitchHysteresis = DefaultFocusSwitchHysteresis;

	/** Trace channel for interaction detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Debug draw traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = true;

	/** Passive focus refresh cadence. Input refreshes immediately before interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Performance", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float TraceIntervalSeconds = 0.15f;

	/** Enable outline highlight on focused actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Highlight")
	bool bEnableHighlight = true;

	/** Post-process material for outline effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Highlight")
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial;

private:
	void DrawInteractionDebugTraceOnInput();

	UFUNCTION()
	void HandlePawnRestarted(APawn* Pawn);

	/** Broadcast focus change through IInteractionService (for HUD prompts). */
	void BroadcastFocusChangedToService();
	void BroadcastPromptStateToService() const;
	FInteractionPromptState BuildPromptState() const;
	void CancelHoldInteraction();
	void CompleteHoldInteraction();
	/**
	 * Server-authoritative interaction execution.
	 *
	 * The client's targeting resolver is the single source of truth for what
	 * the player is pointing at - highlighting and interaction MUST converge on
	 * the same primitive, otherwise the player sees a drawer outlined and a
	 * different drawer opens (the dresser regression). This function therefore
	 * does NOT re-run the resolver. It validates the (Target, HitComponent) the
	 * caller passed (anti-cheat: target is non-null, interactable, within
	 * reasonable range from the pawn) and then dispatches via actor interface
	 * or capability selector.
	 *
	 * Called by both the local authority path (`TryInteract`) and the
	 * `Server_TryInteract` RPC.
	 */
	void ExecuteInteraction_ServerAuth(AActor* Target, UPrimitiveComponent* HitComponent);

	/** Common dispatch: route a focused target to local-auth or RPC path. */
	bool DispatchInteract(AActor* Target, UPrimitiveComponent* HitComponent);

	/** Currently focused interactable actor */
	TWeakObjectPtr<AActor> FocusedActor;

	/** Currently focused component (mesh to highlight) */
	TWeakObjectPtr<UPrimitiveComponent> FocusedComponent;

	/** Current interaction label (e.g., "Open", "Close", "Interact") */
	FText FocusedLabel;

	/** Current interaction execution behavior for the focused target. */
	FInteractionExecutionSpec FocusedExecutionSpec;

	/** Cached trace start (updated by the passive trace timer or input refresh) */
	FVector TraceStart;

	/** Cached trace end (updated by the passive trace timer or input refresh) */
	FVector TraceEnd;

	/** Timer for passive focus refresh. */
	FTimerHandle PassiveTraceTimerHandle;

	/** True while interact input is held for a timed interaction. */
	bool bHoldInteractionActive = false;

	/** Current timed interaction progress (0..1). */
	float HoldInteractionProgress = 0.0f;

	/** World time when the current timed interaction started. */
	float HoldInteractionStartTime = 0.0f;

	/** Focus captured when the current timed interaction started. */
	TWeakObjectPtr<AActor> HoldTargetActor;
	TWeakObjectPtr<UPrimitiveComponent> HoldTargetComponent;

	/** Setup post-process material on camera */
	void SetupPostProcess();

	/** Start or restart passive focus refresh on a world timer. */
	void StartPassiveTraceTimer();

	/** Passive focus/highlight state is local-presentation work, not server/remote-pawn work. */
	bool ShouldRunPassiveFocus() const;

	/** Enable ticking only while hold progress or post-process retry needs it. */
	void RefreshComponentTickEnabled();

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
