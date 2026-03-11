// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ProjectViewModel.h"
#include "ProjectMenuViewModel.generated.h"

/**
 * Menu screen states
 */
UENUM(BlueprintType)
enum class EMenuScreen : uint8
{
    /** Main menu (Play, Settings, Quit) */
    Main UMETA(DisplayName = "Main Menu"),

    /** Map browser/selection */
    MapBrowser UMETA(DisplayName = "Map Browser"),

    /** Settings menu */
    Settings UMETA(DisplayName = "Settings"),

    /** Credits screen */
    Credits UMETA(DisplayName = "Credits")
};

/**
 * Menu ViewModel
 *
 * Manages main menu state and navigation logic.
 * Pure C++ ViewModel with zero UI dependencies - can be unit tested.
 *
 * Responsibilities:
 * - Track current menu screen
 * - Handle navigation between screens
 * - Manage menu state (enabled/disabled, visibility)
 * - Provide commands for menu actions (Play, Settings, Quit)
 */
UCLASS(BlueprintType)
class PROJECTMENUMAIN_API UProjectMenuViewModel : public UProjectViewModel
{
    GENERATED_BODY()

public:
    /** UProjectViewModel lifecycle */
    virtual void Initialize(UObject* Context) override;
    virtual void Shutdown() override;

    /** Navigate to main menu screen */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void NavigateToMain();

    /** Navigate to map browser screen */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void NavigateToMapBrowser();

    /** Navigate to settings screen */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void NavigateToSettings();

    /** Navigate to credits screen */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void NavigateToCredits();

    /** Navigate back to previous screen */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    bool NavigateBack();

    /** Request application quit (shows confirmation) */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void RequestQuit();

    /** Confirm quit */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void ConfirmQuit();

    /** Cancel quit */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void CancelQuit();

    /** Show/hide menu (for pause menu behavior) */
    UFUNCTION(BlueprintCallable, Category = "Project UI|Menu")
    void SetMenuVisible(bool bVisible);

    /** Check if currently on main menu screen */
    UFUNCTION(BlueprintPure, Category = "Project UI|Menu")
    bool IsOnMainScreen() const { return CurrentScreen == EMenuScreen::Main; }

    /** Check if back button should be visible */
    UFUNCTION(BlueprintPure, Category = "Project UI|Menu")
    bool CanNavigateBack() const { return NavigationStack.Num() > 0; }

    // ViewModel properties
    /** Current menu screen */
    VIEWMODEL_PROPERTY_READONLY(EMenuScreen, CurrentScreen)

    /** Whether menu is currently visible */
    VIEWMODEL_PROPERTY_READONLY(bool, bIsMenuVisible)

    /** Whether quit confirmation dialog is shown */
    VIEWMODEL_PROPERTY_READONLY(bool, bShowQuitConfirmation)

    /** Whether back button should be visible */
    VIEWMODEL_PROPERTY_READONLY(bool, bShowBackButton)

    /** Current screen title text */
    VIEWMODEL_PROPERTY_READONLY(FText, ScreenTitle)

protected:
    /** Navigate to screen (internal) */
    void NavigateToScreen(EMenuScreen NewScreen, bool bPushToStack);

    /** Update screen title based on current screen */
    void UpdateScreenTitle();

    /** Update back button visibility */
    void UpdateBackButtonVisibility();

    /** Get display name for menu screen */
    static FText GetScreenDisplayName(EMenuScreen Screen);

private:
    /** Navigation stack (for back button) */
    UPROPERTY()
    TArray<EMenuScreen> NavigationStack;
};

