#include "DataAssets/LocomotionAnimSet.h"
#include "Animation/AnimationAsset.h"

TSoftObjectPtr<UAnimationAsset> ULocomotionAnimSet::FindAnimation(EProjectGait Gait, EProjectStance Stance) const
{
	const TArray<FGaitAnimationEntry>* AnimArray = nullptr;

	switch (Stance)
	{
	case EProjectStance::Upright:
		AnimArray = &UprightAnimations;
		break;
	case EProjectStance::Crouch:
		AnimArray = &CrouchAnimations;
		break;
	case EProjectStance::Prone:
		AnimArray = &ProneAnimations;
		break;
	}

	if (AnimArray)
	{
		for (const FGaitAnimationEntry& Entry : *AnimArray)
		{
			if (Entry.Gait == Gait)
			{
				return Entry.Animation;
			}
		}
	}

	return nullptr;
}
