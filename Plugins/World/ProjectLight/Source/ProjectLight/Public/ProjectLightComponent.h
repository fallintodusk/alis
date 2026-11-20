#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectLightSettings.h"
#include "ProjectLightComponent.generated.h"

/** Explicitly composed runtime controller; does not discover a clock or spawn actors. */
UCLASS()
class PROJECTLIGHT_API UProjectLightComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UProjectLightComponent();
	UPROPERTY() FProjectLightSettings Settings;
	UPROPERTY() FProjectLightBindings Bindings;

	bool Start(UObject& TimeView, double Latitude, double Longitude,
		const FVector& East, const FVector& North, const FVector& Up, FString& OutError);
	void Stop();
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
	uint64 GetApplyCount() const { return ApplyCount; }
	uint64 GetLastApplyFrame() const { return LastApplyFrame; }

private:
	void RequestApply();
	void OnTimeChanged();
	void BeginLightingRecovery();
	void EndLightingRecovery();
	bool ApplyCurrent();
	TWeakObjectPtr<UObject> Provider;
	FDelegateHandle ChangedHandle;
	FTimerHandle Timer;
	FTimerHandle LightingRecoveryTimer;
	TWeakObjectPtr<APostProcessVolume> RecoveryVolume;
	float PreviousSceneUpdateSpeed = 0;
	bool bPreviousSceneUpdateOverride = false;
	bool bClockChangePending = false;
	FVector EastVector, NorthVector, UpVector;
	double LatitudeValue = 0, LongitudeValue = 0;
	int64 LastUtcTicks = -1;
	float CurrentInterval = -1;
	uint64 ApplyCount = 0, LastApplyFrame = 0;
};
