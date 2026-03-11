// Copyright ALIS. All Rights Reserved.
// ProjectUIDebugCommands.cpp - Console command registrations for UI debug subsystem
// Extracted from ProjectUIDebugSubsystem.cpp for SOLID compliance

#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Engine/GameInstance.h"

// =============================================================================
// Verbosity Console Command
// =============================================================================

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugVerbosity(
	TEXT("UI.Debug.Verbosity"),
	TEXT("Set UI debug verbosity level (0=Silent, 1=Error, 2=Warning, 3=Info, 4=Verbose, 5=VeryVerbose)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1) return;

		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;

		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		int32 Level = FCString::Atoi(*Args[0]);
		Level = FMath::Clamp(Level, 0, 5);
		DebugSub->SetVerbosity(static_cast<EProjectUIDebugVerbosity>(Level));
	})
);

// =============================================================================
// Widget Tree Dump Console Command
// =============================================================================

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugDumpTree(
	TEXT("UI.Debug.DumpTree"),
	TEXT("Dump widget tree. Usage: UI.Debug.DumpTree [out=<path>] [format=txt|json] [filter=<WidgetName>]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;

		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;

		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		FString OutPath;
		FString Format = TEXT("txt");
		FString Filter;

		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("out=")))
			{
				OutPath = Arg.Mid(4);
			}
			else if (Arg.StartsWith(TEXT("format=")))
			{
				Format = Arg.Mid(7).ToLower();
			}
			else if (Arg.StartsWith(TEXT("filter=")))
			{
				Filter = Arg.Mid(7);
			}
		}

		DebugSub->DumpWidgetTreeEx(OutPath, Format, Filter);
	})
);

// =============================================================================
// Debug Overlay Console Commands
// =============================================================================

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugOverlay(
	TEXT("UI.Debug.Overlay"),
	TEXT("Toggle debug overlay. Usage: UI.Debug.Overlay [0/1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		if (Args.Num() > 0)
		{
			bool bEnable = FCString::Atoi(*Args[0]) != 0;
			DebugSub->SetOverlayEnabled(bEnable);
		}
		else
		{
			DebugSub->SetOverlayEnabled(!DebugSub->IsOverlayEnabled());
		}
		UE_LOG(LogProjectUIDebug, Display, TEXT("Debug overlay: %s"),
			DebugSub->IsOverlayEnabled() ? TEXT("ON") : TEXT("OFF"));
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugOverlayNames(
	TEXT("UI.Debug.Overlay.ShowNames"),
	TEXT("Toggle widget name labels. Usage: UI.Debug.Overlay.ShowNames [0/1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		bool bShow = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : true;
		DebugSub->SetOverlayShowNames(bShow);
		UE_LOG(LogProjectUIDebug, Display, TEXT("Overlay names: %s"), bShow ? TEXT("ON") : TEXT("OFF"));
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugOverlaySizes(
	TEXT("UI.Debug.Overlay.ShowSizes"),
	TEXT("Toggle widget size labels. Usage: UI.Debug.Overlay.ShowSizes [0/1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		bool bShow = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : true;
		DebugSub->SetOverlayShowSizes(bShow);
		UE_LOG(LogProjectUIDebug, Display, TEXT("Overlay sizes: %s"), bShow ? TEXT("ON") : TEXT("OFF"));
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugOverlayProblems(
	TEXT("UI.Debug.Overlay.FilterProblems"),
	TEXT("Show only problematic widgets. Usage: UI.Debug.Overlay.FilterProblems [0/1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		bool bFilter = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : true;
		DebugSub->SetOverlayFilterProblems(bFilter);
		UE_LOG(LogProjectUIDebug, Display, TEXT("Overlay filter problems: %s"), bFilter ? TEXT("ON") : TEXT("OFF"));
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugOverlayFilter(
	TEXT("UI.Debug.Overlay.FilterName"),
	TEXT("Filter overlay by widget name pattern. Usage: UI.Debug.Overlay.FilterName [pattern]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		FString Pattern = Args.Num() > 0 ? Args[0] : TEXT("");
		DebugSub->SetOverlayNameFilter(Pattern);
		UE_LOG(LogProjectUIDebug, Display, TEXT("Overlay name filter: '%s'"), *Pattern);
	})
);

// =============================================================================
// Event Trace Console Commands
// =============================================================================

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugEventTrace(
	TEXT("UI.Debug.EventTrace"),
	TEXT("Toggle event tracing. Usage: UI.Debug.EventTrace [0/1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		if (Args.Num() > 0)
		{
			bool bEnable = FCString::Atoi(*Args[0]) != 0;
			DebugSub->SetEventTraceEnabled(bEnable);
		}
		else
		{
			DebugSub->SetEventTraceEnabled(!DebugSub->IsEventTraceEnabled());
		}
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugEventTraceDump(
	TEXT("UI.Debug.EventTrace.Dump"),
	TEXT("Dump event trace. Usage: UI.Debug.EventTrace.Dump [out=<path>]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		FString OutPath;
		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("out=")))
			{
				OutPath = Arg.Mid(4);
			}
		}
		DebugSub->DumpEventTrace(OutPath);
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugEventTraceClear(
	TEXT("UI.Debug.EventTrace.Clear"),
	TEXT("Clear event trace buffer."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		DebugSub->ClearEventTrace();
	})
);

static FAutoConsoleCommandWithWorldAndArgs CVarUIDebugEventTraceCapacity(
	TEXT("UI.Debug.EventTrace.Capacity"),
	TEXT("Set event trace capacity. Usage: UI.Debug.EventTrace.Capacity <count>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
		if (!DebugSub) return;

		int32 Capacity = FCString::Atoi(*Args[0]);
		DebugSub->SetEventTraceCapacity(Capacity);
	})
);
