#include "ProjectLoadingModule.h"

#include "ProjectLoadingLog.h"

DEFINE_LOG_CATEGORY(LogProjectLoading);

IMPLEMENT_MODULE(FProjectLoadingModule, ProjectLoading)

void FProjectLoadingModule::StartupModule()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoading module startup"));

	// NOTE: No Asset Manager diagnostics here - intentional.
	//
	// StartupModule() runs BEFORE engine init, so UAssetManager::IsInitialized() is always false.
	// Any diagnostic here would just log non-actionable noise.
	//
	// Asset registration uses two paths:
	// - Editor: Config merge via plugin DefaultGame.ini (works reliably, no diagnostic needed)
	// - Cooked builds: Runtime scanning via EnsureAssetScans() in InitialExperienceLoader.cpp
	//
	// Diagnostics for runtime scanning are in EnsureAssetScans() where they run AFTER
	// ScanPathsForPrimaryAssets() and are actually actionable.
	//
	// See: Plugins/Systems/ProjectLoading/docs/asset_manager_registration.md
}

void FProjectLoadingModule::ShutdownModule()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoading module shutdown"));
}
