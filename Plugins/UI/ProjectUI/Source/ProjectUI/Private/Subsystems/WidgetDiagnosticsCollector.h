// Copyright ALIS. All Rights Reserved.
// WidgetDiagnosticsCollector.h - Collect widget state for diagnostics (SOLID: Single Responsibility)

#pragma once

#include "CoreMinimal.h"

struct FProjectWidgetDiagnostics;
class UWidget;
class UProjectUserWidget;
class UProjectViewModel;

namespace WidgetDiagnosticsCollector
{
	// Collect full diagnostics for any UWidget
	FProjectWidgetDiagnostics Collect(UWidget* Widget);

	// Collect diagnostics for ProjectUserWidget (includes ViewModel)
	FProjectWidgetDiagnostics CollectProjectWidget(UProjectUserWidget* Widget);

	// Get widget path string
	FString GetWidgetPath(UWidget* Widget);

	// Get slot info string
	FString GetSlotInfoString(UWidget* Widget);

	// Collect ViewModel properties
	TMap<FString, FString> CollectViewModelProperties(UProjectViewModel* ViewModel);
}
