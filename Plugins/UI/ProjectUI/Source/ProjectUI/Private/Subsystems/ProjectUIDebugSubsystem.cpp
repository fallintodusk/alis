// Copyright ALIS. All Rights Reserved.
// ProjectUIDebugSubsystem.cpp - Core subsystem lifecycle and coordination
//
// Split into multiple files for SOLID compliance:
// - ProjectUIDebugSubsystem.cpp (this file) - Subsystem lifecycle, verbosity, diagnostics, overlay control
// - ProjectUIDebugCommands.cpp - Console command registrations
// - WidgetTreeDumper.cpp - Widget tree dump (text/JSON), recursive traversal
// - WidgetEventTrace.cpp - Event trace ring buffer
// - WidgetDiagnostics.cpp - FProjectWidgetDiagnostics::ToString() formatting
// - WidgetDiagnosticsCollector.cpp - Collect widget state into diagnostics struct

#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Debug/ProjectUIDebugOverlay.h"
#include "WidgetDiagnosticsCollector.h"
#include "Widgets/ProjectUserWidget.h"
#include "Widgets/SWeakWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogProjectUIDebug);

// =============================================================================
// Subsystem Lifecycle
// =============================================================================

void UProjectUIDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogProjectUIDebug, Display, TEXT("ProjectUIDebugSubsystem initialized (Verbosity=%d)"),
		static_cast<int32>(Verbosity));
}

void UProjectUIDebugSubsystem::Deinitialize()
{
	DestroyOverlay();

	UE_LOG(LogProjectUIDebug, Display, TEXT("ProjectUIDebugSubsystem deinitialized"));
	Super::Deinitialize();
}

// =============================================================================
// Verbosity Control
// =============================================================================

void UProjectUIDebugSubsystem::SetVerbosity(EProjectUIDebugVerbosity InVerbosity)
{
	EProjectUIDebugVerbosity OldVerbosity = Verbosity;
	Verbosity = InVerbosity;

	UE_LOG(LogProjectUIDebug, Display, TEXT("UI Debug verbosity changed: %d -> %d"),
		static_cast<int32>(OldVerbosity), static_cast<int32>(Verbosity));
}

bool UProjectUIDebugSubsystem::IsVerbosityEnabled(EProjectUIDebugVerbosity Level) const
{
	return static_cast<int32>(Verbosity) >= static_cast<int32>(Level);
}

// =============================================================================
// Widget Diagnostics (delegates to WidgetDiagnosticsCollector)
// =============================================================================

FProjectWidgetDiagnostics UProjectUIDebugSubsystem::DiagnoseWidget(UWidget* Widget) const
{
	FProjectWidgetDiagnostics Diag = WidgetDiagnosticsCollector::Collect(Widget);
	Diag.WidgetPath = WidgetDiagnosticsCollector::GetWidgetPath(Widget);
	return Diag;
}

FProjectWidgetDiagnostics UProjectUIDebugSubsystem::DiagnoseProjectWidget(UProjectUserWidget* Widget) const
{
	FProjectWidgetDiagnostics Diag = WidgetDiagnosticsCollector::CollectProjectWidget(Widget);
	Diag.WidgetPath = WidgetDiagnosticsCollector::GetWidgetPath(Widget);
	return Diag;
}

FProjectWidgetDiagnostics UProjectUIDebugSubsystem::DiagnoseWidgetByName(const FString& WidgetName) const
{
	FProjectWidgetDiagnostics Diag;
	Diag.Issues.Add(FString::Printf(TEXT("Widget '%s' not found. Use DiagnoseWidget() with direct reference."), *WidgetName));
	return Diag;
}

FString UProjectUIDebugSubsystem::GetWidgetPath(UWidget* Widget) const
{
	return WidgetDiagnosticsCollector::GetWidgetPath(Widget);
}

// =============================================================================
// Issue Detection
// =============================================================================

bool UProjectUIDebugSubsystem::HasZeroSize(UWidget* Widget) const
{
	if (!Widget)
	{
		return true;
	}

	FVector2D DesiredSize = Widget->GetDesiredSize();
	FVector2D CachedSize = Widget->GetCachedGeometry().GetLocalSize();

	return DesiredSize.IsNearlyZero() || CachedSize.IsNearlyZero();
}

bool UProjectUIDebugSubsystem::IsCanvasSlotWithoutAutoSize(UWidget* Widget) const
{
	if (!Widget || !Widget->Slot)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return false;
	}

	return !CanvasSlot->GetAutoSize();
}

bool UProjectUIDebugSubsystem::HasViewModelBound(UProjectUserWidget* Widget) const
{
	if (!Widget)
	{
		return false;
	}

	return Widget->GetViewModel() != nullptr;
}

// =============================================================================
// Logging Helpers
// =============================================================================

void UProjectUIDebugSubsystem::LogDebug(EProjectUIDebugVerbosity Level, const FString& Message) const
{
	if (!IsVerbosityEnabled(Level))
	{
		return;
	}

	switch (Level)
	{
	case EProjectUIDebugVerbosity::Error:
		UE_LOG(LogProjectUIDebug, Error, TEXT("%s"), *Message);
		break;
	case EProjectUIDebugVerbosity::Warning:
		UE_LOG(LogProjectUIDebug, Warning, TEXT("%s"), *Message);
		break;
	case EProjectUIDebugVerbosity::Info:
		UE_LOG(LogProjectUIDebug, Display, TEXT("%s"), *Message);
		break;
	case EProjectUIDebugVerbosity::Verbose:
		UE_LOG(LogProjectUIDebug, Verbose, TEXT("%s"), *Message);
		break;
	case EProjectUIDebugVerbosity::VeryVerbose:
		UE_LOG(LogProjectUIDebug, VeryVerbose, TEXT("%s"), *Message);
		break;
	default:
		break;
	}
}

void UProjectUIDebugSubsystem::LogWidgetLifecycle(UWidget* Widget, const FString& Event, EProjectUIDebugVerbosity Level) const
{
	if (!IsVerbosityEnabled(Level) || !Widget)
	{
		return;
	}

	FString WidgetInfo = FString::Printf(TEXT("[%s] %s"),
		*Widget->GetClass()->GetName(),
		*Widget->GetName());

	if (IsVerbosityEnabled(EProjectUIDebugVerbosity::VeryVerbose))
	{
		FVector2D Size = Widget->GetCachedGeometry().GetLocalSize();
		WidgetInfo += FString::Printf(TEXT(" (%.0fx%.0f)"), Size.X, Size.Y);
	}

	LogDebug(Level, FString::Printf(TEXT("LIFECYCLE: %s - %s"), *WidgetInfo, *Event));
}

void UProjectUIDebugSubsystem::LogViewModelBinding(UProjectUserWidget* Widget, UProjectViewModel* ViewModel, const FString& Event) const
{
	if (!IsVerbosityEnabled(EProjectUIDebugVerbosity::Info))
	{
		return;
	}

	FString WidgetName = Widget ? Widget->GetName() : TEXT("<null>");
	FString VMName = ViewModel ? ViewModel->GetClass()->GetName() : TEXT("<null>");

	LogDebug(EProjectUIDebugVerbosity::Info,
		FString::Printf(TEXT("VIEWMODEL: %s <- %s : %s"), *WidgetName, *VMName, *Event));
}

void UProjectUIDebugSubsystem::LogPropertyChange(UProjectViewModel* ViewModel, FName PropertyName, const FString& OldValue, const FString& NewValue) const
{
	if (!IsVerbosityEnabled(EProjectUIDebugVerbosity::Verbose))
	{
		return;
	}

	FString VMName = ViewModel ? ViewModel->GetClass()->GetName() : TEXT("<null>");

	LogDebug(EProjectUIDebugVerbosity::Verbose,
		FString::Printf(TEXT("PROPERTY: %s.%s = %s -> %s"),
			*VMName, *PropertyName.ToString(), *OldValue, *NewValue));
}

void UProjectUIDebugSubsystem::LogGeometryChange(UWidget* Widget, const FVector2D& OldSize, const FVector2D& NewSize) const
{
	if (!IsVerbosityEnabled(EProjectUIDebugVerbosity::Verbose))
	{
		return;
	}

	FString WidgetName = Widget ? Widget->GetName() : TEXT("<null>");

	if (NewSize.IsNearlyZero() && !OldSize.IsNearlyZero())
	{
		LogDebug(EProjectUIDebugVerbosity::Warning,
			FString::Printf(TEXT("GEOMETRY: %s size collapsed to 0x0 (was %.0fx%.0f)"),
				*WidgetName, OldSize.X, OldSize.Y));
	}
	else
	{
		LogDebug(EProjectUIDebugVerbosity::Verbose,
			FString::Printf(TEXT("GEOMETRY: %s size %.0fx%.0f -> %.0fx%.0f"),
				*WidgetName, OldSize.X, OldSize.Y, NewSize.X, NewSize.Y));
	}
}

// =============================================================================
// Static Helpers
// =============================================================================

UProjectUIDebugSubsystem* UProjectUIDebugSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<UProjectUIDebugSubsystem>();
}

void UProjectUIDebugSubsystem::QuickDiagnose(const UObject* WorldContextObject, UWidget* Widget)
{
	UProjectUIDebugSubsystem* DebugSub = Get(WorldContextObject);
	if (!DebugSub)
	{
		UE_LOG(LogProjectUIDebug, Warning, TEXT("QuickDiagnose: No debug subsystem available"));
		return;
	}

	FProjectWidgetDiagnostics Diag;

	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		Diag = DebugSub->DiagnoseProjectWidget(ProjectWidget);
	}
	else
	{
		Diag = DebugSub->DiagnoseWidget(Widget);
	}

	UE_LOG(LogProjectUIDebug, Display, TEXT("%s"), *Diag.ToString());
}

// =============================================================================
// Private Helpers (delegated to WidgetDiagnosticsCollector)
// =============================================================================

TMap<FString, FString> UProjectUIDebugSubsystem::CollectViewModelProperties(UProjectViewModel* ViewModel) const
{
	return WidgetDiagnosticsCollector::CollectViewModelProperties(ViewModel);
}

void UProjectUIDebugSubsystem::BuildWidgetPath(UWidget* Widget, TArray<FString>& PathComponents) const
{
	// Delegated to collector - kept for header compatibility
}

FString UProjectUIDebugSubsystem::GetSlotInfoString(UWidget* Widget) const
{
	return WidgetDiagnosticsCollector::GetSlotInfoString(Widget);
}

// =============================================================================
// Debug Overlay Control
// =============================================================================

void UProjectUIDebugSubsystem::SetOverlayEnabled(bool bEnabled)
{
	if (bEnabled == bOverlayEnabled)
	{
		return;
	}

	bOverlayEnabled = bEnabled;

	if (bEnabled)
	{
		CreateOverlay();
	}
	else
	{
		DestroyOverlay();
	}
}

void UProjectUIDebugSubsystem::SetOverlayShowNames(bool bShow)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->GetSettings().bShowNames = bShow;
		OverlayWidget->RefreshWidgetCache();
	}
}

void UProjectUIDebugSubsystem::SetOverlayShowSizes(bool bShow)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->GetSettings().bShowSizes = bShow;
		OverlayWidget->RefreshWidgetCache();
	}
}

void UProjectUIDebugSubsystem::SetOverlayShowZOrder(bool bShow)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->GetSettings().bShowZOrder = bShow;
		OverlayWidget->RefreshWidgetCache();
	}
}

void UProjectUIDebugSubsystem::SetOverlayFilterProblems(bool bFilter)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->GetSettings().bFilterProblems = bFilter;
		OverlayWidget->RefreshWidgetCache();
	}
}

void UProjectUIDebugSubsystem::SetOverlayNameFilter(const FString& NamePattern)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->GetSettings().NameFilter = NamePattern;
		OverlayWidget->RefreshWidgetCache();
	}
}

void UProjectUIDebugSubsystem::CreateOverlay()
{
	if (OverlayWidget.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameViewportClient* ViewportClient = World->GetGameViewport();
	if (!ViewportClient)
	{
		return;
	}

	OverlayWidget = SNew(SProjectUIDebugOverlay);

	ViewportClient->AddViewportWidgetContent(
		SNew(SWeakWidget).PossiblyNullContent(OverlayWidget.ToSharedRef()),
		1000
	);

	UE_LOG(LogProjectUIDebug, Display, TEXT("Debug overlay created"));
}

void UProjectUIDebugSubsystem::DestroyOverlay()
{
	if (!OverlayWidget.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		UGameViewportClient* ViewportClient = World->GetGameViewport();
		if (ViewportClient)
		{
			ViewportClient->RemoveViewportWidgetContent(
				SNew(SWeakWidget).PossiblyNullContent(OverlayWidget.ToSharedRef())
			);
		}
	}

	OverlayWidget.Reset();
	bOverlayEnabled = false;

	UE_LOG(LogProjectUIDebug, Display, TEXT("Debug overlay destroyed"));
}
