// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_MindJournalPanel.h"
#include "MVVM/MindJournalViewModel.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

UW_MindJournalPanel::UW_MindJournalPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectMindUI"), TEXT("MindJournalPanel.json"));
}

void UW_MindJournalPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (RootWidget)
	{
		FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
		BackdropButton = Binder.FindOptional<UButton>(TEXT("BackdropButton"));
		CloseButton = Binder.FindOptional<UButton>(TEXT("CloseButton"));
		ImportantTabButton = Binder.FindOptionalAny<UButton>({ TEXT("ImportantTabButton"), TEXT("JournalTabButton") });
		RecentTabButton = Binder.FindOptional<UButton>(TEXT("RecentTabButton"));
		ImportantContent = Binder.FindOptionalAny<UBorder>({ TEXT("ImportantContent"), TEXT("JournalContent") });
		RecentContent = Binder.FindOptional<UBorder>(TEXT("RecentContent"));
		ImportantHistoryText = Binder.FindOptionalAny<UTextBlock>({
			TEXT("ImportantHistoryText"),
			TEXT("JournalHistoryText"),
			TEXT("HistoryText")
		});
		RecentHistoryText = Binder.FindOptional<UTextBlock>(TEXT("RecentHistoryText"));
		ImportantScroll = Binder.FindOptional<UScrollBox>(TEXT("ImportantScroll"));
		RecentScroll = Binder.FindOptional<UScrollBox>(TEXT("RecentScroll"));
		Binder.LogMissingRequired(TEXT("MindJournalPanel"));
	}

	// Base class callback binding happens before widgets are resolved.
	BindCallbacks();
	RefreshFromViewModel();
}

void UW_MindJournalPanel::BindCallbacks()
{
	if (BackdropButton)
	{
		BackdropButton->OnClicked.AddUniqueDynamic(this, &UW_MindJournalPanel::HandleBackdropClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UW_MindJournalPanel::HandleCloseClicked);
	}

	if (ImportantTabButton)
	{
		ImportantTabButton->OnClicked.AddUniqueDynamic(this, &UW_MindJournalPanel::HandleImportantTabClicked);
	}

	if (RecentTabButton)
	{
		RecentTabButton->OnClicked.AddUniqueDynamic(this, &UW_MindJournalPanel::HandleRecentTabClicked);
	}
}

void UW_MindJournalPanel::RefreshFromViewModel_Implementation()
{
	Super::RefreshFromViewModel_Implementation();

	UMindJournalViewModel* MindVM = Cast<UMindJournalViewModel>(GetViewModel());
	if (!MindVM || !MindVM->GetbPanelVisible())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);

	const bool bJournalTabActive = MindVM->GetbJournalTabActive();
	if (ImportantTabButton)
	{
		ImportantTabButton->SetIsEnabled(true);
		UProjectWidgetLayoutLoader::ApplyButtonVariantStyle(
			ImportantTabButton,
			bJournalTabActive ? TEXT("Primary") : TEXT("Secondary"),
			GetActiveTheme());
	}

	if (RecentTabButton)
	{
		RecentTabButton->SetIsEnabled(true);
		UProjectWidgetLayoutLoader::ApplyButtonVariantStyle(
			RecentTabButton,
			bJournalTabActive ? TEXT("Secondary") : TEXT("Primary"),
			GetActiveTheme());
	}

	if (ImportantContent)
	{
		ImportantContent->SetVisibility(bJournalTabActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (RecentContent)
	{
		RecentContent->SetVisibility(bJournalTabActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	const bool bHasDedicatedRecentText = ImportantHistoryText && RecentHistoryText && RecentHistoryText != ImportantHistoryText;
	if (bHasDedicatedRecentText)
	{
		ImportantHistoryText->SetText(MindVM->GetJournalText());
		RecentHistoryText->SetText(MindVM->GetRecentText());
	}
	else if (ImportantHistoryText)
	{
		ImportantHistoryText->SetText(bJournalTabActive ? MindVM->GetJournalText() : MindVM->GetRecentText());
	}
}

void UW_MindJournalPanel::HandleBackdropClicked()
{
	if (UMindJournalViewModel* MindVM = Cast<UMindJournalViewModel>(GetViewModel()))
	{
		MindVM->HidePanel();
	}
}

void UW_MindJournalPanel::HandleCloseClicked()
{
	HandleBackdropClicked();
}

void UW_MindJournalPanel::HandleImportantTabClicked()
{
	if (UMindJournalViewModel* MindVM = Cast<UMindJournalViewModel>(GetViewModel()))
	{
		MindVM->SetJournalTabActive(true);
		if (ImportantScroll)
		{
			ImportantScroll->ScrollToStart();
		}
	}
}

void UW_MindJournalPanel::HandleRecentTabClicked()
{
	if (UMindJournalViewModel* MindVM = Cast<UMindJournalViewModel>(GetViewModel()))
	{
		MindVM->SetJournalTabActive(false);
		if (RecentScroll)
		{
			RecentScroll->ScrollToStart();
		}
	}
}
