// Copyright ALIS. All Rights Reserved.

#include "DefinitionGeneratorEditorSubsystem.h"
#include "DefinitionGeneratorSubsystem.h"
#include "SyncEvents.h"
#include "SyncHashUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionGeneratorEditor, Log, All);

void UDefinitionGeneratorEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("DefinitionGeneratorEditorSubsystem initialized"));

	// Register for post engine init to run startup validation
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UDefinitionGeneratorEditorSubsystem::OnPostEngineInit);

	SyncBatchHandle = FSyncEvents::OnSyncBatchApplied().AddUObject(this, &UDefinitionGeneratorEditorSubsystem::OnSyncBatchApplied);
}

void UDefinitionGeneratorEditorSubsystem::Deinitialize()
{
	// Unregister directory watchers
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		for (const auto& Pair : DirectoryWatcherHandles)
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
		}
	}
	DirectoryWatcherHandles.Empty();

	// Clear timer
	if (GEditor && DebounceTimerHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
	}

	if (SyncBatchHandle.IsValid())
	{
		FSyncEvents::OnSyncBatchApplied().Remove(SyncBatchHandle);
		SyncBatchHandle.Reset();
	}

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	Super::Deinitialize();
}

void UDefinitionGeneratorEditorSubsystem::OnPostEngineInit()
{
	// First discover and register all manifests
	if (UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get())
	{
		Generator->DiscoverAndRegisterManifests();
	}

	// Setup directory watchers for all registered types
	SetupDirectoryWatchers();

	// Wait for AssetRegistry to complete scan before validation
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.OnFilesLoaded().AddUObject(this, &UDefinitionGeneratorEditorSubsystem::OnAssetRegistryFilesLoaded);
		UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Deferring startup validation until AssetRegistry scan completes"));
	}
	else
	{
		RunStartupValidation();
	}
}

void UDefinitionGeneratorEditorSubsystem::OnAssetRegistryFilesLoaded()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);

	RunStartupValidation();
}

void UDefinitionGeneratorEditorSubsystem::RunStartupValidation()
{
	TArray<FDefinitionValidationResult> Results = ValidateAll();

	// Count out-of-date per type
	OutOfDateCounts.Empty();
	for (const FDefinitionValidationResult& Result : Results)
	{
		if (Result.bNeedsRegeneration)
		{
			int32& Count = OutOfDateCounts.FindOrAdd(Result.TypeName);
			Count++;
		}
	}

	int32 TotalOutOfDate = GetTotalOutOfDateCount();
	if (TotalOutOfDate > 0 && !bStartupNotificationShown)
	{
		ShowOutOfDateNotification();
		bStartupNotificationShown = true;
	}
}

void UDefinitionGeneratorEditorSubsystem::SetupDirectoryWatchers()
{
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return;
	}

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (!DirectoryWatcher)
	{
		return;
	}

	TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
	for (const FString& TypeName : TypeNames)
	{
		const FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
		if (SourceDir.IsEmpty() || DirectoryWatcherHandles.Contains(SourceDir))
		{
			continue;
		}

		FDelegateHandle Handle;
		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
			SourceDir,
			IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::OnDirectoryChanged),
			Handle,
			IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges
		);

		if (Handle.IsValid())
		{
			DirectoryWatcherHandles.Add(SourceDir, Handle);
			UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Watching directory: %s"), *SourceDir);
		}
	}
}

TArray<FDefinitionValidationResult> UDefinitionGeneratorEditorSubsystem::ValidateType(const FString& TypeName)
{
	TArray<FDefinitionValidationResult> Results;

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		UE_LOG(LogDefinitionGeneratorEditor, Warning, TEXT("ValidateType: DefinitionGeneratorSubsystem not available"));
		return Results;
	}

	FDefinitionTypeInfo TypeInfo;
	if (!Generator->GetDefinitionTypeInfo(TypeName, TypeInfo))
	{
		UE_LOG(LogDefinitionGeneratorEditor, Warning, TEXT("ValidateType: Unknown type '%s'"), *TypeName);
		return Results;
	}

	const FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] Validating - SourceDir: %s"), *TypeName, *SourceDir);
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] SourceDir exists: %s"), *TypeName, IFileManager::Get().DirectoryExists(*SourceDir) ? TEXT("YES") : TEXT("NO"));

	// Find JSON files (use SourceFileGlob if set, otherwise all *.json)
	const FString FileGlob = TypeInfo.SourceFileGlob.IsEmpty() ? TEXT("*.json") : TypeInfo.SourceFileGlob;
	TArray<FString> AllJsonFiles;
	IFileManager::Get().FindFilesRecursive(AllJsonFiles, *SourceDir, *FileGlob, true, false);
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] Found %d %s files in source dir"), *TypeName, AllJsonFiles.Num(), *FileGlob);

	// Collect globs claimed by other types sharing the same source directory
	const TArray<FString> ExclusionGlobs = Generator->GetExclusionGlobsForType(TypeName);

	// Filter out schema files, generator manifests, and files belonging to other types
	TArray<FString> JsonFiles;
	for (const FString& FilePath : AllJsonFiles)
	{
		const FString FileName = FPaths::GetCleanFilename(FilePath);
		if (FileName.EndsWith(TEXT(".schema.json")) || FileName.EndsWith(TEXT(".type.json")))
		{
			continue;
		}
		if (UDefinitionGeneratorSubsystem::MatchesAnyGlob(FileName, ExclusionGlobs))
		{
			continue;
		}
		JsonFiles.Add(FilePath);
	}
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] After filtering: %d source JSON files"), *TypeName, JsonFiles.Num());

	for (const FString& JsonFile : JsonFiles)
	{
		FDefinitionValidationResult Result;
		Result.TypeName = TypeName;
		Result.JsonPath = JsonFile;

		// Parse JSON to get the ID
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *JsonFile))
		{
			Result.AssetId = FPaths::GetBaseFilename(JsonFile);
			Result.bNeedsRegeneration = true;
			Result.Reason = TEXT("Could not read JSON file");
			Results.Add(Result);
			continue;
		}

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			Result.AssetId = FPaths::GetBaseFilename(JsonFile);
			Result.bNeedsRegeneration = true;
			Result.Reason = TEXT("Invalid JSON");
			Results.Add(Result);
			continue;
		}

		FString ResolveError;
		if (!UDefinitionGeneratorSubsystem::ResolveAssetIdFromJson(TypeInfo, JsonObject, Result.AssetId, ResolveError))
		{
			Result.AssetId = FPaths::GetBaseFilename(JsonFile);
			Result.bNeedsRegeneration = true;
			Result.Reason = ResolveError;
			Results.Add(Result);
			continue;
		}

		// Find existing asset and check hash/version
		UObject* ExistingAsset = Generator->FindExistingAssetForType(TypeName, Result.AssetId);
		if (ExistingAsset)
		{
			// Get stored hash and version via reflection
			FString StoredHash;
			int32 StoredVersion = 0;

			if (FStrProperty* HashProp = CastField<FStrProperty>(TypeInfo.DefinitionClass->FindPropertyByName(*TypeInfo.HashPropertyName)))
			{
				StoredHash = HashProp->GetPropertyValue_InContainer(ExistingAsset);
			}
			if (FIntProperty* VersionProp = CastField<FIntProperty>(TypeInfo.DefinitionClass->FindPropertyByName(*TypeInfo.VersionPropertyName)))
			{
				StoredVersion = VersionProp->GetPropertyValue_InContainer(ExistingAsset);
			}

			// Compare hash
			const FString CurrentHash = UDefinitionGeneratorSubsystem::ComputeFileHash(JsonFile);
			if (!CurrentHash.IsEmpty())
			{
				if (StoredHash != CurrentHash)
				{
					Result.bNeedsRegeneration = true;
					Result.Reason = TEXT("JSON content changed");
				}
				else if (StoredVersion < TypeInfo.GeneratorVersion)
				{
					Result.bNeedsRegeneration = true;
					Result.Reason = FString::Printf(TEXT("Generator version outdated (%d < %d)"),
						StoredVersion, TypeInfo.GeneratorVersion);
				}
			}
			else
			{
				Result.bNeedsRegeneration = true;
				Result.Reason = TEXT("Could not compute hash");
			}
		}
		else
		{
			Result.bNeedsRegeneration = true;
			Result.Reason = TEXT("No generated asset exists");
		}

		Results.Add(Result);
	}

	// Log summary
	int32 OutOfDate = 0;
	for (const FDefinitionValidationResult& Result : Results)
	{
		if (Result.bNeedsRegeneration)
		{
			OutOfDate++;
		}
	}

	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] Validation: %d total, %d out of date"), *TypeName, Results.Num(), OutOfDate);

	return Results;
}

TArray<FDefinitionValidationResult> UDefinitionGeneratorEditorSubsystem::ValidateAll()
{
	TArray<FDefinitionValidationResult> AllResults;

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return AllResults;
	}

	TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
	for (const FString& TypeName : TypeNames)
	{
		AllResults.Append(ValidateType(TypeName));
	}

	return AllResults;
}

int32 UDefinitionGeneratorEditorSubsystem::RegenerateChanged(const FString& TypeName)
{
	CleanupOrphans(TypeName);

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return 0;
	}

	// Generator broadcasts OnDefinitionRegenerated for each regenerated asset
	// DefinitionActorSyncSubsystem (ProjectPlacementEditor) handles actor updates
	TArray<FDefinitionGenerationResult> Results = Generator->GenerateAllForType(TypeName, false);

	int32 Regenerated = 0;
	for (const FDefinitionGenerationResult& Result : Results)
	{
		if (Result.bSuccess && !Result.bSkipped)
		{
			Regenerated++;
		}
	}

	OutOfDateCounts.Remove(TypeName);
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] Regenerated %d items"), *TypeName, Regenerated);
	return Regenerated;
}

int32 UDefinitionGeneratorEditorSubsystem::RegenerateAll(const FString& TypeName)
{
	CleanupOrphans(TypeName);

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return 0;
	}

	// Generator broadcasts OnDefinitionRegenerated for each regenerated asset
	// DefinitionActorSyncSubsystem (ProjectPlacementEditor) handles actor updates
	TArray<FDefinitionGenerationResult> Results = Generator->GenerateAllForType(TypeName, true);

	int32 Regenerated = 0;
	for (const FDefinitionGenerationResult& Result : Results)
	{
		if (Result.bSuccess)
		{
			Regenerated++;
		}
	}

	OutOfDateCounts.Remove(TypeName);
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("[%s] Force regenerated %d items"), *TypeName, Regenerated);
	return Regenerated;
}

int32 UDefinitionGeneratorEditorSubsystem::CleanupOrphans(const FString& TypeName)
{
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return 0;
	}
	return Generator->CleanupOrphanedAssets(TypeName);
}

void UDefinitionGeneratorEditorSubsystem::OpenSourceFolder(const FString& TypeName)
{
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return;
	}

	const FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
	if (!SourceDir.IsEmpty())
	{
		FPlatformProcess::ExploreFolder(*SourceDir);
	}
}

int32 UDefinitionGeneratorEditorSubsystem::GetOutOfDateCount(const FString& TypeName) const
{
	const int32* Count = OutOfDateCounts.Find(TypeName);
	return Count ? *Count : 0;
}

int32 UDefinitionGeneratorEditorSubsystem::GetTotalOutOfDateCount() const
{
	int32 Total = 0;
	for (const auto& Pair : OutOfDateCounts)
	{
		Total += Pair.Value;
	}
	return Total;
}

void UDefinitionGeneratorEditorSubsystem::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	for (const FFileChangeData& Change : FileChanges)
	{
		// Only process data JSON files, not schemas or manifests
		if (Change.Filename.EndsWith(TEXT(".json")) &&
			!Change.Filename.EndsWith(TEXT(".schema.json")) &&
			!Change.Filename.EndsWith(TEXT(".type.json")))
		{
			const FString NormalizedFilename = FSyncHashUtils::NormalizeFilePath(Change.Filename);
			DirtyFiles.Add(NormalizedFilename);
			UE_LOG(LogDefinitionGeneratorEditor, Verbose, TEXT("JSON file changed: %s"), *NormalizedFilename);
		}
	}

	// Debounce: wait 0.5s before processing
	if (DirtyFiles.Num() > 0 && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
		GEditor->GetTimerManager()->SetTimer(
			DebounceTimerHandle,
			FTimerDelegate::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::ProcessDirtyFiles),
			0.5f,
			false
		);
	}
}

void UDefinitionGeneratorEditorSubsystem::ProcessDirtyFiles()
{
	if (DirtyFiles.Num() == 0)
	{
		return;
	}

	if (IgnoredFiles.Num() > 0)
	{
		for (auto It = DirtyFiles.CreateIterator(); It; ++It)
		{
			const FSyncIgnoredFile* IgnoreEntry = IgnoredFiles.Find(*It);
			if (!IgnoreEntry)
			{
				continue;
			}

			const FString CurrentHash = UDefinitionGeneratorSubsystem::ComputeFileHash(*It);
			if (!CurrentHash.IsEmpty() && CurrentHash == IgnoreEntry->ContentHash)
			{
				UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Ignoring sync change (%s): %s"),
					*IgnoreEntry->Origin.ToString(), **It);
				IgnoredFiles.Remove(*It);
				It.RemoveCurrent();
			}
			else
			{
				UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Sync hash mismatch (%s). Processing file: %s"),
					*IgnoreEntry->Origin.ToString(), **It);
				IgnoredFiles.Remove(*It);
			}
		}
	}

	if (DirtyFiles.Num() == 0)
	{
		return;
	}

	bDismissedUntilChange = false;
	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Watcher detected %d file changes, running full validation"), DirtyFiles.Num());

	// Clear the watcher-detected files - we'll do a full scan instead
	DirtyFiles.Empty();

	// Do a full validation of ALL JSON files (same as startup)
	// This ensures we catch all out-of-date files, not just the ones the watcher detected
	TArray<FDefinitionValidationResult> Results = ValidateAll();

	// Collect all files that need regeneration
	PendingRegenerationFiles.Empty();
	OutOfDateCounts.Empty();

	for (const FDefinitionValidationResult& Result : Results)
	{
		if (Result.bNeedsRegeneration)
		{
			PendingRegenerationFiles.Add(Result.JsonPath);
			int32& Count = OutOfDateCounts.FindOrAdd(Result.TypeName);
			Count++;
		}
	}

	int32 TotalOutOfDate = PendingRegenerationFiles.Num();
	if (TotalOutOfDate > 0)
	{
		// Build a user-friendly message with file names
		FString FileList;
		for (int32 i = 0; i < FMath::Min(TotalOutOfDate, 3); ++i)
		{
			if (!FileList.IsEmpty())
			{
				FileList += TEXT(", ");
			}
			FileList += FPaths::GetBaseFilename(PendingRegenerationFiles[i]);
		}
		if (TotalOutOfDate > 3)
		{
			FileList += FString::Printf(TEXT(" (+%d more)"), TotalOutOfDate - 3);
		}

		// Show notification for ALL out-of-date files
		ShowChangedFilesNotification(TotalOutOfDate, FileList);
	}
}

void UDefinitionGeneratorEditorSubsystem::OnSyncBatchApplied(FName Origin, const TArray<FSyncBatchFile>& ChangedFiles)
{
	if (ChangedFiles.Num() == 0)
	{
		return;
	}

	UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Received sync batch (%s): %d file(s)"),
		*Origin.ToString(), ChangedFiles.Num());

	for (const FSyncBatchFile& FileEntry : ChangedFiles)
	{
		FSyncIgnoredFile IgnoreEntry;
		IgnoreEntry.Origin = Origin;
		IgnoreEntry.ContentHash = FileEntry.ContentHash;
		IgnoredFiles.Add(FSyncHashUtils::NormalizeFilePath(FileEntry.FilePath), MoveTemp(IgnoreEntry));

		UE_LOG(LogDefinitionGeneratorEditor, Verbose, TEXT("Will ignore sync change (%s): %s"),
			*Origin.ToString(), *FileEntry.FilePath);
	}
}

void UDefinitionGeneratorEditorSubsystem::ShowOutOfDateNotification()
{
	if (bDismissedUntilChange)
	{
		return;
	}

	// Close existing notification
	if (TSharedPtr<SNotificationItem> Existing = ActiveNotification.Pin())
	{
		Existing->ExpireAndFadeout();
	}

	// Build message showing counts per type
	int32 TotalCount = GetTotalOutOfDateCount();
	FString TypeBreakdown;
	for (const auto& Pair : OutOfDateCounts)
	{
		if (!TypeBreakdown.IsEmpty())
		{
			TypeBreakdown += TEXT(", ");
		}
		TypeBreakdown += FString::Printf(TEXT("%s: %d"), *Pair.Key, Pair.Value);
	}

	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionGenerator", "OutOfDateNotification", "Definitions out of date: {0} ({1})"),
		FText::AsNumber(TotalCount),
		FText::FromString(TypeBreakdown)
	));

	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 10.0f;
	Info.FadeOutDuration = 1.0f;

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionGenerator", "RegenerateButton", "Regenerate All"),
		NSLOCTEXT("DefinitionGenerator", "RegenerateTooltip", "Regenerate all out-of-date definitions"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::OnRegenerateClicked)
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionGenerator", "DismissButton", "Dismiss"),
		NSLOCTEXT("DefinitionGenerator", "DismissTooltip", "Dismiss this notification"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::OnDismissClicked)
	));

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		ActiveNotification = Notification;
	}
}

void UDefinitionGeneratorEditorSubsystem::OnRegenerateClicked()
{
	if (TSharedPtr<SNotificationItem> Notification = ActiveNotification.Pin())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		Notification->ExpireAndFadeout();
	}
	ActiveNotification.Reset();

	// Regenerate all types
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return;
	}

	int32 TotalRegenerated = 0;
	TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();
	for (const FString& TypeName : TypeNames)
	{
		TotalRegenerated += RegenerateChanged(TypeName);
	}

	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionGenerator", "RegeneratedNotification", "Regenerated {0} definitions"),
		FText::AsNumber(TotalRegenerated)
	));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void UDefinitionGeneratorEditorSubsystem::OnDismissClicked()
{
	bDismissedUntilChange = true;
	PendingRegenerationFiles.Empty();

	if (TSharedPtr<SNotificationItem> Notification = ActiveNotification.Pin())
	{
		Notification->ExpireAndFadeout();
	}
	ActiveNotification.Reset();
}

void UDefinitionGeneratorEditorSubsystem::ShowChangedFilesNotification(int32 FileCount, const FString& FileList)
{
	// Close existing notification
	if (TSharedPtr<SNotificationItem> Existing = ActiveNotification.Pin())
	{
		Existing->ExpireAndFadeout();
	}

	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionGenerator", "ChangedFilesNotification", "{0} definition(s) changed: {1}"),
		FText::AsNumber(FileCount),
		FText::FromString(FileList)
	));

	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 15.0f;
	Info.FadeOutDuration = 1.0f;

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionGenerator", "RegenerateChangedButton", "Regenerate"),
		NSLOCTEXT("DefinitionGenerator", "RegenerateChangedTooltip", "Regenerate only the changed definitions"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::OnRegenerateChangedFilesClicked)
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionGenerator", "DismissChangedButton", "Dismiss"),
		NSLOCTEXT("DefinitionGenerator", "DismissChangedTooltip", "Dismiss this notification"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionGeneratorEditorSubsystem::OnDismissClicked)
	));

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		ActiveNotification = Notification;
	}
}

void UDefinitionGeneratorEditorSubsystem::OnRegenerateChangedFilesClicked()
{
	if (TSharedPtr<SNotificationItem> Notification = ActiveNotification.Pin())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		Notification->ExpireAndFadeout();
	}
	ActiveNotification.Reset();

	// Regenerate only the specific files that changed
	int32 Regenerated = RegenerateSpecificFiles(PendingRegenerationFiles);
	PendingRegenerationFiles.Empty();

	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionGenerator", "RegeneratedChangedNotification", "Regenerated {0} definition(s)"),
		FText::AsNumber(Regenerated)
	));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

int32 UDefinitionGeneratorEditorSubsystem::RegenerateSpecificFiles(const TArray<FString>& FilePaths)
{
	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	if (!Generator)
	{
		return 0;
	}

	int32 TotalRegenerated = 0;
	const TArray<FString> TypeNames = Generator->GetRegisteredTypeNames();

	for (const FString& FilePath : FilePaths)
	{
		const FString FileName = FPaths::GetCleanFilename(FilePath);
		const FString NormalizedFilePath = FPaths::ConvertRelativePathToFull(FilePath).Replace(TEXT("\\"), TEXT("/"));
		FString SelectedTypeName;
		bool bSelectedByExplicitGlob = false;

		// Determine which type this file belongs to using the same glob/exclusion
		// contract as validation paths. This avoids routing DLG_*.json to Object.
		for (const FString& TypeName : TypeNames)
		{
			FDefinitionTypeInfo TypeInfo;
			if (!Generator->GetDefinitionTypeInfo(TypeName, TypeInfo))
			{
				continue;
			}

			FString SourceDir = Generator->GetSourceDirectoryForType(TypeName);
			if (SourceDir.IsEmpty())
			{
				continue;
			}

			FString NormalizedSourceDir = FPaths::ConvertRelativePathToFull(SourceDir).Replace(TEXT("\\"), TEXT("/"));
			if (!NormalizedFilePath.StartsWith(NormalizedSourceDir))
			{
				continue;
			}

			// If this type declares a source glob, file must match it.
			const bool bHasExplicitGlob = !TypeInfo.SourceFileGlob.IsEmpty();
			if (bHasExplicitGlob && !FileName.MatchesWildcard(TypeInfo.SourceFileGlob))
			{
				continue;
			}

			// Exclude files claimed by other types that share the same source dir.
			const TArray<FString> ExclusionGlobs = Generator->GetExclusionGlobsForType(TypeName);
			if (UDefinitionGeneratorSubsystem::MatchesAnyGlob(FileName, ExclusionGlobs))
			{
				continue;
			}

			// Prefer explicit glob matches over generic fallback types.
			if (bHasExplicitGlob)
			{
				SelectedTypeName = TypeName;
				bSelectedByExplicitGlob = true;
				break;
			}

			if (SelectedTypeName.IsEmpty())
			{
				SelectedTypeName = TypeName;
			}
		}

		if (SelectedTypeName.IsEmpty())
		{
			UE_LOG(LogDefinitionGeneratorEditor, Warning,
				TEXT("RegenerateSpecificFiles: No definition type matched '%s' (file '%s')"),
				*FilePath, *FileName);
			continue;
		}

		UE_LOG(LogDefinitionGeneratorEditor, Log,
			TEXT("RegenerateSpecificFiles: Routing '%s' -> type '%s'%s"),
			*FileName,
			*SelectedTypeName,
			bSelectedByExplicitGlob ? TEXT(" (explicit glob)") : TEXT(""));

		// Generator broadcasts OnDefinitionRegenerated on success.
		// DefinitionActorSyncSubsystem (ProjectPlacementEditor) handles actor updates.
		FDefinitionGenerationResult Result = Generator->GenerateFromJson(SelectedTypeName, FilePath, false);
		if (Result.bSuccess && !Result.bSkipped)
		{
			TotalRegenerated++;
			UE_LOG(LogDefinitionGeneratorEditor, Log, TEXT("Regenerated: %s"), *Result.AssetId);
		}
	}

	return TotalRegenerated;
}
