// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Lightweight helper that watches a JSON layout file and triggers a callback
 * when the file changes on disk. Used to support hot-reloading UI layouts
 * while designing JSON-driven widgets.
 */
class PROJECTUI_API FProjectWidgetLayoutWatcher
{
public:
	/** Begin watching the supplied file. */
	void Start(UObject* Owner, const FString& FilePath, TFunction<void()> OnChanged);

	/** Stop watching the file (no-op if not active). */
	void Stop();

private:
#if WITH_EDITOR
	void HandleDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges);

	FString WatchedDirectory;
	FString WatchedFileName;
	TFunction<void()> ChangeCallback;
	FDelegateHandle WatchHandle;
#endif // WITH_EDITOR
};
