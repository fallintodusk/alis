#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IFeatureConfigurable.generated.h"

/**
 * Interface for feature components that can receive configuration from FSinglePlayModeConfig.
 *
 * When ASinglePlayerGameMode attaches a feature component to the pawn, it checks if
 * the component implements this interface. If so, it passes the corresponding
 * FeatureConfigs entry (JSON string) via ConfigureFeature().
 *
 * Usage in feature component:
 *
 *   class UCombatComponent : public UActorComponent, public IFeatureConfigurable
 *   {
 *       virtual void ConfigureFeature(const FString& ConfigJson) override
 *       {
 *           // Parse JSON and apply settings
 *       }
 *
 *       virtual FName GetFeatureName() const override
 *       {
 *           return FName(TEXT("Combat"));
 *       }
 *   };
 *
 * The GameMode looks up FeatureConfigs[GetFeatureName()] and passes it to ConfigureFeature().
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UFeatureConfigurable : public UInterface
{
	GENERATED_BODY()
};

class PROJECTSINGLEPLAY_API IFeatureConfigurable
{
	GENERATED_BODY()

public:
	/**
	 * Returns the feature name used to look up configuration in FeatureConfigs.
	 * Example: "Combat", "Inventory", "Dialogue", "Difficulty"
	 */
	virtual FName GetFeatureName() const = 0;

	/**
	 * Called by GameMode after component is attached, passing the JSON config string.
	 * @param ConfigJson - JSON string from FSinglePlayModeConfig.FeatureConfigs[GetFeatureName()]
	 */
	virtual void ConfigureFeature(const FString& ConfigJson) = 0;
};
