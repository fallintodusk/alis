#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MotionSolverLibrary.generated.h"

/**
 * Static math helpers for motion: spring-damper, smoothing, simple IK.
 * Skeleton-agnostic - pure math functions.
 */
UCLASS()
class PROJECTMOTIONSYSTEM_API UMotionSolverLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spring-damper smoothing for floats.
	 * @param Current Current value
	 * @param Target Target value
	 * @param Velocity Current velocity (modified)
	 * @param DeltaTime Frame delta time
	 * @param Stiffness Spring stiffness
	 * @param Damping Damping ratio
	 * @return Smoothed value
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion|Solvers")
	static float SpringDamperFloat(float Current, float Target, UPARAM(ref) float& Velocity, float DeltaTime, float Stiffness = 100.0f, float Damping = 10.0f);

	/**
	 * Spring-damper smoothing for vectors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion|Solvers")
	static FVector SpringDamperVector(const FVector& Current, const FVector& Target, UPARAM(ref) FVector& Velocity, float DeltaTime, float Stiffness = 100.0f, float Damping = 10.0f);

	/**
	 * Simple two-bone IK solver.
	 * @param RootPos Root bone position (world space)
	 * @param TargetPos Target position (world space)
	 * @param UpperLength Upper bone length
	 * @param LowerLength Lower bone length
	 * @param OutMidPos Calculated mid joint position
	 * @return True if solution found
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion|Solvers")
	static bool SolveTwoBoneIK(const FVector& RootPos, const FVector& TargetPos, float UpperLength, float LowerLength, FVector& OutMidPos);
};
