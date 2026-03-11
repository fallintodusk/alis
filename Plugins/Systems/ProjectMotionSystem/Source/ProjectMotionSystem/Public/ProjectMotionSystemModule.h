#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProjectMotionSystem, Log, All);

/**
 * ProjectMotionSystem module - skeleton-agnostic motion primitives.
 *
 * Provides:
 * - FProjectMotionPose: locomotion state struct (speed, gait, stance, etc.)
 * - UMotionSourceComponent: computes pose from owner velocity/acceleration
 * - UMotionSolverLibrary: spring-damper, smoothing, IK math helpers
 * - Procedural components: sway, bob, rotate for trees/props/objects
 * - IMotionDataProvider: interface for characters to provide motion data
 *
 * Hard rules:
 * - No USkeletalMeshComponent dependencies
 * - No bone names, curve names, or anim assets
 * - GAS-agnostic: receives stamina as float, doesn't know AttributeSets
 */
class FProjectMotionSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
