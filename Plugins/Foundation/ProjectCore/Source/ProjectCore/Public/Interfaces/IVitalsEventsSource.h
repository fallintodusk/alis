// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IVitalsEventsSource.generated.h"

/**
 * Fired when a vitals source's condition decreases (any damage source).
 * Amount is the absolute decrease (always positive).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalsDamageTaken, float, Amount);

/**
 * Fired once when a vitals source transitions from Condition > 0 to <= 0.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVitalsConditionDepleted);

UINTERFACE(MinimalAPI, BlueprintType)
class UVitalsEventsSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * IVitalsEventsSource
 *
 * Read-only event access to a vitals provider (normally an ActorComponent).
 *
 * Consumers (e.g., GameMode death handling) discover the source via
 * UClass::ImplementsInterface(UVitalsEventsSource::StaticClass()) and bind
 * Blueprint-dynamic delegates through the accessors below. The accessors
 * return references to the underlying member delegates so AddDynamic /
 * RemoveDynamic keep working without an ABI shape change on the provider.
 *
 * The provider plugin (ProjectVitals) implements this interface on its
 * ActorComponent. This lets consumer plugins (ProjectSinglePlay, etc.) drop
 * their direct dependency on ProjectVitals and consume events through
 * ProjectCore only.
 */
class PROJECTCORE_API IVitalsEventsSource
{
	GENERATED_BODY()

public:
	/** Accessor for the damage-taken delegate. Used by AddDynamic/RemoveDynamic. */
	virtual FOnVitalsDamageTaken& GetOnDamageTakenDelegate() = 0;

	/** Accessor for the condition-depleted (death) delegate. Used by AddDynamic/RemoveDynamic. */
	virtual FOnVitalsConditionDepleted& GetOnConditionDepletedDelegate() = 0;
};
