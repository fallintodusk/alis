// Copyright Fall.is. All Rights Reserved.

#include "Interfaces/IOrchestratorRegistry.h"
#include "HAL/CriticalSection.h"

// Global registry instance (set by OrchestratorCore at startup)
static IOrchestratorRegistry* GRegistryInstance = nullptr;
static FCriticalSection GRegistryMutex;

IOrchestratorRegistry* GetOrchestratorRegistry()
{
	FScopeLock Lock(&GRegistryMutex);
	return GRegistryInstance;
}

void SetOrchestratorRegistry(IOrchestratorRegistry* Registry)
{
	FScopeLock Lock(&GRegistryMutex);
	GRegistryInstance = Registry;
}
