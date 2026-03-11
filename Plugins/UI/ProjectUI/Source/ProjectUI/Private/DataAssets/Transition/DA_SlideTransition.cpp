#include "DataAssets/Transition/DA_SlideTransition.h"

UDA_SlideTransition::UDA_SlideTransition()
{
	DisplayName = FText::FromString(TEXT("Slide Up Transition"));
	Description = FText::FromString(TEXT("Slides content upward from the bottom of the screen."));
	TransitionType = EProjectUITransitionType::Slide;
	SlideDirection = EProjectUISlideDirection::Bottom;
	Duration = 0.30f;
	Delay = 0.0f;
	EasingFunction = EEasingFunc::EaseOut;
	EasingAmplitude = 0.0f;
}
