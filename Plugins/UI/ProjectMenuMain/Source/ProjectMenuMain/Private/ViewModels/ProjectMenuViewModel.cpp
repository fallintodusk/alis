// Copyright ALIS. All Rights Reserved.

#include "ViewModels/ProjectMenuViewModel.h"
#include "ProjectMenuMainLog.h"
#include "Kismet/KismetSystemLibrary.h"

void UProjectMenuViewModel::Initialize(UObject* Context)
{
    Super::Initialize(Context);

    UE_LOG(LogProjectMenuMain, Display, TEXT("MenuViewModel initialized"));

    // Initialize properties
    CurrentScreen = EMenuScreen::Main;
    bIsMenuVisible = true;
    bShowQuitConfirmation = false;
    bShowBackButton = false;

    UpdateScreenTitle();
    UpdateBackButtonVisibility();
}

void UProjectMenuViewModel::Shutdown()
{
    UE_LOG(LogProjectMenuMain, Display, TEXT("MenuViewModel shutdown"));

    NavigationStack.Empty();

    Super::Shutdown();
}

void UProjectMenuViewModel::NavigateToMain()
{
    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Navigate to Main Menu"));
    NavigationStack.Reset();
    NavigateToScreen(EMenuScreen::Main, false); // Don't push Main to stack
}

void UProjectMenuViewModel::NavigateToMapBrowser()
{
    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Navigate to Map Browser"));
    NavigateToScreen(EMenuScreen::MapBrowser, true);
}

void UProjectMenuViewModel::NavigateToSettings()
{
    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Navigate to Settings"));
    NavigateToScreen(EMenuScreen::Settings, true);
}

void UProjectMenuViewModel::NavigateToCredits()
{
    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Navigate to Credits"));
    NavigateToScreen(EMenuScreen::Credits, true);
}

bool UProjectMenuViewModel::NavigateBack()
{
    if (NavigationStack.Num() == 0)
    {
        UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: NavigateBack called but navigation stack is empty"));
        return false;
    }

    // Pop previous screen from stack
    EMenuScreen PreviousScreen = NavigationStack.Pop();

    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Navigate back to %s"), *GetScreenDisplayName(PreviousScreen).ToString());

    // Navigate without pushing to stack
    NavigateToScreen(PreviousScreen, false);

    return true;
}

void UProjectMenuViewModel::RequestQuit()
{
    UE_LOG(LogProjectMenuMain, Display, TEXT("MenuViewModel: Quit requested"));

    // Show quit confirmation dialog
    bShowQuitConfirmation = true;
    NotifyPropertyChanged(FName(TEXT("bShowQuitConfirmation")));
}

void UProjectMenuViewModel::ConfirmQuit()
{
    UE_LOG(LogProjectMenuMain, Display, TEXT("MenuViewModel: Quit confirmed - exiting application"));

    bShowQuitConfirmation = false;
    NotifyPropertyChanged(FName(TEXT("bShowQuitConfirmation")));

    // Quit application
    if (UWorld* World = GetWorld())
    {
        UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false);
    }
}

void UProjectMenuViewModel::CancelQuit()
{
    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Quit cancelled"));

    bShowQuitConfirmation = false;
    NotifyPropertyChanged(FName(TEXT("bShowQuitConfirmation")));
}

void UProjectMenuViewModel::SetMenuVisible(bool bVisible)
{
    if (bIsMenuVisible == bVisible)
    {
        return; // No change
    }

    UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Menu visibility changed: %d"), bVisible);

    bIsMenuVisible = bVisible;
    NotifyPropertyChanged(FName(TEXT("bIsMenuVisible")));
}

void UProjectMenuViewModel::NavigateToScreen(EMenuScreen NewScreen, bool bPushToStack)
{
    if (CurrentScreen == NewScreen)
    {
        UE_LOG(LogProjectMenuMain, Verbose, TEXT("MenuViewModel: Already on screen %s"), *GetScreenDisplayName(NewScreen).ToString());
        return;
    }

    // Push current screen to stack if requested
    if (bPushToStack)
    {
        NavigationStack.Add(CurrentScreen);
        UE_LOG(LogProjectMenuMain, VeryVerbose, TEXT("MenuViewModel: Pushed %s to navigation stack (depth: %d)"),
            *GetScreenDisplayName(CurrentScreen).ToString(), NavigationStack.Num());
    }

    // Update current screen
    CurrentScreen = NewScreen;
    NotifyPropertyChanged(FName(TEXT("CurrentScreen")));

    // Update screen title
    UpdateScreenTitle();

    // Update back button visibility
    UpdateBackButtonVisibility();

    UE_LOG(LogProjectMenuMain, Display, TEXT("MenuViewModel: Navigated to %s (Stack depth: %d)"),
        *GetScreenDisplayName(NewScreen).ToString(), NavigationStack.Num());
}

void UProjectMenuViewModel::UpdateScreenTitle()
{
    ScreenTitle = GetScreenDisplayName(CurrentScreen);
    NotifyPropertyChanged(FName(TEXT("ScreenTitle")));
}

void UProjectMenuViewModel::UpdateBackButtonVisibility()
{
    bool bShouldShowBackButton = CanNavigateBack();

    if (bShowBackButton != bShouldShowBackButton)
    {
        bShowBackButton = bShouldShowBackButton;
        NotifyPropertyChanged(FName(TEXT("bShowBackButton")));
    }
}

FText UProjectMenuViewModel::GetScreenDisplayName(EMenuScreen Screen)
{
    switch (Screen)
    {
    case EMenuScreen::Main:
        return FText::FromString(TEXT("Main Menu"));
    case EMenuScreen::MapBrowser:
        return FText::FromString(TEXT("Select Map"));
    case EMenuScreen::Settings:
        return FText::FromString(TEXT("Settings"));
    case EMenuScreen::Credits:
        return FText::FromString(TEXT("Credits"));
    default:
        return FText::FromString(TEXT("Unknown"));
    }
}


