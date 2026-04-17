// Copyright ALIS. All Rights Reserved.

#pragma once

#include "Services/IAttributeEffectService.h"

/**
 * GAS-backed implementation of IAttributeEffectService.
 * Bridges TMap<FGameplayTag, float> to UProjectGASLibrary::ApplyMagnitudes().
 */
class FAttributeEffectServiceImpl : public IAttributeEffectService
{
public:
	virtual bool ApplyMagnitudes(AActor* TargetActor, const TMap<FGameplayTag, float>& Magnitudes) override;
};
