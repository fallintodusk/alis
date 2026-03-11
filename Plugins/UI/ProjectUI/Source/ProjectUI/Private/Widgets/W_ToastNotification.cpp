// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_ToastNotification.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogToastWidget, Log, All);

UW_ToastNotification::UW_ToastNotification(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectUI"), TEXT("ToastLayout.json"));
}

void UW_ToastNotification::NativeConstruct()
{
	Super::NativeConstruct();

	// Find child widgets by name
	if (RootWidget)
	{
		MessageText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("MessageText"));
		Background = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(RootWidget, TEXT("Background"));
	}

	// Start hidden
	SetRenderOpacity(0.f);
	SetVisibility(ESlateVisibility::Collapsed);

	UE_LOG(LogToastWidget, Verbose, TEXT("ToastNotification constructed: MessageText=%s Background=%s"),
		MessageText.IsValid() ? TEXT("found") : TEXT("NOT FOUND"),
		Background.IsValid() ? TEXT("found") : TEXT("NOT FOUND"));
}

void UW_ToastNotification::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	switch (CurrentState)
	{
	case EToastState::FadingIn:
		CurrentOpacity = FMath::Min(CurrentOpacity + InDeltaTime * FadeInSpeed, 1.0f);
		UpdateOpacity(CurrentOpacity);

		if (CurrentOpacity >= 1.0f)
		{
			CurrentState = EToastState::Showing;
			UE_LOG(LogToastWidget, Verbose, TEXT("Toast fade-in complete"));
		}
		break;

	case EToastState::FadingOut:
		CurrentOpacity = FMath::Max(CurrentOpacity - InDeltaTime * FadeOutSpeed, 0.0f);
		UpdateOpacity(CurrentOpacity);

		if (CurrentOpacity <= 0.0f)
		{
			CurrentState = EToastState::Hidden;
			bIsShowing = false;
			SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogToastWidget, Verbose, TEXT("Toast fade-out complete"));

			// Notify subsystem
			OnDismissed.ExecuteIfBound();
		}
		break;

	default:
		break;
	}
}

void UW_ToastNotification::Show(const FText& Message, FName Type)
{
	if (MessageText.IsValid())
	{
		MessageText->SetText(Message);
	}

	ApplyTypeStyle(Type);

	// Start visible but transparent
	SetVisibility(ESlateVisibility::HitTestInvisible);
	CurrentOpacity = 0.f;
	UpdateOpacity(0.f);

	CurrentState = EToastState::FadingIn;
	bIsShowing = true;

	UE_LOG(LogToastWidget, Log, TEXT("Toast showing: %s (type: %s)"),
		*Message.ToString(), *Type.ToString());
}

void UW_ToastNotification::Dismiss()
{
	if (CurrentState == EToastState::Hidden || CurrentState == EToastState::FadingOut)
	{
		return;
	}

	CurrentState = EToastState::FadingOut;
	UE_LOG(LogToastWidget, Verbose, TEXT("Toast dismissing"));
}

void UW_ToastNotification::ApplyTypeStyle(FName Type)
{
	CurrentType = Type;

	if (!Background.IsValid())
	{
		return;
	}

	// Default colors (can be themed later)
	FLinearColor BackgroundColor;

	if (Type == FName("Error"))
	{
		BackgroundColor = FLinearColor(0.6f, 0.1f, 0.1f, 0.9f);
	}
	else if (Type == FName("Warning"))
	{
		BackgroundColor = FLinearColor(0.6f, 0.4f, 0.1f, 0.9f);
	}
	else
	{
		// Info (default)
		BackgroundColor = FLinearColor(0.15f, 0.15f, 0.2f, 0.9f);
	}

	Background->SetBrushColor(BackgroundColor);
}

void UW_ToastNotification::UpdateOpacity(float Opacity)
{
	SetRenderOpacity(Opacity);
}
