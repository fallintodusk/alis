#include "ProjectWorldBuildingLayerProducer.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FProjectWorldBuildingEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldBuildingLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldBuildingLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldBuildingEditorModule, ProjectWorldBuildingEditor)
