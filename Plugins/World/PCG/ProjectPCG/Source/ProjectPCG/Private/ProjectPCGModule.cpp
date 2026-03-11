#include "ProjectPCGModule.h"

#define LOCTEXT_NAMESPACE "FProjectPCGModule"

void FProjectPCGModule::StartupModule()
{
	// Stub: will contain PCG engine integration
}

void FProjectPCGModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectPCGModule, ProjectPCG)
