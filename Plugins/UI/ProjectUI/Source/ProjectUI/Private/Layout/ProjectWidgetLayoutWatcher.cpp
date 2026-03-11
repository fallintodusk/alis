// Copyright ALIS. All Rights Reserved.

#include "Layout/ProjectWidgetLayoutWatcher.h"

#if WITH_EDITOR
#include "Async/Async.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"

void FProjectWidgetLayoutWatcher::Start(UObject* Owner, const FString& FilePath, TFunction<void()> OnChanged)
{
	Stop();

	if (!Owner)
	{
		return;
	}

	WatchedDirectory = FPaths::GetPath(FilePath);
	WatchedFileName = FPaths::GetCleanFilename(FilePath);
	ChangeCallback = MoveTemp(OnChanged);

	if (WatchedDirectory.IsEmpty() || WatchedFileName.IsEmpty())
	{
		return;
	}

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (!DirectoryWatcher)
	{
		return;
	}

	if (!DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
			WatchedDirectory,
			IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FProjectWidgetLayoutWatcher::HandleDirectoryChanged),
			WatchHandle,
			IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges))
	{
		WatchHandle.Reset();
	}
}

void FProjectWidgetLayoutWatcher::Stop()
{
	if (!WatchHandle.IsValid())
	{
		return;
	}

	if (FModuleManager::Get().IsModuleLoaded("DirectoryWatcher"))
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		if (IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchedDirectory, WatchHandle);
		}
	}

	WatchHandle.Reset();
	WatchedDirectory.Reset();
	WatchedFileName.Reset();
	ChangeCallback = nullptr;
}

void FProjectWidgetLayoutWatcher::HandleDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	if (!ChangeCallback)
	{
		return;
	}

	for (const FFileChangeData& Change : FileChanges)
	{
		const FString ChangedFileName = FPaths::GetCleanFilename(Change.Filename);
		if (!ChangedFileName.Equals(WatchedFileName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (Change.Action == FFileChangeData::FCA_Removed)
		{
			// A removal is often followed by an add/modified; allow the follow-up event to trigger reload.
			continue;
		}

		TFunction<void()> CallbackCopy = ChangeCallback;
		AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(CallbackCopy)]()
		{
			if (Callback)
			{
				Callback();
			}
		});
		break;
	}
}
#else
void FProjectWidgetLayoutWatcher::Start(UObject*, const FString&, TFunction<void()>)
{
}

void FProjectWidgetLayoutWatcher::Stop()
{
}
#endif // WITH_EDITOR
