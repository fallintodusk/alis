// Copyright ALIS. All Rights Reserved.

#include "Widgets/ProjectUserWidget.h"
#include "Theme/ProjectUIThemeManager.h"
#include "Theme/ProjectUIThemeData.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY(LogProjectUserWidget);

// =============================================================================
// Debug Helpers for Construction Flow
// =============================================================================

namespace ProjectUserWidgetDebug
{
	void LogWidgetTreeRecursive(UWidget* Widget, int32 Depth)
	{
		if (!Widget)
		{
			return;
		}

		FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		FString SlotInfo = TEXT("no slot");

		// Get slot info if in CanvasPanel
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FAnchors Anchors = CanvasSlot->GetAnchors();
			FVector2D SlotSize = CanvasSlot->GetSize();
			FVector2D Offset = CanvasSlot->GetPosition();
			bool bAutoSize = CanvasSlot->GetAutoSize();

			SlotInfo = FString::Printf(TEXT("CanvasSlot AutoSize=%s Size=(%.0f,%.0f) Offset=(%.0f,%.0f) Anchor=(%.2f,%.2f)-(%.2f,%.2f)"),
				bAutoSize ? TEXT("Y") : TEXT("N"),
				SlotSize.X, SlotSize.Y,
				Offset.X, Offset.Y,
				Anchors.Minimum.X, Anchors.Minimum.Y,
				Anchors.Maximum.X, Anchors.Maximum.Y);
		}

		FVector2D DesiredSize = Widget->GetDesiredSize();
		FVector2D CachedSize = Widget->GetCachedGeometry().GetLocalSize();
		bool bVisible = Widget->IsVisible();

		UE_LOG(LogProjectUserWidget, Log, TEXT("%s[%s] %s Desired=(%.0f,%.0f) Cached=(%.0f,%.0f) Vis=%s | %s"),
			*Indent,
			*Widget->GetClass()->GetName(),
			*Widget->GetName(),
			DesiredSize.X, DesiredSize.Y,
			CachedSize.X, CachedSize.Y,
			bVisible ? TEXT("Y") : TEXT("N"),
			*SlotInfo);

		// Recurse into children
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				LogWidgetTreeRecursive(Panel->GetChildAt(i), Depth + 1);
			}
		}
	}

	void LogWidgetTree(UWidget* RootWidget, const FString& Context)
	{
		UE_LOG(LogProjectUserWidget, Log, TEXT("=== WIDGET TREE [%s] ==="), *Context);
		LogWidgetTreeRecursive(RootWidget, 0);
		UE_LOG(LogProjectUserWidget, Log, TEXT("=== END WIDGET TREE ==="));
	}

	void LogSlotConfiguration(UWidget* Widget, const FString& Context)
	{
		if (!Widget)
		{
			return;
		}

		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] %s slot configuration:"), *Context, *Widget->GetName());

		if (!Widget->Slot)
		{
			UE_LOG(LogProjectUserWidget, Log, TEXT("  -> No slot (not added to parent yet or is root)"));
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FAnchors Anchors = CanvasSlot->GetAnchors();
			FVector2D SlotSize = CanvasSlot->GetSize();
			FVector2D Offset = CanvasSlot->GetPosition();
			FVector2D Alignment = CanvasSlot->GetAlignment();
			bool bAutoSize = CanvasSlot->GetAutoSize();
			int32 ZOrder = CanvasSlot->GetZOrder();

			UE_LOG(LogProjectUserWidget, Log, TEXT("  -> CanvasPanelSlot:"));
			UE_LOG(LogProjectUserWidget, Log, TEXT("     AutoSize=%s"), bAutoSize ? TEXT("TRUE") : TEXT("FALSE"));
			UE_LOG(LogProjectUserWidget, Log, TEXT("     SlotSize=(%.0f, %.0f)"), SlotSize.X, SlotSize.Y);
			UE_LOG(LogProjectUserWidget, Log, TEXT("     Offset=(%.0f, %.0f)"), Offset.X, Offset.Y);
			UE_LOG(LogProjectUserWidget, Log, TEXT("     Alignment=(%.2f, %.2f)"), Alignment.X, Alignment.Y);
			UE_LOG(LogProjectUserWidget, Log, TEXT("     Anchors Min=(%.2f, %.2f) Max=(%.2f, %.2f)"),
				Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);
			UE_LOG(LogProjectUserWidget, Log, TEXT("     ZOrder=%d"), ZOrder);

			// Warn about potential issues
			if (!bAutoSize && SlotSize.IsNearlyZero())
			{
				UE_LOG(LogProjectUserWidget, Warning, TEXT("     [ISSUE] AutoSize=false with 0x0 size - widget will be INVISIBLE!"));
			}
		}
		else
		{
			UE_LOG(LogProjectUserWidget, Log, TEXT("  -> Slot type: %s (not CanvasPanelSlot)"), *Widget->Slot->GetClass()->GetName());
		}
	}
}

UProjectUserWidget::UProjectUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// Focus Handling (defensive, with logging for TextInputFramework debugging)
// Logs all focus events to help diagnose Windows TSF crashes in TextInputFramework.dll
// =============================================================================

FReply UProjectUserWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// Log focus event details for debugging TextInputFramework.dll crashes
	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] FOCUS_RECEIVED: Cause=%d, User=%d"),
		*GetClass()->GetName(),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	// Defensive: wrap in try-catch equivalent using UE crash handling
	// The crash occurs in Windows TSF during focus transitions
	FReply Reply = FReply::Unhandled();

	// Call parent - this is where TSF may crash if IME is active
	Reply = Super::NativeOnFocusReceived(InGeometry, InFocusEvent);

	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] FOCUS_RECEIVED: Complete, Reply.IsHandled=%s"),
		*GetClass()->GetName(),
		Reply.IsEventHandled() ? TEXT("true") : TEXT("false"));

	return Reply;
}

void UProjectUserWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] FOCUS_LOST: Cause=%d, User=%d"),
		*GetClass()->GetName(),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	// Call parent - TSF may crash here during focus transitions
	Super::NativeOnFocusLost(InFocusEvent);

	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] FOCUS_LOST: Complete"),
		*GetClass()->GetName());
}

void UProjectUserWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] ADDED_TO_FOCUS_PATH: Cause=%d, User=%d"),
		*GetClass()->GetName(),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	Super::NativeOnAddedToFocusPath(InFocusEvent);
}

void UProjectUserWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] REMOVED_FROM_FOCUS_PATH: Cause=%d, User=%d"),
		*GetClass()->GetName(),
		static_cast<int32>(InFocusEvent.GetCause()),
		InFocusEvent.GetUser());

	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
}

// =============================================================================
// Lifecycle
// =============================================================================

TSharedRef<SWidget> UProjectUserWidget::RebuildWidget()
{
	// CRITICAL: Build data-driven layout BEFORE Slate takes the root widget.
	// For pure C++ UserWidgets, if WidgetTree->RootWidget is null when RebuildWidget()
	// runs, UE returns SSpacer (empty) and the widget appears invisible.
	EnsureLayoutInitialized();

	// Now call Super which will use our WidgetTree->RootWidget
	TSharedRef<SWidget> Result = Super::RebuildWidget();

	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] RebuildWidget: Slate widget type = %s"),
		*GetClass()->GetName(), *Result->GetType().ToString());

	return Result;
}

void UProjectUserWidget::EnsureLayoutInitialized()
{
	if (bLayoutInitialized)
	{
		return;
	}

	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] EnsureLayoutInitialized"), *GetClass()->GetName());

	// Ensure WidgetTree exists
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, UWidgetTree::StaticClass(), TEXT("WidgetTree"));
		WidgetTree->SetFlags(RF_Transactional);
	}

	// Load JSON layout if ConfigFilePath is set
	if (!ConfigFilePath.IsEmpty())
	{
		LoadLayoutFromConfig();
	}

	// Fallback root if loader didn't set anything
	if (!WidgetTree->RootWidget)
	{
		UE_LOG(LogProjectUserWidget, Warning, TEXT("[%s] EnsureLayoutInitialized: No RootWidget after load, creating fallback canvas"),
			*GetClass()->GetName());

		UCanvasPanel* FallbackCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FallbackCanvas"));
		WidgetTree->RootWidget = FallbackCanvas;
		RootWidget = FallbackCanvas;
	}

	bLayoutInitialized = true;
}

void UProjectUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsConstructed = true;

	UE_LOG(LogProjectUserWidget, Log, TEXT("=== [%s] NATIVE CONSTRUCT START ==="), *GetClass()->GetName());

	// Debug subsystem logging
	PROJECT_UI_LIFECYCLE(this, TEXT("NativeConstruct"));

	// Log THIS widget's slot configuration (how parent added us)
	ProjectUserWidgetDebug::LogSlotConfiguration(this, FString::Printf(TEXT("%s THIS WIDGET"), *GetClass()->GetName()));

	// Log parent info
	UWidget* ParentWidget = GetParent();
	if (ParentWidget)
	{
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Parent: %s (%s)"),
			*GetClass()->GetName(), *ParentWidget->GetName(), *ParentWidget->GetClass()->GetName());
		FVector2D ParentSize = ParentWidget->GetCachedGeometry().GetLocalSize();
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Parent CachedSize: (%.0f, %.0f) %s"),
			*GetClass()->GetName(), ParentSize.X, ParentSize.Y,
			ParentSize.IsNearlyZero() ? TEXT("[ZERO - CONTAINER MAY BE INVISIBLE!]") : TEXT(""));
	}
	else
	{
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Parent: NULL (is viewport root)"), *GetClass()->GetName());
	}

	// Log own geometry
	FVector2D OwnDesired = GetDesiredSize();
	FVector2D OwnCached = GetCachedGeometry().GetLocalSize();
	UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Own DesiredSize: (%.0f, %.0f)"), *GetClass()->GetName(), OwnDesired.X, OwnDesired.Y);
	UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Own CachedSize: (%.0f, %.0f) %s"),
		*GetClass()->GetName(), OwnCached.X, OwnCached.Y,
		OwnCached.IsNearlyZero() ? TEXT("[ZERO - expected at construction, will resolve after layout pass]") : TEXT(""));

	// Log RootWidget info
	if (RootWidget)
	{
		FVector2D RootDesired = RootWidget->GetDesiredSize();
		FVector2D RootCached = RootWidget->GetCachedGeometry().GetLocalSize();
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] RootWidget: %s (%s) Desired=(%.0f,%.0f) Cached=(%.0f,%.0f)"),
			*GetClass()->GetName(),
			*RootWidget->GetName(), *RootWidget->GetClass()->GetName(),
			RootDesired.X, RootDesired.Y,
			RootCached.X, RootCached.Y);
	}
	else
	{
		UE_LOG(LogProjectUserWidget, Warning, TEXT("[%s] RootWidget: NULL (layout not loaded?)"), *GetClass()->GetName());
	}

	// Layout is already built by RebuildWidget() / EnsureLayoutInitialized().
	// Here we just do bindings and register for events.

	// Register for theme changes
	UProjectUIThemeManager* ThemeManager = GetThemeManager();
	if (ThemeManager)
	{
		ThemeManager->OnThemeChanged.AddDynamic(this, &UProjectUserWidget::HandleThemeChanged);
		OnThemeChanged(ThemeManager->GetActiveTheme());
	}

#if WITH_EDITOR
	// Start config watcher for hot reload
	if (!ConfigFilePath.IsEmpty())
	{
		InitializeConfigWatcher();
	}
#endif

	// Refresh from ViewModel if already set
	if (ViewModel)
	{
		RefreshFromViewModel();
	}

	UE_LOG(LogProjectUserWidget, Log, TEXT("=== [%s] NATIVE CONSTRUCT END ==="), *GetClass()->GetName());
}

void UProjectUserWidget::NativeDestruct()
{
	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] NativeDestruct"), *GetClass()->GetName());

	// Debug subsystem logging
	PROJECT_UI_LIFECYCLE(this, TEXT("NativeDestruct"));

	bIsConstructed = false;

#if WITH_EDITOR
	ShutdownConfigWatcher();
#endif

	// Unregister from theme changes
	UProjectUIThemeManager* ThemeManager = GetThemeManager();
	if (ThemeManager)
	{
		ThemeManager->OnThemeChanged.RemoveDynamic(this, &UProjectUserWidget::HandleThemeChanged);
	}

	// Unbind from ViewModel
	SetViewModel(nullptr);

	Super::NativeDestruct();
}

// =============================================================================
// JSON Layout Loading (automatic for all derived classes)
// =============================================================================

void UProjectUserWidget::LoadLayoutFromConfig()
{
	UE_LOG(LogProjectUserWidget, Log, TEXT("=== [%s] LAYOUT CONSTRUCTION START ==="), *GetClass()->GetName());
	UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] ConfigFilePath: %s"), *GetClass()->GetName(), *ConfigFilePath);

	if (ConfigFilePath.IsEmpty())
	{
		UE_LOG(LogProjectUserWidget, Warning, TEXT("[%s] LoadLayoutFromConfig: ConfigFilePath is empty"), *GetClass()->GetName());
		return;
	}

	// Load layout from JSON using theme if available
	UProjectUIThemeData* ActiveTheme = GetActiveTheme();
	if (!ActiveTheme)
	{
		UE_LOG(LogProjectUserWidget, Warning, TEXT("[%s] No active theme - widgets will have default styling"), *GetClass()->GetName());
	}
	else
	{
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Using theme: %s"), *GetClass()->GetName(), *ActiveTheme->ThemeName.ToString());
	}

	UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Calling ProjectWidgetLayoutLoader::LoadLayoutFromFile..."), *GetClass()->GetName());
	UWidget* LoadedRoot = UProjectWidgetLayoutLoader::LoadLayoutFromFile(this, ConfigFilePath, ActiveTheme);

	if (!LoadedRoot)
	{
		UE_LOG(LogProjectUserWidget, Error, TEXT("[%s] LAYOUT FAILED - LoadLayoutFromFile returned nullptr"), *GetClass()->GetName());
		return;
	}

	UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] LoadedRoot created: %s (%s)"),
		*GetClass()->GetName(), *LoadedRoot->GetName(), *LoadedRoot->GetClass()->GetName());

	// Wrap in Overlay to ensure root fills the UserWidget (CanvasPanel alone doesn't fill)
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	UOverlaySlot* ChildSlot = RootOverlay->AddChildToOverlay(LoadedRoot);

	// CRITICAL: Set Fill alignment so LoadedRoot expands to fill the Overlay
	// SynchronizeProperties() ensures Slate slot syncs with UPROPERTY values
	// (fixes UE5 race condition where BuildSlot can run before Set*Alignment)
	if (ChildSlot)
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Fill);
		ChildSlot->SynchronizeProperties();
	}

	RootWidget = RootOverlay;

	// Add root widget to the UserWidget's visual tree
	if (WidgetTree)
	{
		WidgetTree->RootWidget = RootWidget;
		UE_LOG(LogProjectUserWidget, Log, TEXT("[%s] Set WidgetTree->RootWidget to: %s"), *GetClass()->GetName(), *RootWidget->GetName());
	}
	else
	{
		UE_LOG(LogProjectUserWidget, Warning, TEXT("[%s] WidgetTree is null, cannot set root widget"), *GetClass()->GetName());
	}

	// Log the entire widget tree created from JSON
	ProjectUserWidgetDebug::LogWidgetTree(RootWidget, GetClass()->GetName());

	UE_LOG(LogProjectUserWidget, Log, TEXT("=== [%s] LAYOUT CONSTRUCTION END ==="), *GetClass()->GetName());

	// Bind callbacks to buttons
	BindCallbacks();
}

void UProjectUserWidget::ReloadLayout()
{
	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] ReloadLayout from: %s"), *GetClass()->GetName(), *ConfigFilePath);

	// Clear existing layout
	if (WidgetTree)
	{
		WidgetTree->RootWidget = nullptr;
	}
	RootWidget = nullptr;

	// Force layout re-init on next RebuildWidget
	bLayoutInitialized = false;

	// Invalidate layout - UE will call RebuildWidget when needed
	InvalidateLayoutAndVolatility();
}

void UProjectUserWidget::SetConfigFilePath(const FString& InConfigFilePath)
{
	if (ConfigFilePath == InConfigFilePath)
	{
		return;
	}

	ConfigFilePath = InConfigFilePath;

	// If already constructed, reload immediately to apply the new layout.
	if (bIsConstructed)
	{
		ReloadLayout();
	}
}

void UProjectUserWidget::BindCallbacks()
{
	// Base implementation does nothing
	// Derived classes override to bind button handlers
}

// =============================================================================
// Config Watcher (editor only)
// =============================================================================

#if WITH_EDITOR
void UProjectUserWidget::InitializeConfigWatcher()
{
	if (ConfigFilePath.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UProjectUserWidget> WeakThis(this);
	ConfigWatcher.Start(this, ConfigFilePath, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HandleConfigFileChanged();
		}
	});

	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] Config watcher started for: %s"), *GetClass()->GetName(), *ConfigFilePath);
}

void UProjectUserWidget::ShutdownConfigWatcher()
{
	ConfigWatcher.Stop();
}

void UProjectUserWidget::HandleConfigFileChanged()
{
	UE_LOG(LogProjectUserWidget, Display, TEXT("[%s] Detected change to %s; reloading layout"), *GetClass()->GetName(), *ConfigFilePath);
	ReloadLayout();
}
#endif

// =============================================================================
// ViewModel Binding
// =============================================================================

void UProjectUserWidget::SetViewModel(UProjectViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		return;
	}

	UProjectViewModel* OldViewModel = ViewModel;

	// Unbind from old ViewModel
	if (OldViewModel)
	{
		OldViewModel->OnPropertyChanged.RemoveDynamic(this, &UProjectUserWidget::HandleViewModelPropertyChanged);
		UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] Unbound from ViewModel %s"), *GetClass()->GetName(), *OldViewModel->GetName());
	}

	// Set new ViewModel
	ViewModel = InViewModel;

	// Bind to new ViewModel
	if (ViewModel)
	{
		ViewModel->OnPropertyChanged.AddDynamic(this, &UProjectUserWidget::HandleViewModelPropertyChanged);
		UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] Bound to ViewModel %s"), *GetClass()->GetName(), *ViewModel->GetName());

		// Debug subsystem logging
		if (UProjectUIDebugSubsystem* DebugSub = UProjectUIDebugSubsystem::Get(this))
		{
			DebugSub->LogViewModelBinding(this, ViewModel, TEXT("Bound"));
		}
	}

	// Notify derived classes
	OnViewModelChanged(OldViewModel, ViewModel);

	// Refresh UI
	if (bIsConstructed)
	{
		RefreshFromViewModel();
	}
}

void UProjectUserWidget::RefreshFromViewModel_Implementation()
{
	// Base implementation does nothing
	// Derived classes override to update their UI elements
}

void UProjectUserWidget::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
	// Base implementation does nothing
	// Derived classes override to bind to ViewModel properties
}

// =============================================================================
// Theme Support
// =============================================================================

void UProjectUserWidget::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
	// Base implementation does nothing
	// Derived classes override to apply theme styling
}

UProjectUIThemeManager* UProjectUserWidget::GetThemeManager() const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UProjectUIThemeManager>();
}

UProjectUIThemeData* UProjectUserWidget::GetActiveTheme() const
{
	UProjectUIThemeManager* ThemeManager = GetThemeManager();
	return ThemeManager ? ThemeManager->GetActiveTheme() : nullptr;
}

// =============================================================================
// Internal Handlers
// =============================================================================

void UProjectUserWidget::HandleViewModelPropertyChanged(FName PropertyName)
{
	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] Property changed: %s"), *GetClass()->GetName(), *PropertyName.ToString());
	RefreshFromViewModel();
}

void UProjectUserWidget::HandleThemeChanged(UProjectUIThemeData* NewTheme)
{
	UE_LOG(LogProjectUserWidget, Verbose, TEXT("[%s] Theme changed: %s"),
		*GetClass()->GetName(), NewTheme ? *NewTheme->ThemeName.ToString() : TEXT("null"));
	OnThemeChanged(NewTheme);
}

// =============================================================================
// Debug Support
// =============================================================================

FProjectWidgetDiagnostics UProjectUserWidget::GetDiagnostics() const
{
	UProjectUIDebugSubsystem* DebugSub = UProjectUIDebugSubsystem::Get(this);
	if (DebugSub)
	{
		return DebugSub->DiagnoseProjectWidget(const_cast<UProjectUserWidget*>(this));
	}

	// Fallback: minimal diagnostics without subsystem
	FProjectWidgetDiagnostics Diag;
	Diag.WidgetName = GetName();
	Diag.ClassName = GetClass()->GetName();
	Diag.bIsVisible = IsVisible();
	Diag.bIsInViewport = IsInViewport();
	Diag.DesiredSize = GetDesiredSize();
	Diag.CachedGeometrySize = GetCachedGeometry().GetLocalSize();
	Diag.CaptureTime = FDateTime::Now();

	if (ViewModel)
	{
		Diag.ViewModelClass = ViewModel->GetClass()->GetName();
	}
	else
	{
		Diag.Issues.Add(TEXT("No ViewModel bound"));
	}

	if (Diag.CachedGeometrySize.IsNearlyZero() && Diag.bIsVisible)
	{
		Diag.Issues.Add(TEXT("Widget has 0x0 size but is marked visible - likely invisible on screen"));
		Diag.Recommendations.Add(TEXT("Check slot AutoSize setting or parent container sizing"));
	}

	return Diag;
}

void UProjectUserWidget::LogDiagnostics() const
{
	FProjectWidgetDiagnostics Diag = GetDiagnostics();
	UE_LOG(LogProjectUserWidget, Display, TEXT("%s"), *Diag.ToString());
}

// =============================================================================
// Game Entity Access Protection
// =============================================================================
// Architecture rule: Widgets access game data ONLY through ViewModel.
// - GetOwningPlayer(): Overridden to pass through. UMG infrastructure needs this.
// - GetOwningPlayerPawn(): NOT virtual in UUserWidget, cannot override.
//   Convention enforced via code review, not compile-time.

APlayerController* UProjectUserWidget::GetOwningPlayer() const
{
	// UMG infrastructure needs this (CreateWidget, focus routing, etc.)
	// No logging - called frequently by engine, would spam
	return Super::GetOwningPlayer();
}
