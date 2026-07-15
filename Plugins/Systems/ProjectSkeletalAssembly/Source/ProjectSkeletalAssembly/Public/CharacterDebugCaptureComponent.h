// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterDebugCaptureComponent.generated.h"

/**
 * Runtime debug capture for character parity comparison.
 *
 * Collects high-signal diagnostic state from the owning pawn and writes it
 * as structured JSON sidecar + screenshot. Designed to catch subtle bugs:
 * camera not rotating body, first-person clipping, wrong animation,
 * movement config mismatch, assembly lifecycle stall.
 *
 * Used by definition-driven characters through the DebugCapture capability.
 *
 * Console commands (registered in module startup):
 * - project.character.debug 0/1  -- toggle on-screen overlay
 * - project.character.capture [label] -- write screenshot + JSON to Saved/Validation/CharacterDebug/
 */
UCLASS(ClassGroup=(Project), meta=(BlueprintSpawnableComponent))
class PROJECTSKELETALASSEMBLY_API UCharacterDebugCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterDebugCaptureComponent();

	/** Registers as "DebugCapture" capability so FCapabilityRegistry discovers it. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("DebugCapture")));
	}

	/** Toggle on-screen debug overlay. */
	void SetOverlayEnabled(bool bEnabled);
	bool IsOverlayEnabled() const { return bShowOverlay; }

	/** Capture current state to Saved/Validation/CharacterDebug/. */
	void CaptureSnapshot(const FString& Label = TEXT(""));

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Collect all diagnostic state into a JSON object. */
	TSharedPtr<FJsonObject> CollectDiagnosticState() const;

	/** Format overlay text from diagnostic state. */
	FString FormatOverlayText(const TSharedPtr<FJsonObject>& State) const;

	/** Collect skeletal mesh component info. */
	void CollectMeshInfo(TSharedPtr<FJsonObject>& OutObj, const FString& Prefix, class USkeletalMeshComponent* Mesh) const;

	/** Collect bone world transforms for key bones. */
	void CollectBoneTransforms(TSharedPtr<FJsonObject>& OutObj, class USkeletalMeshComponent* Mesh) const;

	bool bShowOverlay = false;
};
