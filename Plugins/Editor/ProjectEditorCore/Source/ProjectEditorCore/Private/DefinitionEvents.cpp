// Copyright ALIS. All Rights Reserved.

#include "DefinitionEvents.h"

FOnDefinitionRegenerated FDefinitionEvents::DefinitionRegeneratedDelegate;

FOnDefinitionRegenerated& FDefinitionEvents::OnDefinitionRegenerated()
{
	return DefinitionRegeneratedDelegate;
}
