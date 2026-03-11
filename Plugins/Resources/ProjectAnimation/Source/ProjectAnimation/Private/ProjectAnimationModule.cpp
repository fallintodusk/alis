#include "ProjectAnimationModule.h"

DEFINE_LOG_CATEGORY(LogProjectAnimation);

IMPLEMENT_MODULE(FProjectAnimationModule, ProjectAnimation)

void FProjectAnimationModule::StartupModule()
{
	UE_LOG(LogProjectAnimation, Display, TEXT("ProjectAnimation module startup"));
}

void FProjectAnimationModule::ShutdownModule()
{
	UE_LOG(LogProjectAnimation, Display, TEXT("ProjectAnimation module shutdown"));
}
