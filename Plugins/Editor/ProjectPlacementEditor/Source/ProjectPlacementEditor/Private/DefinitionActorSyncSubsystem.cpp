// Copyright ALIS. All Rights Reserved.

#include "DefinitionActorSyncSubsystem.h"
#include "DefinitionEvents.h"
#include "ProjectObjectActorFactory.h"
#include "IDefinitionApplicable.h"
#include "ObjectDefinitionHostHelpers.h"
#include "Data/ObjectDefinition.h"

#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "TimerManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionActorSync, Log, All);

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	TMap<FPrimaryAssetId, TWeakObjectPtr<UPrimaryDataAsset>> GTestDefinitionOverrides;
}

void UDefinitionActorSyncSubsystem::TestOnly_RegisterDefinitionOverride(
	const FPrimaryAssetId& DefinitionId,
	UPrimaryDataAsset* Definition)
{
	if (!DefinitionId.IsValid())
	{
		return;
	}

	if (Definition)
	{
		GTestDefinitionOverrides.Add(DefinitionId, Definition);
	}
	else
	{
		GTestDefinitionOverrides.Remove(DefinitionId);
	}
}

void UDefinitionActorSyncSubsystem::TestOnly_ClearDefinitionOverrides()
{
	GTestDefinitionOverrides.Reset();
}
#endif

void UDefinitionActorSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Subscribe to definition regeneration events via ProjectEditorCore (decoupled)
	OnDefinitionRegeneratedHandle = FDefinitionEvents::OnDefinitionRegenerated().AddUObject(
		this, &UDefinitionActorSyncSubsystem::OnDefinitionRegenerated);
	UE_LOG(LogDefinitionActorSync, Log, TEXT("Subscribed to FDefinitionEvents::OnDefinitionRegenerated"));

	// Subscribe to world creation to set up level hooks when world is ready
	// Mode 1: Passive updates when actors are loaded from external packages
	// IMPORTANT: Cannot access GEditor->GetEditorWorldContext().World() during subsystem init
	// - EditorWorldContext may not be fully initialized yet (crash in GetEditorWorldContext)
	// - Use FWorldDelegates::OnPostWorldCreation to defer level hook setup
	OnWorldCreatedHandle = FWorldDelegates::OnPostWorldCreation.AddUObject(
		this, &UDefinitionActorSyncSubsystem::OnWorldCreated);
	UE_LOG(LogDefinitionActorSync, Log, TEXT("Subscribed to FWorldDelegates::OnPostWorldCreation (Mode 1)"));
}

void UDefinitionActorSyncSubsystem::Deinitialize()
{
	// Unsubscribe from definition regeneration delegate
	if (OnDefinitionRegeneratedHandle.IsValid())
	{
		FDefinitionEvents::OnDefinitionRegenerated().Remove(OnDefinitionRegeneratedHandle);
	}

	// Unsubscribe from world creation delegate
	if (OnWorldCreatedHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldCreation.Remove(OnWorldCreatedHandle);
	}

	// Unsubscribe from actor load delegate (Mode 1)
	// Note: Level delegate cleanup happens automatically when levels are destroyed
	// but we should clear the handle to be safe
	if (OnLoadedActorAddedHandle.IsValid() && GEditor)
	{
		if (UWorld* World = GEditor->GetEditorWorldContext(false).World())
		{
			for (ULevel* Level : World->GetLevels())
			{
				if (Level)
				{
					Level->OnLoadedActorAddedToLevelPostEvent.Remove(OnLoadedActorAddedHandle);
				}
			}
		}
	}

	// Clear timer
	if (GEditor && ActorUpdateCountdownHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(ActorUpdateCountdownHandle);
	}

	Super::Deinitialize();
}

void UDefinitionActorSyncSubsystem::OnWorldCreated(UWorld* World)
{
	// Only care about editor worlds - ignore PIE, game, etc.
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	// Set up level hooks for World Partition actor loading
	// Mode 1: Passive updates when actors are loaded from external packages
	for (ULevel* Level : World->GetLevels())
	{
		if (Level)
		{
			OnLoadedActorAddedHandle = Level->OnLoadedActorAddedToLevelPostEvent.AddUObject(
				this, &UDefinitionActorSyncSubsystem::OnActorsLoadedIntoLevel);
		}
	}
	UE_LOG(LogDefinitionActorSync, Log, TEXT("Subscribed to ULevel::OnLoadedActorAddedToLevelPostEvent (Mode 1) for world: %s"), *World->GetName());
}

void UDefinitionActorSyncSubsystem::OnDefinitionRegenerated(const FString& TypeName, UObject* RegeneratedAsset)
{
	// Only handle ObjectDefinition types
	if (TypeName != TEXT("Object"))
	{
		return;
	}

	// Generic: works with UObjectDefinition
	UPrimaryDataAsset* Def = Cast<UPrimaryDataAsset>(RegeneratedAsset);
	if (!Def || !GEditor)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	const FPrimaryAssetId DefId = Def->GetPrimaryAssetId();
	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Definition regenerated: %s"), *DefId.ToString());

	// Find matching actors via host contract.
	TArray<AActor*> AffectedActors;
	TArray<AActor*> HostsWithoutDefId;
	int32 TotalTrackedActors = 0;

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Searching for DefId: %s"), *DefId.ToString());

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FPrimaryAssetId ActorDefId;
		FString StructureHash;
		FString ContentHash;
		if (!ProjectWorldDefinitionHost::ReadHostState(*It, ActorDefId, StructureHash, ContentHash))
		{
			continue;
		}

		// Host-capable actors can exist without definition tracking metadata.
		// Only include actors that actually participate in definition tracking.
		const bool bHasTrackingMetadata =
			ActorDefId.IsValid() ||
			!StructureHash.IsEmpty() ||
			!ContentHash.IsEmpty();
		if (!bHasTrackingMetadata)
		{
			continue;
		}

		TotalTrackedActors++;
		if (!ActorDefId.IsValid())
		{
			HostsWithoutDefId.Add(*It);
			continue;
		}

		if (ActorDefId == DefId)
		{
			AffectedActors.Add(*It);
			UE_LOG(LogDefinitionActorSync, Log, TEXT("  [MATCH] %s -> %s"), *It->GetActorLabel(), *ActorDefId.ToString());
		}
		else
		{
			UE_LOG(LogDefinitionActorSync, Log, TEXT("  [OTHER] %s -> %s"), *It->GetActorLabel(), *ActorDefId.ToString());
		}
	}

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Summary: %d tracked, %d match, %d missing DefId"),
		TotalTrackedActors, AffectedActors.Num(), HostsWithoutDefId.Num());

	// Actors with host but missing ObjectDefinitionId are misconfigured.
	if (HostsWithoutDefId.Num() > 0)
	{
		// Show error notification popup.
		FNotificationInfo ErrorInfo(FText::Format(
			NSLOCTEXT("DefinitionActorSync", "ActorsWithoutDefIdError", "ERROR: {0} tracked actor(s) have no ObjectDefinitionId!\nRe-place via drag-drop from Project Placement panel."),
			FText::AsNumber(HostsWithoutDefId.Num())
		));
		ErrorInfo.bFireAndForget = true;
		ErrorInfo.bUseSuccessFailIcons = true;
		ErrorInfo.ExpireDuration = 8.0f;
		ErrorInfo.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.Error"));

		TSharedPtr<SNotificationItem> ErrorNotification = FSlateNotificationManager::Get().AddNotification(ErrorInfo);
		if (ErrorNotification.IsValid())
		{
			ErrorNotification->SetCompletionState(SNotificationItem::CS_Fail);
		}

		// Log to Message Log with clickable actor references
		FMessageLog MessageLog("PIE");
		MessageLog.Error(FText::Format(
			NSLOCTEXT("DefinitionActorSync", "ActorsWithoutDefIdHeader", "Definition Sync: {0} tracked actor(s) missing ObjectDefinitionId (click to select):"),
			FText::AsNumber(HostsWithoutDefId.Num())
		));

		for (AActor* Actor : HostsWithoutDefId)
		{
			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
			Message->AddToken(FTextToken::Create(FText::Format(
				NSLOCTEXT("DefinitionActorSync", "ActorEntry", "  - {0}"),
				FText::FromString(Actor->GetActorLabel())
			)));
			Message->AddToken(FUObjectToken::Create(Actor)->OnMessageTokenActivated(
				FOnMessageTokenActivated::CreateLambda([](const TSharedRef<IMessageToken>& Token)
				{
					if (const FUObjectToken* ObjToken = static_cast<const FUObjectToken*>(&Token.Get()))
					{
						if (AActor* TokenActor = Cast<AActor>(ObjToken->GetObject().Get()))
						{
							GEditor->SelectNone(false, true);
							GEditor->SelectActor(TokenActor, true, true);
							GEditor->MoveViewportCamerasToActor(*TokenActor, false);
						}
					}
				})
			));
			MessageLog.AddMessage(Message);
		}

		MessageLog.Open(EMessageSeverity::Warning);
	}

	if (AffectedActors.Num() == 0)
	{
		return;
	}

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Found %d actors to update - showing countdown"), AffectedActors.Num());

	// Show countdown notification
	ShowActorUpdateCountdown(Def, AffectedActors.Num());
}

void UDefinitionActorSyncSubsystem::ShowActorUpdateCountdown(UPrimaryDataAsset* Def, int32 ActorCount)
{
	if (!Def || !GEditor)
	{
		return;
	}

	// Cancel any existing countdown
	if (ActorUpdateCountdownHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(ActorUpdateCountdownHandle);
	}

	// Close existing notification
	if (TSharedPtr<SNotificationItem> Existing = ActorUpdateNotification.Pin())
	{
		Existing->ExpireAndFadeout();
	}

	// Store pending update data
	PendingUpdateDefinition = Def;
	PendingActorCount = ActorCount;
	ActorUpdateCountdownSeconds = 5; // 5 second countdown

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Starting %d second countdown for %d actors"),
		ActorUpdateCountdownSeconds, PendingActorCount);

	// Create notification with countdown
	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionActorSync", "ActorUpdateCountdown", "Updating {0} actors in {1}s..."),
		FText::AsNumber(ActorCount),
		FText::AsNumber(ActorUpdateCountdownSeconds)
	));

	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeOutDuration = 0.5f;

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionActorSync", "ApplyNowButton", "Apply Now"),
		NSLOCTEXT("DefinitionActorSync", "ApplyNowTooltip", "Apply changes immediately"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionActorSyncSubsystem::OnApplyActorUpdateNow)
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionActorSync", "CancelUpdateButton", "Cancel"),
		NSLOCTEXT("DefinitionActorSync", "CancelUpdateTooltip", "Cancel actor update"),
		FSimpleDelegate::CreateUObject(this, &UDefinitionActorSyncSubsystem::OnCancelActorUpdate)
	));

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		ActorUpdateNotification = Notification;
	}

	// Start countdown timer
	GEditor->GetTimerManager()->SetTimer(
		ActorUpdateCountdownHandle,
		FTimerDelegate::CreateUObject(this, &UDefinitionActorSyncSubsystem::OnActorUpdateCountdownTick),
		1.0f,
		true
	);
}

void UDefinitionActorSyncSubsystem::OnActorUpdateCountdownTick()
{
	ActorUpdateCountdownSeconds--;

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Countdown: %d seconds remaining"), ActorUpdateCountdownSeconds);

	if (ActorUpdateCountdownSeconds <= 0)
	{
		ExecutePendingActorUpdate();
	}
	else
	{
		if (TSharedPtr<SNotificationItem> Notification = ActorUpdateNotification.Pin())
		{
			Notification->SetText(FText::Format(
				NSLOCTEXT("DefinitionActorSync", "ActorUpdateCountdown", "Updating {0} actors in {1}s..."),
				FText::AsNumber(PendingActorCount),
				FText::AsNumber(ActorUpdateCountdownSeconds)
			));
		}
	}
}

void UDefinitionActorSyncSubsystem::OnCancelActorUpdate()
{
	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Countdown cancelled by user"));

	if (GEditor && ActorUpdateCountdownHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(ActorUpdateCountdownHandle);
	}

	PendingUpdateDefinition.Reset();
	PendingActorCount = 0;

	if (TSharedPtr<SNotificationItem> Notification = ActorUpdateNotification.Pin())
	{
		Notification->SetText(NSLOCTEXT("DefinitionActorSync", "UpdateCancelled", "Actor update cancelled"));
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
		Notification->ExpireAndFadeout();
	}
	ActorUpdateNotification.Reset();
}

void UDefinitionActorSyncSubsystem::OnApplyActorUpdateNow()
{
	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Apply Now clicked"));
	ExecutePendingActorUpdate();
}

void UDefinitionActorSyncSubsystem::ExecutePendingActorUpdate()
{
	if (GEditor && ActorUpdateCountdownHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(ActorUpdateCountdownHandle);
	}

	UPrimaryDataAsset* Def = PendingUpdateDefinition.Get();
	if (!Def || !GEditor)
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[ActorSync] Definition no longer valid"));
		if (TSharedPtr<SNotificationItem> Notification = ActorUpdateNotification.Pin())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
			Notification->ExpireAndFadeout();
		}
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	const FPrimaryAssetId DefId = Def->GetPrimaryAssetId();

	// Find actors again (may have changed during countdown).
	TArray<AActor*> AffectedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FPrimaryAssetId ActorDefId;
		FString StructureHash;
		FString ContentHash;
		if (ProjectWorldDefinitionHost::ReadHostState(*It, ActorDefId, StructureHash, ContentHash)
			&& ActorDefId == DefId)
		{
			AffectedActors.Add(*It);
		}
	}

	if (AffectedActors.Num() == 0)
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[ActorSync] No actors to update"));
		if (TSharedPtr<SNotificationItem> Notification = ActorUpdateNotification.Pin())
		{
			Notification->SetText(NSLOCTEXT("DefinitionActorSync", "NoActorsToUpdate", "No actors to update"));
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
			Notification->ExpireAndFadeout();
		}
		PendingUpdateDefinition.Reset();
		return;
	}

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Executing update for %d actors"), AffectedActors.Num());

	FScopedTransaction Transaction(FText::Format(
		NSLOCTEXT("DefinitionActorSync", "UpdateActors", "Update {0} actors from definition"),
		FText::AsNumber(AffectedActors.Num())));

	int32 ReappliedCount = 0;
	int32 ReplacedCount = 0;
	TArray<AActor*> ManualFixNeeded;

	for (AActor* Actor : AffectedActors)
	{
		// Try to apply definition via interface (actor-specific logic)
		if (ApplyDefinitionToActor(Actor, Def))
		{
			// Actor successfully applied definition in-place
			ReappliedCount++;
			UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Applied: %s"), *Actor->GetActorLabel());
		}
		else
		{
			// Apply failed (structure mismatch or interface not implemented) - need Replace
			// Only ObjectDefinition types support Replace (via UProjectObjectActorFactory)
			if (DefId.PrimaryAssetType == FName(TEXT("ObjectDefinition")))
			{
				const FString OldActorName = Actor->GetName();
				UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Replacing: %s"), *OldActorName);
				AActor* NewActor = ReplaceActorFromDefinition(Actor, Def);
				if (NewActor)
				{
					ReplacedCount++;
					UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] Replace complete: %s -> %s"),
						*OldActorName, *NewActor->GetName());
				}
			}
			else
			{
				// Unknown definition type - no factory exists
				UE_LOG(LogDefinitionActorSync, Warning, TEXT("[ActorSync] Cannot replace: %s (no factory for type '%s')"),
					*Actor->GetActorLabel(), *DefId.PrimaryAssetType.ToString());
				ManualFixNeeded.Add(Actor);
			}
		}
	}

	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	UE_LOG(LogDefinitionActorSync, Log, TEXT("[ActorSync] === Update complete ==="));
	UE_LOG(LogDefinitionActorSync, Log, TEXT("  Definition: %s"), *DefId.ToString());
	UE_LOG(LogDefinitionActorSync, Log, TEXT("  Reapplied: %d actors"), ReappliedCount);
	UE_LOG(LogDefinitionActorSync, Log, TEXT("  Replaced: %d actors"), ReplacedCount);
	UE_LOG(LogDefinitionActorSync, Log, TEXT("  Manual fix needed: %d actors"), ManualFixNeeded.Num());
	UE_LOG(LogDefinitionActorSync, Log, TEXT("  Time: %.2fms"), ElapsedMs);

	// Show warning notification for actors that need manual fix (structural changes, no factory)
	if (ManualFixNeeded.Num() > 0)
	{
		FNotificationInfo WarningInfo(FText::Format(
			NSLOCTEXT("DefinitionActorSync", "ManualFixNeeded", "WARNING: {0} actor(s) need manual fix!\nStructural changes require manual re-placement."),
			FText::AsNumber(ManualFixNeeded.Num())
		));
		WarningInfo.bFireAndForget = true;
		WarningInfo.bUseSuccessFailIcons = true;
		WarningInfo.ExpireDuration = 10.0f;
		WarningInfo.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.Warning"));

		TSharedPtr<SNotificationItem> WarningNotification = FSlateNotificationManager::Get().AddNotification(WarningInfo);
		if (WarningNotification.IsValid())
		{
			WarningNotification->SetCompletionState(SNotificationItem::CS_Pending);
		}

		// Log to Message Log with clickable actor references
		FMessageLog MessageLog("PIE");
		MessageLog.Warning(FText::Format(
			NSLOCTEXT("DefinitionActorSync", "ManualFixHeader", "Definition Sync: {0} actor(s) need manual fix (click to select):"),
			FText::AsNumber(ManualFixNeeded.Num())
		));

		for (AActor* Actor : ManualFixNeeded)
		{
			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
			Message->AddToken(FTextToken::Create(FText::Format(
				NSLOCTEXT("DefinitionActorSync", "ManualFixEntry", "  - {0} (structural change - delete & re-place)"),
				FText::FromString(Actor->GetActorLabel())
			)));
			Message->AddToken(FUObjectToken::Create(Actor)->OnMessageTokenActivated(
				FOnMessageTokenActivated::CreateLambda([](const TSharedRef<IMessageToken>& Token)
				{
					if (const FUObjectToken* ObjToken = static_cast<const FUObjectToken*>(&Token.Get()))
					{
						if (AActor* TokenActor = Cast<AActor>(ObjToken->GetObject().Get()))
						{
							GEditor->SelectNone(false, true);
							GEditor->SelectActor(TokenActor, true, true);
							GEditor->MoveViewportCamerasToActor(*TokenActor, false);
						}
					}
				})
			));
			MessageLog.AddMessage(Message);
		}

		MessageLog.Open(EMessageSeverity::Warning);
	}

	if (TSharedPtr<SNotificationItem> Notification = ActorUpdateNotification.Pin())
	{
		Notification->SetText(FText::Format(
			NSLOCTEXT("DefinitionActorSync", "ActorsUpdated", "Updated {0} actors from '{1}'"),
			FText::AsNumber(ReappliedCount + ReplacedCount),
			FText::FromString(DefId.ToString())
		));
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		Notification->ExpireAndFadeout();
	}

	PendingUpdateDefinition.Reset();
	PendingActorCount = 0;
}

// -------------------------------------------------------------------------
// Mode 1: Passive Actor Updates (World Partition / Level Streaming)
// -------------------------------------------------------------------------

void UDefinitionActorSyncSubsystem::OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors)
{
	// Mode 1: Check loaded actors for definition updates
	// Silent updates - no countdown, no notification (passive sync on load)
	for (AActor* Actor : Actors)
	{
		FPrimaryAssetId DefinitionId;
		FString AppliedStructureHash;
		FString AppliedContentHash;
		if (!ProjectWorldDefinitionHost::ReadHostState(Actor, DefinitionId, AppliedStructureHash, AppliedContentHash))
		{
			continue;
		}

		if (!DefinitionId.IsValid())
		{
			continue;
		}

		// Load definition
		UPrimaryDataAsset* Def = LoadDefinition(DefinitionId);
		if (!Def)
		{
			continue;
		}

		// Check if update needed (content hash mismatch)
		UObjectDefinition* ObjDef = Cast<UObjectDefinition>(Def);
		if (ObjDef && !ObjDef->DefinitionContentHash.IsEmpty())
		{
			if (AppliedContentHash == ObjDef->DefinitionContentHash)
			{
				// Already up to date
				continue;
			}

			UE_LOG(LogDefinitionActorSync, Log, TEXT("[Mode1] Actor %s needs update: %s -> %s"),
				*Actor->GetActorLabel(),
				*AppliedContentHash.Left(8),
				*ObjDef->DefinitionContentHash.Left(8));

			// Try to apply silently
			if (ApplyDefinitionToActor(Actor, Def))
			{
				UE_LOG(LogDefinitionActorSync, Log, TEXT("[Mode1] Applied: %s"), *Actor->GetActorLabel());
			}
			else
			{
				// Fall back to replace for ObjectDefinition actors in passive mode.
				if (DefinitionId.PrimaryAssetType == FName(TEXT("ObjectDefinition")))
				{
					if (AActor* ReplacedActor = ReplaceActorFromDefinition(Actor, Def))
					{
						UE_LOG(LogDefinitionActorSync, Log, TEXT("[Mode1] Replaced: %s"), *ReplacedActor->GetActorLabel());
					}
					else
					{
						ShowStructuralMismatchNotification(Actor);
					}
				}
				else
				{
					ShowStructuralMismatchNotification(Actor);
				}
			}
		}
	}
}

UPrimaryDataAsset* UDefinitionActorSyncSubsystem::LoadDefinition(const FPrimaryAssetId& DefinitionId)
{
	if (!DefinitionId.IsValid())
	{
		return nullptr;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (const TWeakObjectPtr<UPrimaryDataAsset>* Override = GTestDefinitionOverrides.Find(DefinitionId))
	{
		if (Override->IsValid())
		{
			return Override->Get();
		}
	}
#endif

	// Get Asset Manager
	UAssetManager& AssetManager = UAssetManager::Get();

	// Try to load the definition asset
	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(DefinitionId);
	if (AssetPath.IsNull())
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[LoadDefinition] Cannot find asset path for: %s"), *DefinitionId.ToString());
		return nullptr;
	}

	// Load synchronously (editor-only, acceptable)
	UObject* LoadedAsset = AssetPath.TryLoad();
	if (!LoadedAsset)
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[LoadDefinition] Failed to load: %s"), *AssetPath.ToString());
		return nullptr;
	}

	UPrimaryDataAsset* Def = Cast<UPrimaryDataAsset>(LoadedAsset);
	if (!Def)
	{
		UE_LOG(LogDefinitionActorSync, Error, TEXT("[LoadDefinition] Asset is not UPrimaryDataAsset: %s"), *AssetPath.ToString());
		return nullptr;
	}

	return Def;
}

void UDefinitionActorSyncSubsystem::ShowStructuralMismatchNotification(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	FNotificationInfo WarningInfo(FText::Format(
		NSLOCTEXT("DefinitionActorSync", "StructuralMismatch", "Actor '{0}' has structural changes!\nRequires manual re-placement."),
		FText::FromString(Actor->GetActorLabel())
	));
	WarningInfo.bFireAndForget = true;
	WarningInfo.bUseSuccessFailIcons = true;
	WarningInfo.ExpireDuration = 8.0f;
	WarningInfo.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.Warning"));

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(WarningInfo);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}

	// Log to Message Log with clickable actor reference
	FMessageLog MessageLog("PIE");
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
	Message->AddToken(FTextToken::Create(FText::Format(
		NSLOCTEXT("DefinitionActorSync", "StructuralMismatchEntry", "Actor '{0}' has structural mismatch (click to select)"),
		FText::FromString(Actor->GetActorLabel())
	)));
	Message->AddToken(FUObjectToken::Create(Actor)->OnMessageTokenActivated(
		FOnMessageTokenActivated::CreateLambda([](const TSharedRef<IMessageToken>& Token)
		{
			if (const FUObjectToken* ObjToken = static_cast<const FUObjectToken*>(&Token.Get()))
			{
				if (AActor* TokenActor = Cast<AActor>(ObjToken->GetObject().Get()))
				{
					GEditor->SelectNone(false, true);
					GEditor->SelectActor(TokenActor, true, true);
					GEditor->MoveViewportCamerasToActor(*TokenActor, false);
				}
			}
		})
	));
	MessageLog.AddMessage(Message);
	MessageLog.Open(EMessageSeverity::Warning);
}

AActor* UDefinitionActorSyncSubsystem::ReplaceActorFromDefinition(AActor* OldActor, UPrimaryDataAsset* Def)
{
	if (!OldActor || !Def || !GEditor)
	{
		return nullptr;
	}

	// Get or cache factory
	UProjectObjectActorFactory* Factory = CachedObjectFactory.Get();
	if (!Factory)
	{
		for (UActorFactory* F : GEditor->ActorFactories)
		{
			Factory = Cast<UProjectObjectActorFactory>(F);
			if (Factory)
			{
				CachedObjectFactory = Factory;
				break;
			}
		}
	}
	if (!Factory)
	{
		UE_LOG(LogDefinitionActorSync, Error, TEXT("[ActorSync] ProjectObjectActorFactory not found!"));
		return nullptr;
	}

	// Capture state
	FTransform OldTransform = OldActor->GetActorTransform();
	ULevel* Level = OldActor->GetLevel();
	FString OldLabel = OldActor->GetActorLabel();
	FName OldFolderPath = OldActor->GetFolderPath();

	// Delete old actor
	OldActor->Modify();
	UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	bool bDeleted = ActorSubsystem ? ActorSubsystem->DestroyActor(OldActor) : false;
	if (!bDeleted)
	{
		OldActor->Destroy();
	}

	// Spawn new
	FActorSpawnParameters Params;
	AActor* NewActor = Factory->SpawnActor(Def, Level, OldTransform, Params);

	if (NewActor)
	{
		NewActor->SetActorLabel(OldLabel);
		NewActor->SetFolderPath(OldFolderPath);
	}

	return NewActor;
}

// -------------------------------------------------------------------------
// Update Strategy: Apply via Interface (Polymorphic)
// -------------------------------------------------------------------------
// SOC: Actor knows HOW to apply its definition type (ApplyDefinition implementation)
//      Subsystem knows WHEN to update (orchestration, timing, notifications)

bool UDefinitionActorSyncSubsystem::ApplyDefinitionToActor(AActor* Actor, UPrimaryDataAsset* Def)
{
	if (!Actor || !Def)
	{
		return false;
	}

	// Check if actor implements IDefinitionApplicable interface
	if (!Actor->Implements<UDefinitionApplicable>())
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[%s] Does not implement IDefinitionApplicable - will Replace"),
			*Actor->GetActorLabel());
		return false;
	}

	// Actor->Modify() called inside actor's ApplyDefinition implementation (actor owns undo state)

	// Call the interface method - actor handles its specific definition type
	// Example: AInteractableActor casts to UObjectDefinition
	const bool bSuccess = IDefinitionApplicable::Execute_ApplyDefinition(Actor, Def);

	if (!bSuccess)
	{
		UE_LOG(LogDefinitionActorSync, Warning, TEXT("[%s] ApplyDefinition returned false - will Replace"),
			*Actor->GetActorLabel());
	}

	return bSuccess;
}
