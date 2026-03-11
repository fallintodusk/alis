#include "Components/MotionSourceComponent.h"
#include "Interfaces/IMotionDataProvider.h"

UMotionSourceComponent::UMotionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMotionSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TODO: Implement pose computation
	// - Query IMotionDataProvider if owner implements it
	// - Otherwise compute from owner velocity
	// - Apply smoothing and gait selection
}
