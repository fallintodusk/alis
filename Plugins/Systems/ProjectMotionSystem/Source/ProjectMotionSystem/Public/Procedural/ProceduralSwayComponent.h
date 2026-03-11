#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralSwayComponent.generated.h"

/**
 * Procedural wind sway for trees, flags, foliage.
 * Skeleton-agnostic - applies offset/rotation to owner.
 */
UCLASS(ClassGroup = (Motion), meta = (BlueprintSpawnableComponent))
class PROJECTMOTIONSYSTEM_API UProceduralSwayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProceduralSwayComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sway amplitude (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float SwayAmplitude = 5.0f;

	/** Sway frequency (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float SwayFrequency = 0.5f;

	/** Wind direction (world space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

protected:
	float TimeAccumulator = 0.0f;
};
