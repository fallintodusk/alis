// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/W_GameMenu.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IGameMenuService.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectServiceLocator.h"
#include "ProjectWidgetHelpers.h"
#include "Services/ILoadingService.h"
#include "Types/ProjectLoadRequest.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectGameMenu, Log, All);

UW_GameMenu::UW_GameMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(
		TEXT("ProjectMenuGame"), TEXT("GameMenu.json"));
}

void UW_GameMenu::BindCallbacks()
{
	if (UButton* Button = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Resume")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UW_GameMenu::ResumeGame);
	}
	if (UButton* Button = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_MainMenu")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UW_GameMenu::ReturnToMainMenu);
	}
	if (UButton* Button = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Quit")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UW_GameMenu::QuitGame);
	}
}

FReply UW_GameMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		ResumeGame();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UW_GameMenu::ResumeGame()
{
	APlayerController* PlayerController = GetOwningPlayer();
	const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>();
	if (PlayerController != nullptr && MenuService.IsValid())
	{
		MenuService->SetVisible(*PlayerController, false);
	}
}

void UW_GameMenu::ReturnToMainMenu()
{
	APlayerController* PlayerController = GetOwningPlayer();
	const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>();
	const TSharedPtr<ILoadingService> LoadingService = FProjectServiceLocator::Resolve<ILoadingService>();
	if (PlayerController == nullptr || !MenuService.IsValid() || !LoadingService.IsValid())
	{
		return;
	}

	FLoadRequest Request;
	FText Error;
	if (!LoadingService->BuildLoadRequestForExperience(TEXT("MainMenuWorld"), Request, Error))
	{
		UE_LOG(LogProjectGameMenu, Error, TEXT("Cannot resolve MainMenuWorld: %s"), *Error.ToString());
		return;
	}

	MenuService->SetVisible(*PlayerController, false);
	LoadingService->StartLoad(Request);
}

void UW_GameMenu::QuitGame()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController != nullptr)
	{
		if (const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>())
		{
			MenuService->SetVisible(*PlayerController, false);
		}
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
	}
}
