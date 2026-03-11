// Copyright ALIS. All Rights Reserved.

#include "JsonRefSync/JsonRefSyncSubsystem.h"
#include "JsonRefSync/JsonAssetRefFixer.h"
#include "SyncEvents.h"
#include "SyncHashUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogJsonRefSync, Log, All);
static const FName SyncOriginJsonRefSync(TEXT("JsonRefSync"));

void UJsonRefSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Hook asset registry rename delegate
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnAssetRenamed().AddUObject(this, &UJsonRefSyncSubsystem::OnAssetRenamed);

	UE_LOG(LogJsonRefSync, Log, TEXT("JsonRefSyncSubsystem initialized - listening for asset renames"));
}

void UJsonRefSyncSubsystem::Deinitialize()
{
	// Clear pending timer
	if (DebounceTimerHandle.IsValid() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
	}

	// Unhook delegate
	if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		if (IAssetRegistry* AssetRegistry = Module->TryGet())
		{
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
		}
	}

	UE_LOG(LogJsonRefSync, Log, TEXT("JsonRefSyncSubsystem deinitialized"));

	Super::Deinitialize();
}

void UJsonRefSyncSubsystem::OnAssetRenamed(const FAssetData& NewAssetData, const FString& OldObjectPath)
{
	// Get new path from asset data
	FString NewObjectPath = NewAssetData.GetObjectPathString();

	// Skip if paths are the same (shouldn't happen but safety check)
	if (OldObjectPath == NewObjectPath)
	{
		return;
	}

	UE_LOG(LogJsonRefSync, Verbose, TEXT("Asset renamed: %s -> %s"), *OldObjectPath, *NewObjectPath);

	// Add to pending renames (may override if renamed multiple times before processing)
	PendingRenames.Add(OldObjectPath, NewObjectPath);

	// Start/reset debounce timer
	StartDebounceTimer();
}

void UJsonRefSyncSubsystem::StartDebounceTimer()
{
	if (!GEditor)
	{
		return;
	}

	// Clear existing timer if any
	if (DebounceTimerHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
	}

	// Set new timer
	GEditor->GetTimerManager()->SetTimer(
		DebounceTimerHandle,
		FTimerDelegate::CreateUObject(this, &UJsonRefSyncSubsystem::ProcessPendingRenames),
		DebounceDelay,
		false // Don't loop
	);
}

void UJsonRefSyncSubsystem::ProcessPendingRenamesNow()
{
	// Clear timer if running
	if (DebounceTimerHandle.IsValid() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
	}

	ProcessPendingRenames();
}

void UJsonRefSyncSubsystem::ProcessPendingRenames()
{
	if (PendingRenames.Num() == 0)
	{
		return;
	}

	UE_LOG(LogJsonRefSync, Log, TEXT("=== Processing %d pending asset rename(s) ==="), PendingRenames.Num());

	// Log each rename mapping
	for (const auto& Pair : PendingRenames)
	{
		UE_LOG(LogJsonRefSync, Log, TEXT("  %s -> %s"), *Pair.Key, *Pair.Value);
	}

	// Get JSON root directories
	TArray<FString> JsonRoots = GetJsonRootDirectories();

	if (JsonRoots.Num() == 0)
	{
		UE_LOG(LogJsonRefSync, Warning, TEXT("No JSON root directories found - skipping"));
		PendingRenames.Empty();
		return;
	}

	UE_LOG(LogJsonRefSync, Log, TEXT("Scanning %d JSON root directories"), JsonRoots.Num());

	// Fix references
	TMap<FString, FString> ChangedFileHashes;
	int32 FixedCount = FJsonAssetRefFixer::FixReferences(PendingRenames, JsonRoots, nullptr, &ChangedFileHashes);

	if (ChangedFileHashes.Num() > 0)
	{
		TArray<FSyncBatchFile> BatchFiles;
		BatchFiles.Reserve(ChangedFileHashes.Num());

		for (const auto& Pair : ChangedFileHashes)
		{
			FSyncBatchFile Entry;
			Entry.FilePath = FSyncHashUtils::NormalizeFilePath(Pair.Key);
			Entry.ContentHash = Pair.Value;
			BatchFiles.Add(MoveTemp(Entry));
		}

		UE_LOG(LogJsonRefSync, Log, TEXT("Sync batch applied (%s): %d file(s) updated"),
			*SyncOriginJsonRefSync.ToString(), BatchFiles.Num());
		for (const FSyncBatchFile& Entry : BatchFiles)
		{
			UE_LOG(LogJsonRefSync, Verbose, TEXT("  Sync updated: %s"), *Entry.FilePath);
		}

		FSyncEvents::OnSyncBatchApplied().Broadcast(SyncOriginJsonRefSync, BatchFiles);
	}

	// Show notification if any refs were fixed
	if (FixedCount > 0)
	{
		FNotificationInfo Info(FText::Format(
			NSLOCTEXT("JsonRefSync", "RefsFixedNotification", "Updated {0} JSON asset reference(s)"),
			FText::AsNumber(FixedCount)
		));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		UE_LOG(LogJsonRefSync, Log, TEXT("=== Fixed %d JSON asset reference(s) ==="), FixedCount);
	}
	else
	{
		UE_LOG(LogJsonRefSync, Log, TEXT("=== No JSON references needed updating ==="));
	}

	// Clear pending renames
	PendingRenames.Empty();
}

TArray<FString> UJsonRefSyncSubsystem::GetJsonRootDirectories() const
{
	TArray<FString> Roots;

	// Discover JSON directories from schema files (x-alis-generator.SourceSubDir)
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		const FString BaseDir = Plugin->GetBaseDir();
		const FString SchemaDir = BaseDir / TEXT("Data/Schemas");

		if (!IFileManager::Get().DirectoryExists(*SchemaDir))
		{
			continue;
		}

		// Find all *.schema.json files
		TArray<FString> SchemaFiles;
		IFileManager::Get().FindFilesRecursive(SchemaFiles, *SchemaDir, TEXT("*.schema.json"), true, false);

		for (const FString& SchemaFile : SchemaFiles)
		{
			// Parse schema to get SourceSubDir from x-alis-generator
			FString JsonString;
			if (!FFileHelper::LoadFileToString(JsonString, *SchemaFile))
			{
				continue;
			}

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				continue;
			}

			// Get x-alis-generator.SourceSubDir
			const TSharedPtr<FJsonObject>* GeneratorObj;
			if (!JsonObject->TryGetObjectField(TEXT("x-alis-generator"), GeneratorObj) || !GeneratorObj->IsValid())
			{
				continue;
			}

			FString SourceSubDir;
			if (!(*GeneratorObj)->TryGetStringField(TEXT("SourceSubDir"), SourceSubDir))
			{
				continue;
			}

			// Resolve relative path from Data directory (parent of Schemas)
			// Schema files are always in Data/Schemas/, and SourceSubDir is relative to Data/
			FString SchemaFileDir = FPaths::GetPath(SchemaFile);
			FString DataDir = FPaths::GetPath(SchemaFileDir); // Go up from Data/Schemas to Data
			FString JsonRootDir;

			if (SourceSubDir.IsEmpty())
			{
				// Empty SourceSubDir means Data directory itself
				JsonRootDir = DataDir;
			}
			else
			{
				// Combine with Data directory and convert to absolute path
				JsonRootDir = FPaths::Combine(DataDir, SourceSubDir);
				JsonRootDir = FPaths::ConvertRelativePathToFull(JsonRootDir);
			}

			FPaths::NormalizeDirectoryName(JsonRootDir);

			if (IFileManager::Get().DirectoryExists(*JsonRootDir))
			{
				Roots.AddUnique(JsonRootDir);
				UE_LOG(LogJsonRefSync, Log, TEXT("Discovered JSON root from schema %s: %s"),
					*FPaths::GetCleanFilename(SchemaFile), *JsonRootDir);
			}
			else
			{
				UE_LOG(LogJsonRefSync, Warning, TEXT("Schema %s points to non-existent directory: %s"),
					*FPaths::GetCleanFilename(SchemaFile), *JsonRootDir);
			}
		}
	}

	return Roots;
}
