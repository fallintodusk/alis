// Copyright ALIS. All Rights Reserved.

#include "OrchestratorEditorBootSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "OrchestratorPluginRegistry.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorEditor, Log, All);

void UOrchestratorEditorBootSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogOrchestratorEditor, Log, TEXT("OrchestratorEditorBootSubsystem initialized"));

	// Hook PIE start event
	if (GEditor)
	{
		FEditorDelegates::BeginPIE.AddUObject(this, &UOrchestratorEditorBootSubsystem::OnBeginPIE);
		UE_LOG(LogOrchestratorEditor, Log, TEXT("  BeginPIE delegate registered"));
	}

	// Preload pawn class + all deps in background so PIE starts fast.
	// OnPostEngineInit fires after AssetManager is fully initialized,
	// unlike FTSTicker which throttles to 3 fps when editor loses focus.
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UOrchestratorEditorBootSubsystem::OnPostEngineInit);
}

void UOrchestratorEditorBootSubsystem::Deinitialize()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	// Unhook PIE delegate
	if (GEditor)
	{
		FEditorDelegates::BeginPIE.RemoveAll(this);
	}

	Super::Deinitialize();
}

void UOrchestratorEditorBootSubsystem::OnPostEngineInit()
{
	PreloadPawnClass();
}

void UOrchestratorEditorBootSubsystem::OnBeginPIE(bool bIsSimulating)
{
	UE_LOG(LogOrchestratorEditor, Log, TEXT("PIE starting - loading external plugins..."));
	LoadExternalPlugins();
}

void UOrchestratorEditorBootSubsystem::ScanForUpluginFiles(const FString& DirectoryPath, TArray<FString>& OutUpluginFiles)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if directory exists (optional - no warning if missing)
	if (!PlatformFile.DirectoryExists(*DirectoryPath))
	{
		UE_LOG(LogOrchestratorEditor, Log, TEXT("External plugins directory does not exist (optional): %s"), *DirectoryPath);
		return;
	}

	// Visitor to collect .uplugin files
	class FUpluginVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		TArray<FString>& UpluginFiles;

		explicit FUpluginVisitor(TArray<FString>& InUpluginFiles)
			: UpluginFiles(InUpluginFiles)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				FString File(FilenameOrDirectory);
				if (File.EndsWith(TEXT(".uplugin")))
				{
					UpluginFiles.Add(File);
				}
			}
			return true; // Continue iteration
		}
	};

	FUpluginVisitor Visitor(OutUpluginFiles);
	PlatformFile.IterateDirectoryRecursively(*DirectoryPath, Visitor);

	UE_LOG(LogOrchestratorEditor, Log, TEXT("Found %d .uplugin files in %s"), OutUpluginFiles.Num(), *DirectoryPath);
}

void UOrchestratorEditorBootSubsystem::LoadExternalPlugins()
{
	// Get project directory
	const FString ProjectDir = FPaths::ProjectDir();
	TArray<FString> UpluginFiles;
	const TArray<FString> ExternalPluginDirs = {
		FPaths::Combine(ProjectDir, TEXT("Plugins"), TEXT("ThirdParty")),
		FPaths::Combine(ProjectDir, TEXT("Plugins"), TEXT("Local"))
	};

	for (const FString& ExternalPluginsDir : ExternalPluginDirs)
	{
		UE_LOG(LogOrchestratorEditor, Log, TEXT("Scanning for external plugins in: %s"), *ExternalPluginsDir);
		ScanForUpluginFiles(ExternalPluginsDir, UpluginFiles);
	}

	if (UpluginFiles.Num() == 0)
	{
		UE_LOG(LogOrchestratorEditor, Log, TEXT("  No external plugins found"));
		return;
	}

	// Register and load each plugin
	int32 LoadedCount = 0;
	for (const FString& UpluginPath : UpluginFiles)
	{
		UE_LOG(LogOrchestratorEditor, Log, TEXT("Processing plugin: %s"), *UpluginPath);

		// Register plugin with IPluginManager
		FPluginRegistrationResult Result = FOrchestratorPluginRegistry::RegisterPlugin(UpluginPath);

		if (!Result.bSuccess)
		{
			UE_LOG(LogOrchestratorEditor, Error, TEXT("  Failed to register plugin: %s"), *Result.ErrorMessage);
			continue;
		}

		// Get plugin info
		FString PluginPath;
		FString ModuleName;
		if (!FOrchestratorPluginRegistry::GetPluginInfo(Result.PluginName, PluginPath, ModuleName))
		{
			UE_LOG(LogOrchestratorEditor, Error, TEXT("  Failed to get plugin info for: %s"), *Result.PluginName);
			continue;
		}

		// Only load if plugin has code modules
		if (ModuleName.IsEmpty())
		{
			UE_LOG(LogOrchestratorEditor, Log, TEXT("  Plugin '%s' is content-only, skipping module load"), *Result.PluginName);
			continue;
		}

		// Load the module
		UE_LOG(LogOrchestratorEditor, Log, TEXT("  Loading module: %s"), *ModuleName);

		if (FModuleManager::Get().ModuleExists(*ModuleName))
		{
			FModuleManager::Get().LoadModuleChecked(*ModuleName);
			UE_LOG(LogOrchestratorEditor, Log, TEXT("  Module loaded successfully: %s"), *ModuleName);
			LoadedCount++;
		}
		else
		{
			UE_LOG(LogOrchestratorEditor, Warning, TEXT("  Module does not exist: %s"), *ModuleName);
		}
	}

	UE_LOG(LogOrchestratorEditor, Log, TEXT("External plugin loading complete - %d/%d modules loaded"), LoadedCount, UpluginFiles.Num());
}

void UOrchestratorEditorBootSubsystem::PreloadPawnClass()
{
	// BP_Hero references MotionMatching animation content (~1.9 GB of anim sequences).
	// LoadSynchronous in SinglePlayerGameMode::InitGame blocks PIE for ~9s loading these deps.
	// By requesting async load at editor startup, deps are warm by the time user hits Play.
	const FSoftObjectPath PawnPath(TEXT("/ProjectObject/Human/Hero/BP_Hero.BP_Hero_C"));

	// Disable CPU throttle while preloading so async loading runs at full speed
	// even when the editor is in background. UE processes async-loaded UObjects on the
	// game thread with a per-frame time budget; at 3 fps throttle that budget is ~15 ms/s
	// vs ~300 ms/s at 60 fps, making 1.9 GB of anim content take minutes instead of seconds.
	bPawnPreloadInProgress = true;
	if (GEditor)
	{
		GEditor->ShouldDisableCPUThrottlingDelegates.Add(
			UEditorEngine::FShouldDisableCPUThrottling::CreateWeakLambda(this, [this]()
			{
				return bPawnPreloadInProgress;
			})
		);
	}

	FNotificationInfo Info(NSLOCTEXT("Orchestrator", "PawnPreload",
		"Preloading pawn class (faster PIE startup)"));
	Info.bFireAndForget = false;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = true;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 0.0f;
	TSharedPtr<SNotificationItem> Notification =
		FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PawnPreloadHandle = StreamableManager.RequestAsyncLoad(
		PawnPath,
		FStreamableDelegate::CreateLambda([this, PawnPath, Notification]()
		{
			bPawnPreloadInProgress = false;

			UE_LOG(LogOrchestratorEditor, Log, TEXT("Pawn class preloaded: %s"),
				*PawnPath.ToString());

			if (Notification.IsValid())
			{
				Notification->SetText(NSLOCTEXT("Orchestrator", "PawnPreloadDone",
					"Pawn class ready"));
				Notification->SetCompletionState(SNotificationItem::CS_Success);
				Notification->ExpireAndFadeout();
			}
		}),
		FStreamableManager::AsyncLoadHighPriority,
		false,
		false,
		TEXT("PIE Pawn Preload")
	);

	if (PawnPreloadHandle.IsValid())
	{
		UE_LOG(LogOrchestratorEditor, Log,
			TEXT("  Started async preload for pawn class: %s"), *PawnPath.ToString());
	}
	else
	{
		bPawnPreloadInProgress = false;
	}
}

#endif // WITH_EDITOR
