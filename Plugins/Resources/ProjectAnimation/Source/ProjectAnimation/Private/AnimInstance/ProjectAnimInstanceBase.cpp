#include "AnimInstance/ProjectAnimInstanceBase.h"

void UProjectAnimInstanceBase::SetMotionPose(const FProjectMotionPose& InPose)
{
	MotionPose = InPose;

	// Update convenience accessors
	Speed = InPose.Speed;
	bIsInAir = InPose.bInAir;
	GaitIndex = static_cast<int32>(InPose.Gait);
	StanceIndex = static_cast<int32>(InPose.Stance);
}
