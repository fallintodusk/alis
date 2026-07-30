// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/AssemblyTypes.h"
#include "IAssemblyViewConfigSource.generated.h"

/**
 * Read-only data source for assembly configuration.
 *
 * Separated from IAssemblyCapability (lifecycle) so consumers can read
 * assembly data without depending on lifecycle management concerns.
 *
 * Populated at construction time by the spawn path (ObjectSpawnUtility).
 * Consumed by gameplay code (character camera, NPC viewpoint, etc.).
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAssemblyViewConfigSource : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IAssemblyViewConfigSource
{
	GENERATED_BODY()

public:
	/**
	 * Set view configuration during spawn (construction-time only).
	 * Called by the spawn path before CompleteAssembly().
	 */
	virtual void SetViewConfig(const FAssemblyViewConfig& Config) = 0;

	/**
	 * Read view configuration.
	 * Returns true if view config was set and written to OutConfig.
	 */
	virtual bool GetViewConfig(FAssemblyViewConfig& OutConfig) const = 0;
};
