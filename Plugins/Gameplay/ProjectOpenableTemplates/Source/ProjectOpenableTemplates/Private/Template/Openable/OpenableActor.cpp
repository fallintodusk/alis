// Copyright ALIS. All Rights Reserved.

#include "Template/Openable/OpenableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Access/LockableComponent.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundCue.h"
#include "UObject/ConstructorHelpers.h"

AOpenableActor::AOpenableActor()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);
	if (DefaultMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DefaultMesh.Object);
	}

	LockComponent = CreateDefaultSubobject<ULockableComponent>(TEXT("LockComponent"));

	// Default lock sounds
	static ConstructorHelpers::FObjectFinder<USoundCue> UnlockSoundFinder(
		TEXT("/ProjectAudio/Interaction/LOCK_Key_In_Wood_Door_Lock_01_mono_Cue.LOCK_Key_In_Wood_Door_Lock_01_mono_Cue"));
	if (UnlockSoundFinder.Succeeded())
	{
		LockComponent->UnlockSound = UnlockSoundFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundCue> AccessDeniedSoundFinder(
		TEXT("/ProjectAudio/Interaction/CREAK_Wood_Hollow_02_mono_Cue.CREAK_Wood_Hollow_02_mono_Cue"));
	if (AccessDeniedSoundFinder.Succeeded())
	{
		LockComponent->AccessDeniedSound = AccessDeniedSoundFinder.Object;
	}
}
