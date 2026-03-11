// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MotionDrivers/SliderMotionDriver.h"

class USpringSliderComponent;

class FSliderKinematicDriver : public ISliderMotionDriver
{
public:
	explicit FSliderKinematicDriver(USpringSliderComponent& InComponent);

	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void SetTarget(float NewTarget, bool bImmediate) override;
	virtual float GetCurrent() const override;
	virtual bool Tick(const FMotionTickParams& Params) override;

private:
	float CalculateProgressFromMeshPosition() const;

	USpringSliderComponent* Component = nullptr;
};
