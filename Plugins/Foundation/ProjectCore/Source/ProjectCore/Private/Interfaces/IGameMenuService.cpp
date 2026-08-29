// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Interfaces/IGameMenuService.h"

IGameMenuService::IGameMenuService() = default;
IGameMenuService::~IGameMenuService() = default;

FName IGameMenuService::ServiceKey()
{
	static const FName Key(TEXT("IGameMenuService"));
	return Key;
}
