// Shared upper-chain math helpers for LocalBody correction strategies.
// Consumed by LocalBodyCorrectionAngleClamp.cpp and LocalBodyCorrectionTransitionGuard.cpp.

#pragma once

#include "Math/Vector.h"
#include "Math/UnrealMathUtility.h"

namespace LocalBodyCorrectionMath
{

inline float ComputeUpperChainForwardAngleDeg(const FVector& UpperChainCameraDelta)
{
	const float ForwardCm = FMath::Max(0.0f, UpperChainCameraDelta.X);
	const float DownCm = FMath::Abs(FMath::Min(0.0f, UpperChainCameraDelta.Z));
	if (ForwardCm <= KINDA_SMALL_NUMBER && DownCm <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::RadiansToDegrees(FMath::Atan2(ForwardCm, FMath::Max(KINDA_SMALL_NUMBER, DownCm)));
}

inline FVector ConstrainUpperChainCameraDelta(
	const FVector& SourceUpperChainCameraDelta,
	const float ChainLengthCm,
	const float MaxForwardAngleDeg,
	const float MinDropCm)
{
	if (ChainLengthCm <= KINDA_SMALL_NUMBER)
	{
		return SourceUpperChainCameraDelta;
	}

	const float LateralCm = SourceUpperChainCameraDelta.Y;
	const float PlanarLenSq = FMath::Max(
		0.0f,
		FMath::Square(ChainLengthCm) - FMath::Square(LateralCm));
	const float PlanarLen = FMath::Sqrt(PlanarLenSq);
	if (PlanarLen <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float SafeMinDropCm = FMath::Min(MinDropCm, PlanarLen);
	const float MaxForwardByAngleCm =
		PlanarLen * FMath::Sin(FMath::DegreesToRadians(MaxForwardAngleDeg));
	const float MaxForwardByDropCm = FMath::Sqrt(FMath::Max(
		0.0f,
		PlanarLenSq - FMath::Square(SafeMinDropCm)));
	const float MaxForwardCm = FMath::Min(MaxForwardByAngleCm, MaxForwardByDropCm);

	FVector Result = SourceUpperChainCameraDelta;
	if (Result.X > 0.0f)
	{
		Result.X = FMath::Min(Result.X, MaxForwardCm);
	}

	Result.Z = -FMath::Sqrt(FMath::Max(0.0f, PlanarLenSq - FMath::Square(Result.X)));
	return Result;
}

} // namespace LocalBodyCorrectionMath
