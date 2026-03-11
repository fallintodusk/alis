// Copyright ALIS. All Rights Reserved.
#include "Widgets/W_VitalsPanel.h"
#include "MVVM/VitalsViewModel.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Theme/ProjectUIThemeData.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "VitalsConfig.h"

DEFINE_LOG_CATEGORY(LogVitalsPanel);

UW_VitalsPanel::UW_VitalsPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectVitalsUI"), TEXT("VitalsPanel.json"));
	SetIsFocusable(true);
}

void UW_VitalsPanel::SetVitalsViewModel(UVitalsViewModel* InViewModel)
{
	if (VitalsVM == InViewModel)
	{
		return;
	}

	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.RemoveDynamic(this, &UW_VitalsPanel::HandleVitalsPanelViewModelPropertyChanged);
	}

	VitalsVM = InViewModel;

	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.AddUniqueDynamic(this, &UW_VitalsPanel::HandleVitalsPanelViewModelPropertyChanged);
		RefreshFromViewModel();
		SetVisibility(VitalsVM->GetbPanelVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_VitalsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (!RootWidget)
	{
		UE_LOG(LogVitalsPanel, Warning, TEXT("NativeConstruct: RootWidget is null"));
		return;
	}

	FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());

	// Left column: Condition system
	ConditionRow = Binder.FindOptional<UHorizontalBox>(TEXT("ConditionRow"));
	ConditionBar = Binder.FindRequired<UProgressBar>(TEXT("ConditionBar"));
	ConditionValue = Binder.FindRequired<UTextBlock>(TEXT("ConditionValue"));
	ConditionRegenLabel = Binder.FindOptional<UTextBlock>(TEXT("ConditionRegenLabel"));
	ConditionRegenDays = Binder.FindOptional<UTextBlock>(TEXT("ConditionRegenDays"));
	ConditionRate = Binder.FindOptional<UTextBlock>(TEXT("ConditionRate"));

	CaloriesRow = Binder.FindOptional<UHorizontalBox>(TEXT("CaloriesRow"));
	CaloriesBar = Binder.FindRequired<UProgressBar>(TEXT("CaloriesBar"));
	CaloriesValue = Binder.FindRequired<UTextBlock>(TEXT("CaloriesValue"));
	CaloriesRate = Binder.FindOptional<UTextBlock>(TEXT("CaloriesRate"));
	CaloriesLifeDays = Binder.FindOptional<UTextBlock>(TEXT("CaloriesLifeDays"));

	HydrationRow = Binder.FindOptional<UHorizontalBox>(TEXT("HydrationRow"));
	HydrationBar = Binder.FindRequired<UProgressBar>(TEXT("HydrationBar"));
	HydrationValue = Binder.FindRequired<UTextBlock>(TEXT("HydrationValue"));
	HydrationRate = Binder.FindOptional<UTextBlock>(TEXT("HydrationRate"));
	HydrationLifeDays = Binder.FindOptional<UTextBlock>(TEXT("HydrationLifeDays"));

	FatigueRow = Binder.FindOptional<UHorizontalBox>(TEXT("FatigueRow"));
	FatigueBar = Binder.FindRequired<UProgressBar>(TEXT("FatigueBar"));
	FatigueValue = Binder.FindRequired<UTextBlock>(TEXT("FatigueValue"));
	FatigueRate = Binder.FindOptional<UTextBlock>(TEXT("FatigueRate"));
	FatigueTimeline = Binder.FindOptional<UTextBlock>(TEXT("FatigueTimeline"));

	// Collapsible sections
	StatusToggle = Binder.FindOptional<UButton>(TEXT("StatusToggle"));
	StatusContent = Binder.FindOptional<UVerticalBox>(TEXT("StatusContent"));
	BleedingValue = Binder.FindOptional<UTextBlock>(TEXT("BleedingValue"));
	PoisonedValue = Binder.FindOptional<UTextBlock>(TEXT("PoisonedValue"));
	RadiationValue = Binder.FindOptional<UTextBlock>(TEXT("RadiationValue"));

	MetabolismToggle = Binder.FindOptional<UButton>(TEXT("MetabolismToggle"));
	MetabolismContent = Binder.FindOptional<UVerticalBox>(TEXT("MetabolismContent"));
	MetabolismWeight = Binder.FindOptional<UTextBlock>(TEXT("MetabolismWeight"));
	MetabolismHydration = Binder.FindOptional<UTextBlock>(TEXT("MetabolismHydration"));
	ActivityIdle = Binder.FindOptional<UTextBlock>(TEXT("ActivityIdle"));
	ActivityWalk = Binder.FindOptional<UTextBlock>(TEXT("ActivityWalk"));
	ActivityJog = Binder.FindOptional<UTextBlock>(TEXT("ActivityJog"));
	ActivitySprint = Binder.FindOptional<UTextBlock>(TEXT("ActivitySprint"));

	// Right column: Stamina system
	StaminaRow = Binder.FindOptional<UHorizontalBox>(TEXT("StaminaRow"));
	StaminaBar = Binder.FindRequired<UProgressBar>(TEXT("StaminaBar"));
	StaminaValue = Binder.FindRequired<UTextBlock>(TEXT("StaminaValue"));
	StaminaRate = Binder.FindOptional<UTextBlock>(TEXT("StaminaRate"));

	MovementSprint = Binder.FindOptional<UTextBlock>(TEXT("MovementSprint"));
	MovementJog = Binder.FindOptional<UTextBlock>(TEXT("MovementJog"));
	MovementRegen = Binder.FindOptional<UTextBlock>(TEXT("MovementRegen"));
	InteractionsRow1 = Binder.FindOptional<UTextBlock>(TEXT("InteractionsRow1"));
	InteractionsRow2 = Binder.FindOptional<UTextBlock>(TEXT("InteractionsRow2"));
	CombatRow1 = Binder.FindOptional<UTextBlock>(TEXT("CombatRow1"));
	CombatRow2 = Binder.FindOptional<UTextBlock>(TEXT("CombatRow2"));

	StatusEffects = Binder.FindOptional<UTextBlock>(TEXT("StatusEffects"));
	CloseButton = Binder.FindOptional<UButton>(TEXT("CloseButton"));
	OverlayButton = Binder.FindOptional<UButton>(TEXT("OverlayButton"));
	Binder.LogMissingRequired(TEXT("UW_VitalsPanel::NativeConstruct"));

	// Default collapsed state
	if (StatusContent) { StatusContent->SetVisibility(ESlateVisibility::Collapsed); }
	if (MetabolismContent) { MetabolismContent->SetVisibility(ESlateVisibility::Collapsed); }

	// One-time setup
	SetupRowTooltips();
	PopulateStaticValues();

	// Button callbacks
	if (CloseButton) { CloseButton->OnClicked.AddUniqueDynamic(this, &UW_VitalsPanel::HandleCloseClicked); }
	if (OverlayButton) { OverlayButton->OnClicked.AddUniqueDynamic(this, &UW_VitalsPanel::HandleOverlayClicked); }
	if (StatusToggle) { StatusToggle->OnClicked.AddUniqueDynamic(this, &UW_VitalsPanel::HandleStatusToggle); }
	if (MetabolismToggle) { MetabolismToggle->OnClicked.AddUniqueDynamic(this, &UW_VitalsPanel::HandleMetabolismToggle); }
}

void UW_VitalsPanel::NativeDestruct()
{
	if (VitalsVM)
	{
		VitalsVM->OnPropertyChanged.RemoveDynamic(this, &UW_VitalsPanel::HandleVitalsPanelViewModelPropertyChanged);
	}
	Super::NativeDestruct();
}

void UW_VitalsPanel::HandleVitalsPanelViewModelPropertyChanged(FName PropertyName)
{
	if (!VitalsVM)
	{
		return;
	}

	if (PropertyName == TEXT("bPanelVisible"))
	{
		bool bVisible = VitalsVM->GetbPanelVisible();
		SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		// Reset collapsible sections when panel reopens
		if (bVisible)
		{
			if (StatusContent) { StatusContent->SetVisibility(ESlateVisibility::Collapsed); }
			if (MetabolismContent) { MetabolismContent->SetVisibility(ESlateVisibility::Collapsed); }
			if (StatusToggle) { StatusToggle->SetToolTipText(FText::FromString(TEXT("[+] STATUS"))); }
			if (MetabolismToggle) { MetabolismToggle->SetToolTipText(FText::FromString(TEXT("[+] METABOLISM"))); }
		}
		return;
	}

	RefreshFromViewModel();
}

FReply UW_VitalsPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (VitalsVM) { VitalsVM->HidePanel(); }
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UW_VitalsPanel::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
	Super::OnViewModelChanged_Implementation(OldViewModel, NewViewModel);
	if (UVitalsViewModel* NewVitalsVM = Cast<UVitalsViewModel>(NewViewModel))
	{
		SetVitalsViewModel(NewVitalsVM);
	}
}

void UW_VitalsPanel::RefreshFromViewModel_Implementation()
{
	Super::RefreshFromViewModel_Implementation();

	if (!VitalsVM) { return; }

	// Condition
	if (ConditionBar) { ConditionBar->SetPercent(VitalsVM->GetConditionPercent()); }
	if (ConditionValue) { ConditionValue->SetText(FormatConditionValue(VitalsVM->GetConditionCurrent())); }

	// Stamina
	if (StaminaBar) { StaminaBar->SetPercent(VitalsVM->GetStaminaPercent()); }
	if (StaminaValue) { StaminaValue->SetText(FormatStaminaValue(VitalsVM->GetStaminaCurrent())); }

	// Calories
	if (CaloriesBar) { CaloriesBar->SetPercent(VitalsVM->GetCaloriesPercent()); }
	if (CaloriesValue) { CaloriesValue->SetText(FormatCaloriesValue(VitalsVM->GetCaloriesCurrent())); }

	// Hydration
	if (HydrationBar) { HydrationBar->SetPercent(VitalsVM->GetHydrationPercent()); }
	if (HydrationValue) { HydrationValue->SetText(FormatHydrationValue(VitalsVM->GetHydrationCurrent())); }

	// Fatigue
	if (FatigueBar) { FatigueBar->SetPercent(VitalsVM->GetFatiguePercent()); }
	if (FatigueValue) { FatigueValue->SetText(FormatFatigueValue(VitalsVM->GetFatiguePercent())); }

	UpdateBarColors();
	UpdateRateTexts();
	UpdateStatusValues();
	UpdateStatusEffects();
}

void UW_VitalsPanel::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
	Super::OnThemeChanged_Implementation(NewTheme);
	CurrentTheme = NewTheme;
	UpdateBarColors();
}

void UW_VitalsPanel::HandleCloseClicked()
{
	if (VitalsVM) { VitalsVM->HidePanel(); }
}

void UW_VitalsPanel::HandleOverlayClicked()
{
	if (VitalsVM) { VitalsVM->HidePanel(); }
}

void UW_VitalsPanel::HandleStatusToggle()
{
	if (!StatusContent) { return; }

	bool bCollapsed = StatusContent->GetVisibility() == ESlateVisibility::Collapsed;
	StatusContent->SetVisibility(bCollapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_VitalsPanel::HandleMetabolismToggle()
{
	if (!MetabolismContent) { return; }

	bool bCollapsed = MetabolismContent->GetVisibility() == ESlateVisibility::Collapsed;
	MetabolismContent->SetVisibility(bCollapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_VitalsPanel::UpdateBarColors()
{
	if (!VitalsVM) { return; }

	if (ConditionBar) { ConditionBar->SetFillColorAndOpacity(GetStateColor(VitalsVM->GetConditionState())); }
	if (StaminaBar) { StaminaBar->SetFillColorAndOpacity(GetStateColor(VitalsVM->GetStaminaState())); }
	if (CaloriesBar) { CaloriesBar->SetFillColorAndOpacity(GetStateColor(VitalsVM->GetCaloriesState())); }
	if (HydrationBar) { HydrationBar->SetFillColorAndOpacity(GetStateColor(VitalsVM->GetHydrationState())); }
	if (FatigueBar) { FatigueBar->SetFillColorAndOpacity(GetFatigueStateColor(VitalsVM->GetFatigueState())); }
}

void UW_VitalsPanel::UpdateStatusValues()
{
	if (!VitalsVM) { return; }

	if (BleedingValue)
	{
		BleedingValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), VitalsVM->GetBleedingSeverity())));
	}
	if (PoisonedValue)
	{
		PoisonedValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), VitalsVM->GetPoisonedSeverity())));
	}
	if (RadiationValue)
	{
		RadiationValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), VitalsVM->GetRadiationLevel())));
	}
}

void UW_VitalsPanel::UpdateStatusEffects()
{
	if (!StatusEffects || !VitalsVM) { return; }

	TArray<FString> Effects;
	if (VitalsVM->GetbIsBleeding()) { Effects.Add(TEXT("Bleeding")); }
	if (VitalsVM->GetbIsPoisoned()) { Effects.Add(TEXT("Poisoned")); }
	if (VitalsVM->GetRadiationLevel() > 0.f) { Effects.Add(TEXT("Irradiated")); }
	if (VitalsVM->GetCaloriesState() == EVitalState::Critical || VitalsVM->GetCaloriesState() == EVitalState::Empty)
	{
		Effects.Add(TEXT("Starving"));
	}
	if (VitalsVM->GetHydrationState() == EVitalState::Critical || VitalsVM->GetHydrationState() == EVitalState::Empty)
	{
		Effects.Add(TEXT("Dehydrated"));
	}
	if (VitalsVM->GetFatigueState() == EFatigueState::Exhausted || VitalsVM->GetFatigueState() == EFatigueState::Critical)
	{
		Effects.Add(TEXT("Exhausted"));
	}

	if (Effects.Num() > 0)
	{
		StatusEffects->SetText(FText::FromString(FString::Join(Effects, TEXT(" | "))));
		StatusEffects->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		StatusEffects->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_VitalsPanel::UpdateRateTexts()
{
	if (!VitalsVM) { return; }

	if (ConditionRate) { ConditionRate->SetText(VitalsVM->GetConditionRateText()); }
	if (StaminaRate) { StaminaRate->SetText(VitalsVM->GetStaminaRateText()); }
	if (CaloriesRate) { CaloriesRate->SetText(VitalsVM->GetCaloriesRateText()); }
	if (HydrationRate) { HydrationRate->SetText(VitalsVM->GetHydrationRateText()); }
	if (FatigueRate) { FatigueRate->SetText(VitalsVM->GetFatigueRateText()); }

	if (ConditionRate)
	{
		FLinearColor RateColor = (VitalsVM->GetbIsBleeding() ||
			VitalsVM->GetConditionState() == EVitalState::Critical ||
			VitalsVM->GetConditionState() == EVitalState::Empty)
			? GetStateColor(EVitalState::Critical)
			: GetStateColor(EVitalState::OK);
		ConditionRate->SetColorAndOpacity(FSlateColor(RateColor));
	}
}

UWidget* UW_VitalsPanel::CreateTooltipWidget(const FText& Text)
{
	UTextBlock* Tip = NewObject<UTextBlock>();
	Tip->SetText(Text);
	Tip->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodyMedium"), CurrentTheme));
	Tip->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.93f, 0.93f)));

	UBorder* Border = NewObject<UBorder>();
	Border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.85f));
	Border->SetPadding(FMargin(10.f, 6.f));
	Border->SetContent(Tip);

	TooltipWidgets.Add(Border);
	return Border;
}

void UW_VitalsPanel::SetupRowTooltips()
{
	const FVitalsConfig& Config = FVitalsConfigLoader::Get();
	const FVitalsConditionConfig& C = Config.Condition;
	const FVitalsMetabolism& M = C.Metabolism;
	const FVitalsActivity& A = M.Activity;

	auto SetTooltip = [this](UWidget* W, const FText& Tip)
	{
		if (W) { W->SetToolTip(CreateTooltipWidget(Tip)); }
	};

	// Condition bar
	SetTooltip(ConditionBar, FText::FromString(FString::Printf(
		TEXT("Condition (Health)\n")
		TEXT("Base regen: %.6f HP/s when all vitals above critical\n")
		TEXT("Bleeding: stops all regen\n")
		TEXT("Drain when vitals depleted (additive):\n")
		TEXT("  Dehydration: %.6f HP/s\n")
		TEXT("  Starvation: %.6f HP/s\n")
		TEXT("  Exhaustion: %.6f HP/s"),
		C.BaseRegenPerSec,
		C.DehydrationDrainPerSec, C.StarvationDrainPerSec, C.ExhaustionDrainPerSec)));

	SetTooltip(ConditionRegenDays, FText::FromString(FString::Printf(
		TEXT("Time to regenerate 0->100 when all vitals healthy\n")
		TEXT("Rate: %.6f HP/s"),
		C.BaseRegenPerSec)));

	// Stamina
	SetTooltip(StaminaBar, FText::FromString(
		TEXT("Stamina (Short-term energy)\n")
		TEXT("Regen penalties:\n")
		TEXT("  Low Calories: -20%%\n")
		TEXT("  High Fatigue: -30%%\n")
		TEXT("  Both combined: -50%%\n")
		TEXT("  Dehydration: reduces max stamina")));

	// Calories
	SetTooltip(CaloriesBar, FText::FromString(FString::Printf(
		TEXT("Calories (Energy reserves)\n")
		TEXT("Drain = Weight x MET / 3600 per second\n")
		TEXT("MET: Idle %.1f | Walk %.1f | Jog %.1f | Sprint %.1f\n")
		TEXT("Weight: %.0f kg"),
		A.MET_Idle, A.MET_Walk, A.MET_Jog, A.MET_Sprint, M.CharacterWeightKg)));

	SetTooltip(CaloriesLifeDays, FText::FromString(FString::Printf(
		TEXT("Survival time from Calories=0%% until death\n")
		TEXT("Total: %.0f + %.0f = %.0f days (drain + condition death)"),
		1.f, C.Survival.Calories.ConditionDeathDays,
		1.f + C.Survival.Calories.ConditionDeathDays)));

	// Hydration
	SetTooltip(HydrationBar, FText::FromString(FString::Printf(
		TEXT("Hydration (Water)\n")
		TEXT("Drain tied to calorie burn: %.3f L per kcal\n")
		TEXT("Higher activity = faster dehydration"),
		M.HydrationPerKcal)));

	SetTooltip(HydrationLifeDays, FText::FromString(FString::Printf(
		TEXT("Survival time from Hydration=0%% until death\n")
		TEXT("Total: %.0f + %.0f = %.0f days (drain + condition death)"),
		1.f, C.Survival.Hydration.ConditionDeathDays,
		1.f + C.Survival.Hydration.ConditionDeathDays)));

	// Fatigue
	SetTooltip(FatigueBar, FText::FromString(FString::Printf(
		TEXT("Fatigue (Tiredness, inverted: 0=good, 100=bad)\n")
		TEXT("%.0fh to full exhaustion\n")
		TEXT("High fatigue reduces stamina regen\n")
		TEXT("Sleep to reduce fatigue"),
		C.Survival.Fatigue.HoursToEmpty)));

	SetTooltip(FatigueTimeline, FText::FromString(FString::Printf(
		TEXT("Hours of wakefulness until fatigue reaches 100%%\n")
		TEXT("Then condition drains over %.0f days"),
		C.Survival.Fatigue.ConditionDeathDays)));

	// Status effects
	SetTooltip(BleedingValue, FText::FromString(
		TEXT("Severity 0-1. Tiers: light 0.02, moderate 0.1, severe 0.5, critical 1.0\n")
		TEXT("Formula: Drain = Severity x 0.5 HP/s")));

	SetTooltip(PoisonedValue, FText::FromString(
		TEXT("Severity 0-1. Tiers: mild 0.1, moderate 0.3, severe 0.7, lethal 1.0")));

	SetTooltip(RadiationValue, FText::FromString(
		TEXT("Level 0-1. Tiers: low 0.1, moderate 0.3, high 0.6, critical 1.0")));

	// Metabolism
	SetTooltip(MetabolismWeight, FText::FromString(
		TEXT("Character mass for calorie burn formula: MET x Weight x 3.5 / 200")));

	SetTooltip(MetabolismHydration, FText::FromString(
		TEXT("Liters of water lost per kcal burned (medical standard: ~1L per 1000 kcal)")));

	// Movement costs
	SetTooltip(MovementSprint, FText::FromString(
		TEXT("Stamina cost per second while sprinting. At 100 stamina = 10s sprint")));
	SetTooltip(MovementJog, FText::FromString(
		TEXT("Stamina cost per second while jogging")));
	SetTooltip(MovementRegen, FText::FromString(
		TEXT("Stamina regeneration per second when not draining")));

	// Interactions
	SetTooltip(InteractionsRow1, FText::FromString(
		TEXT("Stamina cost per interaction action")));
	SetTooltip(InteractionsRow2, FText::FromString(
		TEXT("Heavy: high-cost actions. Min Required: cannot act below this stamina")));

	// Combat
	SetTooltip(CombatRow1, FText::FromString(
		TEXT("Stamina per attack. Light attacks are fast, heavy attacks cost more")));
	SetTooltip(CombatRow2, FText::FromString(
		TEXT("Dodge: per dodge roll. Block: per second while holding block")));
}

void UW_VitalsPanel::PopulateStaticValues()
{
	const FVitalsConfig& Config = FVitalsConfigLoader::Get();
	const FVitalsConditionConfig& C = Config.Condition;
	const FVitalsStaminaConfig& S = Config.Stamina;
	const FVitalsMetabolism& M = C.Metabolism;
	const FVitalsActivity& A = M.Activity;

	// Condition regen info
	if (ConditionRegenLabel)
	{
		ConditionRegenLabel->SetText(FText::FromString(FString::Printf(TEXT("Regen %.1fx"), C.RegenRate)));
	}
	if (ConditionRegenDays)
	{
		ConditionRegenDays->SetText(FText::FromString(FString::Printf(TEXT("%.1f days"), C.BaseRegenDays)));
	}

	// Life days (total = drain time + condition death)
	if (CaloriesLifeDays)
	{
		float Total = 1.f + C.Survival.Calories.ConditionDeathDays;
		CaloriesLifeDays->SetText(FText::FromString(FString::Printf(TEXT("Life: %.0f days"), Total)));
	}
	if (HydrationLifeDays)
	{
		float Total = 1.f + C.Survival.Hydration.ConditionDeathDays;
		HydrationLifeDays->SetText(FText::FromString(
			(Total > 1.5f) ? FString::Printf(TEXT("Life: %.0f days"), Total) : FString::Printf(TEXT("Life: %.0f day"), Total)));
	}
	if (FatigueTimeline)
	{
		FatigueTimeline->SetText(FText::FromString(FString::Printf(
			TEXT("%.0fh to empty | Life: %.0fd"),
			C.Survival.Fatigue.HoursToEmpty,
			C.Survival.Fatigue.HoursToEmpty / 24.f + C.Survival.Fatigue.ConditionDeathDays)));
	}

	// Metabolism section
	if (MetabolismWeight)
	{
		MetabolismWeight->SetText(FText::FromString(FString::Printf(TEXT("Weight: %.0f kg"), M.CharacterWeightKg)));
	}
	if (MetabolismHydration)
	{
		MetabolismHydration->SetText(FText::FromString(FString::Printf(TEXT("Hydration: %.3f L/kcal"), M.HydrationPerKcal)));
	}
	if (ActivityIdle)
	{
		ActivityIdle->SetText(FText::FromString(FString::Printf(TEXT("  Idle   %.1f  < %.0f cm/s"), A.MET_Idle, A.SpeedThreshold_Idle)));
	}
	if (ActivityWalk)
	{
		ActivityWalk->SetText(FText::FromString(FString::Printf(TEXT("  Walk   %.1f  < %.0f cm/s"), A.MET_Walk, A.SpeedThreshold_Walk)));
	}
	if (ActivityJog)
	{
		ActivityJog->SetText(FText::FromString(FString::Printf(TEXT("  Jog    %.1f  < %.0f cm/s"), A.MET_Jog, A.SpeedThreshold_Jog)));
	}
	if (ActivitySprint)
	{
		ActivitySprint->SetText(FText::FromString(FString::Printf(TEXT("  Sprint %.1f  >= %.0f cm/s"), A.MET_Sprint, A.SpeedThreshold_Jog)));
	}

	// Stamina right column static values
	if (MovementSprint)
	{
		MovementSprint->SetText(FText::FromString(FString::Printf(TEXT("Sprint  %.1f /s"), S.Movement.SprintDrainPerSec)));
	}
	if (MovementJog)
	{
		MovementJog->SetText(FText::FromString(FString::Printf(TEXT("Jog      %.1f /s"), S.Movement.JogDrainPerSec)));
	}
	if (MovementRegen)
	{
		MovementRegen->SetText(FText::FromString(FString::Printf(TEXT("Regen    %.2f /s"), S.Movement.BaseRegenPerSec)));
	}
	if (InteractionsRow1)
	{
		InteractionsRow1->SetText(FText::FromString(FString::Printf(TEXT("Light %.1f  Medium %.1f"), S.Interactions.Light, S.Interactions.Medium)));
	}
	if (InteractionsRow2)
	{
		InteractionsRow2->SetText(FText::FromString(FString::Printf(TEXT("Heavy %.1f Min Req %.1f"), S.Interactions.Heavy, S.Interactions.MinRequired)));
	}
	if (CombatRow1)
	{
		CombatRow1->SetText(FText::FromString(FString::Printf(TEXT("Light Atk %.1f  Heavy Atk %.1f"), S.Combat.LightAttack, S.Combat.HeavyAttack)));
	}
	if (CombatRow2)
	{
		CombatRow2->SetText(FText::FromString(FString::Printf(TEXT("Dodge %.1f      Block %.1f/s"), S.Combat.Dodge, S.Combat.BlockPerSec)));
	}
}

FLinearColor UW_VitalsPanel::GetStateColor(EVitalState State) const
{
	if (!CurrentTheme)
	{
		switch (State)
		{
		case EVitalState::OK:       return FLinearColor::Green;
		case EVitalState::Low:      return FLinearColor(1.f, 0.5f, 0.f);
		case EVitalState::Critical: return FLinearColor::Red;
		case EVitalState::Empty:    return FLinearColor::Red;
		default:                    return FLinearColor::White;
		}
	}

	switch (State)
	{
	case EVitalState::OK:       return CurrentTheme->Colors.Success;
	case EVitalState::Low:      return CurrentTheme->Colors.Warning;
	case EVitalState::Critical: return CurrentTheme->Colors.Error;
	case EVitalState::Empty:    return CurrentTheme->Colors.Error;
	default:                    return CurrentTheme->Colors.TextPrimary;
	}
}

FLinearColor UW_VitalsPanel::GetFatigueStateColor(EFatigueState State) const
{
	if (!CurrentTheme)
	{
		switch (State)
		{
		case EFatigueState::Rested:    return FLinearColor::Green;
		case EFatigueState::Tired:     return FLinearColor(1.f, 0.5f, 0.f);
		case EFatigueState::Exhausted: return FLinearColor::Red;
		case EFatigueState::Critical:  return FLinearColor::Red;
		default:                       return FLinearColor::White;
		}
	}

	switch (State)
	{
	case EFatigueState::Rested:    return CurrentTheme->Colors.Success;
	case EFatigueState::Tired:     return CurrentTheme->Colors.Warning;
	case EFatigueState::Exhausted: return CurrentTheme->Colors.Error;
	case EFatigueState::Critical:  return CurrentTheme->Colors.Error;
	default:                       return CurrentTheme->Colors.TextPrimary;
	}
}

FText UW_VitalsPanel::FormatConditionValue(float Current)
{
	return FText::FromString(FString::Printf(TEXT("%.0f HP"), Current));
}

FText UW_VitalsPanel::FormatStaminaValue(float Current)
{
	return FText::FromString(FString::Printf(TEXT("%.0f"), Current));
}

FText UW_VitalsPanel::FormatCaloriesValue(float Current)
{
	return FText::FromString(FString::Printf(TEXT("%.0f kcal"), Current));
}

FText UW_VitalsPanel::FormatHydrationValue(float Current)
{
	return FText::FromString(FString::Printf(TEXT("%.1f L"), Current));
}

FText UW_VitalsPanel::FormatFatigueValue(float Percent)
{
	return FText::FromString(FString::Printf(TEXT("%.0f%%"), Percent * 100.f));
}
