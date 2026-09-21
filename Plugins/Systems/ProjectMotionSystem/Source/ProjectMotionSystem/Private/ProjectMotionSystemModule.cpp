#include "ProjectMotionSystemModule.h"
#include "Registry/RegisteredClassProviderRegistry.h"

DEFINE_LOG_CATEGORY(LogProjectMotionSystem);

IMPLEMENT_MODULE(FProjectMotionSystemModule, ProjectMotionSystem)

void FProjectMotionSystemModule::StartupModule()
{
	FRegisteredClassProviderRegistry::RegisterProviderModule(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		TEXT("ProjectMotionSystem"));
	UE_LOG(LogProjectMotionSystem, Display, TEXT("ProjectMotionSystem module startup"));
	// Capability validation self-registers via static initializers in component .cpp files
}

void FProjectMotionSystemModule::ShutdownModule()
{
	UE_LOG(LogProjectMotionSystem, Display, TEXT("ProjectMotionSystem module shutdown"));
}
