// Copyright ALIS. All Rights Reserved.

#include "RedirectorAutoFix/RedirectorAutoFixSubsystem.h"

#include "AssetSyncSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogRedirectorAutoFix, Log, All);

void URedirectorAutoFixSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Hook asset registry rename delegate
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnAssetRenamed().AddUObject(this, &URedirectorAutoFixSubsystem::OnAssetRenamed);

	UE_LOG(LogRedirectorAutoFix, Log, TEXT("RedirectorAutoFixSubsystem initialized - auto-fix %s"),
		IsAutoFixEnabled() ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void URedirectorAutoFixSubsystem::Deinitialize()
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

	UE_LOG(LogRedirectorAutoFix, Log, TEXT("RedirectorAutoFixSubsystem deinitialized"));

	Super::Deinitialize();
}

bool URedirectorAutoFixSubsystem::IsAutoFixEnabled() const
{
	const UAssetSyncSettings* Settings = GetDefault<UAssetSyncSettings>();
	return Settings && Settings->bAutoFixRedirectorsOnRename;
}

void URedirectorAutoFixSubsystem::OnAssetRenamed(const FAssetData& NewAssetData, const FString& OldObjectPath)
{
	if (!IsAutoFixEnabled())
	{
		return;
	}

	// Get new path from asset data
	FString NewObjectPath = NewAssetData.GetObjectPathString();

	// Skip if paths are the same
	if (OldObjectPath == NewObjectPath)
	{
		return;
	}

	UE_LOG(LogRedirectorAutoFix, Verbose, TEXT("Asset renamed: %s -> %s"), *OldObjectPath, *NewObjectPath);

	// Add to pending renames
	PendingRenames.Add(OldObjectPath, NewObjectPath);

	// Start/reset debounce timer
	StartDebounceTimer();
}

void URedirectorAutoFixSubsystem::StartDebounceTimer()
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

	// Set new timer (slightly longer than JsonRefSync to run after it)
	GEditor->GetTimerManager()->SetTimer(
		DebounceTimerHandle,
		FTimerDelegate::CreateUObject(this, &URedirectorAutoFixSubsystem::ProcessPendingRenames),
		DebounceDelay,
		false
	);
}

void URedirectorAutoFixSubsystem::FixRedirectorsNow()
{
	// Clear timer if running
	if (DebounceTimerHandle.IsValid() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(DebounceTimerHandle);
	}

	ProcessPendingRenames();
}

void URedirectorAutoFixSubsystem::ProcessPendingRenames()
{
	if (PendingRenames.Num() == 0)
	{
		return;
	}

	UE_LOG(LogRedirectorAutoFix, Log, TEXT("=== Processing %d pending rename(s) for redirector fixup ==="), PendingRenames.Num());

	// Fix redirectors
	FixRedirectors(PendingRenames);

	// Clear pending renames
	PendingRenames.Empty();
}

void URedirectorAutoFixSubsystem::FixRedirectors(const TMap<FString, FString>& RenameMappings)
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<UObjectRedirector*> RedirectorsToFix;
	TArray<FString> FixedRedirectorPaths;

	// Find and load redirectors at old paths
	for (const auto& Pair : RenameMappings)
	{
		const FString& OldPath = Pair.Key;
		const FString& NewPath = Pair.Value;

		UE_LOG(LogRedirectorAutoFix, Verbose, TEXT("Checking for redirector at: %s"), *OldPath);

		// Try to load redirector at old path
		UObject* LoadedObject = LoadObject<UObject>(nullptr, *OldPath, nullptr, LOAD_NoWarn | LOAD_Quiet);

		if (!LoadedObject)
		{
			UE_LOG(LogRedirectorAutoFix, Verbose, TEXT("  No object found at old path (may have been fully replaced)"));
			continue;
		}

		// Check if it's a redirector
		UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedObject);
		if (!Redirector)
		{
			UE_LOG(LogRedirectorAutoFix, Verbose, TEXT("  Object at old path is not a redirector (type: %s)"),
				*LoadedObject->GetClass()->GetName());
			continue;
		}

		UE_LOG(LogRedirectorAutoFix, Log, TEXT("Found redirector to fix: %s -> %s"), *OldPath, *NewPath);
		RedirectorsToFix.Add(Redirector);
		FixedRedirectorPaths.Add(OldPath);
	}

	if (RedirectorsToFix.Num() == 0)
	{
		UE_LOG(LogRedirectorAutoFix, Log, TEXT("=== No redirectors needed fixing ==="));
		return;
	}

	// Use Epic's AssetTools to fix up referencers
	// This handles: loading referencing packages, replacing refs, saving, deleting redirectors
	UE_LOG(LogRedirectorAutoFix, Log, TEXT("Fixing %d redirector(s) using AssetTools..."), RedirectorsToFix.Num());

	AssetTools.FixupReferencers(RedirectorsToFix);

	// Note: FixupReferencers handles all the heavy lifting:
	// - Finds all assets referencing these redirectors
	// - Loads referencing packages
	// - Replaces redirector refs with direct refs to destination
	// - Saves modified packages (with source control checkout)
	// - Deletes redirectors if safe (no locked files, etc.)

	// Show notification
	// Note: We don't have exact save count from FixupReferencers, so we estimate
	ShowResultsNotification(FixedRedirectorPaths.Num(), FixedRedirectorPaths.Num(), FixedRedirectorPaths);

	UE_LOG(LogRedirectorAutoFix, Log, TEXT("=== Fixed %d redirector(s) ==="), FixedRedirectorPaths.Num());
}

void URedirectorAutoFixSubsystem::ShowResultsNotification(
	int32 FixedCount,
	int32 SavedCount,
	const TArray<FString>& FixedRedirectors)
{
	// Build notification text - emphasize AUTO-save behavior
	FText NotificationText;
	if (FixedCount == 1)
	{
		NotificationText = FText::Format(
			NSLOCTEXT("RedirectorAutoFix", "SingleFixed", "Auto-fixed 1 redirector and saved {0} package(s)"),
			FText::AsNumber(SavedCount)
		);
	}
	else
	{
		NotificationText = FText::Format(
			NSLOCTEXT("RedirectorAutoFix", "MultipleFixed", "Auto-fixed {0} redirectors and saved {1} package(s)"),
			FText::AsNumber(FixedCount),
			FText::AsNumber(SavedCount)
		);
	}

	FNotificationInfo Info(NotificationText);
	Info.ExpireDuration = 6.0f; // Slightly longer to read warning
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = true;

	// Build subtext: redirector names + auto-save warning
	FString SubText;

	// List first few redirectors
	int32 ShowCount = FMath::Min(3, FixedRedirectors.Num());
	for (int32 i = 0; i < ShowCount; ++i)
	{
		FString AssetName = FPackageName::GetShortName(FixedRedirectors[i]);
		SubText += AssetName;
		if (i < ShowCount - 1)
		{
			SubText += TEXT(", ");
		}
	}
	if (FixedRedirectors.Num() > 3)
	{
		SubText += FString::Printf(TEXT(" (+%d more)"), FixedRedirectors.Num() - 3);
	}

	// Add auto-save warning
	SubText += TEXT("\nReferencing packages were automatically checked out and saved.");

	Info.SubText = FText::FromString(SubText);

	FSlateNotificationManager::Get().AddNotification(Info);
}
