// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrchestratorBootLibrary.generated.h"

/**
 * Dynamic delegate for Boot progress updates (Blueprint-friendly).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnBootProgressUpdateDynamic,
	const FString&, CurrentPlugin,
	int32, LoadedCount,
	int32, TotalBootPlugins,
	const FString&, StatusMessage
);

/**
 * Dynamic delegate for Boot complete (Blueprint-friendly).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBootCompleteDynamic);

/**
 * Blueprint-accessible helpers for Orchestrator boot progress.
 * Used by L_OrchestratorBoot map to show loading progress.
 *
 * Usage in Blueprint:
 * 1. On BeginPlay: Call "Get Orchestrator Boot Events" to get the event dispatcher object
 * 2. Bind to OnBootProgress and OnBootComplete events
 * 3. Update UI when events fire
 * 4. On OnBootComplete: Open MainMenuWorld map
 */
UCLASS()
class ORCHESTRATORCORE_API UOrchestratorBootEvents : public UObject
{
	GENERATED_BODY()

public:
	/** Event fired as each Boot plugin loads */
	UPROPERTY(BlueprintAssignable, Category = "Orchestrator|Boot")
	FOnBootProgressUpdateDynamic OnBootProgress;

	/** Event fired when all Boot plugins finish loading */
	UPROPERTY(BlueprintAssignable, Category = "Orchestrator|Boot")
	FOnBootCompleteDynamic OnBootComplete;

	/** Initialize and bind to Orchestrator C++ delegates */
	void Initialize();

	/** Cleanup and unbind from Orchestrator C++ delegates */
	void Shutdown();

private:
	/** Handle for C++ OnBootProgressUpdate delegate */
	FDelegateHandle ProgressHandle;

	/** Handle for C++ OnBootComplete delegate */
	FDelegateHandle CompleteHandle;
};

/**
 * Blueprint function library to access Orchestrator boot events.
 */
UCLASS()
class ORCHESTRATORCORE_API UOrchestratorBootLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get the Orchestrator boot events object (singleton).
	 * Use this to bind to OnBootProgress and OnBootComplete events.
	 *
	 * @return Boot events object (creates if doesn't exist)
	 */
	UFUNCTION(BlueprintPure, Category = "Orchestrator|Boot", meta = (WorldContext = "WorldContextObject"))
	static UOrchestratorBootEvents* GetOrchestratorBootEvents(UObject* WorldContextObject);

private:
	/** Singleton instance of boot events */
	static UOrchestratorBootEvents* BootEventsInstance;
};
