// Copyright ALIS. All Rights Reserved.

#include "GenerateDefinitionsCommandlet.h"
#include "DefinitionGeneratorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateDefinitions, Log, All);

UGenerateDefinitionsCommandlet::UGenerateDefinitionsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGenerateDefinitionsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGenerateDefinitions, Log, TEXT("========================================"));
	UE_LOG(LogGenerateDefinitions, Log, TEXT("GenerateDefinitions Commandlet"));
	UE_LOG(LogGenerateDefinitions, Log, TEXT("========================================"));

	// Parse options
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

	const bool bForceRegenerate = Switches.Contains(TEXT("force"));
	const bool bCleanup = Switches.Contains(TEXT("cleanup"));
	const bool bAll = Switches.Contains(TEXT("all"));
	const FString* TypeParam = ParamsMap.Find(TEXT("type"));

	if (bForceRegenerate)
	{
		UE_LOG(LogGenerateDefinitions, Log, TEXT("Force regeneration enabled"));
	}

	// Get universal generator
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		UE_LOG(LogGenerateDefinitions, Error, TEXT("DefinitionGeneratorSubsystem not available"));
		return 1;
	}

	// Auto-discover manifests
	Generator->DiscoverAndRegisterManifests();

	// Get registered types
	TArray<FString> RegisteredTypes = Generator->GetRegisteredTypeNames();
	if (RegisteredTypes.Num() == 0)
	{
		UE_LOG(LogGenerateDefinitions, Warning, TEXT("No definition types registered. Check plugin manifests."));
		return 0;
	}

	UE_LOG(LogGenerateDefinitions, Log, TEXT("Registered types: %s"), *FString::Join(RegisteredTypes, TEXT(", ")));

	int32 TotalFailures = 0;

	if (bAll)
	{
		// Generate all registered types
		for (const FString& TypeName : RegisteredTypes)
		{
			TotalFailures += GenerateType(TypeName, bForceRegenerate, bCleanup);
		}
	}
	else if (TypeParam && !TypeParam->IsEmpty())
	{
		// Generate specific type
		if (!RegisteredTypes.Contains(*TypeParam))
		{
			UE_LOG(LogGenerateDefinitions, Error, TEXT("Unknown type: %s"), **TypeParam);
			UE_LOG(LogGenerateDefinitions, Log, TEXT("Available types: %s"), *FString::Join(RegisteredTypes, TEXT(", ")));
			return 1;
		}
		TotalFailures = GenerateType(*TypeParam, bForceRegenerate, bCleanup);
	}
	else
	{
		UE_LOG(LogGenerateDefinitions, Error, TEXT("Usage: -run=GenerateDefinitions -type=<TypeName> or -all"));
		UE_LOG(LogGenerateDefinitions, Log, TEXT("Available types: %s"), *FString::Join(RegisteredTypes, TEXT(", ")));
		return 1;
	}

	UE_LOG(LogGenerateDefinitions, Log, TEXT("========================================"));
	UE_LOG(LogGenerateDefinitions, Log, TEXT("Commandlet complete. Total failures: %d"), TotalFailures);
	UE_LOG(LogGenerateDefinitions, Log, TEXT("========================================"));

	return TotalFailures > 0 ? 1 : 0;
}

int32 UGenerateDefinitionsCommandlet::GenerateType(const FString& TypeName, bool bForceRegenerate, bool bCleanup)
{
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return 1;
	}

	UE_LOG(LogGenerateDefinitions, Log, TEXT(""));
	UE_LOG(LogGenerateDefinitions, Log, TEXT("--- Generating: %s ---"), *TypeName);

	// Log source directory
	const FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
	UE_LOG(LogGenerateDefinitions, Log, TEXT("Source: %s"), *SourceDir);

	// Generate
	TArray<FDefinitionGenerationResult> Results = Generator->GenerateAllForType(TypeName, bForceRegenerate);

	// Cleanup orphans if requested
	if (bCleanup)
	{
		const int32 OrphansDeleted = Generator->CleanupOrphanedAssets(TypeName);
		UE_LOG(LogGenerateDefinitions, Log, TEXT("Deleted %d orphaned assets"), OrphansDeleted);
	}

	// Count results
	int32 SuccessCount = 0;
	int32 SkippedCount = 0;
	int32 FailedCount = 0;

	for (const FDefinitionGenerationResult& Result : Results)
	{
		if (Result.bSkipped)
		{
			SkippedCount++;
		}
		else if (Result.bSuccess)
		{
			SuccessCount++;
		}
		else
		{
			FailedCount++;
			UE_LOG(LogGenerateDefinitions, Error, TEXT("FAILED: %s - %s"), *Result.AssetId, *Result.ErrorMessage);
		}
	}

	LogSummary(TypeName, SuccessCount, SkippedCount, FailedCount);
	return FailedCount;
}

void UGenerateDefinitionsCommandlet::LogSummary(const FString& TypeName, int32 SuccessCount, int32 SkippedCount, int32 FailedCount)
{
	UE_LOG(LogGenerateDefinitions, Log, TEXT("[%s] Results: %d generated, %d skipped, %d failed"),
		*TypeName, SuccessCount, SkippedCount, FailedCount);
}
