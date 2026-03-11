#include "Procedural/ProceduralSwayComponent.h"

UProceduralSwayComponent::UProceduralSwayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProceduralSwayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TODO: Implement wind sway
	// - Accumulate time
	// - Calculate sway angle from sin wave
	// - Apply to owner rotation
	TimeAccumulator += DeltaTime;
}
