// Copyright ALIS. All Rights Reserved.

#include "TravelPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FTravelPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("TravelPhase", "Travel");
}

bool FTravelPhaseExecutor::ResolveMapPath(const FProjectPhaseContext& Context, FString& OutMapPath, FText& OutError)
{
	// Try PrimaryAssetId first (preferred)
	if (Context.Request.MapAssetId.IsValid())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		FAssetData MapAssetData;
		if (AssetManager.GetPrimaryAssetData(Context.Request.MapAssetId, MapAssetData) && MapAssetData.IsValid())
		{
			OutMapPath = MapAssetData.PackageName.ToString();
			if (!OutMapPath.IsEmpty())
			{
				UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Resolved map via AssetManager: %s"), *OutMapPath);
				return true;
			}
		}
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 5: Travel - AssetManager failed to resolve '%s', trying soft path fallback"),
			*Context.Request.MapAssetId.ToString());
	}

	// Fallback to MapSoftPath (for modular plugins where Asset Manager may not have scanned yet)
	if (!Context.Request.MapSoftPath.IsNull())
	{
		OutMapPath = Context.Request.MapSoftPath.GetLongPackageName();
		if (!OutMapPath.IsEmpty())
		{
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Using soft path fallback: %s"), *OutMapPath);
			return true;
		}
	}

	OutError = LOCTEXT("NoMapPath", "No valid map path available (MapAssetId invalid and MapSoftPath empty)");
	return false;
}

FProjectPhaseResult FTravelPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Starting"));
	ReportProgress(Context, 0.0f, LOCTEXT("TravelStart", "Preparing for map travel..."));

	// Check cancellation
	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 5: Travel - Cancelled before travel"));
		return FProjectPhaseResult::Cancelled();
	}

	// Get world context
	UObject* WorldContextObject = Cast<UObject>(Context.Subsystem.Get());
	if (!WorldContextObject)
	{
		const FText Error = LOCTEXT("NoWorldContext", "World context object is invalid");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 5: Travel - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::TravelFailed);
	}

	// Resolve map path
	FString MapPath;
	FText ResolutionError;
	if (!ResolveMapPath(Context, MapPath, ResolutionError))
	{
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 5: Travel - %s"), *ResolutionError.ToString());
		return FProjectPhaseResult::Failure(ResolutionError, ProjectLoadingErrors::TravelFailed);
	}

	ReportProgress(Context, 0.25f, LOCTEXT("TravelExecuting", "Traveling to map..."));

	// Build travel URL with options
	FString TravelURL = MapPath;

	// IMPORTANT: The 'game=' URL option must come LAST because Unreal's parser
	// reads this option value to end-of-string (not until next ? separator).
	// If other options follow 'game=', they get incorrectly appended to the class name.
	FString GameOptionValue;

	// First pass: add all options EXCEPT 'game='
	// NOTE: Unreal Engine URL options use '?' as separator for ALL options (not '&' like web URLs).
	// Format: MapName?Option1=Value?Option2=Value?game=/Script/Module.ClassName
	// NOTE: UClass name does NOT include 'A' prefix (C++ ASinglePlayerGameMode -> UClass SinglePlayerGameMode)
	// See: https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine
	// See: Plugins/Gameplay/ProjectGameplay/README.md for full syntax reference
	for (const auto& Option : Context.Request.CustomOptions)
	{
		if (Option.Key.Equals(TEXT("game"), ESearchCase::IgnoreCase))
		{
			// Hold 'game=' for last
			GameOptionValue = Option.Value;
			continue;
		}

		TravelURL += FString::Printf(TEXT("?%s=%s"), *Option.Key, *Option.Value);
	}

	// Second pass: add 'game=' option LAST (if present)
	if (!GameOptionValue.IsEmpty())
	{
		TravelURL += FString::Printf(TEXT("?game=%s"), *GameOptionValue);
	}

	UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Travel URL: %s"), *TravelURL);

	// Determine travel type based on load mode
	ETravelType TravelType = TRAVEL_Absolute;
	switch (Context.Request.LoadMode)
	{
	case ELoadMode::MultiplayerClient:
		// Client connecting to server - use absolute travel
		TravelType = TRAVEL_Absolute;
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Using TRAVEL_Absolute (multiplayer client)"));
		break;

	case ELoadMode::MultiplayerHost:
	case ELoadMode::DedicatedServer:
		// Server/host - use seamless travel if possible
		TravelType = TRAVEL_Absolute; // Can be changed to TRAVEL_Seamless if enabled in project settings
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Using TRAVEL_Absolute (server/host)"));
		break;

	case ELoadMode::SinglePlayer:
	default:
		// Single player - use absolute travel
		TravelType = TRAVEL_Absolute;
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Using TRAVEL_Absolute (single player)"));
		break;
	}

	ReportProgress(Context, 0.5f, FText::Format(
		LOCTEXT("TravelInProgress", "Traveling to {0}..."),
		FText::FromString(FPaths::GetBaseFilename(MapPath))
	));

	// Execute travel
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		const FText Error = LOCTEXT("NoWorld", "Cannot get world for travel");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 5: Travel - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::TravelFailed);
	}

	// Set progress to 90% and flush rendering to ensure loading widget is visible
	// This creates a "freeze frame" that stays on screen during the blocking ServerTravel call
	ReportProgress(Context, 0.9f, FText::Format(
		LOCTEXT("PreparingTravel", "Entering {0}..."),
		FText::FromString(FPaths::GetBaseFilename(MapPath))
	));

	// Flush rendering commands to ensure the loading widget's last frame is rendered
	FlushRenderingCommands();

	// Small delay to let the viewport render the final frame before travel freeze
	FPlatformProcess::Sleep(0.033f); // One frame at ~30 FPS

	UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Initiating travel..."));

	// Note: Travel is async - the actual map load happens after this call
	// We return success here; the new map's BeginPlay will fire later
	const bool bTravelSuccess = World->ServerTravel(TravelURL, TravelType == TRAVEL_Absolute);

	if (!bTravelSuccess)
	{
		const FText Error = FText::Format(
			LOCTEXT("TravelFailed", "Server travel failed for '{0}'"),
			FText::FromString(MapPath)
		);
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 5: Travel - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::TravelFailed);
	}

	ReportProgress(Context, 1.0f, LOCTEXT("TravelComplete", "Travel initiated"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 5: Travel - Completed successfully (map loading...)"));

	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE

