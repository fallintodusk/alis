// Copyright ALIS. All Rights Reserved.

#include "MVVM/DialogueViewModel.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogueVM, Log, All);

void UDialogueViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);
	SubscribeToService();
}

void UDialogueViewModel::Shutdown()
{
	StopServiceRetry();
	UnsubscribeFromService();
	Super::Shutdown();
}

bool UDialogueViewModel::GetBoolProperty(FName PropertyName) const
{
	static const FName ActiveProp("bIsActive");
	if (PropertyName == ActiveProp)
	{
		return GetbIsActive();
	}
	return Super::GetBoolProperty(PropertyName);
}

void UDialogueViewModel::OnOptionSelected(int32 OptionIndex)
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (Service.IsValid())
	{
		Service->SelectOption(OptionIndex);
	}
}

void UDialogueViewModel::OnAdvanceClicked()
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (Service.IsValid())
	{
		Service->AdvanceOrEnd();
	}
}

void UDialogueViewModel::OnEndClicked()
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (Service.IsValid())
	{
		Service->EndDialogue();
	}
}

void UDialogueViewModel::SubscribeToService()
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (!Service.IsValid())
	{
		UE_LOG(LogDialogueVM, Log, TEXT("SubscribeToService: IDialogueService not yet available, will retry"));
		ServiceRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UDialogueViewModel::RetrySubscribeToService), 0.5f);
		return;
	}

	StateChangedHandle = Service->OnDialogueStateChanged().AddUObject(
		this, &UDialogueViewModel::HandleDialogueStateChanged);
	UE_LOG(LogDialogueVM, Log, TEXT("SubscribeToService: Bound to IDialogueService (Subscribers=%d)"),
		Service->OnDialogueStateChanged().IsBound() ? 1 : 0);

	RefreshFromService();
}

void UDialogueViewModel::StopServiceRetry()
{
	if (ServiceRetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ServiceRetryHandle);
		ServiceRetryHandle.Reset();
	}
}

bool UDialogueViewModel::RetrySubscribeToService(float /*DeltaTime*/)
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (!Service.IsValid())
	{
		return true; // keep ticking
	}

	StateChangedHandle = Service->OnDialogueStateChanged().AddUObject(
		this, &UDialogueViewModel::HandleDialogueStateChanged);
	RefreshFromService();

	UE_LOG(LogDialogueVM, Log, TEXT("IDialogueService found, subscribed"));
	ServiceRetryHandle.Reset();
	return false; // stop ticker
}

void UDialogueViewModel::UnsubscribeFromService()
{
	if (!StateChangedHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (Service.IsValid())
	{
		Service->OnDialogueStateChanged().Remove(StateChangedHandle);
	}

	StateChangedHandle.Reset();
}

void UDialogueViewModel::HandleDialogueStateChanged()
{
	UE_LOG(LogDialogueVM, Log, TEXT("HandleDialogueStateChanged: callback fired"));
	RefreshFromService();
}

void UDialogueViewModel::RefreshFromService()
{
	TSharedPtr<IDialogueService> Service = FProjectServiceLocator::Resolve<IDialogueService>();
	if (!Service.IsValid())
	{
		UpdatebIsActive(false);
		return;
	}

	const bool bActive = Service->IsDialogueActive();
	UE_LOG(LogDialogueVM, Log, TEXT("RefreshFromService: bActive=%d"), bActive);
	UpdatebIsActive(bActive);

	if (!bActive)
	{
		UpdateSpeakerName(FText::GetEmpty());
		UpdateDialogueText(FText::GetEmpty());
		UpdatebHasOptions(false);
		UpdatebIsTerminal(false);
		UpdatebHasSpeaker(false);
		UpdateOptions(TArray<FDialogueOptionView>());
		return;
	}

	const FText Speaker = Service->GetCurrentSpeaker();
	UpdateSpeakerName(Speaker);
	UpdatebHasSpeaker(!Speaker.IsEmpty());
	UpdateDialogueText(Service->GetCurrentText());
	UpdatebIsTerminal(Service->IsCurrentNodeTerminal());

	TArray<FDialogueOptionView> CurrentOptions;
	Service->GetCurrentOptions(CurrentOptions);
	UpdatebHasOptions(CurrentOptions.Num() > 0);
	UpdateOptions(CurrentOptions);
}
