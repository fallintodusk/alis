// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadRequest.h"

/**
 * FLoadRequestValidator
 *
 * Single Responsibility: Validate FLoadRequest objects before pipeline execution.
 *
 * Validation rules:
 * 1. Map must be specified (either MapAssetId or MapSoftPath)
 * 2. Multiplayer modes require session data
 * 3. Feature and content pack names must not be empty
 * 4. No duplicate features or content packs
 *
 * Observability:
 * - Returns detailed error messages for each validation failure
 * - Logs validation results for debugging
 * - Supports both quick check (IsValid) and detailed validation (Validate)
 */
class FLoadRequestValidator
{
public:
	/**
	 * Validate a load request with detailed error reporting.
	 *
	 * @param Request The request to validate
	 * @param OutErrors Array populated with detailed error messages
	 * @return True if request is valid, false otherwise
	 */
	static bool Validate(const FLoadRequest& Request, TArray<FText>& OutErrors);

	/**
	 * Quick validation check without error details.
	 * Use for fast checks where detailed errors aren't needed.
	 *
	 * @param Request The request to validate
	 * @return True if request is valid
	 */
	static bool IsValid(const FLoadRequest& Request);

	/**
	 * Get a human-readable summary of validation errors.
	 *
	 * @param Errors Array of error messages
	 * @return Combined error message string
	 */
	static FText GetErrorSummary(const TArray<FText>& Errors);
};
