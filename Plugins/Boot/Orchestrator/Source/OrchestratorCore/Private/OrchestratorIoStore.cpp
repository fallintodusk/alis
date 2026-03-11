// Copyright ALIS. All Rights Reserved.

#include "OrchestratorIoStore.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorIoStore, Log, All);

FIoStoreMountResult FOrchestratorIoStore::MountContainer(
	const FString& ContainerPath,
	const FString& MountPoint)
{
	UE_LOG(LogOrchestratorIoStore, Log, TEXT("Mounting IoStore container: %s"), *ContainerPath);
	UE_LOG(LogOrchestratorIoStore, Log, TEXT("  Mount point: %s"), *MountPoint);

	// Verify .utoc file exists
	FString UtocPath = ContainerPath;
	if (!UtocPath.EndsWith(TEXT(".utoc")))
	{
		UtocPath += TEXT(".utoc");
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*UtocPath))
	{
		const FString Error = FString::Printf(TEXT("Container .utoc file not found: %s"), *UtocPath);
		UE_LOG(LogOrchestratorIoStore, Error, TEXT("%s"), *Error);
		return FIoStoreMountResult::MakeFailure(Error);
	}

	// Verify .ucas file exists
	FString UcasPath = FPaths::ChangeExtension(UtocPath, TEXT(".ucas"));
	if (!PlatformFile.FileExists(*UcasPath))
	{
		const FString Error = FString::Printf(TEXT("Container .ucas file not found: %s"), *UcasPath);
		UE_LOG(LogOrchestratorIoStore, Error, TEXT("%s"), *Error);
		return FIoStoreMountResult::MakeFailure(Error);
	}

	UE_LOG(LogOrchestratorIoStore, Log, TEXT("  Found .utoc: %s"), *UtocPath);
	UE_LOG(LogOrchestratorIoStore, Log, TEXT("  Found .ucas: %s"), *UcasPath);

	// TODO: Implement actual IoStore mounting
	// Steps needed:
	// 1. Create FIoStoreReader for the .utoc/.ucas pair
	// 2. Create appropriate IIoDispatcherBackend
	// 3. Call FIoDispatcher::Mount() with the backend
	// 4. Store mount info for later unmounting
	//
	// Research notes:
	// - FIoDispatcher is the global I/O system singleton
	// - Mount() takes a TSharedRef<IIoDispatcherBackend> and priority
	// - Need to understand container IDs and how they relate to mount points
	// - Look at FPakPlatformFile or ChunkDownloader for mounting examples
	//
	// For now, this placeholder validates the files exist but doesn't actually mount

	UE_LOG(LogOrchestratorIoStore, Warning, TEXT("IoStore mounting not yet implemented - hot updates require restart"));
	UE_LOG(LogOrchestratorIoStore, Warning, TEXT("For now, use full plugin replacement instead of hot content mounting"));

	// Return success with note that mounting is not yet functional
	return FIoStoreMountResult::MakeSuccess(MountPoint);
}

bool FOrchestratorIoStore::UnmountContainer(const FString& MountPoint)
{
	UE_LOG(LogOrchestratorIoStore, Log, TEXT("Unmounting IoStore container: %s"), *MountPoint);

	// TODO: Implement unmounting
	// - Remove backend from FIoDispatcher
	// - Clean up mount info tracking
	// - Ensure no references remain to unmounted content

	UE_LOG(LogOrchestratorIoStore, Warning, TEXT("IoStore unmounting not yet implemented"));
	return false;
}

bool FOrchestratorIoStore::IsContainerMounted(const FString& MountPoint)
{
	// TODO: Implement mount status tracking
	// - Check if container is currently mounted in FIoDispatcher
	// - Query mount info structure

	return false;
}

int32 FOrchestratorIoStore::GetMountedContainers(TArray<FString>& OutMountedContainers)
{
	OutMountedContainers.Empty();

	// TODO: Implement mount list retrieval
	// - Query FIoDispatcher for all mounted containers
	// - Return their mount points

	return 0;
}
