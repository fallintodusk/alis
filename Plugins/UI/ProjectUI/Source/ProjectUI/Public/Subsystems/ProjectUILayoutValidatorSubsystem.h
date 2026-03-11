// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProjectUIDefinitions.h"
#include "ProjectUILayoutValidatorSubsystem.generated.h"

/**
 * Validator for UI definitions and runtime layout assumptions.
 * Keeps validation logic separate from registry/factory.
 */
UCLASS()
class PROJECTUI_API UProjectUILayoutValidatorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Validate a definition. Returns false and fills OutError on failure.
	 */
	bool ValidateDefinition(const FProjectUIDefinition& Definition, FString& OutError) const;

	/**
	 * Validate and log any failures for a single definition.
	 */
	bool ValidateAndLog(const FProjectUIDefinition& Definition) const;

	/**
	 * Validate that a layout JSON path exists on disk.
	 */
	bool ValidateLayoutFile(const FProjectUIDefinition& Definition, FString& OutError) const;
};
