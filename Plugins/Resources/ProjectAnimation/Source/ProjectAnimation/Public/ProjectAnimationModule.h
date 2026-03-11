#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProjectAnimation, Log, All);

/**
 * ProjectAnimation module - skeleton-specific animation content and drivers.
 *
 * Provides:
 * - UProjectAnimInstanceBase: light base AnimInstance with motion pose variables
 * - USkeletonLocomotionDriver: interface for skeleton-specific bone/curve mapping
 * - ULocomotionAnimSet: data asset mapping gait/stance -> sequences/blendspaces
 *
 * Content (in Content/ folder):
 * - Skeletal meshes, skeletons, physics assets
 * - Animation sequences, blendspaces, montages
 * - Animation Blueprints per skeleton family
 *
 * Hard rules:
 * - May depend on Foundation + Systems only
 * - May NOT import Gameplay modules (Character, GAS, etc.)
 * - If gameplay logic creeps in, move it to ProjectMotionSystem
 */
class FProjectAnimationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
