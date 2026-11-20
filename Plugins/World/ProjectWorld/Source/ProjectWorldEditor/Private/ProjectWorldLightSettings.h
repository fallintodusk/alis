#pragma once

#include "ProjectLightSettings.h"
#include "ProjectWorldPresentationProfile.h"

namespace ProjectWorldLightSettings
{
	inline FProjectLightSettings FromProfile(const FProjectWorldPresentationProfile& Profile)
	{
		FProjectLightSettings Value;
		Value.SunRotation = Profile.SunRotation;
		Value.SunIntensityLux = Profile.SunIntensityLux;
		Value.SkyLightIntensity = Profile.SkyLightIntensity;
		Value.FogDensity = Profile.FogDensity;
		Value.FogHeightFalloff = Profile.FogHeightFalloff;
		Value.FixedExposureEv100 = Profile.FixedExposureEv100;
		Value.bHistogram = Profile.GameplayHistogramExposure.IsSet();
		if (Value.bHistogram)
		{
			const auto& Histogram = Profile.GameplayHistogramExposure.GetValue();
			Value.MinEv100 = Histogram.MinEv100;
			Value.MaxEv100 = Histogram.MaxEv100;
			Value.HistogramMinEv100 = Histogram.HistogramMinEv100;
			Value.HistogramMaxEv100 = Histogram.HistogramMaxEv100;
			Value.ExposureSpeedUp = Histogram.SpeedUp;
			Value.ExposureSpeedDown = Histogram.SpeedDown;
		}
		return Value;
	}
}
