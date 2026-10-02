#include "Modules/ModuleManager.h"

#include "ProjectWorldMeshTerrainRuntimeGate.h"

class FProjectWorldMeshTerrainModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RuntimeGate = MakeUnique<FProjectWorldMeshTerrainRuntimeGate>();
		RuntimeGate->StartIfRequested();
	}

	virtual void ShutdownModule() override
	{
		RuntimeGate.Reset();
	}

private:
	TUniquePtr<FProjectWorldMeshTerrainRuntimeGate> RuntimeGate;
};

IMPLEMENT_MODULE(FProjectWorldMeshTerrainModule, ProjectWorldMeshTerrain)
