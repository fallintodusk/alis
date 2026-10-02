#include "Modules/ModuleManager.h"

#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldTerrainProducerRegistry.h"

class FProjectWorldMeshTerrainEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FString Error;
		if (!ProjectWorldTerrainProducerRegistry::Register(
			ProjectWorldMeshTerrainProducer::Contract(), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("ProjectWorld Mesh Terrain registration failed: %s"), *Error);
		}
	}

	virtual void ShutdownModule() override
	{
		ProjectWorldTerrainProducerRegistry::Unregister(TEXT("project_mesh_terrain"), 1);
	}
};

IMPLEMENT_MODULE(FProjectWorldMeshTerrainEditorModule, ProjectWorldMeshTerrainEditor)
