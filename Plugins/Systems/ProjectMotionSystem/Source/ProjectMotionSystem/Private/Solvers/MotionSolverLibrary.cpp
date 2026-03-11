#include "Solvers/MotionSolverLibrary.h"

float UMotionSolverLibrary::SpringDamperFloat(float Current, float Target, float& Velocity, float DeltaTime, float Stiffness, float Damping)
{
	// TODO: Implement spring-damper
	// F = -k*(x - target) - c*v
	const float Delta = Current - Target;
	const float Acceleration = -Stiffness * Delta - Damping * Velocity;
	Velocity += Acceleration * DeltaTime;
	return Current + Velocity * DeltaTime;
}

FVector UMotionSolverLibrary::SpringDamperVector(const FVector& Current, const FVector& Target, FVector& Velocity, float DeltaTime, float Stiffness, float Damping)
{
	// TODO: Implement spring-damper for vectors
	const FVector Delta = Current - Target;
	const FVector Acceleration = -Stiffness * Delta - Damping * Velocity;
	Velocity += Acceleration * DeltaTime;
	return Current + Velocity * DeltaTime;
}

bool UMotionSolverLibrary::SolveTwoBoneIK(const FVector& RootPos, const FVector& TargetPos, float UpperLength, float LowerLength, FVector& OutMidPos)
{
	// TODO: Implement two-bone IK solver
	// Basic law of cosines approach
	OutMidPos = FMath::Lerp(RootPos, TargetPos, 0.5f);
	return true;
}
