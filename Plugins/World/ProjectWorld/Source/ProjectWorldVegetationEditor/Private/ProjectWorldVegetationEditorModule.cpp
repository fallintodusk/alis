#include "ProjectWorldVegetationLayerProducer.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FProjectWorldVegetationEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldVegetationLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldVegetationLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldVegetationEditorModule, ProjectWorldVegetationEditor)
