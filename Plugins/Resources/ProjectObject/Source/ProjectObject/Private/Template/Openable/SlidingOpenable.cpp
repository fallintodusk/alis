// Copyright ALIS. All Rights Reserved.

#include "Template/Openable/SlidingOpenable.h"
#include "Components/SpringSliderComponent.h"
#include "Sound/SoundCue.h"

ASlidingOpenable::ASlidingOpenable()
{
	SliderComponent = CreateDefaultSubobject<USpringSliderComponent>(TEXT("SliderComponent"));
	SliderComponent->TargetMesh = Mesh;

	// Default slider sounds
	static ConstructorHelpers::FObjectFinder<USoundCue> OpenSoundFinder(
		TEXT("/ProjectAudio/Interaction/DRAWER_Wood_Kitchen_Open_01_mono_Cue.DRAWER_Wood_Kitchen_Open_01_mono_Cue"));
	if (OpenSoundFinder.Succeeded())
	{
		SliderComponent->OpenSound = OpenSoundFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundCue> CloseSoundFinder(
		TEXT("/ProjectAudio/Interaction/DRAWER_Wood_Kitchen_Open_02_mono_Cue.DRAWER_Wood_Kitchen_Open_02_mono_Cue"));
	if (CloseSoundFinder.Succeeded())
	{
		SliderComponent->CloseSound = CloseSoundFinder.Object;
	}
}
