// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_DialoguePanel.h"
#include "MVVM/DialogueViewModel.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Styling/SlateColor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialoguePanel, Log, All);

UW_DialoguePanel::UW_DialoguePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UW_DialoguePanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (!RootWidget)
	{
		UE_LOG(LogDialoguePanel, Warning, TEXT("NativeConstruct: RootWidget is null - layout_json missing?"));
		return;
	}

	FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
	SpeakerText = Binder.FindOptional<UTextBlock>(TEXT("SpeakerText"));
	DialogueText = Binder.FindOptional<UTextBlock>(TEXT("DialogueText"));
	OptionsContainer = Binder.FindOptional<UVerticalBox>(TEXT("OptionsContainer"));
	ContinueButton = Binder.FindOptional<UButton>(TEXT("ContinueButton"));
	Binder.LogMissingRequired(TEXT("DialoguePanel"));

	// Base class calls BindCallbacks() during Super::NativeConstruct(), before
	// the binder resolves widgets. Call again now that ContinueButton is valid.
	BindCallbacks();
}

void UW_DialoguePanel::BindCallbacks()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleContinueClicked);
	}
}

void UW_DialoguePanel::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
	DialogueVM = Cast<UDialogueViewModel>(NewViewModel);
}

void UW_DialoguePanel::RefreshFromViewModel_Implementation()
{
	if (!DialogueVM)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bActive = DialogueVM->GetbIsActive();
	SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (!bActive)
	{
		return;
	}

	// Guard: RefreshFromViewModel can fire before NativeConstruct resolves widgets
	if (!RootWidget)
	{
		return;
	}

	if (SpeakerText)
	{
		const bool bHasSpeaker = DialogueVM->GetbHasSpeaker();
		SpeakerText->SetVisibility(bHasSpeaker ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasSpeaker)
		{
			SpeakerText->SetText(DialogueVM->GetSpeakerName());
		}
	}

	if (DialogueText)
	{
		DialogueText->SetText(DialogueVM->GetDialogueText());
	}

	if (ContinueButton)
	{
		const bool bShowContinue = !DialogueVM->GetbHasOptions();
		ContinueButton->SetVisibility(bShowContinue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (OptionsContainer)
	{
		const bool bShowOptions = DialogueVM->GetbHasOptions();
		OptionsContainer->SetVisibility(bShowOptions ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	RebuildOptionButtons();
}

void UW_DialoguePanel::RebuildOptionButtons()
{
	if (!OptionsContainer || !DialogueVM)
	{
		return;
	}

	OptionsContainer->ClearChildren();

	if (!DialogueVM->GetbHasOptions())
	{
		return;
	}

	const TArray<FDialogueOptionView>& Options = DialogueVM->GetOptions();
	const int32 Count = FMath::Min(Options.Num(), MaxVisibleOptions);

	for (int32 i = 0; i < Count; ++i)
	{
		const FDialogueOptionView& OptionView = Options[i];

		UButton* OptionButton = NewObject<UButton>(this);
		if (!OptionButton)
		{
			continue;
		}
		OptionButton->SetIsEnabled(!OptionView.bLocked);

		if (OptionView.bLocked)
		{
			OptionButton->SetBackgroundColor(FLinearColor(0.35f, 0.35f, 0.35f, 1.0f));
		}
		else if (OptionView.bConditionSatisfied)
		{
			// Subtle cue for condition-gated options that are currently available.
			OptionButton->SetBackgroundColor(FLinearColor(0.72f, 0.90f, 0.72f, 1.0f));
		}
		else
		{
			OptionButton->SetBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f));
		}

		UTextBlock* OptionText = NewObject<UTextBlock>(OptionButton);
		if (OptionText)
		{
			FText DisplayText = OptionView.Text;
			if (OptionView.bLocked && OptionView.bHasCondition)
			{
				DisplayText = FText::Format(
					NSLOCTEXT("DialoguePanel", "UnavailableOptionFmt", "{0} (Unavailable)"),
					DisplayText);
				OptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.68f, 0.68f, 1.0f)));
			}
			else
			{
				OptionText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			}

			OptionText->SetText(DisplayText);
			OptionButton->AddChild(OptionText);
		}

		// AddDynamic is a macro that stringifies the function pointer.
		// Cannot use array lookup - the macro would stringify "Handlers[i]"
		// which lacks "::" and crashes GetTrimmedMemberFunctionName.
		switch (i)
		{
		case 0: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption0Clicked); break;
		case 1: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption1Clicked); break;
		case 2: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption2Clicked); break;
		case 3: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption3Clicked); break;
		case 4: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption4Clicked); break;
		case 5: OptionButton->OnClicked.AddDynamic(this, &UW_DialoguePanel::HandleOption5Clicked); break;
		default: break;
		}

		OptionsContainer->AddChild(OptionButton);
	}
}

void UW_DialoguePanel::HandleOptionClicked(int32 Index)
{
	if (DialogueVM)
	{
		DialogueVM->OnOptionSelected(Index);
	}
}

void UW_DialoguePanel::HandleOption0Clicked() { HandleOptionClicked(0); }
void UW_DialoguePanel::HandleOption1Clicked() { HandleOptionClicked(1); }
void UW_DialoguePanel::HandleOption2Clicked() { HandleOptionClicked(2); }
void UW_DialoguePanel::HandleOption3Clicked() { HandleOptionClicked(3); }
void UW_DialoguePanel::HandleOption4Clicked() { HandleOptionClicked(4); }
void UW_DialoguePanel::HandleOption5Clicked() { HandleOptionClicked(5); }

void UW_DialoguePanel::HandleContinueClicked()
{
	if (DialogueVM)
	{
		DialogueVM->OnAdvanceClicked();
	}
}
