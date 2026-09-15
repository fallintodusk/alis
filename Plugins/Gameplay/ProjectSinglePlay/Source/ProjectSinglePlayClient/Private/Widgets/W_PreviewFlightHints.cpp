// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/W_PreviewFlightHints.h"

#include "Layout/ProjectWidgetLayoutLoader.h"

UW_PreviewFlightHints::UW_PreviewFlightHints(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(
		TEXT("ProjectSinglePlay"), TEXT("PreviewFlightHints.json"));
}
