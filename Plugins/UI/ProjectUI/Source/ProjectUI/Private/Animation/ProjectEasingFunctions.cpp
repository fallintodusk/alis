// Copyright ALIS. All Rights Reserved.

#include "Animation/ProjectEasingFunctions.h"
#include "Kismet/KismetMathLibrary.h"

// Use Unreal Engine's math constants to avoid macro redefinition warnings
#define EASING_PI UE_PI
#define EASING_HALF_PI UE_HALF_PI
#define EASING_TWO_PI (UE_PI * 2.0)

float UProjectEasingFunctions::Ease(float Time, EProjectEasingType EasingType)
{
	// Clamp input to valid range
	Time = FMath::Clamp(Time, 0.0f, 1.0f);

	switch (EasingType)
	{
	case EProjectEasingType::Linear:       return Linear(Time);
	case EProjectEasingType::EaseIn:       return EaseIn(Time);
	case EProjectEasingType::EaseOut:      return EaseOut(Time);
	case EProjectEasingType::EaseInOut:    return EaseInOut(Time);
	case EProjectEasingType::CubicIn:      return CubicIn(Time);
	case EProjectEasingType::CubicOut:     return CubicOut(Time);
	case EProjectEasingType::CubicInOut:   return CubicInOut(Time);
	case EProjectEasingType::QuartIn:      return QuartIn(Time);
	case EProjectEasingType::QuartOut:     return QuartOut(Time);
	case EProjectEasingType::QuartInOut:   return QuartInOut(Time);
	case EProjectEasingType::SineIn:       return SineIn(Time);
	case EProjectEasingType::SineOut:      return SineOut(Time);
	case EProjectEasingType::SineInOut:    return SineInOut(Time);
	case EProjectEasingType::ExpoIn:       return ExpoIn(Time);
	case EProjectEasingType::ExpoOut:      return ExpoOut(Time);
	case EProjectEasingType::ExpoInOut:    return ExpoInOut(Time);
	case EProjectEasingType::CircIn:       return CircIn(Time);
	case EProjectEasingType::CircOut:      return CircOut(Time);
	case EProjectEasingType::CircInOut:    return CircInOut(Time);
	case EProjectEasingType::ElasticIn:    return ElasticIn(Time);
	case EProjectEasingType::ElasticOut:   return ElasticOut(Time);
	case EProjectEasingType::ElasticInOut: return ElasticInOut(Time);
	case EProjectEasingType::BounceIn:     return BounceIn(Time);
	case EProjectEasingType::BounceOut:    return BounceOut(Time);
	case EProjectEasingType::BounceInOut:  return BounceInOut(Time);
	case EProjectEasingType::BackIn:       return BackIn(Time);
	case EProjectEasingType::BackOut:      return BackOut(Time);
	case EProjectEasingType::BackInOut:    return BackInOut(Time);
	default:                               return Linear(Time);
	}
}

float UProjectEasingFunctions::EaseInOut(float Time)
{
	return Time < 0.5f
		? 2.0f * Time * Time
		: -1.0f + (4.0f - 2.0f * Time) * Time;
}

float UProjectEasingFunctions::CubicOut(float Time)
{
	const float f = Time - 1.0f;
	return f * f * f + 1.0f;
}

float UProjectEasingFunctions::CubicInOut(float Time)
{
	return Time < 0.5f
		? 4.0f * Time * Time * Time
		: 1.0f + (Time - 1.0f) * (2.0f * (Time - 1.0f)) * (2.0f * (Time - 1.0f));
}

float UProjectEasingFunctions::QuartOut(float Time)
{
	const float f = Time - 1.0f;
	return 1.0f - f * f * f * f;
}

float UProjectEasingFunctions::QuartInOut(float Time)
{
	if (Time < 0.5f)
	{
		return 8.0f * Time * Time * Time * Time;
	}
	else
	{
		const float f = Time - 1.0f;
		return 1.0f - 8.0f * f * f * f * f;
	}
}

float UProjectEasingFunctions::SineIn(float Time)
{
	return 1.0f - FMath::Cos(Time * EASING_HALF_PI);
}

float UProjectEasingFunctions::SineOut(float Time)
{
	return FMath::Sin(Time * EASING_HALF_PI);
}

float UProjectEasingFunctions::SineInOut(float Time)
{
	return 0.5f * (1.0f - FMath::Cos(EASING_PI * Time));
}

float UProjectEasingFunctions::ExpoIn(float Time)
{
	return Time == 0.0f ? 0.0f : FMath::Pow(2.0f, 10.0f * (Time - 1.0f));
}

float UProjectEasingFunctions::ExpoOut(float Time)
{
	return Time == 1.0f ? 1.0f : 1.0f - FMath::Pow(2.0f, -10.0f * Time);
}

float UProjectEasingFunctions::ExpoInOut(float Time)
{
	if (Time == 0.0f || Time == 1.0f)
	{
		return Time;
	}

	if (Time < 0.5f)
	{
		return 0.5f * FMath::Pow(2.0f, 20.0f * Time - 10.0f);
	}
	else
	{
		return 1.0f - 0.5f * FMath::Pow(2.0f, -20.0f * Time + 10.0f);
	}
}

float UProjectEasingFunctions::CircIn(float Time)
{
	return 1.0f - FMath::Sqrt(1.0f - Time * Time);
}

float UProjectEasingFunctions::CircOut(float Time)
{
	const float f = Time - 1.0f;
	return FMath::Sqrt(1.0f - f * f);
}

float UProjectEasingFunctions::CircInOut(float Time)
{
	if (Time < 0.5f)
	{
		return 0.5f * (1.0f - FMath::Sqrt(1.0f - 4.0f * Time * Time));
	}
	else
	{
		const float f = 2.0f * Time - 2.0f;
		return 0.5f * (FMath::Sqrt(1.0f - f * f) + 1.0f);
	}
}

float UProjectEasingFunctions::ElasticIn(float Time)
{
	if (Time == 0.0f || Time == 1.0f)
	{
		return Time;
	}

	const float p = 0.3f;
	const float s = p / 4.0f;
	const float PostFix = FMath::Pow(2.0f, 10.0f * (Time - 1.0f));

	return -(PostFix * FMath::Sin((Time - 1.0f - s) * EASING_TWO_PI / p));
}

float UProjectEasingFunctions::ElasticOut(float Time)
{
	if (Time == 0.0f || Time == 1.0f)
	{
		return Time;
	}

	const float p = 0.3f;
	const float s = p / 4.0f;

	return FMath::Pow(2.0f, -10.0f * Time) * FMath::Sin((Time - s) * EASING_TWO_PI / p) + 1.0f;
}

float UProjectEasingFunctions::ElasticInOut(float Time)
{
	if (Time == 0.0f || Time == 1.0f)
	{
		return Time;
	}

	Time *= 2.0f;
	const float p = 0.45f;
	const float s = p / 4.0f;

	if (Time < 1.0f)
	{
		const float PostFix = FMath::Pow(2.0f, 10.0f * (Time - 1.0f));
		return -0.5f * (PostFix * FMath::Sin((Time - 1.0f - s) * EASING_TWO_PI / p));
	}
	else
	{
		const float PostFix = FMath::Pow(2.0f, -10.0f * (Time - 1.0f));
		return PostFix * FMath::Sin((Time - 1.0f - s) * EASING_TWO_PI / p) * 0.5f + 1.0f;
	}
}

float UProjectEasingFunctions::BounceOut(float Time)
{
	if (Time < 1.0f / 2.75f)
	{
		return 7.5625f * Time * Time;
	}
	else if (Time < 2.0f / 2.75f)
	{
		const float PostFix = Time - (1.5f / 2.75f);
		return 7.5625f * PostFix * PostFix + 0.75f;
	}
	else if (Time < 2.5f / 2.75f)
	{
		const float PostFix = Time - (2.25f / 2.75f);
		return 7.5625f * PostFix * PostFix + 0.9375f;
	}
	else
	{
		const float PostFix = Time - (2.625f / 2.75f);
		return 7.5625f * PostFix * PostFix + 0.984375f;
	}
}

float UProjectEasingFunctions::BounceIn(float Time)
{
	return 1.0f - BounceOut(1.0f - Time);
}

float UProjectEasingFunctions::BounceInOut(float Time)
{
	if (Time < 0.5f)
	{
		return BounceIn(Time * 2.0f) * 0.5f;
	}
	else
	{
		return BounceOut(Time * 2.0f - 1.0f) * 0.5f + 0.5f;
	}
}

float UProjectEasingFunctions::BackIn(float Time)
{
	const float s = 1.70158f;
	return Time * Time * ((s + 1.0f) * Time - s);
}

float UProjectEasingFunctions::BackOut(float Time)
{
	const float s = 1.70158f;
	const float f = Time - 1.0f;
	return f * f * ((s + 1.0f) * f + s) + 1.0f;
}

float UProjectEasingFunctions::BackInOut(float Time)
{
	const float s = 1.70158f * 1.525f;

	if (Time < 0.5f)
	{
		const float f = 2.0f * Time;
		return 0.5f * (f * f * ((s + 1.0f) * f - s));
	}
	else
	{
		const float f = 2.0f * Time - 2.0f;
		return 0.5f * (f * f * ((s + 1.0f) * f + s) + 2.0f);
	}
}
