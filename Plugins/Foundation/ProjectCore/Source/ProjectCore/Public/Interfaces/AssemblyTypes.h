// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "AssemblyTypes.generated.h"

/**
 * Assembly lifecycle states (domain contract).
 *
 * Consumed by any module that needs to react to assembly readiness
 * without depending on concrete assembly implementations.
 *
 * Linear progression: Idle -> Assembling -> Ready.
 * TearingDown entered from Assembling or Ready.
 */
UENUM(BlueprintType)
enum class EAssemblyState : uint8
{
	Idle,
	Assembling,
	Ready,
	TearingDown
};

/**
 * View/camera configuration for assembled actors.
 *
 * Plain data struct in ProjectCore so gameplay consumers (character, NPC,
 * camera system) can read view config without depending on definition
 * storage types (UObjectDefinition, FViewSection).
 *
 * Only fields with active runtime consumers belong here. Add fields
 * when a consumer needs them, with strong types (enum/tag, not FName).
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FAssemblyViewConfig
{
	GENERATED_BODY()

	/** Camera relative offset from attach point. */
	UPROPERTY(BlueprintReadOnly, Category = "View")
	FVector RelativeOffset = FVector::ZeroVector;
};
