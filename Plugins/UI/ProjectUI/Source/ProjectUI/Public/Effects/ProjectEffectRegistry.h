// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "ProjectEffectRegistry.generated.h"

class UProjectEffectWidget;
class UProjectUIThemeData;

/**
 * Registry for procedural UI effects.
 *
 * SOLID Architecture:
 * - Open/Closed: New effects are registered without modifying registry code
 * - Single Responsibility: Only handles effect creation/lookup
 * - Dependency Inversion: Works with effect base class, not concrete types
 *
 * Usage:
 *   // Create effect by name (returns nullptr if not found)
 *   UProjectEffectWidget* Effect = UProjectEffectRegistry::CreateEffect(Outer, TEXT("FireSparks"));
 *
 *   // Create from JSON config
 *   UProjectEffectWidget* Effect = UProjectEffectRegistry::CreateEffectFromJson(Outer, JsonObject, Theme);
 *
 * Registering New Effects:
 *   1. Derive from UProjectEffectWidget
 *   2. Add factory to Initialize() in LayoutEffectRegistry namespace
 *   3. Add property applier if JSON config needed
 */
UCLASS()
class PROJECTUI_API UProjectEffectRegistry : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Create an effect widget by type name.
	 *
	 * @param Outer - Outer object for widget creation
	 * @param EffectType - Effect type name (e.g., "FireSparks")
	 * @param WidgetName - Optional widget name
	 * @return Created effect widget, or nullptr if type not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects")
	static UProjectEffectWidget* CreateEffect(UObject* Outer, const FString& EffectType, FName WidgetName = NAME_None);

	/**
	 * Create an effect widget from JSON configuration.
	 *
	 * @param Outer - Outer object for widget creation
	 * @param JsonObject - JSON object with effect configuration
	 * @param Theme - Theme for resolving color references (optional)
	 * @return Created and configured effect widget, or nullptr on failure
	 */
	static UProjectEffectWidget* CreateEffectFromJson(UObject* Outer, const TSharedPtr<FJsonObject>& JsonObject,
		UProjectUIThemeData* Theme = nullptr);

	/**
	 * Apply JSON effect parameters to an existing effect widget.
	 *
	 * @param Effect - Effect widget to configure
	 * @param JsonObject - JSON object with "effect" field containing parameters
	 * @param Theme - Theme for resolving color references (optional)
	 */
	static void ApplyEffectProperties(UProjectEffectWidget* Effect, const TSharedPtr<FJsonObject>& JsonObject,
		UProjectUIThemeData* Theme);

	/**
	 * Get list of registered effect types.
	 *
	 * @return Array of registered effect type names
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects")
	static TArray<FString> GetRegisteredEffectTypes();

	/**
	 * Check if an effect type is registered.
	 *
	 * @param EffectType - Effect type name to check
	 * @return true if type is registered
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects")
	static bool IsEffectTypeRegistered(const FString& EffectType);
};

/**
 * Internal namespace for effect registry implementation.
 * Separated for SOLID compliance (single responsibility).
 */
namespace LayoutEffectRegistry
{
	/** Create effect widget by type name */
	UProjectEffectWidget* CreateEffectByType(const FString& Type, UObject* Outer, FName WidgetName);

	/** Get all registered effect type names */
	TArray<FString> GetRegisteredTypes();

	/** Check if type is registered */
	bool IsTypeRegistered(const FString& Type);
}
