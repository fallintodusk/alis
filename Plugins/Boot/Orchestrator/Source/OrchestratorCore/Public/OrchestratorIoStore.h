// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IoStore mounting result.
 */
struct ORCHESTRATORCORE_API FIoStoreMountResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString MountedPath;

	FIoStoreMountResult() = default;

	static FIoStoreMountResult MakeSuccess(const FString& Path)
	{
		FIoStoreMountResult Result;
		Result.bSuccess = true;
		Result.MountedPath = Path;
		return Result;
	}

	static FIoStoreMountResult MakeFailure(const FString& Error)
	{
		FIoStoreMountResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

/**
 * IoStore utilities for Orchestrator.
 * Handles mounting .utoc/.ucas content packages for hot updates.
 *
 * TODO: This is a placeholder implementation that needs to be completed
 * for production hot update support. Currently allows testing with full
 * plugin replacements instead of hot content mounting.
 */
class ORCHESTRATORCORE_API FOrchestratorIoStore
{
public:
	/**
	 * Mount IoStore container (.utoc/.ucas files) at runtime.
	 * Enables hot content updates without engine restart.
	 *
	 * @param ContainerPath Path to .utoc file (or base path without extension)
	 * @param MountPoint Virtual mount point for the container
	 * @return Mount result
	 *
	 * TODO: Implement using FIoDispatcher::Mount() with appropriate backend
	 * Research needed:
	 * - Create IIoDispatcherBackend for .utoc/.ucas files
	 * - Handle container ID generation
	 * - Manage mount priorities
	 * - Handle unmounting and cleanup
	 */
	static FIoStoreMountResult MountContainer(
		const FString& ContainerPath,
		const FString& MountPoint);

	/**
	 * Unmount previously mounted IoStore container.
	 *
	 * @param MountPoint Virtual mount point to unmount
	 * @return true if successful
	 *
	 * TODO: Implement unmounting logic
	 */
	static bool UnmountContainer(const FString& MountPoint);

	/**
	 * Check if IoStore container is currently mounted.
	 *
	 * @param MountPoint Virtual mount point to check
	 * @return true if mounted
	 */
	static bool IsContainerMounted(const FString& MountPoint);

	/**
	 * Get list of currently mounted containers.
	 *
	 * @param OutMountedContainers Array to fill with mounted container paths
	 * @return Number of mounted containers
	 */
	static int32 GetMountedContainers(TArray<FString>& OutMountedContainers);
};
