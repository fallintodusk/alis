#include "CombatExperienceDescriptor.h"

#include "Types/ProjectLoadRequest.h"

UCombatExperienceDescriptor::UCombatExperienceDescriptor()
{
	ExperienceName = TEXT("ProjectCombat");

	// Populate soft refs when assets are ready (maps/data/UI). Keeping empty is safe for stub usage.
}

void UCombatExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);
}
