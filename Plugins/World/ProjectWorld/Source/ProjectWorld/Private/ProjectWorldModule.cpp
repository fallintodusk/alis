#include "Modules/ModuleManager.h"
#include "Presentation/ProjectWorldPresentationGate.h"
#include "Presentation/ProjectWorldProductPerformanceGate.h"
#include "Presentation/ProjectWorldProductRouteGate.h"
#include "Presentation/ProjectWorldShippingWaterProofGate.h"

class FProjectWorldModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		PresentationGate = MakeUnique<FProjectWorldPresentationGate>();
		PresentationGate->StartIfRequested();
		ProductPerformanceGate = MakeUnique<FProjectWorldProductPerformanceGate>();
		ProductPerformanceGate->StartIfRequested();
		ProductRouteGate = MakeUnique<FProjectWorldProductRouteGate>();
		ProductRouteGate->StartIfRequested();
		ShippingWaterProofGate = MakeUnique<FProjectWorldShippingWaterProofGate>();
		ShippingWaterProofGate->StartIfRequested();
	}

	virtual void ShutdownModule() override
	{
		PresentationGate.Reset();
		ProductPerformanceGate.Reset();
		ProductRouteGate.Reset();
		ShippingWaterProofGate.Reset();
	}

private:
	TUniquePtr<FProjectWorldPresentationGate> PresentationGate;
	TUniquePtr<FProjectWorldProductPerformanceGate> ProductPerformanceGate;
	TUniquePtr<FProjectWorldProductRouteGate> ProductRouteGate;
	TUniquePtr<FProjectWorldShippingWaterProofGate> ShippingWaterProofGate;
};

IMPLEMENT_MODULE(FProjectWorldModule, ProjectWorld)
