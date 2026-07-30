// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EnvironmentEffectComponent.generated.h"

class UShapeComponent;

/**
 * Environmental hazard capability - applies GAS magnitudes on overlap.
 *
 * Attach to any actor that already owns a UShapeComponent trigger.
 * The capability binds to that trigger's overlap events and applies
 * Entry/Periodic/Exit magnitude sets to overlapping actors via
 * UProjectGASLibrary::ApplyMagnitudes().
 *
 * Data-driven: all behavior configured via properties (JSON or editor).
 * No per-hazard subclass needed.
 *
 * Examples:
 * - Sniper zone: Entry {Condition -10, Bleeding +0.1}, one-shot persistent
 * - Fire area: Periodic {Condition -5} every 1.0s while inside
 * - Radiation: Entry {Radiation +0.3}, Periodic {Condition -1} every 1.0s
 *
 * Consumed by: ProjectGAS (ApplyMagnitudes at runtime)
 * Data contract: TMap<FGameplayTag, float> from ProjectCore
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UEnvironmentEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnvironmentEffectComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	// -------------------------------------------------------------------------
	// Trigger
	// -------------------------------------------------------------------------

	// Name of the UShapeComponent on the owning actor to bind overlap events to.
	// Must be a real UShapeComponent (Box, Sphere, Capsule). No mesh collision fallback.
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	FName TriggerComponentName = TEXT("Trigger");

	// -------------------------------------------------------------------------
	// Magnitude Payloads
	// -------------------------------------------------------------------------

	// Applied once on begin overlap
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	TMap<FGameplayTag, float> EntryMagnitudes;

	// Applied every TickInterval while actor remains inside
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	TMap<FGameplayTag, float> PeriodicMagnitudes;

	// Applied once on end overlap
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	TMap<FGameplayTag, float> ExitMagnitudes;

	// -------------------------------------------------------------------------
	// Timing & Limits
	// -------------------------------------------------------------------------

	// Seconds between periodic applications (0 = no periodic, entry-only)
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect", meta = (ClampMin = "0.0"))
	float TickInterval = 0.0f;

	// Max total applications per actor (0 = unlimited).
	// Counts both entry and periodic applications.
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect", meta = (ClampMin = "0"))
	int32 MaxApplications = 0;

	// When true, MaxApplications persists across re-entries (sniper: once ever).
	// When false, application count resets each overlap session (fire: repeatable).
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	bool bPersistentApplicationLimit = false;

	// When true, only APawn subclasses trigger the effect
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect")
	bool bAffectPawnsOnly = true;

	// -------------------------------------------------------------------------
	// Actions (capability cross-communication)
	// -------------------------------------------------------------------------

	// Action string dispatched to IProjectActionReceiver on entry and each periodic tick.
	// Format: "namespace.command:argument" (e.g., "audio.play:sniper_shot")
	// Audio capability on the same actor receives this and plays spatial sound.
	UPROPERTY(EditAnywhere, Category = "EnvironmentEffect|Actions")
	FString EntryAction;

private:
	// -------------------------------------------------------------------------
	// Overlap Handlers
	// -------------------------------------------------------------------------

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// -------------------------------------------------------------------------
	// Internal Logic
	// -------------------------------------------------------------------------

	// Apply a magnitude map to the target actor's ASC
	bool ApplyMagnitudes(AActor* TargetActor, const TMap<FGameplayTag, float>& Magnitudes);

	// Periodic tick for a specific actor
	void HandlePeriodicTick(TWeakObjectPtr<AActor> WeakActor);

	// Check if actor is valid target (pawn filter, alive, etc.)
	bool IsValidTarget(AActor* Actor) const;

	void ValidateConfig() const;

	// -------------------------------------------------------------------------
	// Per-Actor State
	// -------------------------------------------------------------------------

	struct FEffectState
	{
		int32 ApplicationCount = 0;
		FTimerHandle PeriodicTimerHandle;
	};

	// Active overlaps (cleared on EndOverlap)
	TMap<TWeakObjectPtr<AActor>, FEffectState> ActiveActors;

	// Actors that already hit MaxApplications (survives EndOverlap, cleared on EndPlay)
	TSet<TWeakObjectPtr<AActor>> PermanentAppliedActors;

	// Set to true in BeginPlay if trigger is found and bound
	bool bIsEffectEnabled = false;

	// Dispatch EntryAction to all IProjectActionReceiver on the owning actor
	void DispatchAction();
};
