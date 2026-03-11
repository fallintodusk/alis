// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectToastSubsystem.h"
#include "Widgets/W_ToastNotification.h"
#include "Components/PanelWidget.h"
#include "TimerManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogToast, Log, All);

void UProjectToastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogToast, Log, TEXT("ProjectToastSubsystem initialized"));
}

void UProjectToastSubsystem::Deinitialize()
{
	ClearQueue();

	if (ToastWidget)
	{
		ToastWidget->RemoveFromParent();
		ToastWidget = nullptr;
	}

	ToastContainer.Reset();

	Super::Deinitialize();
}

void UProjectToastSubsystem::ShowToast(const FText& Message, float Duration, FName Type)
{
	ShowToastMessage(FToastMessage(Message, Duration, Type));
}

void UProjectToastSubsystem::ShowToastMessage(const FToastMessage& ToastMessage)
{
	if (ToastMessage.Message.IsEmpty())
	{
		return;
	}

	ToastQueue.Add(ToastMessage);
	UE_LOG(LogToast, Verbose, TEXT("Toast queued: %s (queue size: %d)"), *ToastMessage.Message.ToString(), ToastQueue.Num());

	// If widget exists and not currently showing a toast, start processing
	// Note: If ToastWidget is null, we rely on SetToastContainer() to create it and call ProcessNextToast()
	if (ToastWidget && !ToastWidget->IsShowing())
	{
		ProcessNextToast();
	}
}

void UProjectToastSubsystem::ClearQueue()
{
	ToastQueue.Empty();
	UE_LOG(LogToast, Verbose, TEXT("Toast queue cleared"));
}

void UProjectToastSubsystem::DismissCurrent()
{
	if (ToastWidget && ToastWidget->IsShowing())
	{
		// Clear timer since we're manually dismissing
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DismissTimerHandle);
		}

		ToastWidget->Dismiss();
	}
}

void UProjectToastSubsystem::SetToastContainer(UPanelWidget* InContainer, APlayerController* OwningPlayer)
{
	ToastContainer = InContainer;

	if (!InContainer || !OwningPlayer)
	{
		return;
	}

	// Create toast widget if not exists
	if (!ToastWidget)
	{
		ToastWidget = CreateWidget<UW_ToastNotification>(OwningPlayer);
		if (ToastWidget)
		{
			ToastWidget->OnDismissed.BindUObject(this, &UProjectToastSubsystem::OnToastDismissed);
			InContainer->AddChild(ToastWidget);
			UE_LOG(LogToast, Log, TEXT("Toast widget created and added to container"));
		}
	}

	// Process any pending toasts
	if (ToastQueue.Num() > 0 && ToastWidget && !ToastWidget->IsShowing())
	{
		ProcessNextToast();
	}
}

void UProjectToastSubsystem::ProcessNextToast()
{
	if (ToastQueue.Num() == 0)
	{
		return;
	}

	if (!ToastWidget)
	{
		UE_LOG(LogToast, Warning, TEXT("ProcessNextToast: No toast widget available"));
		return;
	}

	// Pop the next message
	FToastMessage NextMessage = ToastQueue[0];
	ToastQueue.RemoveAt(0);

	UE_LOG(LogToast, Log, TEXT("Showing toast: %s"), *NextMessage.Message.ToString());

	// Show the toast
	ToastWidget->Show(NextMessage.Message, NextMessage.Type);

	// Set timer for auto-dismiss
	if (UWorld* World = GetWorld())
	{
		// Total time = fade-in + display duration + small buffer
		float FadeInTime = 0.3f;
		float TotalTime = FadeInTime + NextMessage.Duration;

		World->GetTimerManager().SetTimer(
			DismissTimerHandle,
			this,
			&UProjectToastSubsystem::OnDismissTimerExpired,
			TotalTime,
			false);
	}
}

void UProjectToastSubsystem::OnToastDismissed()
{
	UE_LOG(LogToast, Verbose, TEXT("Toast dismissed, queue remaining: %d"), ToastQueue.Num());

	// Process next toast in queue
	ProcessNextToast();
}

void UProjectToastSubsystem::OnDismissTimerExpired()
{
	if (ToastWidget && ToastWidget->IsShowing())
	{
		ToastWidget->Dismiss();
	}
}
