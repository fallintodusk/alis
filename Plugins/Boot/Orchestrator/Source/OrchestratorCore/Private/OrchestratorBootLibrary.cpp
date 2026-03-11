// Copyright ALIS. All Rights Reserved.

#include "OrchestratorBootLibrary.h"
#include "OrchestratorCoreModule.h"

// Static singleton instance
UOrchestratorBootEvents* UOrchestratorBootLibrary::BootEventsInstance = nullptr;

void UOrchestratorBootEvents::Initialize()
{
	// Bind to C++ multicast delegates
	ProgressHandle = FOrchestratorCoreModule::OnBootProgressUpdate.AddLambda(
		[this](const FString& CurrentPlugin, int32 LoadedCount, int32 TotalBootPlugins, const FString& StatusMessage)
		{
			// Broadcast to Blueprint dynamic delegate
			OnBootProgress.Broadcast(CurrentPlugin, LoadedCount, TotalBootPlugins, StatusMessage);
		}
	);

	CompleteHandle = FOrchestratorCoreModule::OnBootComplete.AddLambda(
		[this]()
		{
			// Broadcast to Blueprint dynamic delegate
			OnBootComplete.Broadcast();
		}
	);
}

void UOrchestratorBootEvents::Shutdown()
{
	// Unbind from C++ multicast delegates
	if (ProgressHandle.IsValid())
	{
		FOrchestratorCoreModule::OnBootProgressUpdate.Remove(ProgressHandle);
		ProgressHandle.Reset();
	}

	if (CompleteHandle.IsValid())
	{
		FOrchestratorCoreModule::OnBootComplete.Remove(CompleteHandle);
		CompleteHandle.Reset();
	}
}

UOrchestratorBootEvents* UOrchestratorBootLibrary::GetOrchestratorBootEvents(UObject* WorldContextObject)
{
	if (!BootEventsInstance)
	{
		// Create singleton instance
		BootEventsInstance = NewObject<UOrchestratorBootEvents>(GetTransientPackage());
		BootEventsInstance->AddToRoot(); // Prevent garbage collection
		BootEventsInstance->Initialize();
	}

	return BootEventsInstance;
}
