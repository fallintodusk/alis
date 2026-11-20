#include "ProjectTimeDemoUISubsystem.h"
#include "ProjectTimeDemoViewModel.h"
#include "ProjectTimeSubsystem.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Widgets/ProjectUserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
	const FName PanelId(TEXT("ProjectTime.Demo"));
	const FName HintId(TEXT("ProjectTime.Hint"));
}

void UProjectTimeDemoUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,
		&UProjectTimeDemoUISubsystem::Refresh), 1.0f);
}

void UProjectTimeDemoUISubsystem::UnbindInput()
{
	if (Input.IsValid()) Input->KeyBindings.RemoveAll([this](FInputKeyBinding& Binding)
	{
		return Binding.KeyDelegate.GetDelegateForManualSet().IsBoundToObject(this);
	});
	Input.Reset();
}

bool UProjectTimeDemoUISubsystem::Refresh(float)
{
	auto* World = GetWorld();
	auto* Controller = World ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
	auto* Time = World ? World->GetSubsystem<UProjectTimeSubsystem>() : nullptr;
	const bool bAllowed = Time && Time->CanControlTime();
	auto* NewInput = Controller && bAllowed ? Controller->InputComponent.Get() : nullptr;
	if (Input.Get() != NewInput)
	{
		UnbindInput();
		if (NewInput)
		{
			Input = NewInput;
			Input->BindKey(EKeys::F6, IE_Pressed, this, &UProjectTimeDemoUISubsystem::Toggle);
		}
	}
	if (Model.IsValid())
	{
		if (!bAllowed || Model->GetBoundWorld() != World) Close();
		else Model->Refresh();
	}
	if (auto* Host = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
	{
		const bool bHint = bAllowed && !Model.IsValid();
		if (bHint && !Host->IsDefinitionVisible(HintId)) Host->ShowDefinition(HintId);
		else if (!bHint && Host->IsDefinitionVisible(HintId)) Host->HideDefinition(HintId);
	}
	return true;
}

void UProjectTimeDemoUISubsystem::Toggle()
{
	if (Model.IsValid()) { Close(); return; }
	auto* World = GetWorld();
	auto* Time = World ? World->GetSubsystem<UProjectTimeSubsystem>() : nullptr;
	if (!Time || !Time->CanControlTime()) return;
	auto* Host = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>();
	if (!Host) return;
	Host->HideDefinition(HintId);
	if (auto* Widget = Cast<UProjectUserWidget>(Host->ShowDefinition(PanelId)))
	{
		Model = Cast<UProjectTimeDemoViewModel>(Widget->GetViewModel());
		if (Model.IsValid()) Model->CloseRequested.AddUObject(this, &UProjectTimeDemoUISubsystem::Close);
	}
}

void UProjectTimeDemoUISubsystem::Close()
{
	if (Model.IsValid()) Model->CloseRequested.RemoveAll(this);
	Model.Reset();
	if (auto* Host = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>()) Host->HideDefinition(PanelId);
}

void UProjectTimeDemoUISubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	UnbindInput();
	Close();
	if (auto* Host = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>()) Host->HideDefinition(HintId);
	Super::Deinitialize();
}
