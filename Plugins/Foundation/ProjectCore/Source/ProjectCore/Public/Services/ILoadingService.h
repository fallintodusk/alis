// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ILoadingHandle;
struct FLoadRequest;

/**
 * ILoadingService
 *
 * Abstract interface for the loading system following the Dependency Inversion Principle (DIP).
 * This interface allows high-level Feature plugins to trigger loading operations without
 * directly depending on the low-level ProjectLoading system plugin.
 *
 * Architecture:
 *   ProjectMenuMain (Feature, high-level)
 *       ↓ depends on abstraction
 *   ILoadingService (ProjectCore interface)
 *       ↑ implemented by
 *   UProjectLoadingSubsystem (System, low-level)
 *
 * Benefits:
 * - Features can trigger loading without knowing ProjectLoading exists
 * - Can swap implementations or mock for testing
 * - Proper tier separation (Features don't depend on Systems)
 * - Follows SOLID principles
 *
 * Usage Example:
 * @code
 * // In ProjectMenuMain (Feature plugin)
 * TSharedPtr<ILoadingService> LoadingService =
 *     FProjectServiceLocator::Resolve<ILoadingService>();
 *
 * if (LoadingService)
 * {
 *     FLoadRequest Request;
 *     Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("GameplayMap"));
 *     Request.LoadMode = ELoadMode::SinglePlayer;
 *
 *     TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);
 * }
 * @endcode
 *
 * Implementation:
 * - Implemented by UProjectLoadingSubsystem
 * - Registered in FProjectServiceLocator during subsystem Initialize()
 * - Unregistered during subsystem Deinitialize()
 *
 * @see UProjectLoadingSubsystem
 * @see FProjectServiceLocator
 * @see ILoadingHandle
 * @see FLoadRequest
 */
class PROJECTCORE_API ILoadingService
{
protected:
	ILoadingService();

public:
	virtual ~ILoadingService();

	/** DLL-stable key for ServiceLocator registration/lookup. */
	static FName ServiceKey()
	{
		static FName Key(TEXT("ILoadingService"));
		return Key;
	}

	/**
	 * Begin processing a load request.
	 * Expects FLoadRequest to reference valid manifests (UProjectWorldManifest from ProjectWorld).
	 * Returns an async handle for monitoring/cancellation.
	 *
	 * @param Request The load request describing what to load and how
	 * @return Shared pointer to loading handle for tracking progress and cancellation
	 */
	virtual TSharedPtr<ILoadingHandle> StartLoad(const FLoadRequest& Request) = 0;

	/**
	 * Cancel the active load if one exists.
	 *
	 * @param bForce If true, force immediate cancellation without cleanup
	 * @return True if cancellation was initiated, false if no load was active
	 */
	virtual bool CancelActiveLoad(bool bForce = false) = 0;

	/**
	 * Check if a load request is currently running.
	 *
	 * @return True when a load operation is in progress
	 */
	virtual bool IsLoadInProgress() const = 0;

	/**
	 * Get the active loading handle for observers.
	 * May be null if no load is running.
	 *
	 * @return Shared pointer to active loading handle, or null if none
	 */
	virtual TSharedPtr<ILoadingHandle> GetActiveLoadHandle() const = 0;

	/**
	 * Build a load request from an experience name.
	 * This ensures asset scans are registered (runtime discovery for modular content)
	 * and MapAssetId is properly resolved from the experience descriptor.
	 *
	 * Use this instead of manually building FLoadRequest when loading an experience.
	 *
	 * @param ExperienceName Name of the experience (e.g., "City17", "MainMenuWorld")
	 * @param OutRequest Populated load request with resolved MapAssetId
	 * @param OutError Error message if build fails
	 * @return True if request was built successfully
	 */
	virtual bool BuildLoadRequestForExperience(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError) = 0;
};
