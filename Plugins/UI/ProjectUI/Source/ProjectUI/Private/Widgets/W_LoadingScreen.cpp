// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_LoadingScreen.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Components/Widget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"

DEFINE_LOG_CATEGORY_STATIC(LogW_LoadingScreen, Log, All);

UW_LoadingScreen::UW_LoadingScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set JSON config path - base class handles loading and hot reload automatically
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectUI"), TEXT("LoadingScreen.json"));
}

void UW_LoadingScreen::NativeConstruct()
{
	UE_LOG(LogW_LoadingScreen, Display, TEXT("[W_LoadingScreen] NativeConstruct - ConfigFilePath: %s"), *ConfigFilePath);

	// Initialize default values BEFORE calling Super (which loads JSON)
	CurrentProgress = 0.0f;
	CurrentPhaseText = FText::FromString(TEXT("Initializing..."));
	CurrentStatusMessage = FText::FromString(TEXT("Please wait"));
	bErrorVisible = false;

	// Base class handles: JSON loading, BindCallbacks(), ConfigWatcher setup
	Super::NativeConstruct();

	// Diagnostic: Check if RootWidget was created
	if (RootWidget)
	{
		UE_LOG(LogW_LoadingScreen, Display, TEXT("[W_LoadingScreen] RootWidget OK: %s (class: %s)"),
			*RootWidget->GetName(), *RootWidget->GetClass()->GetName());
		UE_LOG(LogW_LoadingScreen, Display, TEXT("[W_LoadingScreen] RootWidget Visibility: %s"),
			*UEnum::GetValueAsString(RootWidget->GetVisibility()));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Error, TEXT("[W_LoadingScreen] RootWidget is NULL after NativeConstruct!"));
	}

	// Diagnostic: Check key widgets
	LogWidgetDiagnostics();

	// After layout is loaded, update widgets with initial values
	UpdateProgressBar();
	UpdatePhaseText();
	UpdateStatusMessage();
}

void UW_LoadingScreen::NativeDestruct()
{
	Super::NativeDestruct();
}

void UW_LoadingScreen::BindCallbacks()
{
	if (!RootWidget)
	{
		return;
	}

	// Find and bind cancel button
	if (UButton* CancelButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Cancel"), true, true))
	{
		CancelButton->OnClicked.AddDynamic(this, &UW_LoadingScreen::OnCancelClicked);
		UE_LOG(LogW_LoadingScreen, Display, TEXT("Bound OnCancelClicked to Button_Cancel"));
	}
}

// Public setters
void UW_LoadingScreen::SetProgress(float InProgress)
{
	CurrentProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	UpdateProgressBar();
}

void UW_LoadingScreen::SetPhaseText(const FText& InPhaseText)
{
	CurrentPhaseText = InPhaseText;
	UpdatePhaseText();
}

void UW_LoadingScreen::SetStatusMessage(const FText& InStatusMessage)
{
	CurrentStatusMessage = InStatusMessage;
	UpdateStatusMessage();
}

void UW_LoadingScreen::ShowError(const FText& InErrorMessage)
{
	CurrentErrorMessage = InErrorMessage;
	bErrorVisible = true;

	if (UWidget* ErrorPanel = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Panel_Error"), true, true))
	{
		ErrorPanel->SetVisibility(ESlateVisibility::Visible);

		if (UTextBlock* ErrorText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(ErrorPanel, TEXT("Text_Error"), true, true))
		{
			ErrorText->SetText(InErrorMessage);
		}
		UE_LOG(LogW_LoadingScreen, Error, TEXT("Loading error shown: %s"), *InErrorMessage.ToString());
	}
}

void UW_LoadingScreen::HideError()
{
	CurrentErrorMessage = FText::GetEmpty();
	bErrorVisible = false;

	if (UWidget* ErrorPanel = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("Panel_Error"), true, true))
	{
		ErrorPanel->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogW_LoadingScreen, Display, TEXT("Error panel hidden"));
	}
}

// Widget updates
void UW_LoadingScreen::UpdateProgressBar()
{
	if (!RootWidget)
	{
		return;
	}

	if (UProgressBar* ProgressBar = UProjectWidgetHelpers::FindWidgetByNameTyped<UProgressBar>(RootWidget, TEXT("ProgressBar_Loading"), true, true))
	{
		ProgressBar->SetPercent(CurrentProgress);
	}

	// Also update percentage text
	if (UTextBlock* PercentText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Percentage"), true, true))
	{
		PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(CurrentProgress * 100.0f))));
	}
}

void UW_LoadingScreen::UpdatePhaseText()
{
	if (!RootWidget)
	{
		return;
	}

	if (UTextBlock* PhaseText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Phase"), true, true))
	{
		PhaseText->SetText(CurrentPhaseText);
	}
}

void UW_LoadingScreen::UpdateStatusMessage()
{
	if (!RootWidget)
	{
		return;
	}

	if (UTextBlock* StatusText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Status"), true, true))
	{
		StatusText->SetText(CurrentStatusMessage);
	}
}

void UW_LoadingScreen::OnCancelClicked()
{
	UE_LOG(LogW_LoadingScreen, Display, TEXT("Cancel clicked - requesting load cancellation"));

	if (TSharedPtr<ILoadingService> LoadingService = FProjectServiceLocator::Resolve<ILoadingService>())
	{
		LoadingService->CancelActiveLoad(false); // graceful cancel
	}
}

void UW_LoadingScreen::LogWidgetDiagnostics()
{
	if (!RootWidget)
	{
		UE_LOG(LogW_LoadingScreen, Error, TEXT("[Diagnostics] RootWidget is NULL - cannot inspect widget tree"));
		return;
	}

	UE_LOG(LogW_LoadingScreen, Display, TEXT("=== W_LoadingScreen Widget Diagnostics ==="));

	// Check Background (Border - new root) with detailed geometry
	FGeometry RootGeometry = RootWidget->GetCachedGeometry();
	FVector2D RootSize = RootGeometry.GetLocalSize();
	FVector2D RootPosition = RootGeometry.GetAbsolutePosition();

	UE_LOG(LogW_LoadingScreen, Display, TEXT("  Root: %s (type: %s)"),
		*RootWidget->GetName(), *RootWidget->GetClass()->GetName());
	UE_LOG(LogW_LoadingScreen, Display, TEXT("    Visibility: %s"),
		*UEnum::GetValueAsString(RootWidget->GetVisibility()));
	UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f"),
		RootSize.X, RootSize.Y);
	UE_LOG(LogW_LoadingScreen, Display, TEXT("    Position: (%.1f, %.1f)"),
		RootPosition.X, RootPosition.Y);
	UE_LOG(LogW_LoadingScreen, Display, TEXT("    IsVisible: %s, IsInViewport: %s"),
		RootWidget->IsVisible() ? TEXT("true") : TEXT("false"),
		RootWidget->IsInViewport() ? TEXT("true") : TEXT("false"));

	// Check CanvasPanel (RootCanvas)
	if (UWidget* Canvas = UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("RootCanvas"), true, false))
	{
		FGeometry CanvasGeometry = Canvas->GetCachedGeometry();
		FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] RootCanvas found (type: %s, visibility: %s)"),
			*Canvas->GetClass()->GetName(), *UEnum::GetValueAsString(Canvas->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, IsVisible: %s"),
			CanvasSize.X, CanvasSize.Y, Canvas->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] RootCanvas not found!"));
	}

	// Check ProgressBar
	if (UProgressBar* ProgressBar = UProjectWidgetHelpers::FindWidgetByNameTyped<UProgressBar>(RootWidget, TEXT("ProgressBar_Loading"), true, false))
	{
		FGeometry BarGeometry = ProgressBar->GetCachedGeometry();
		FVector2D BarSize = BarGeometry.GetLocalSize();
		FVector2D BarPosition = BarGeometry.GetAbsolutePosition();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] ProgressBar_Loading found (percent: %.2f, visibility: %s)"),
			ProgressBar->GetPercent(), *UEnum::GetValueAsString(ProgressBar->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, Position: (%.1f, %.1f), IsVisible: %s"),
			BarSize.X, BarSize.Y, BarPosition.X, BarPosition.Y,
			ProgressBar->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] ProgressBar_Loading not found!"));
	}

	// Check Text_Phase
	if (UTextBlock* PhaseText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Phase"), true, false))
	{
		FGeometry TextGeometry = PhaseText->GetCachedGeometry();
		FVector2D TextSize = TextGeometry.GetLocalSize();
		FVector2D TextPosition = TextGeometry.GetAbsolutePosition();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] Text_Phase found (text: '%s', visibility: %s)"),
			*PhaseText->GetText().ToString(), *UEnum::GetValueAsString(PhaseText->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, Position: (%.1f, %.1f), IsVisible: %s"),
			TextSize.X, TextSize.Y, TextPosition.X, TextPosition.Y,
			PhaseText->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] Text_Phase not found!"));
	}

	// Check Text_Percentage
	if (UTextBlock* PercentText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Percentage"), true, false))
	{
		FGeometry TextGeometry = PercentText->GetCachedGeometry();
		FVector2D TextSize = TextGeometry.GetLocalSize();
		FVector2D TextPosition = TextGeometry.GetAbsolutePosition();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] Text_Percentage found (text: '%s', visibility: %s)"),
			*PercentText->GetText().ToString(), *UEnum::GetValueAsString(PercentText->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, Position: (%.1f, %.1f), IsVisible: %s"),
			TextSize.X, TextSize.Y, TextPosition.X, TextPosition.Y,
			PercentText->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] Text_Percentage not found!"));
	}

	// Check Text_Status
	if (UTextBlock* StatusText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("Text_Status"), true, false))
	{
		FGeometry TextGeometry = StatusText->GetCachedGeometry();
		FVector2D TextSize = TextGeometry.GetLocalSize();
		FVector2D TextPosition = TextGeometry.GetAbsolutePosition();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] Text_Status found (text: '%s', visibility: %s)"),
			*StatusText->GetText().ToString(), *UEnum::GetValueAsString(StatusText->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, Position: (%.1f, %.1f), IsVisible: %s"),
			TextSize.X, TextSize.Y, TextPosition.X, TextPosition.Y,
			StatusText->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] Text_Status not found!"));
	}

	// Check Button_Cancel
	if (UButton* CancelButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Cancel"), true, false))
	{
		FGeometry ButtonGeometry = CancelButton->GetCachedGeometry();
		FVector2D ButtonSize = ButtonGeometry.GetLocalSize();
		FVector2D ButtonPosition = ButtonGeometry.GetAbsolutePosition();

		UE_LOG(LogW_LoadingScreen, Display, TEXT("  [OK] Button_Cancel found (visibility: %s)"),
			*UEnum::GetValueAsString(CancelButton->GetVisibility()));
		UE_LOG(LogW_LoadingScreen, Display, TEXT("    Size: %.1f x %.1f, Position: (%.1f, %.1f), IsVisible: %s"),
			ButtonSize.X, ButtonSize.Y, ButtonPosition.X, ButtonPosition.Y,
			CancelButton->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogW_LoadingScreen, Warning, TEXT("  [MISSING] Button_Cancel not found!"));
	}

	UE_LOG(LogW_LoadingScreen, Display, TEXT("=== End Widget Diagnostics ==="));
}
