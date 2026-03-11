// Copyright ALIS. All Rights Reserved.

#include "MVVM/VitalsViewModel.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/ProjectTagTextSubsystem.h"
#include "VitalsConfig.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVitalsViewModel, Log, All);

// Notify-if-changed helper: assigns NewVal to Member and fires OnPropertyChanged if different
#define SET_VM_PROP(Member, NewVal) \
	if (Member != NewVal) { Member = NewVal; NotifyPropertyChanged(FName(TEXT(#Member))); }

void UVitalsViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);

	// Resolve ASC from various context types (ViewModel handles game entity access, not widgets)
	UAbilitySystemComponent* ASC = nullptr;

	if (UAbilitySystemComponent* DirectASC = Cast<UAbilitySystemComponent>(Context))
	{
		ASC = DirectASC;
		UE_LOG(LogVitalsViewModel, Log, TEXT("Initialize: ASC from direct context [%s]"), *GetNameSafe(ASC->GetOwner()));
	}
	else if (APawn* Pawn = Cast<APawn>(Context))
	{
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		UE_LOG(LogVitalsViewModel, Log, TEXT("Initialize: ASC from Pawn [%s]"), *GetNameSafe(Pawn));
	}
	else if (APlayerController* PC = Cast<APlayerController>(Context))
	{
		if (APawn* ControlledPawn = PC->GetPawn())
		{
			ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn);
			UE_LOG(LogVitalsViewModel, Log, TEXT("Initialize: ASC from PC->Pawn [%s]"), *GetNameSafe(ControlledPawn));
		}
	}
	else if (UUserWidget* Widget = Cast<UUserWidget>(Context))
	{
		if (APlayerController* OwningPC = Widget->GetOwningPlayer())
		{
			if (APawn* ControlledPawn = OwningPC->GetPawn())
			{
				ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn);
				UE_LOG(LogVitalsViewModel, Log, TEXT("Initialize: ASC from Widget->PC->Pawn [%s]"), *GetNameSafe(ControlledPawn));
			}
		}
	}

	if (ASC)
	{
		SetAbilitySystemComponent(ASC);
	}
	else
	{
		UE_LOG(LogVitalsViewModel, Warning, TEXT("Initialize: Could not resolve ASC from context [%s], call SetAbilitySystemComponent manually"), *GetNameSafe(Context));
	}
}

void UVitalsViewModel::Shutdown()
{
	UE_LOG(LogVitalsViewModel, Log, TEXT("Shutdown"));
	StopRefreshTimer();
	CachedASC.Reset();
	Super::Shutdown();
}

void UVitalsViewModel::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	StopRefreshTimer();
	CachedASC = InASC;

	if (InASC)
	{
		UE_LOG(LogVitalsViewModel, Verbose, TEXT("SetAbilitySystemComponent: Bound to [%s]"), *GetNameSafe(InASC->GetOwner()));
		RefreshFromASC();
		StartRefreshTimer();
	}
	else
	{
		UE_LOG(LogVitalsViewModel, Warning, TEXT("SetAbilitySystemComponent: Called with null ASC"));
	}
}

void UVitalsViewModel::StartRefreshTimer()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return;
	}

	if (UWorld* World = ASC->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			FTimerDelegate::CreateUObject(this, &UVitalsViewModel::RefreshFromASC),
			1.0f,
			true
		);
	}
}

void UVitalsViewModel::StopRefreshTimer()
{
	if (RefreshTimerHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedASC.Get())
		{
			if (UWorld* World = ASC->GetWorld())
			{
				World->GetTimerManager().ClearTimer(RefreshTimerHandle);
			}
		}
		RefreshTimerHandle.Invalidate();
	}
}

void UVitalsViewModel::RefreshFromASC()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		UE_LOG(LogVitalsViewModel, Warning, TEXT("RefreshFromASC: Cached ASC is stale/null, skipping refresh"));
		return;
	}

	// Condition
	if (const UHealthAttributeSet* HealthSet = ASC->GetSet<UHealthAttributeSet>())
	{
		float Current = HealthSet->GetCondition();
		float Max = HealthSet->GetMaxCondition();
		float Percent = (Max > 0.f) ? (Current / Max) : 0.f;
		EVitalState State = ReadVitalStateFromTags(ASC, ProjectTags::State_Condition, Percent);
		SET_VM_PROP(ConditionCurrent, Current);
		SET_VM_PROP(ConditionMax, Max);
		SET_VM_PROP(ConditionPercent, Percent);
		SET_VM_PROP(ConditionState, State);
	}

	// Stamina
	if (const UStaminaAttributeSet* StaminaSet = ASC->GetSet<UStaminaAttributeSet>())
	{
		float Current = StaminaSet->GetStamina();
		float Max = StaminaSet->GetMaxStamina();
		float Percent = (Max > 0.f) ? (Current / Max) : 0.f;
		EVitalState State = ReadVitalStateFromTags(ASC, ProjectTags::State_Stamina, Percent);
		SET_VM_PROP(StaminaCurrent, Current);
		SET_VM_PROP(StaminaMax, Max);
		SET_VM_PROP(StaminaPercent, Percent);
		SET_VM_PROP(StaminaState, State);
	}

	// Survival Vitals
	if (const USurvivalAttributeSet* SurvivalSet = ASC->GetSet<USurvivalAttributeSet>())
	{
		// Calories
		{
			float Current = SurvivalSet->GetCalories();
			float Max = SurvivalSet->GetMaxCalories();
			float Percent = (Max > 0.f) ? (Current / Max) : 0.f;
			EVitalState State = ReadVitalStateFromTags(ASC, ProjectTags::State_Calories, Percent);
			SET_VM_PROP(CaloriesCurrent, Current);
			SET_VM_PROP(CaloriesMax, Max);
			SET_VM_PROP(CaloriesPercent, Percent);
			SET_VM_PROP(CaloriesState, State);
		}
		// Hydration
		{
			float Current = SurvivalSet->GetHydration();
			float Max = SurvivalSet->GetMaxHydration();
			float Percent = (Max > 0.f) ? (Current / Max) : 0.f;
			EVitalState State = ReadVitalStateFromTags(ASC, ProjectTags::State_Hydration, Percent);
			SET_VM_PROP(HydrationCurrent, Current);
			SET_VM_PROP(HydrationMax, Max);
			SET_VM_PROP(HydrationPercent, Percent);
			SET_VM_PROP(HydrationState, State);
		}
		// Fatigue (inverted: 0=good, 100=bad)
		{
			float Current = SurvivalSet->GetFatigue();
			float Max = SurvivalSet->GetMaxFatigue();
			float Percent = (Max > 0.f) ? (Current / Max) : 0.f;
			EFatigueState State = ReadFatigueStateFromTags(ASC, Percent);
			SET_VM_PROP(FatigueCurrent, Current);
			SET_VM_PROP(FatigueMax, Max);
			SET_VM_PROP(FatiguePercent, Percent);
			SET_VM_PROP(FatigueState, State);
		}
	}

	// Status Effects
	if (const UStatusAttributeSet* StatusSet = ASC->GetSet<UStatusAttributeSet>())
	{
		float BleedVal = StatusSet->GetBleeding();
		float PoisonVal = StatusSet->GetPoisoned();
		float Radiation = StatusSet->GetRadiation();
		bool bBleeding = BleedVal > 0.f;
		bool bPoisoned = PoisonVal > 0.f;
		SET_VM_PROP(bIsBleeding, bBleeding);
		SET_VM_PROP(BleedingSeverity, BleedVal);
		SET_VM_PROP(bIsPoisoned, bPoisoned);
		SET_VM_PROP(PoisonedSeverity, PoisonVal);
		SET_VM_PROP(RadiationLevel, Radiation);

		bool bHasStatus = bBleeding || bPoisoned || (Radiation > 0.f);
		SET_VM_PROP(bHasStatusEffect, bHasStatus);
	}

	// =========================================================================
	// Update Warning Flags + Rate Texts
	// =========================================================================
	UpdateWarningFlags();
	UpdateRateTexts();
}

EVitalState UVitalsViewModel::ComputeVitalState(float Percent) const
{
	// Must match ProjectVitalsComponent thresholds
	if (Percent > THRESHOLD_OK)
	{
		return EVitalState::OK;
	}
	else if (Percent > THRESHOLD_LOW)
	{
		return EVitalState::Low;
	}
	else if (Percent > THRESHOLD_CRITICAL)
	{
		return EVitalState::Critical;
	}
	else
	{
		return EVitalState::Empty;
	}
}

EFatigueState UVitalsViewModel::ComputeFatigueState(float Percent) const
{
	// Fatigue is inverted: higher % = worse state
	if (Percent < FATIGUE_RESTED)
	{
		return EFatigueState::Rested;
	}
	else if (Percent < FATIGUE_TIRED)
	{
		return EFatigueState::Tired;
	}
	else if (Percent < FATIGUE_EXHAUSTED)
	{
		return EFatigueState::Exhausted;
	}
	else
	{
		return EFatigueState::Critical;
	}
}

EVitalState UVitalsViewModel::ReadVitalStateFromTags(UAbilitySystemComponent* ASC, const FGameplayTag& ParentTag, float Percent) const
{
	// SOT: Read state from gameplay tags (set by ProjectVitals with hysteresis)
	// Falls back to percent-based compute if tags are missing

	if (!ASC)
	{
		return ComputeVitalState(Percent);
	}

	// Check for child tags under the parent (e.g., State.Condition.OK)
	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	// Build child tag names to check
	FString ParentTagStr = ParentTag.ToString();

	FGameplayTag OKTag = FGameplayTag::RequestGameplayTag(FName(*(ParentTagStr + TEXT(".OK"))), false);
	FGameplayTag LowTag = FGameplayTag::RequestGameplayTag(FName(*(ParentTagStr + TEXT(".Low"))), false);
	FGameplayTag CriticalTag = FGameplayTag::RequestGameplayTag(FName(*(ParentTagStr + TEXT(".Critical"))), false);
	FGameplayTag EmptyTag = FGameplayTag::RequestGameplayTag(FName(*(ParentTagStr + TEXT(".Empty"))), false);

	if (OwnedTags.HasTag(EmptyTag))
	{
		return EVitalState::Empty;
	}
	if (OwnedTags.HasTag(CriticalTag))
	{
		return EVitalState::Critical;
	}
	if (OwnedTags.HasTag(LowTag))
	{
		return EVitalState::Low;
	}
	if (OwnedTags.HasTag(OKTag))
	{
		return EVitalState::OK;
	}

	// No state tags found - fallback to percent-based computation
	UE_LOG(LogVitalsViewModel, Verbose, TEXT("ReadVitalStateFromTags: No %s.* tags found, using percent fallback (%.1f%%)"), *ParentTagStr, Percent * 100.f);
	return ComputeVitalState(Percent);
}

EFatigueState UVitalsViewModel::ReadFatigueStateFromTags(UAbilitySystemComponent* ASC, float Percent) const
{
	// SOT: Read fatigue state from gameplay tags (set by ProjectVitals with hysteresis)
	// Falls back to percent-based compute if tags are missing

	if (!ASC)
	{
		return ComputeFatigueState(Percent);
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	if (OwnedTags.HasTag(ProjectTags::State_Fatigue_Critical))
	{
		return EFatigueState::Critical;
	}
	if (OwnedTags.HasTag(ProjectTags::State_Fatigue_Exhausted))
	{
		return EFatigueState::Exhausted;
	}
	if (OwnedTags.HasTag(ProjectTags::State_Fatigue_Tired))
	{
		return EFatigueState::Tired;
	}
	if (OwnedTags.HasTag(ProjectTags::State_Fatigue_Rested))
	{
		return EFatigueState::Rested;
	}

	// No state tags found - fallback to percent-based computation
	UE_LOG(LogVitalsViewModel, Verbose, TEXT("ReadFatigueStateFromTags: No State.Fatigue.* tags found, using percent fallback (%.1f%%)"), Percent * 100.f);
	return ComputeFatigueState(Percent);
}

void UVitalsViewModel::UpdateWarningFlags()
{
	// Check for any Low state
	bool bAnyLow =
		(ConditionState == EVitalState::Low) ||
		(StaminaState == EVitalState::Low) ||
		(CaloriesState == EVitalState::Low) ||
		(HydrationState == EVitalState::Low) ||
		(FatigueState == EFatigueState::Tired);

	// Check for any Critical/Empty state
	bool bAnyCritical =
		(ConditionState == EVitalState::Critical || ConditionState == EVitalState::Empty) ||
		(StaminaState == EVitalState::Critical || StaminaState == EVitalState::Empty) ||
		(CaloriesState == EVitalState::Critical || CaloriesState == EVitalState::Empty) ||
		(HydrationState == EVitalState::Critical || HydrationState == EVitalState::Empty) ||
		(FatigueState == EFatigueState::Exhausted || FatigueState == EFatigueState::Critical);

	SET_VM_PROP(bWarnAnyLow, bAnyLow);
	SET_VM_PROP(bWarnAnyCritical, bAnyCritical);
}

void UVitalsViewModel::UpdateRateTexts()
{
	const FVitalsConfig& Config = FVitalsConfigLoader::Get();
	const FVitalsConditionConfig& C = Config.Condition;
	const FVitalsMetabolism& M = C.Metabolism;

	// Helper: assign FText and notify if changed
	auto SetTextProp = [this](FText& Member, const FText& NewVal, const TCHAR* PropName)
	{
		if (!Member.EqualTo(NewVal))
		{
			Member = NewVal;
			NotifyPropertyChanged(FName(PropName));
		}
	};

	// Calories: WeightKg * MET_Idle kcal/hr (idle baseline)
	float CalDrainPerHr = M.CharacterWeightKg * M.Activity.MET_Idle;
	SetTextProp(CaloriesRateText,
		FText::FromString(FString::Printf(TEXT("-%.0f kcal/hr (Idle)"), CalDrainPerHr)),
		TEXT("CaloriesRateText"));

	// Hydration: tied to calorie burn
	float HydDrainPerHr = CalDrainPerHr * M.HydrationPerKcal;
	SetTextProp(HydrationRateText,
		FText::FromString(FString::Printf(TEXT("-%.2f L/hr (Idle)"), HydDrainPerHr)),
		TEXT("HydrationRateText"));

	// Fatigue: derived from hoursToEmpty
	float FatigueGainPerHour = (C.Survival.Fatigue.HoursToEmpty > 0.f)
		? (C.Survival.Fatigue.Max / C.Survival.Fatigue.HoursToEmpty) : 0.f;
	SetTextProp(FatigueRateText,
		FText::FromString(FString::Printf(TEXT("+%.1f%%/hr"), FatigueGainPerHour)),
		TEXT("FatigueRateText"));

	// Condition: depends on bleeding and critical needs
	float TotalConditionDrain = 0.f;
	if (HydrationPercent <= 0.f)
	{
		TotalConditionDrain += C.DehydrationDrainPerSec;
	}
	if (CaloriesPercent <= 0.f)
	{
		TotalConditionDrain += C.StarvationDrainPerSec;
	}
	if (FatiguePercent >= 0.95f)
	{
		TotalConditionDrain += C.ExhaustionDrainPerSec;
	}

	FText NewCondRate;
	if (bIsBleeding)
	{
		NewCondRate = FText::FromString(TEXT("Draining (bleeding)"));
	}
	else if (TotalConditionDrain > 0.f)
	{
		NewCondRate = FText::FromString(FString::Printf(TEXT("-%.6f HP/s"), TotalConditionDrain));
	}
	else
	{
		NewCondRate = FText::FromString(FString::Printf(TEXT("+%.6f HP/s"), C.BaseRegenPerSec));
	}
	SetTextProp(ConditionRateText, NewCondRate, TEXT("ConditionRateText"));

	// Stamina: summarize debuff state affecting regen
	bool bCalLow = (CaloriesState == EVitalState::Low);
	bool bFatigueHigh = (FatigueState == EFatigueState::Exhausted || FatigueState == EFatigueState::Critical);
	FText NewStaRate;
	if (bCalLow && bFatigueHigh) { NewStaRate = FText::FromString(TEXT("Regen: -50%")); }
	else if (bCalLow) { NewStaRate = FText::FromString(TEXT("Regen: -20%")); }
	else if (bFatigueHigh) { NewStaRate = FText::FromString(TEXT("Regen: -30%")); }
	else { NewStaRate = FText::FromString(TEXT("Regen: OK")); }
	SetTextProp(StaminaRateText, NewStaRate, TEXT("StaminaRateText"));
}

void UVitalsViewModel::TogglePanel()
{
	UpdatebPanelVisible(!bPanelVisible);
}

void UVitalsViewModel::ShowPanel()
{
	UpdatebPanelVisible(true);
}

void UVitalsViewModel::HidePanel()
{
	UpdatebPanelVisible(false);
}

TArray<FText> UVitalsViewModel::GetActiveStateTexts() const
{
	TArray<FText> Result;

	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return Result;
	}

	// Get the TagText subsystem via the ASC's outer (typically the owning actor's world)
	UGameInstance* GI = nullptr;
	if (UWorld* World = ASC->GetWorld())
	{
		GI = World->GetGameInstance();
	}
	if (!GI)
	{
		return Result;
	}

	UProjectTagTextSubsystem* TagTextSub = GI->GetSubsystem<UProjectTagTextSubsystem>();
	if (!TagTextSub)
	{
		return Result;
	}

	// Get all owned tags and filter to State.* tags with registered text
	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	Result = TagTextSub->GetTextsForTags(OwnedTags, ProjectTags::State);

	return Result;
}
