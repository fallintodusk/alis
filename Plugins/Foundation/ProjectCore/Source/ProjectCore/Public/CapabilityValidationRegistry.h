// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Capability validation result. */
struct PROJECTCORE_API FCapabilityValidationResult
{
	FString PropertyPath;
	FString Message;
	bool bIsWarning = false;
};

/** Validation function: properties -> validation results. */
using FCapabilityValidateFunc = TFunction<TArray<FCapabilityValidationResult>(const TMap<FName, FString>& Properties)>;

/**
 * Registry for capability validation functions.
 *
 * ## When to Register
 *
 * Only capabilities with REQUIRED properties need validation:
 * - Pickup: InitialQuantity, PickupTime (have defaults) -> validation for warnings only
 * - Lockable: LockTag (optional, empty=unlocked) -> NO validation needed
 * - Hinged/Sliding: All properties have defaults -> validation for warnings only
 *
 * ## How to Register
 *
 * Use static initializer in component .cpp with #if WITH_EDITOR guard:
 *
 * ```cpp
 * #if WITH_EDITOR
 * static struct FMyCapValidationAutoReg
 * {
 *     FMyCapValidationAutoReg()
 *     {
 *         FCapabilityValidationRegistry::RegisterValidation(
 *             FName(TEXT("MyCap")),
 *             &UMyCapComponent::ValidateCapabilityProperties);
 *     }
 * } GMyCapValidationAutoReg;
 * #endif
 * ```
 *
 * ## Architecture
 *
 * - Components self-register (no central module dependency)
 * - DefinitionValidator queries during JSON->DataAsset generation
 * - Separate from FCapabilityRegistry (UClass* mapping) for proper dependency direction
 *
 * See: SpringRotatorComponent.cpp, PickupCapabilityComponent.cpp
 */
class PROJECTCORE_API FCapabilityValidationRegistry
{
public:
	/** Register validation function for a capability type. */
	static void RegisterValidation(FName CapabilityId, FCapabilityValidateFunc&& ValidateFunc);

	/** Get validation function. Returns nullptr if none registered. */
	static const FCapabilityValidateFunc* GetValidationFunc(FName CapabilityId);

	/** Check if validation is registered for a capability type. */
	static bool HasValidation(FName CapabilityId);

private:
	static TMap<FName, FCapabilityValidateFunc> ValidationRegistry;
};
