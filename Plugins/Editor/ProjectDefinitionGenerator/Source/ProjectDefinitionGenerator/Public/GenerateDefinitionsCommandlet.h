// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateDefinitionsCommandlet.generated.h"

/**
 * Universal commandlet for generating definition assets from JSON.
 *
 * Usage:
 *   -run=GenerateDefinitions -type=Item [-force] [-cleanup]
 *   -run=GenerateDefinitions -all [-force] [-cleanup]
 *
 * Options:
 *   -type=<TypeName>  Generate only this type (e.g., "Item", "Object")
 *   -all              Generate all registered types
 *   -force            Force regenerate even if hash matches
 *   -cleanup          Delete orphaned assets after generation
 */
UCLASS()
class PROJECTDEFINITIONGENERATOR_API UGenerateDefinitionsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGenerateDefinitionsCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	/** Generate assets for a single type */
	int32 GenerateType(const FString& TypeName, bool bForceRegenerate, bool bCleanup);

	/** Log summary for a type */
	void LogSummary(const FString& TypeName, int32 SuccessCount, int32 SkippedCount, int32 FailedCount);
};
