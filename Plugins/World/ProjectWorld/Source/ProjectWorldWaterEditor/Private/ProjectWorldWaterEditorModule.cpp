#include "ProjectWorldWaterLayerProducer.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FProjectWorldWaterEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldWaterLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldWaterLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldWaterEditorModule, ProjectWorldWaterEditor)
