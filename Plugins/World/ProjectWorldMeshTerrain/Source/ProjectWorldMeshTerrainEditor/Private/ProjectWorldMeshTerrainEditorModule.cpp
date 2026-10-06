#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "ProjectWorldMeshTerrainLayerProducer.h"

class FProjectWorldMeshTerrainEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldMeshTerrainLayerProducer::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldLayerProducer::GetModularFeatureName(), &FProjectWorldMeshTerrainLayerProducer::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldMeshTerrainEditorModule, ProjectWorldMeshTerrainEditor)
