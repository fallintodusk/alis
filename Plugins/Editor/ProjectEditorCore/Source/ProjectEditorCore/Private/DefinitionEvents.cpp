// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "DefinitionEvents.h"

FOnDefinitionRegenerated FDefinitionEvents::DefinitionRegeneratedDelegate;

FOnDefinitionRegenerated& FDefinitionEvents::OnDefinitionRegenerated()
{
	return DefinitionRegeneratedDelegate;
}
