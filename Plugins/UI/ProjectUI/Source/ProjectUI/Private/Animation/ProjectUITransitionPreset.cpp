#include "Animation/ProjectUITransitionPreset.h"

UProjectUITransitionPreset::UProjectUITransitionPreset()
{
	DisplayName = FText::FromString(TEXT("Transition"));
	Description = FText::GetEmpty();
	TransitionType = EProjectUITransitionType::Fade;
	SlideDirection = EProjectUISlideDirection::Bottom;
	Duration = 0.25f;
	Delay = 0.0f;
	EasingFunction = EEasingFunc::EaseInOut;
	EasingAmplitude = 0.0f;
}

FPrimaryAssetId UProjectUITransitionPreset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ProjectUITransition"), GetFName());
}

