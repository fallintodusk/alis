// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Layout/ProjectWidgetLayoutWatcher.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "ProjectUserWidget.generated.h"

class UProjectUIThemeData;

DECLARE_LOG_CATEGORY_EXTERN(LogProjectUserWidget, Log, All);

/**
 * Base class for all Project UI widgets
 *
 * Provides common functionality:
 * - JSON layout loading with hot reload (no duplication in derived classes)
 * - ViewModel binding and data updates
 * - Theme integration
 * - Lifecycle management
 *
 * JSON Layout (automatic):
 * - Set ConfigFilePath in derived constructor
 * - Override BindCallbacks() to wire button handlers
 * - Hot reload via ReloadLayout() or automatic file watching in editor
 *
 * MVVM Architecture:
 * - Widgets are "dumb views" that display ViewModel data
 * - Override OnViewModelChanged() to bind to ViewModel properties
 * - Override RefreshFromViewModel() to update UI elements
 *
 * Usage:
 * 1. Derive from UProjectUserWidget in C++
 * 2. Set ConfigFilePath in constructor (use GetPluginUIConfigPath)
 * 3. Override BindCallbacks() to bind button click handlers
 * 4. Create UMG widget blueprint that extends your C++ class
 */
UCLASS(Abstract, Blueprintable)
class PROJECTUI_API UProjectUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UProjectUserWidget(const FObjectInitializer& ObjectInitializer);

	// ==========================================================================
	// UUserWidget Lifecycle
	// ==========================================================================

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ==========================================================================
	// Focus Handling (defensive, with logging for TextInputFramework debugging)
	// ==========================================================================

	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	// ==========================================================================
	// JSON Layout Loading (automatic - no code duplication needed)
	// ==========================================================================

	/**
	 * Reload the layout from JSON config file.
	 * Called automatically by file watcher in editor, or manually.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Layout")
	virtual void ReloadLayout();

	/**
	 * Override the JSON config file path before layout initialization.
	 * Use this when definitions are loaded from ui_definitions.json.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Layout")
	void SetConfigFilePath(const FString& InConfigFilePath);

protected:
	/**
	 * Ensure layout is initialized. Called from RebuildWidget() before Slate takes the root.
	 * This is the correct place to build data-driven UI for pure C++ UserWidgets.
	 */
	void EnsureLayoutInitialized();

	/**
	 * Load the widget layout from ConfigFilePath.
	 * Called by EnsureLayoutInitialized().
	 */
	virtual void LoadLayoutFromConfig();

	/**
	 * Bind callbacks to buttons created from JSON.
	 * Override in derived classes to wire up button handlers.
	 * Called after LoadLayoutFromConfig() and ReloadLayout().
	 */
	virtual void BindCallbacks();

	// ==========================================================================
	// ViewModel Binding
	// ==========================================================================

public:
	/**
	 * Set the ViewModel for this widget.
	 * Automatically calls OnViewModelChanged() and refreshes UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|ViewModel")
	void SetViewModel(UProjectViewModel* InViewModel);

	/**
	 * Get the current ViewModel.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|ViewModel")
	UProjectViewModel* GetViewModel() const { return ViewModel; }

	/**
	 * Refresh the widget's presentation from ViewModel data.
	 * Called automatically when ViewModel changes or properties update.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|ViewModel")
	void RefreshFromViewModel();
	virtual void RefreshFromViewModel_Implementation();

protected:
	/**
	 * Called when ViewModel is set or changed.
	 * Override to bind to ViewModel properties and events.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|ViewModel")
	void OnViewModelChanged(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel);
	virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel);

	// ==========================================================================
	// Theme Support
	// ==========================================================================

	/**
	 * Called when the active theme changes.
	 * Override to apply theme colors, fonts, spacing to UI elements.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Project UI|Theme")
	void OnThemeChanged(UProjectUIThemeData* NewTheme);
	virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme);

	/**
	 * Get the theme manager subsystem.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	class UProjectUIThemeManager* GetThemeManager() const;

	/**
	 * Get the active theme.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Theme")
	class UProjectUIThemeData* GetActiveTheme() const;

	// ==========================================================================
	// Debug Support
	// ==========================================================================

public:
	/**
	 * Get full diagnostics for this widget.
	 * Includes geometry, visibility, ViewModel state, and issue detection.
	 *
	 * Usage:
	 * @code
	 * FProjectWidgetDiagnostics Diag = MyWidget->GetDiagnostics();
	 * UE_LOG(LogTemp, Display, TEXT("%s"), *Diag.ToString());
	 * @endcode
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Debug")
	FProjectWidgetDiagnostics GetDiagnostics() const;

	/**
	 * Log diagnostics to output log (convenience method).
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Debug")
	void LogDiagnostics() const;

	// ==========================================================================
	// Game Entity Access Protection
	// ==========================================================================
	// Widgets access game data ONLY through ViewModel, never directly.
	// - GetOwningPlayer(): Override allowed (virtual in base). Use only for UMG infrastructure.
	// - GetOwningPlayerPawn(): NOT virtual in UUserWidget, cannot be overridden.
	//   Convention: Do not call from widget code. Use ViewModel for pawn data.
	//   This is a code review item, not compile-time enforcement.

	/**
	 * Get owning player controller.
	 * NOTE: Only for UMG infrastructure (CreateWidget, focus routing).
	 * Do NOT use for game logic - use ViewModel instead.
	 */
	virtual APlayerController* GetOwningPlayer() const override;

	// ==========================================================================
	// Protected Members (for derived classes)
	// ==========================================================================

	/** Path to JSON config file. Set in derived constructor. */
	FString ConfigFilePath;

	/** Root widget created from JSON layout. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> RootWidget;

private:
	// ==========================================================================
	// Internal Implementation
	// ==========================================================================

	UPROPERTY()
	TObjectPtr<UProjectViewModel> ViewModel;

	UFUNCTION()
	void HandleViewModelPropertyChanged(FName PropertyName);

	UFUNCTION()
	void HandleThemeChanged(UProjectUIThemeData* NewTheme);

	bool bIsConstructed = false;
	bool bLayoutInitialized = false;

#if WITH_EDITOR
	FProjectWidgetLayoutWatcher ConfigWatcher;

	void InitializeConfigWatcher();
	void ShutdownConfigWatcher();
	void HandleConfigFileChanged();
#endif
};
