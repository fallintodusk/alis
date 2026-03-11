// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MotionDrivers/RotatorMotionDriver.h"

class USpringRotatorComponent;

class FRotatorKinematicDriver : public IRotatorMotionDriver
{
public:
	explicit FRotatorKinematicDriver(USpringRotatorComponent& InComponent);

	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void SetTarget(float NewTarget, bool bImmediate) override;
	virtual float GetCurrent() const override;
	virtual bool Tick(const FMotionTickParams& Params) override;

private:
	float CalculateAngleFromMeshRotation() const;

	USpringRotatorComponent* Component = nullptr;
};
