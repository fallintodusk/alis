// Copyright ALIS. All Rights Reserved.

#include "Template/Openable/HingedOpenable.h"
#include "Components/SpringRotatorComponent.h"
#include "Sound/SoundCue.h"

AHingedOpenable::AHingedOpenable()
{
	RotatorComponent = CreateDefaultSubobject<USpringRotatorComponent>(TEXT("RotatorComponent"));
	RotatorComponent->TargetMesh = Mesh;

	// Default door sounds
	static ConstructorHelpers::FObjectFinder<USoundCue> OpenSoundFinder(
		TEXT("/ProjectAudio/Interaction/DOOR_Indoor_Wood_Open_stereo_Cue.DOOR_Indoor_Wood_Open_stereo_Cue"));
	if (OpenSoundFinder.Succeeded())
	{
		RotatorComponent->OpenSound = OpenSoundFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundCue> CloseSoundFinder(
		TEXT("/ProjectAudio/Interaction/DOOR_Indoor_Wood_Close_Gentle_stereo_Cue.DOOR_Indoor_Wood_Close_Gentle_stereo_Cue"));
	if (CloseSoundFinder.Succeeded())
	{
		RotatorComponent->CloseSound = CloseSoundFinder.Object;
	}
}
