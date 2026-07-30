// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Services/IAttributeEffectService.h"

IAttributeEffectService::IAttributeEffectService() = default;
IAttributeEffectService::~IAttributeEffectService() = default;

FName IAttributeEffectService::ServiceKey()
{
	static FName Key(TEXT("IAttributeEffectService"));
	return Key;
}
