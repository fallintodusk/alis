#include "DataAssets/Transition/DA_FadeTransition.h"

UDA_FadeTransition::UDA_FadeTransition()
{
	DisplayName = FText::FromString(TEXT("Fade Transition"));
	Description = FText::FromString(TEXT("Opacity-based fade used by dialogs and modals."));
	TransitionType = EProjectUITransitionType::Fade;
	SlideDirection = EProjectUISlideDirection::Bottom;
	Duration = 0.35f;
	Delay = 0.0f;
	EasingFunction = EEasingFunc::EaseInOut;
	EasingAmplitude = 0.0f;
}
