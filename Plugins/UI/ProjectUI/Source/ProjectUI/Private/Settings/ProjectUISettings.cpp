// Copyright ALIS. All Rights Reserved.

#include "Settings/ProjectUISettings.h"

#define LOCTEXT_NAMESPACE "ProjectUISettings"

UProjectUISettings::UProjectUISettings()
{
	HUDLayoutClass = FSoftClassPath(TEXT("/Script/ProjectHUD.W_HUDLayout"));
}

#if WITH_EDITOR
FName UProjectUISettings::GetCategoryName() const
{
	return TEXT("Project");
}

FText UProjectUISettings::GetSectionText() const
{
	return LOCTEXT("ProjectUISettingsSection", "Project UI");
}
#endif

#undef LOCTEXT_NAMESPACE
