#pragma once

#include "CoreMinimal.h"
#include "FeatureInitContext.h"

/**
 * Global registry of gameplay features.
 * Features self-register on module startup, GameModes call InitializeFeature.
 *
 * Usage (Feature side - in module StartupModule):
 *   FFeatureRegistry::RegisterFeature(TEXT("Combat"), [](const FFeatureInitContext& Ctx) {
 *       // Attach components, parse config, etc.
 *   });
 *
 * Usage (GameMode side - in HandleStartingNewPlayer):
 *   for (const FName& FeatureName : ModeConfig.FeatureNames)
 *   {
 *       FFeatureInitContext Ctx(World, this, Pawn, ModeName, ModeConfig.FeatureConfigs.FindRef(FeatureName));
 *       FFeatureRegistry::InitializeFeature(FeatureName, Ctx);
 *   }
 */
class PROJECTFEATURE_API FFeatureRegistry
{
public:
	/** Function signature for feature initialization */
	using FFeatureInitFunction = TFunction<void(const FFeatureInitContext&)>;

	/**
	 * Register a feature with the registry.
	 * Called by feature plugins in their module StartupModule.
	 * Thread-safe (module startups can race).
	 *
	 * @param FeatureName Unique name for the feature (e.g., "Combat", "Inventory")
	 * @param InitFn Function to call when feature should initialize
	 */
	static void RegisterFeature(FName FeatureName, FFeatureInitFunction InitFn);

	/**
	 * Initialize a registered feature.
	 * Called by GameMode when setting up gameplay.
	 *
	 * @param FeatureName Name of the feature to initialize
	 * @param Context Initialization context (world, pawn, config, etc.)
	 * @return true if feature was found and initialized, false if not registered
	 */
	static bool InitializeFeature(FName FeatureName, const FFeatureInitContext& Context);

	/**
	 * Check if a feature is registered.
	 *
	 * @param FeatureName Name of the feature to check
	 * @return true if feature is registered
	 */
	static bool IsFeatureRegistered(FName FeatureName);

	/**
	 * Get list of all registered feature names.
	 *
	 * @return Array of registered feature names
	 */
	static TArray<FName> GetRegisteredFeatures();

private:
	/** Thread-safe access to the registry map */
	static FCriticalSection RegistryLock;

	/** Map of feature name to init function */
	static TMap<FName, FFeatureInitFunction> Registry;
};
