// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefinitionTypeInfo.h"

class UObject;

/**
 * Validation result for a single check.
 */
struct FDefinitionValidationError
{
	FString AssetId;
	FString PropertyPath;
	FString Message;
	bool bIsWarning = false;
};

/**
 * Validator for definition assets.
 * Validates asset references, capability types, and data integrity.
 *
 * SOLID: Single responsibility - only validation logic.
 * Shows editor notifications (30 sec) for validation errors.
 */
class PROJECTDEFINITIONGENERATOR_API FDefinitionValidator
{
public:
	/**
	 * Validate a generated asset.
	 * @param TypeInfo Type configuration
	 * @param Asset The asset to validate
	 * @param AssetId Asset identifier for error messages
	 * @return Array of validation errors (empty if valid)
	 */
	static TArray<FDefinitionValidationError> ValidateAsset(
		const FDefinitionTypeInfo& TypeInfo,
		UObject* Asset,
		const FString& AssetId);

	/**
	 * Show validation errors as editor notification.
	 * Displays for 30 seconds with error details.
	 * @param Errors Array of validation errors
	 * @param TypeName Definition type name for context
	 */
	static void ShowValidationNotification(
		const TArray<FDefinitionValidationError>& Errors,
		const FString& TypeName);

	/**
	 * Validate and notify in one call.
	 * @return True if no errors (warnings are allowed)
	 */
	static bool ValidateAndNotify(
		const FDefinitionTypeInfo& TypeInfo,
		UObject* Asset,
		const FString& AssetId,
		const FString& TypeName);

	/**
	 * Validate object definition capabilities.
	 * Checks scope mesh IDs exist, delegates to capability-specific validators via
	 * FCapabilityValidationRegistry. See: CapabilityValidationRegistry.h
	 */
	static void ValidateObjectCapabilities(
		const class UObjectDefinition* ObjectDef,
		const FString& AssetId,
		TArray<FDefinitionValidationError>& OutErrors);

private:
	/** Validate soft object references resolve to valid assets. */
	static void ValidateSoftObjectReferences(
		UObject* Asset,
		const FString& AssetId,
		TArray<FDefinitionValidationError>& OutErrors);

	/** Validate soft class references resolve to valid classes. */
	static void ValidateSoftClassReferences(
		UObject* Asset,
		const FString& AssetId,
		TArray<FDefinitionValidationError>& OutErrors);

	/** Helper to add validation error with consistent format. */
	static void AddCapabilityError(
		TArray<FDefinitionValidationError>& OutErrors,
		const FString& AssetId,
		int32 CapIndex,
		const FString& CapabilityType,
		const FString& PropertyPath,
		const FString& Message,
		bool bIsWarning);
};
