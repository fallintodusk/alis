// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Delegate for definition regeneration events.
 *
 * SOC Pattern:
 * - Publisher: ProjectDefinitionGenerator (broadcasts when definition regenerated from JSON)
 * - Subscriber: ProjectPlacementEditor (updates placed actors)
 *
 * Both depend on ProjectEditorCore (this module), not on each other.
 * This follows Dependency Inversion Principle.
 *
 * @param TypeName The definition type (e.g., "Object", "Item")
 * @param RegeneratedAsset The regenerated UObject asset
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDefinitionRegenerated, const FString& /*TypeName*/, UObject* /*RegeneratedAsset*/);

/**
 * Global accessor for definition regeneration delegate.
 * Allows decoupled publish/subscribe without module dependencies.
 */
class PROJECTEDITORCORE_API FDefinitionEvents
{
public:
	/** Get the singleton delegate instance */
	static FOnDefinitionRegenerated& OnDefinitionRegenerated();

private:
	static FOnDefinitionRegenerated DefinitionRegeneratedDelegate;
};
