#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "ProjectWorldMeshTerrainEvidenceSubject.h"

class FProjectWorldMeshTerrainEvidenceModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IProjectWorldEvidenceSubject::GetModularFeatureName(), &FProjectWorldMeshTerrainEvidenceSubject::Get());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IProjectWorldEvidenceSubject::GetModularFeatureName(), &FProjectWorldMeshTerrainEvidenceSubject::Get());
	}
};

IMPLEMENT_MODULE(FProjectWorldMeshTerrainEvidenceModule, ProjectWorldMeshTerrainEvidence)
