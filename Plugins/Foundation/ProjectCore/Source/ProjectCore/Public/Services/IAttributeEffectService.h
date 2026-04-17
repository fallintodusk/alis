// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * IAttributeEffectService
 *
 * Abstract interface for applying attribute magnitudes to actors via GAS.
 * Follows Dependency Inversion Principle - consumers depend on abstraction.
 *
 * Architecture:
 *   ProjectObjectCapabilities (Gameplay, consumer)
 *       | depends on abstraction
 *   IAttributeEffectService (ProjectCore interface)
 *       ^ implemented by
 *   ProjectGAS (Gameplay, provider)
 *
 * Benefits:
 * - Capabilities can apply GAS effects without depending on ProjectGAS
 * - Same data contract as item magnitudes (TMap<FGameplayTag, float>)
 * - Can mock for testing
 * - Fails soft when GAS is unavailable
 *
 * Usage:
 * @code
 * TSharedPtr<IAttributeEffectService> EffectService =
 *     FProjectServiceLocator::Resolve<IAttributeEffectService>();
 *
 * if (EffectService)
 * {
 *     TMap<FGameplayTag, float> Magnitudes;
 *     Magnitudes.Add(ProjectTags::SetByCaller_Condition, -10.0f);
 *     EffectService->ApplyMagnitudes(TargetActor, Magnitudes);
 * }
 * @endcode
 *
 * @see FProjectServiceLocator
 * @see UProjectGASLibrary::ApplyMagnitudes
 */
class PROJECTCORE_API IAttributeEffectService
{
protected:
	IAttributeEffectService();

public:
	virtual ~IAttributeEffectService();

	static FName ServiceKey();


	/**
	 * Apply attribute magnitudes to an actor's AbilitySystemComponent.
	 *
	 * @param TargetActor Actor to apply effects to (must have ASC)
	 * @param Magnitudes SetByCaller tag to value mapping
	 * @return true if applied successfully, false if no ASC or application failed
	 */
	virtual bool ApplyMagnitudes(AActor* TargetActor, const TMap<FGameplayTag, float>& Magnitudes) = 0;
};
