// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldConstructedRuntimeInput.h"
class ACharacter;
class ANavMeshBoundsVolume;
class ANavigationData;
class FAutomationTestBase;
class UWorld;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	struct FRouteMetrics
	{
		int32 Routes=0,WalkingSegments=0,WalkingFrames=0,FallingFrames=0,UnknownFrames=0,NavSegments=0;
		double MaximumFootError=0,MaximumNavError=0;
	};
	struct FWalkLane { FString RouteId; double OffsetCentimeters=0; TArray<FVector> Points; };
	bool BuildWalkingLanes(const FInput& Input,double CapsuleRadiusCentimeters,TArray<FWalkLane>& Out,FString& Error);
	bool WalkRoutes(const FInput& Input,ACharacter* Character,TFunctionRef<void(float)> Tick,FAutomationTestBase& Test,FRouteMetrics& Out);
	bool NavRoutes(const FInput& Input,UWorld* World,ACharacter* Character,ANavMeshBoundsVolume* Bounds,
		ANavigationData*& NavData,FAutomationTestBase& Test,FRouteMetrics& Out);
}
#endif
