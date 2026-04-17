// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_MindThoughtToast.h"
#include "MVVM/MindThoughtViewModel.h"
#include "Components/TextBlock.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "ProjectWidgetHelpers.h"
#include "TimerManager.h"

UW_MindThoughtToast::UW_MindThoughtToast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMindUI"), TEXT("MindThoughtToast.json"));
}

void UW_MindThoughtToast::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootWidget)
	{
		ThoughtText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("ThoughtText"));
		ThoughtMetaText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("ThoughtMetaText"));
	}

	SetVisibility(ESlateVisibility::Collapsed);
	RefreshFromViewModel();
}

void UW_MindThoughtToast::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoHideTimerHandle);
	}

	Super::NativeDestruct();
}

void UW_MindThoughtToast::RefreshFromViewModel_Implementation()
{
	Super::RefreshFromViewModel_Implementation();

	UMindThoughtViewModel* MindVM = Cast<UMindThoughtViewModel>(GetViewModel());
	if (!MindVM || !MindVM->GetbHasThought() || MindVM->GetThoughtText().IsEmpty())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoHideTimerHandle);
		}
		return;
	}

	if (ThoughtText.IsValid())
	{
		// Cache default font on first use
		if (!bDefaultFontCached)
		{
			DefaultThoughtFont = ThoughtText->GetFont();
			bDefaultFontCached = true;
		}

		ThoughtText->SetText(MindVM->GetThoughtText());

		// System thoughts (death, critical events): 3x font, red
		if (MindVM->GetbIsSystemThought())
		{
			FSlateFontInfo SystemFont = DefaultThoughtFont;
			SystemFont.Size = DefaultThoughtFont.Size * 3;
			ThoughtText->SetFont(SystemFont);
			ThoughtText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
		}
		else
		{
			ThoughtText->SetFont(DefaultThoughtFont);
			ThoughtText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
	if (ThoughtMetaText.IsValid())
	{
		ThoughtMetaText->SetText(MindVM->GetThoughtMetaText());
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	const float TtlSec = FMath::Max(0.1f, MindVM->GetThoughtTimeToLiveSec());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AutoHideTimerHandle,
			this,
			&UW_MindThoughtToast::HandleAutoHideExpired,
			TtlSec,
			false);
	}
}

void UW_MindThoughtToast::HandleAutoHideExpired()
{
	if (UMindThoughtViewModel* MindVM = Cast<UMindThoughtViewModel>(GetViewModel()))
	{
		MindVM->ClearCurrentThought();
	}
}
