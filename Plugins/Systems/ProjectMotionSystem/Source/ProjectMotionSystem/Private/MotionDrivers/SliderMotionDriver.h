// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MotionDrivers/MotionDriverTypes.h"

class ISliderMotionDriver
{
public:
	virtual ~ISliderMotionDriver() = default;

	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void SetTarget(float NewTarget, bool bImmediate) = 0;
	virtual float GetCurrent() const = 0;
	virtual bool Tick(const FMotionTickParams& Params) = 0;
};
