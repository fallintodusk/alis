#include "DialogueExperienceDescriptor.h"

#include "Types/ProjectLoadRequest.h"

UDialogueExperienceDescriptor::UDialogueExperienceDescriptor()
{
	ExperienceName = TEXT("ProjectDialogue");
	// Add soft refs when dialogue assets are ready (UI/data/etc.).
}

void UDialogueExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);
}
