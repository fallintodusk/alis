// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MotionDrivers/RotatorMotionDriver.h"
#include "TimerManager.h"

class UBoxComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USpringRotatorComponent;

class FRotatorChaosDriver : public IRotatorMotionDriver
{
public:
	explicit FRotatorChaosDriver(USpringRotatorComponent& InComponent);

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

	USpringRotatorComponent* Component = nullptr;

	TWeakObjectPtr<UPhysicsConstraintComponent> Constraint;
	TWeakObjectPtr<UBoxComponent> AnchorBox;
	TWeakObjectPtr<UPrimitiveComponent> CachedAnchorComp;

	float CurrentTargetAngle = 0.0f;

	FTimerHandle RetryHandle;
	int32 RetryAttempts = 0;
	bool bPendingRetry = false;
	bool bShuttingDown = false;

	static constexpr int32 MaxRetryAttempts = 10;
	static constexpr float RetryDelaySeconds = 0.1f;
};
