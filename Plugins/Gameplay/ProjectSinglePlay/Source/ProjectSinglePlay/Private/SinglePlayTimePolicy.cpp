#include "SinglePlayerGameMode.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Engine/World.h"

bool ASinglePlayerGameMode::AllowsEnvironmentalTimeControl() const
{
	const auto Loading = FProjectServiceLocator::Resolve<ILoadingService>();
	FName ExperienceName;
	const UWorld* ExperienceWorld = nullptr;
	if (!Loading || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone ||
		Loading->IsLoadInProgress() ||
		!Loading->TryGetLoadedExperienceContext(ExperienceName, ExperienceWorld) || ExperienceWorld != GetWorld()) return false;
	const auto* Experience = UProjectExperienceRegistry::Get()->FindDescriptor(ExperienceName);
	return Experience && Experience->AllowEnvironmentalTimeControls &&
		Experience->LoadAssets.Map.ToSoftObjectPath().GetLongPackageName() ==
		UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
}
