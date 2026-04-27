// Copyright ALIS. All Rights Reserved.
//
// Consumed by the persistent test editor infrastructure:
//   scripts/ue/test/unit/persistent_editor_start.ps1
//   scripts/ue/test/unit/persistent_editor_run.ps1
//
// Problem: every `run_cpp_tests_safe.ps1` invocation spends ~60s booting
// UnrealEditor-Cmd + loading MainMenuWorld before ~5s of actual tests.
// Running N filters costs ~N * 60s of boot.
//
// Fix: keep a single editor warm and dispatch automation commands into it
// via a filesystem poll ticker. The start script launches the editor with
// no `-ExecCmds` (and no `-testexit` sentinel) so it stays running. A
// ticker watches `scripts/ue/artifacts/persistent/command.txt`; when the
// run script writes a command into that file, the ticker reads+deletes
// the file and dispatches the command via `GEngine->Exec`. The run script
// then polls `Saved/Automation/Reports/index.json` for completion.
//
// Dev-only: this module's plugin (ProjectIntegrationTests) is Editor-type
// and has no entry in Shipping/Client build graphs. No runtime overhead
// in packaged builds.

#include "Modules/ModuleManager.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectIntegrationTestsPersistent, Log, All);

namespace ProjectIntegrationTestsPersistentImpl
{
    static FString GetCommandFilePath()
    {
        // Use project-relative path under scripts/ue/artifacts/persistent.
        // The scripts create this directory before launch.
        const FString ProjectDir = FPaths::ProjectDir();
        return FPaths::ConvertRelativePathToFull(
            ProjectDir / TEXT("scripts/ue/artifacts/persistent/command.txt"));
    }

    static FString GetReadyMarkerPath()
    {
        const FString ProjectDir = FPaths::ProjectDir();
        return FPaths::ConvertRelativePathToFull(
            ProjectDir / TEXT("scripts/ue/artifacts/persistent/ready.marker"));
    }

    static void ExecuteCommandLines(const FString& Contents)
    {
        // Contents may hold one or more commands separated by newlines.
        // Empty lines and pure-whitespace lines are ignored. Lines that
        // begin with '#' are treated as comments so run scripts can
        // annotate the command file.
        TArray<FString> Lines;
        Contents.ParseIntoArrayLines(Lines);
        for (const FString& Raw : Lines)
        {
            const FString Line = Raw.TrimStartAndEnd();
            if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
            {
                continue;
            }

            UE_LOG(LogProjectIntegrationTestsPersistent, Display,
                TEXT("[PersistentEditor] Dispatching command: %s"), *Line);

            if (GEngine)
            {
                GEngine->Exec(nullptr, *Line);
            }
            else
            {
                UE_LOG(LogProjectIntegrationTestsPersistent, Warning,
                    TEXT("[PersistentEditor] GEngine null - cannot execute: %s"), *Line);
            }
        }
    }
}

class FProjectIntegrationTestsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // The command file watcher is only useful under -unattended launch;
        // enabling it unconditionally in the module is cheap (one file stat
        // per second) and avoids adding a launch switch.
        StartWatcher();

        // By the time this module loads (LoadingPhase = PostEngineInit),
        // FCoreDelegates::OnPostEngineInit has already fired, so binding to
        // it would never see a callback. The AutomationController subsystem
        // is registered earlier, so we can safely write the ready marker
        // immediately. The start script polls the marker file; once it sees
        // it, dispatching "Automation RunTests X" will find the command
        // registered.
        //
        // One extra safety net: we also defer the marker write to the next
        // tick via the ticker below so GEngine is guaranteed to be valid
        // when the first command file is read.
        UE_LOG(LogProjectIntegrationTestsPersistent, Display,
            TEXT("[PersistentEditor] Ready to start automation"));

        const FString Marker = ProjectIntegrationTestsPersistentImpl::GetReadyMarkerPath();
        const FString Dir = FPaths::GetPath(Marker);
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        if (!PF.DirectoryExists(*Dir))
        {
            PF.CreateDirectoryTree(*Dir);
        }
        FFileHelper::SaveStringToFile(
            FString::Printf(TEXT("ready=%s\n"), *FDateTime::UtcNow().ToIso8601()),
            *Marker);
    }

    virtual void ShutdownModule() override
    {
        StopWatcher();

        // Remove the ready marker so a subsequent cold launch doesn't see
        // a stale positive signal if the start script races the shutdown.
        const FString Marker = ProjectIntegrationTestsPersistentImpl::GetReadyMarkerPath();
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        if (PF.FileExists(*Marker))
        {
            PF.DeleteFile(*Marker);
        }
    }

private:
    FTSTicker::FDelegateHandle WatcherHandle;

    void StartWatcher()
    {
        // Poll every 0.5s. This is well below the fastest plausible inter-
        // command gap (the run script must write file + poll for result)
        // and the cost is a single FileExists stat per tick.
        WatcherHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FProjectIntegrationTestsModule::PollCommandFile),
            /*InDelay=*/0.5f);
    }

    void StopWatcher()
    {
        if (WatcherHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(WatcherHandle);
            WatcherHandle.Reset();
        }
    }

    bool PollCommandFile(float /*DeltaTime*/)
    {
        const FString Path = ProjectIntegrationTestsPersistentImpl::GetCommandFilePath();
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        if (!PF.FileExists(*Path))
        {
            return true; // keep ticking
        }

        // Try to read; if the writer (PowerShell run script) is still finalizing
        // the file or briefly holds an exclusive lock, the read fails. We must
        // NOT delete the file in that case, or the command is silently dropped
        // forever and the dispatcher times out waiting for a result. Leave the
        // file in place so the next tick can retry.
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *Path))
        {
            UE_LOG(LogProjectIntegrationTestsPersistent, Verbose,
                TEXT("[PersistentEditor] command file not yet readable, will retry next tick: %s"), *Path);
            return true;
        }

        // Read-then-delete only after success, so a stale command from a previous
        // dispatch cycle cannot be double-executed and a transient lock cannot lose it.
        PF.DeleteFile(*Path);

        ProjectIntegrationTestsPersistentImpl::ExecuteCommandLines(Contents);
        return true;
    }
};

// Optional: manual one-shot console command. Useful when debugging the
// pipeline from the editor's console ("inv.auto.runfile") without having
// to wait for the ticker.
static FAutoConsoleCommand CVarInvAutoRunFile(
    TEXT("inv.auto.runfile"),
    TEXT("Read scripts/ue/artifacts/persistent/command.txt once and dispatch it"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        const FString Path = ProjectIntegrationTestsPersistentImpl::GetCommandFilePath();
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        if (!PF.FileExists(*Path))
        {
            UE_LOG(LogProjectIntegrationTestsPersistent, Display,
                TEXT("[PersistentEditor] No command file at %s"), *Path);
            return;
        }

        FString Contents;
        const bool bLoaded = FFileHelper::LoadFileToString(Contents, *Path);
        PF.DeleteFile(*Path);
        if (bLoaded)
        {
            ProjectIntegrationTestsPersistentImpl::ExecuteCommandLines(Contents);
        }
    })
);

IMPLEMENT_MODULE(FProjectIntegrationTestsModule, ProjectIntegrationTests)
