// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MotionDrivers/SliderMotionDriver.h"
#include "TimerManager.h"

class UBoxComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USpringSliderComponent;

class FSliderChaosDriver : public ISliderMotionDriver
{
public:
	explicit FSliderChaosDriver(USpringSliderComponent& InComponent);

	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void SetTarget(float NewTarget, bool bImmediate) override;
	virtual float GetCurrent() const override;
	virtual bool Tick(const FMotionTickParams& Params) override;

private:
	void SetupConstraint();
	void ScheduleRetry(const TCHAR* Reason);
	void CancelRetry();
	void RetrySetup();
	void DestroyConstraint();

	void SetMotorEnabled(bool bEnable);

	USpringSliderComponent* Component = nullptr;

	TWeakObjectPtr<UPhysicsConstraintComponent> Constraint;
	TWeakObjectPtr<UBoxComponent> AnchorBox;
	TWeakObjectPtr<UPrimitiveComponent> CachedAnchorComp;

	FVector CachedSlideAxisWorld = FVector::ForwardVector;
	float CurrentTargetPosition = 0.0f;

	FTimerHandle RetryHandle;
	int32 RetryAttempts = 0;
	bool bPendingRetry = false;
	bool bShuttingDown = false;

	static constexpr int32 MaxRetryAttempts = 10;
	static constexpr float RetryDelaySeconds = 0.1f;
};
