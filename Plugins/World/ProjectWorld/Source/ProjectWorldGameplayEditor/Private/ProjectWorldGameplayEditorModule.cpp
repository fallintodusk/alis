#include "ProjectWorldGameplayLayerProducer.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FProjectWorldGameplayEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldGameplayLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldGameplayLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldGameplayEditorModule, ProjectWorldGameplayEditor)
