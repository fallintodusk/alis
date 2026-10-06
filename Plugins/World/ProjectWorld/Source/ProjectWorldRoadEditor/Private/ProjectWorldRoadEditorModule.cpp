#include "ProjectWorldRoadLayerProducer.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FProjectWorldRoadEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldRoadLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldRoadLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldRoadEditorModule, ProjectWorldRoadEditor)
