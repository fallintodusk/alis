// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/ProjectMenuExperienceLaunchBinding.h"

#include "Components/Button.h"
#include "Subsystems/MenuMainComposerSubsystem.h"
#include "ProjectMenuMainLog.h"

void UProjectMenuExperienceLaunchBinding::BindTo(
	UButton& InButton,
	UMenuMainComposerSubsystem* InComposer,
	const FString& InExperienceId,
	const FString& InMode)
{
	Button = &InButton;
	Composer = InComposer;
	ExperienceId = InExperienceId;
	Mode = InMode;
	InButton.OnClicked.AddUniqueDynamic(this, &UProjectMenuExperienceLaunchBinding::HandleClicked);
}

void UProjectMenuExperienceLaunchBinding::Unbind()
{
	if (UButton* BoundButton = Button.Get())
	{
		BoundButton->OnClicked.RemoveDynamic(this, &UProjectMenuExperienceLaunchBinding::HandleClicked);
	}
	Button.Reset();
}

void UProjectMenuExperienceLaunchBinding::HandleClicked()
{
	if (ExperienceId.IsEmpty())
	{
		UE_LOG(LogProjectMenuMain, Error,
			TEXT("[ExperienceLaunchBinding] Ignored click - no experience id configured"));
		return;
	}

	if (!Composer.IsValid())
	{
		UE_LOG(LogProjectMenuMain, Error,
			TEXT("[ExperienceLaunchBinding] Failed - MenuMainComposerSubsystem unavailable for '%s'"),
			*ExperienceId);
		return;
	}

	UE_LOG(LogProjectMenuMain, Display,
		TEXT("[ExperienceLaunchBinding] Requesting experience - %s"), *ExperienceId);
	Composer->RequestStartGame(ExperienceId, Mode);
}
