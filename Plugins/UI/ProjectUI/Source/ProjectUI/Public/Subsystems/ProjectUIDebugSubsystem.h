// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectUIDebugSubsystem.generated.h"

class UWidget;
class UProjectViewModel;
class UProjectUserWidget;
class UPanelWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogProjectUIDebug, Log, All);

/**
 * Widget lifecycle event types for event trace (Phase 3).
 */
UENUM(BlueprintType)
enum class EProjectUIWidgetEvent : uint8
{
	/** Widget created */
	Created,
	/** Widget added to parent/viewport */
	AddedToParent,
	/** Widget removed from parent/viewport */
	RemovedFromParent,
	/** Widget visibility changed */
	VisibilityChanged,
	/** Widget slot changed (moved, resized) */
	SlotChanged,
	/** Widget geometry changed (actual size) */
	GeometryChanged,
	/** ViewModel bound */
	ViewModelBound,
	/** ViewModel unbound */
	ViewModelUnbound,
	/** Widget destroyed */
	Destroyed
};

/**
 * Single event in the widget event trace ring buffer.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectUIWidgetEventEntry
{
	GENERATED_BODY()

	/** When the event occurred */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	double Timestamp = 0.0;

	/** Frame number when event occurred */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	int64 FrameNumber = 0;

	/** Event type */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	EProjectUIWidgetEvent EventType = EProjectUIWidgetEvent::Created;

	/** Widget name */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString WidgetName;

	/** Widget class */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString WidgetClass;

	/** Parent widget (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ParentName;

	/** Event details (visibility state, old/new size, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString Details;

	/** Format as human-readable string */
	FString ToString() const;
};

/**
 * UI Debug verbosity levels.
 * Higher levels include all lower levels.
 */
UENUM(BlueprintType)
enum class EProjectUIDebugVerbosity : uint8
{
	/** No debug output */
	Silent = 0,

	/** Only errors (invisible widgets, null ViewModels, binding failures) */
	Error = 1,

	/** Errors + warnings (zero-size widgets, missing properties) */
	Warning = 2,

	/** Errors + warnings + lifecycle events (construct, destruct, visibility) */
	Info = 3,

	/** All above + property changes, geometry updates */
	Verbose = 4,

	/** Full trace: all events with stack traces and timing */
	VeryVerbose = 5
};

/**
 * Widget diagnostic snapshot - complete state capture for debugging.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectWidgetDiagnostics
{
	GENERATED_BODY()

	/** Widget name (GetName()) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString WidgetName;

	/** Widget class name */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ClassName;

	/** Full path in widget tree (Parent > Child > ...) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString WidgetPath;

	/** Is widget visible? */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	bool bIsVisible = false;

	/** Is widget in viewport? */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	bool bIsInViewport = false;

	/** Desired size from GetDesiredSize() */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FVector2D DesiredSize = FVector2D::ZeroVector;

	/** Cached geometry size (actual rendered size) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FVector2D CachedGeometrySize = FVector2D::ZeroVector;

	/** Cached geometry position (screen space) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FVector2D CachedGeometryPosition = FVector2D::ZeroVector;

	/** Parent widget name (if any) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ParentName;

	/** Parent widget class */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ParentClass;

	/** Slot type (CanvasPanelSlot, OverlaySlot, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString SlotType;

	/** Slot auto-size setting (for CanvasPanelSlot) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	bool bSlotAutoSize = false;

	/** ViewModel class (if bound) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ViewModelClass;

	/** ViewModel property dump */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	TMap<FString, FString> ViewModelProperties;

	/** Number of child widgets */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	int32 ChildCount = 0;

	/** Diagnosis: issues found */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	TArray<FString> Issues;

	/** Diagnosis: recommendations */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	TArray<FString> Recommendations;

	/** Timestamp of capture */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FDateTime CaptureTime;

	/** Full parent chain with visibility/size */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	TArray<FString> ParentChain;

	/** Render transform info */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString RenderTransformInfo;

	/** Clipping info */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString ClippingInfo;

	/** Z-Order in parent */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	int32 ZOrder = 0;

	/** Anchors info (for CanvasPanelSlot) */
	UPROPERTY(BlueprintReadOnly, Category = "UI Debug")
	FString AnchorsInfo;

	/** Format as human-readable string */
	FString ToString() const;
};

/**
 * UI Debug Subsystem - Fast widget issue investigation.
 *
 * Provides comprehensive debugging for UI widgets:
 * - Verbosity levels (Silent -> VeryVerbose)
 * - Widget diagnostics snapshot
 * - Geometry/sizing inspection
 * - ViewModel binding validation
 * - Automatic issue detection
 *
 * Usage:
 * @code
 * // Enable debugging
 * UProjectUIDebugSubsystem* DebugUI = GI->GetSubsystem<UProjectUIDebugSubsystem>();
 * DebugUI->SetVerbosity(EProjectUIDebugVerbosity::Verbose);
 *
 * // Get diagnostics for a widget
 * FProjectWidgetDiagnostics Diag = DebugUI->DiagnoseWidget(MyWidget);
 * UE_LOG(LogTemp, Display, TEXT("%s"), *Diag.ToString());
 *
 * // Quick check in code
 * PROJECT_UI_DEBUG(Verbose, TEXT("Widget created: %s"), *Widget->GetName());
 * @endcode
 *
 * Console commands:
 * - UI.Debug.Verbosity [0-5] - Set debug level
 * - UI.Debug.Diagnose [WidgetName] - Print diagnostics
 * - UI.Debug.DumpTree - Dump full widget tree
 */
UCLASS()
class PROJECTUI_API UProjectUIDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==========================================================================
	// Verbosity Control
	// ==========================================================================

	/**
	 * Set debug verbosity level.
	 * @param InVerbosity - New verbosity level
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetVerbosity(EProjectUIDebugVerbosity InVerbosity);

	/**
	 * Get current verbosity level.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	EProjectUIDebugVerbosity GetVerbosity() const { return Verbosity; }

	/**
	 * Check if a verbosity level is enabled.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool IsVerbosityEnabled(EProjectUIDebugVerbosity Level) const;

	// ==========================================================================
	// Widget Diagnostics
	// ==========================================================================

	/**
	 * Capture full diagnostics for a widget.
	 * Inspects geometry, visibility, ViewModel binding, slot configuration.
	 *
	 * @param Widget - Widget to diagnose
	 * @return Diagnostic snapshot with issues and recommendations
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	FProjectWidgetDiagnostics DiagnoseWidget(UWidget* Widget) const;

	/**
	 * Diagnose a ProjectUserWidget with ViewModel inspection.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	FProjectWidgetDiagnostics DiagnoseProjectWidget(UProjectUserWidget* Widget) const;

	/**
	 * Find and diagnose a widget by name (searches viewport).
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	FProjectWidgetDiagnostics DiagnoseWidgetByName(const FString& WidgetName) const;

	/**
	 * Get widget tree path (Parent > Child > Target).
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	FString GetWidgetPath(UWidget* Widget) const;

	/**
	 * Dump entire widget tree to log.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void DumpWidgetTree() const;

	/**
	 * Dump widget tree with options for agent/automation use.
	 * @param OutPath - Optional file path to write output (empty = log only, relative paths are under Saved/)
	 * @param Format - Output format: "txt" or "json"
	 * @param Filter - Optional widget name filter (only dump subtree containing this widget)
	 * @return True if dump succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	bool DumpWidgetTreeEx(const FString& OutPath = TEXT(""), const FString& Format = TEXT("txt"), const FString& Filter = TEXT("")) const;

	// ==========================================================================
	// Issue Detection
	// ==========================================================================

	/**
	 * Check if widget has zero size (invisible but exists).
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool HasZeroSize(UWidget* Widget) const;

	/**
	 * Check if widget is in Canvas slot without AutoSize.
	 * Common cause of invisible widgets.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool IsCanvasSlotWithoutAutoSize(UWidget* Widget) const;

	/**
	 * Check if ProjectUserWidget has ViewModel bound.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool HasViewModelBound(UProjectUserWidget* Widget) const;

	// ==========================================================================
	// Logging Helpers (for use with macros)
	// ==========================================================================

	/**
	 * Log a debug message if verbosity level allows.
	 */
	void LogDebug(EProjectUIDebugVerbosity Level, const FString& Message) const;

	/**
	 * Log widget lifecycle event.
	 */
	void LogWidgetLifecycle(UWidget* Widget, const FString& Event, EProjectUIDebugVerbosity Level = EProjectUIDebugVerbosity::Info) const;

	/**
	 * Log ViewModel binding event.
	 */
	void LogViewModelBinding(UProjectUserWidget* Widget, UProjectViewModel* ViewModel, const FString& Event) const;

	/**
	 * Log property change event.
	 */
	void LogPropertyChange(UProjectViewModel* ViewModel, FName PropertyName, const FString& OldValue, const FString& NewValue) const;

	/**
	 * Log geometry change event.
	 */
	void LogGeometryChange(UWidget* Widget, const FVector2D& OldSize, const FVector2D& NewSize) const;

	// ==========================================================================
	// Static Helpers
	// ==========================================================================

	/**
	 * Get singleton instance from world context.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Debug", meta = (WorldContext = "WorldContextObject"))
	static UProjectUIDebugSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Quick diagnose and log (convenience for debugging).
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug", meta = (WorldContext = "WorldContextObject"))
	static void QuickDiagnose(const UObject* WorldContextObject, UWidget* Widget);

	// ==========================================================================
	// Debug Overlay (Phase 2)
	// ==========================================================================

	/**
	 * Toggle debug overlay on/off.
	 * Shows colored rectangles over all widgets:
	 * - Green = visible, correct size
	 * - Yellow = clipped by parent
	 * - Red = zero size or invisible
	 * - Blue = focused
	 * - Orange = missing ViewModel
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayEnabled(bool bEnabled);

	/** Check if debug overlay is currently visible. */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool IsOverlayEnabled() const { return bOverlayEnabled; }

	/** Toggle overlay name labels. */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayShowNames(bool bShow);

	/** Toggle overlay size labels. */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayShowSizes(bool bShow);

	/** Toggle overlay Z-order labels. */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayShowZOrder(bool bShow);

	/** Filter overlay to show only problematic widgets. */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayFilterProblems(bool bFilter);

	/** Filter overlay by widget name pattern. */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetOverlayNameFilter(const FString& NamePattern);

	// ==========================================================================
	// Event Trace (Phase 3) - Widget lifecycle ring buffer
	// ==========================================================================

	/**
	 * Enable/disable widget event tracing.
	 * When enabled, widget lifecycle events are recorded to a ring buffer.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetEventTraceEnabled(bool bEnabled);

	/** Check if event tracing is enabled. */
	UFUNCTION(BlueprintPure, Category = "UI|Debug")
	bool IsEventTraceEnabled() const { return bEventTraceEnabled; }

	/**
	 * Record a widget event to the trace buffer.
	 * Only records if tracing is enabled.
	 */
	void RecordWidgetEvent(
		EProjectUIWidgetEvent EventType,
		UWidget* Widget,
		const FString& Details = TEXT(""));

	/**
	 * Get all events currently in the ring buffer.
	 * Returns oldest to newest.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	TArray<FProjectUIWidgetEventEntry> GetEventTrace() const;

	/**
	 * Get events filtered by widget name pattern.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	TArray<FProjectUIWidgetEventEntry> GetEventTraceFiltered(const FString& WidgetNamePattern) const;

	/**
	 * Clear the event trace buffer.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void ClearEventTrace();

	/**
	 * Dump event trace to log or file.
	 * @param OutPath - File path (empty = log only, relative paths under Saved/)
	 * @return True if dump succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	bool DumpEventTrace(const FString& OutPath = TEXT("")) const;

	/**
	 * Set ring buffer capacity (default 256 events).
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void SetEventTraceCapacity(int32 NewCapacity);

private:
	UPROPERTY()
	EProjectUIDebugVerbosity Verbosity = EProjectUIDebugVerbosity::Warning;

	/** Debug overlay enabled state */
	bool bOverlayEnabled = false;

	/** Debug overlay Slate widget (added to viewport when enabled) */
	TSharedPtr<class SProjectUIDebugOverlay> OverlayWidget;

	/** Event trace enabled state */
	bool bEventTraceEnabled = false;

	/** Ring buffer for widget events */
	TArray<FProjectUIWidgetEventEntry> EventTraceBuffer;

	/** Current write position in ring buffer */
	int32 EventTraceWriteIndex = 0;

	/** Number of events recorded (may exceed capacity) */
	int32 EventTraceCount = 0;

	/** Ring buffer capacity */
	int32 EventTraceCapacity = 256;

	/** Create and show the debug overlay */
	void CreateOverlay();

	/** Destroy and hide the debug overlay */
	void DestroyOverlay();

	/** Collect ViewModel properties as string map */
	TMap<FString, FString> CollectViewModelProperties(UProjectViewModel* ViewModel) const;

	/** Build widget path recursively */
	void BuildWidgetPath(UWidget* Widget, TArray<FString>& PathComponents) const;

	/** Get slot info string */
	FString GetSlotInfoString(UWidget* Widget) const;

	/** Dump widget tree recursively */
	void DumpWidgetTreeRecursive(UWidget* Widget, int32 Depth) const;

	/** Dump widget tree recursively to string array (for file output) */
	void DumpWidgetTreeRecursiveToArray(UWidget* Widget, int32 Depth, TArray<FString>& OutLines) const;

	/** Build JSON object for widget tree */
	TSharedPtr<FJsonObject> BuildWidgetTreeJson(UWidget* Widget) const;

	/** Get determinism inputs as JSON */
	TSharedPtr<FJsonObject> GetDeterminismInputsJson() const;
};

// =============================================================================
// Debug Macros (for use in widget code)
// =============================================================================

/**
 * Log UI debug message if verbosity allows.
 * Usage: PROJECT_UI_DEBUG(Verbose, TEXT("My message: %s"), *SomeString);
 */
#define PROJECT_UI_DEBUG(Level, Format, ...) \
	do { \
		if (UProjectUIDebugSubsystem* DebugSub = UProjectUIDebugSubsystem::Get(this)) \
		{ \
			if (DebugSub->IsVerbosityEnabled(EProjectUIDebugVerbosity::Level)) \
			{ \
				DebugSub->LogDebug(EProjectUIDebugVerbosity::Level, FString::Printf(Format, ##__VA_ARGS__)); \
			} \
		} \
	} while (0)

/**
 * Log widget lifecycle event.
 * Usage: PROJECT_UI_LIFECYCLE(this, TEXT("NativeConstruct"));
 */
#define PROJECT_UI_LIFECYCLE(Widget, Event) \
	do { \
		if (UProjectUIDebugSubsystem* DebugSub = UProjectUIDebugSubsystem::Get(Widget)) \
		{ \
			DebugSub->LogWidgetLifecycle(Widget, Event); \
		} \
	} while (0)

/**
 * Quick diagnose and log a widget.
 * Usage: PROJECT_UI_DIAGNOSE(MyWidget);
 */
#define PROJECT_UI_DIAGNOSE(Widget) \
	do { \
		UProjectUIDebugSubsystem::QuickDiagnose(Widget, Widget); \
	} while (0)

/**
 * Log error and diagnose widget (always logs at Error level).
 * Usage: PROJECT_UI_ERROR_DIAGNOSE(MyWidget, TEXT("Widget not visible!"));
 */
#define PROJECT_UI_ERROR_DIAGNOSE(Widget, Message) \
	do { \
		if (UProjectUIDebugSubsystem* DebugSub = UProjectUIDebugSubsystem::Get(Widget)) \
		{ \
			DebugSub->LogDebug(EProjectUIDebugVerbosity::Error, Message); \
			DebugSub->QuickDiagnose(Widget, Widget); \
		} \
	} while (0)
