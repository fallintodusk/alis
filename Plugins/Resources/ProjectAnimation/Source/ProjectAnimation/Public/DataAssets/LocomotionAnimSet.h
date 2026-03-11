#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/ProjectMotionTypes.h"
#include "LocomotionAnimSet.generated.h"

class UAnimationAsset;

/**
 * Entry mapping gait to animation asset.
 */
USTRUCT(BlueprintType)
struct FGaitAnimationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	EProjectGait Gait = EProjectGait::Idle;

	/** Animation asset (sequence, blendspace, or montage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSoftObjectPtr<UAnimationAsset> Animation;
};

/**
 * Data asset mapping gait/stance to animation assets.
 * One per skeleton family (MaleHero, FemaleHero, NPCs).
 */
UCLASS(BlueprintType)
class PROJECTANIMATION_API ULocomotionAnimSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Skeleton family this set is for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName SkeletonFamily;

	/** Animations for upright stance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Upright")
	TArray<FGaitAnimationEntry> UprightAnimations;

	/** Animations for crouch stance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Crouch")
	TArray<FGaitAnimationEntry> CrouchAnimations;

	/** Animations for prone stance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Prone")
	TArray<FGaitAnimationEntry> ProneAnimations;

	/** Find animation asset for given gait and stance */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	TSoftObjectPtr<UAnimationAsset> FindAnimation(EProjectGait Gait, EProjectStance Stance) const;
};
