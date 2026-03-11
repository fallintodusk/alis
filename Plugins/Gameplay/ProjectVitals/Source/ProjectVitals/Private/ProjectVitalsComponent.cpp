// Copyright ALIS. All Rights Reserved.

#include "ProjectVitalsComponent.h"
#include "VitalsConfig.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "ProjectGameplayTags.h"
#include "ProjectGASLibrary.h"
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"
#include "Effects/GE_ThresholdDebuffs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UProjectVitalsComponent::UProjectVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

#if WITH_DEV_AUTOMATION_TESTS
EVitalState UProjectVitalsComponent::TestComputeVitalStateWithHysteresis(float Percent, EVitalState PrevState) const
{
	return ComputeVitalStateWithHysteresis(Percent, PrevState);
}

EFatigueState UProjectVitalsComponent::TestComputeFatigueStateWithHysteresis(float Percent, EFatigueState PrevState) const
{
	return ComputeFatigueStateWithHysteresis(Percent, PrevState);
}

void UProjectVitalsComponent::TestTickVitalsOnce()
{
	TickVitals();
}

void UProjectVitalsComponent::TestSeedDebuffHandles()
{
	DebuffHandle_HydrationLow = FActiveGameplayEffectHandle(101);
	DebuffHandle_HydrationCritical = FActiveGameplayEffectHandle(102);
	DebuffHandle_CaloriesLow = FActiveGameplayEffectHandle(103);
	DebuffHandle_CaloriesCritical = FActiveGameplayEffectHandle(104);
	DebuffHandle_FatigueHigh = FActiveGameplayEffectHandle(105);
	DebuffHandle_WeightHeavy = FActiveGameplayEffectHandle(106);
	DebuffHandle_WeightOverweight = FActiveGameplayEffectHandle(107);
}

void UProjectVitalsComponent::TestClearDebuffsWithoutASC()
{
	CachedASC.Reset();
	RemoveAllDebuffs();
}

bool UProjectVitalsComponent::TestAreAllDebuffHandlesInvalid() const
{
	return
		!DebuffHandle_HydrationLow.IsValid() &&
		!DebuffHandle_HydrationCritical.IsValid() &&
		!DebuffHandle_CaloriesLow.IsValid() &&
		!DebuffHandle_CaloriesCritical.IsValid() &&
		!DebuffHandle_FatigueHigh.IsValid() &&
		!DebuffHandle_WeightHeavy.IsValid() &&
		!DebuffHandle_WeightOverweight.IsValid();
}
#endif

void UProjectVitalsComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UProjectVitalsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	Super::EndPlay(EndPlayReason);
}

void UProjectVitalsComponent::Start()
{
	if (bIsRunning)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Server-only: only tick on authority
	if (!Owner->HasAuthority())
	{
		return;
	}

	// Cache ASC reference
	CachedASC = GetASC();
	if (!CachedASC.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectVitalsComponent::Start - No ASC found on %s"), *Owner->GetName());
		return;
	}

	// Load JSON config: apply metabolism tuning + initial attributes
	ApplyConfig();

	// Start periodic timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VitalsTickTimerHandle,
			this,
			&UProjectVitalsComponent::TickVitals,
			TickInterval,
			true // bLoop
		);
		bIsRunning = true;

		// Initial state update
		TickVitals();
	}
}

void UProjectVitalsComponent::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VitalsTickTimerHandle);
	}

	// Remove any active debuffs to prevent leaking effects after component stops
	RemoveAllDebuffs();

	bIsRunning = false;
	CachedASC.Reset();
}

UAbilitySystemComponent* UProjectVitalsComponent::GetASC() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Try IAbilitySystemInterface first
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
	{
		return ASI->GetAbilitySystemComponent();
	}

	// Fallback: direct component search
	return Owner->FindComponentByClass<UAbilitySystemComponent>();
}

void UProjectVitalsComponent::ApplyConfig()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const FVitalsConfig& Cfg = FVitalsConfigLoader::Get();
	const FVitalsConditionConfig& C = Cfg.Condition;
	const FVitalsMetabolism& M = C.Metabolism;
	const FVitalsActivity& A = M.Activity;

	// Apply metabolism tuning to component properties
	CharacterWeightKg = M.CharacterWeightKg;
	HydrationPerKcal = M.HydrationPerKcal;
	MET_Idle = A.MET_Idle;
	MET_Walk = A.MET_Walk;
	MET_Jog = A.MET_Jog;
	MET_Sprint = A.MET_Sprint;
	SpeedThreshold_Idle = A.SpeedThreshold_Idle;
	SpeedThreshold_Walk = A.SpeedThreshold_Walk;
	SpeedThreshold_Jog = A.SpeedThreshold_Jog;

	// Derived rates from timeline config
	BaseConditionRegenPerSec = C.BaseRegenPerSec;
	DehydrationDrainPerSec = C.DehydrationDrainPerSec;
	StarvationDrainPerSec = C.StarvationDrainPerSec;
	ExhaustionDrainPerSec = C.ExhaustionDrainPerSec;

	// Fatigue gain derived from hoursToEmpty
	FatigueGainPerHour = (C.Survival.Fatigue.HoursToEmpty > 0.f)
		? (C.Survival.Fatigue.Max / C.Survival.Fatigue.HoursToEmpty) : 6.25f;

	// Apply initial attribute values (server-only)
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		ASC = GetASC();
		CachedASC = ASC;
	}
	if (!ASC)
	{
		return;
	}

	// Set max values FIRST so current values clamp properly
	ASC->SetNumericAttributeBase(UHealthAttributeSet::GetMaxConditionAttribute(), C.Max);
	ASC->SetNumericAttributeBase(UStaminaAttributeSet::GetMaxStaminaAttribute(), Cfg.Stamina.Max);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxCaloriesAttribute(), C.Survival.Calories.Max);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxHydrationAttribute(), C.Survival.Hydration.Max);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxFatigueAttribute(), C.Survival.Fatigue.Max);

	// Set current values
	ASC->SetNumericAttributeBase(UHealthAttributeSet::GetConditionAttribute(), C.Current);
	ASC->SetNumericAttributeBase(UHealthAttributeSet::GetConditionRegenRateAttribute(), C.RegenRate);
	ASC->SetNumericAttributeBase(UStaminaAttributeSet::GetStaminaAttribute(), Cfg.Stamina.Current);
	ASC->SetNumericAttributeBase(UStaminaAttributeSet::GetStaminaRegenRateAttribute(), Cfg.Stamina.RegenRate);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetCaloriesAttribute(), C.Survival.Calories.Current);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetHydrationAttribute(), C.Survival.Hydration.Current);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetFatigueAttribute(), C.Survival.Fatigue.Current);
	ASC->SetNumericAttributeBase(UStatusAttributeSet::GetBleedingAttribute(), C.Status.Bleeding.Current);
	ASC->SetNumericAttributeBase(UStatusAttributeSet::GetPoisonedAttribute(), C.Status.Poisoned.Current);
	ASC->SetNumericAttributeBase(UStatusAttributeSet::GetRadiationAttribute(), C.Status.Radiation.Current);
}

void UProjectVitalsComponent::TickVitals()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		// Try to re-acquire
		CachedASC = GetASC();
		ASC = CachedASC.Get();
		if (!ASC)
		{
			return;
		}
	}

	// === Phase 4: Metabolism calculations ===
	ApplyMetabolism(ASC, TickInterval);

	// === Phase 4: Bleeding condition drain ===
	ApplyBleedingDrain(ASC, TickInterval);

	// === Phase 5: Update State.* tags based on thresholds (with hysteresis) ===
	UpdateStateTags(ASC);

	// === Phase 6: Apply/remove threshold debuffs (modify multipliers only) ===
	UpdateThresholdDebuffs(ASC);

	// === Phase 7: Condition regen/drain (inline computation, single source of truth) ===
	ApplyConditionDelta(ASC, TickInterval);
}

void UProjectVitalsComponent::UpdateStateTags(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	// === Condition (with hysteresis) ===
	if (const UHealthAttributeSet* HealthSet = ASC->GetSet<UHealthAttributeSet>())
	{
		float Condition = HealthSet->GetCondition();
		float MaxCondition = HealthSet->GetMaxCondition();
		float Percent = (MaxCondition > 0.f) ? (Condition / MaxCondition) : 0.f;
		EVitalState State = ComputeVitalStateWithHysteresis(Percent, PrevConditionState);

		FGameplayTag NewTag;
		switch (State)
		{
		case EVitalState::OK:       NewTag = ProjectTags::State_Condition_OK; break;
		case EVitalState::Low:      NewTag = ProjectTags::State_Condition_Low; break;
		case EVitalState::Critical: NewTag = ProjectTags::State_Condition_Critical; break;
		case EVitalState::Empty:    NewTag = ProjectTags::State_Condition_Empty; break;
		}
		SetStateTag(ASC, NewTag, ProjectTags::State_Condition);
		PrevConditionState = State;
	}

	// === Stamina (with hysteresis) ===
	if (const UStaminaAttributeSet* StaminaSet = ASC->GetSet<UStaminaAttributeSet>())
	{
		float Stamina = StaminaSet->GetStamina();
		float MaxStamina = StaminaSet->GetMaxStamina();
		float Percent = (MaxStamina > 0.f) ? (Stamina / MaxStamina) : 0.f;
		EVitalState State = ComputeVitalStateWithHysteresis(Percent, PrevStaminaState);

		FGameplayTag NewTag;
		switch (State)
		{
		case EVitalState::OK:       NewTag = ProjectTags::State_Stamina_OK; break;
		case EVitalState::Low:      NewTag = ProjectTags::State_Stamina_Low; break;
		case EVitalState::Critical: NewTag = ProjectTags::State_Stamina_Critical; break;
		case EVitalState::Empty:    NewTag = ProjectTags::State_Stamina_Empty; break;
		}
		SetStateTag(ASC, NewTag, ProjectTags::State_Stamina);
		PrevStaminaState = State;
	}

	// === Survival Attributes (Calories, Hydration, Fatigue) with hysteresis ===
	if (const USurvivalAttributeSet* SurvivalSet = ASC->GetSet<USurvivalAttributeSet>())
	{
		// Calories
		{
			float Calories = SurvivalSet->GetCalories();
			float MaxCalories = SurvivalSet->GetMaxCalories();
			float Percent = (MaxCalories > 0.f) ? (Calories / MaxCalories) : 0.f;
			EVitalState State = ComputeVitalStateWithHysteresis(Percent, PrevCaloriesState);

			FGameplayTag NewTag;
			switch (State)
			{
			case EVitalState::OK:       NewTag = ProjectTags::State_Calories_OK; break;
			case EVitalState::Low:      NewTag = ProjectTags::State_Calories_Low; break;
			case EVitalState::Critical: NewTag = ProjectTags::State_Calories_Critical; break;
			case EVitalState::Empty:    NewTag = ProjectTags::State_Calories_Empty; break;
			}
			SetStateTag(ASC, NewTag, ProjectTags::State_Calories);
			PrevCaloriesState = State;
		}

		// Hydration
		{
			float Hydration = SurvivalSet->GetHydration();
			float MaxHydration = SurvivalSet->GetMaxHydration();
			float Percent = (MaxHydration > 0.f) ? (Hydration / MaxHydration) : 0.f;
			EVitalState State = ComputeVitalStateWithHysteresis(Percent, PrevHydrationState);

			FGameplayTag NewTag;
			switch (State)
			{
			case EVitalState::OK:       NewTag = ProjectTags::State_Hydration_OK; break;
			case EVitalState::Low:      NewTag = ProjectTags::State_Hydration_Low; break;
			case EVitalState::Critical: NewTag = ProjectTags::State_Hydration_Critical; break;
			case EVitalState::Empty:    NewTag = ProjectTags::State_Hydration_Empty; break;
			}
			SetStateTag(ASC, NewTag, ProjectTags::State_Hydration);
			PrevHydrationState = State;
		}

		// Fatigue (inverted: 0=good, 100=bad)
		{
			float Fatigue = SurvivalSet->GetFatigue();
			float MaxFatigue = SurvivalSet->GetMaxFatigue();
			float Percent = (MaxFatigue > 0.f) ? (Fatigue / MaxFatigue) : 0.f;
			EFatigueState State = ComputeFatigueStateWithHysteresis(Percent, PrevFatigueState);

			FGameplayTag NewTag;
			switch (State)
			{
			case EFatigueState::Rested:    NewTag = ProjectTags::State_Fatigue_Rested; break;
			case EFatigueState::Tired:     NewTag = ProjectTags::State_Fatigue_Tired; break;
			case EFatigueState::Exhausted: NewTag = ProjectTags::State_Fatigue_Exhausted; break;
			case EFatigueState::Critical:  NewTag = ProjectTags::State_Fatigue_Critical; break;
			}
			SetStateTag(ASC, NewTag, ProjectTags::State_Fatigue);
			PrevFatigueState = State;
		}
	}
}

EVitalState UProjectVitalsComponent::ComputeVitalStateWithHysteresis(float Percent, EVitalState PrevState) const
{
	// Hysteresis: use different thresholds for entry vs exit
	// Entry: cross INTO a state at the threshold
	// Exit: cross OUT of a state at threshold - HYSTERESIS_BUFFER
	// This prevents flickering when values hover near boundaries
	//
	// IMPORTANT: Iterate until stable to handle big jumps (e.g., OK -> Empty in one tick)
	// Max 3 iterations for 4 states

	const float ExitOK = THRESHOLD_OK - HYSTERESIS_BUFFER;       // 68% to leave OK
	const float ExitLow = THRESHOLD_LOW - HYSTERESIS_BUFFER;     // 38% to leave Low
	const float ExitCritical = THRESHOLD_CRITICAL - HYSTERESIS_BUFFER; // 18% to leave Critical

	EVitalState State = PrevState;

	for (int32 i = 0; i < 3; ++i)
	{
		EVitalState NewState = State;

		switch (State)
		{
		case EVitalState::OK:
			NewState = (Percent > ExitOK) ? EVitalState::OK : EVitalState::Low;
			break;
		case EVitalState::Low:
			if (Percent > THRESHOLD_OK) NewState = EVitalState::OK;
			else if (Percent <= ExitLow) NewState = EVitalState::Critical;
			break;
		case EVitalState::Critical:
			if (Percent > THRESHOLD_LOW) NewState = EVitalState::Low;
			else if (Percent <= ExitCritical) NewState = EVitalState::Empty;
			break;
		case EVitalState::Empty:
			if (Percent > THRESHOLD_CRITICAL) NewState = EVitalState::Critical;
			break;
		}

		if (NewState == State)
		{
			break; // Stable
		}
		State = NewState;
	}

	return State;
}

EFatigueState UProjectVitalsComponent::ComputeFatigueStateWithHysteresis(float Percent, EFatigueState PrevState) const
{
	// Fatigue is inverted: higher % = worse state
	// Entry thresholds: cross INTO state at threshold
	// Exit thresholds: cross OUT at threshold + HYSTERESIS_BUFFER (since inverted)
	//
	// IMPORTANT: Iterate until stable to handle big jumps (e.g., Rested -> Critical in one tick)
	// Max 3 iterations for 4 states

	const float ExitRested = FATIGUE_RESTED + HYSTERESIS_BUFFER;     // 32% to leave Rested
	const float ExitTired = FATIGUE_TIRED + HYSTERESIS_BUFFER;       // 62% to leave Tired
	const float ExitExhausted = FATIGUE_EXHAUSTED + HYSTERESIS_BUFFER; // 87% to leave Exhausted

	EFatigueState State = PrevState;

	for (int32 i = 0; i < 3; ++i)
	{
		EFatigueState NewState = State;

		switch (State)
		{
		case EFatigueState::Rested:
			NewState = (Percent < ExitRested) ? EFatigueState::Rested : EFatigueState::Tired;
			break;
		case EFatigueState::Tired:
			if (Percent < FATIGUE_RESTED) NewState = EFatigueState::Rested;
			else if (Percent >= ExitTired) NewState = EFatigueState::Exhausted;
			break;
		case EFatigueState::Exhausted:
			if (Percent < FATIGUE_TIRED) NewState = EFatigueState::Tired;
			else if (Percent >= ExitExhausted) NewState = EFatigueState::Critical;
			break;
		case EFatigueState::Critical:
			if (Percent < FATIGUE_EXHAUSTED) NewState = EFatigueState::Exhausted;
			break;
		}

		if (NewState == State)
		{
			break; // Stable
		}
		State = NewState;
	}

	return State;
}

void UProjectVitalsComponent::SetStateTag(UAbilitySystemComponent* ASC, const FGameplayTag& NewTag, const FGameplayTag& ParentTag)
{
	if (!ASC || !NewTag.IsValid() || !ParentTag.IsValid())
	{
		return;
	}

	// Get current tags matching parent (e.g., all State.Condition.* tags)
	FGameplayTagContainer CurrentTags;
	ASC->GetOwnedGameplayTags(CurrentTags);

	// Remove any existing child tags of the parent
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (Tag.MatchesTag(ParentTag) && Tag != ParentTag && Tag != NewTag)
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}

	// Add the new tag if not already present
	if (!ASC->HasMatchingGameplayTag(NewTag))
	{
		ASC->AddLooseGameplayTag(NewTag);
	}
}

// -------------------------------------------------------------------------
// Metabolism Calculations
// -------------------------------------------------------------------------

float UProjectVitalsComponent::GetCurrentMET() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return MET_Idle;
	}

	// Try to get movement component from character
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (const UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			// Use Size2D to avoid jump/fall inflating drain
			// Fixed thresholds ensure consistent metabolism regardless of sprint/crouch state
			float Speed = MovementComp->Velocity.Size2D();

			if (Speed < SpeedThreshold_Idle)
			{
				return MET_Idle;
			}
			else if (Speed < SpeedThreshold_Walk)
			{
				return MET_Walk;
			}
			else if (Speed < SpeedThreshold_Jog)
			{
				return MET_Jog;
			}
			else
			{
				return MET_Sprint;
			}
		}
	}

	return MET_Idle;
}

void UProjectVitalsComponent::ApplyMetabolism(UAbilitySystemComponent* ASC, float DeltaTime)
{
	if (!ASC)
	{
		return;
	}

	// Calculate calorie burn based on MET
	// Formula: kcal/hr = WeightKg * MET
	// For DeltaTime: kcal = (WeightKg * MET * DeltaTime) / 3600
	float CurrentMET = GetCurrentMET();
	float KcalPerSecond = (CharacterWeightKg * CurrentMET) / 3600.f;
	float CaloriesDrain = KcalPerSecond * DeltaTime;

	// Calculate hydration drain (tied to calorie burn)
	// HydrationPerKcal is L per kcal
	float HydrationDrain = CaloriesDrain * HydrationPerKcal;

	// Calculate fatigue gain
	// FatigueGainPerHour is % per hour
	float FatigueGain = (FatigueGainPerHour / 3600.f) * DeltaTime;

	// Apply drains via GE_GenericInstant using ApplyMagnitudes
	TArray<FAttributeMagnitude> Magnitudes;

	if (CaloriesDrain > 0.f)
	{
		Magnitudes.Add(FAttributeMagnitude(ProjectTags::SetByCaller_Calories, -CaloriesDrain));
	}

	if (HydrationDrain > 0.f)
	{
		Magnitudes.Add(FAttributeMagnitude(ProjectTags::SetByCaller_Hydration, -HydrationDrain));
	}

	if (FatigueGain > 0.f)
	{
		Magnitudes.Add(FAttributeMagnitude(ProjectTags::SetByCaller_Fatigue, FatigueGain));
	}

	if (Magnitudes.Num() > 0)
	{
		UProjectGASLibrary::ApplyMagnitudes(ASC, Magnitudes);
	}
}

void UProjectVitalsComponent::ApplyBleedingDrain(UAbilitySystemComponent* ASC, float DeltaTime)
{
	if (!ASC)
	{
		return;
	}

	// Check for bleeding severity from StatusAttributeSet
	const UStatusAttributeSet* StatusSet = ASC->GetSet<UStatusAttributeSet>();
	if (!StatusSet)
	{
		return;
	}

	float BleedingSeverity = StatusSet->GetBleeding();
	if (BleedingSeverity <= 0.f)
	{
		return;
	}

	// Drain condition based on bleeding severity
	// BleedingDrainPerSecond is % per second per severity level
	float ConditionDrain = BleedingSeverity * BleedingDrainPerSecond * DeltaTime;

	if (ConditionDrain > 0.f)
	{
		UProjectGASLibrary::ApplySingleMagnitude(
			ASC,
			ProjectTags::SetByCaller_Condition,
			-ConditionDrain
		);
	}
}

// -------------------------------------------------------------------------
// Threshold Debuffs
// -------------------------------------------------------------------------

void UProjectVitalsComponent::UpdateThresholdDebuffs(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	// Use the states already computed by UpdateStateTags (with hysteresis)
	// UpdateStateTags runs before this in TickVitals, so PrevCaloriesState etc. are current
	EVitalState CaloriesState = PrevCaloriesState;
	EVitalState HydrationState = PrevHydrationState;
	EFatigueState FatigueState = PrevFatigueState;

	// === Hydration debuffs ===
	// HydrationLow: MaxStamina -15% when Low
	ApplyOrRemoveDebuff(ASC, DebuffHandle_HydrationLow,
		UGE_ThresholdDebuff_HydrationLow::StaticClass(),
		HydrationState == EVitalState::Low);

	// HydrationCritical: ConditionRegenRate = 0 when Critical or Empty
	ApplyOrRemoveDebuff(ASC, DebuffHandle_HydrationCritical,
		UGE_ThresholdDebuff_HydrationCritical::StaticClass(),
		HydrationState == EVitalState::Critical || HydrationState == EVitalState::Empty);

	// === Calories debuffs ===
	// CaloriesLow: StaminaRegenRate -20% when Low
	ApplyOrRemoveDebuff(ASC, DebuffHandle_CaloriesLow,
		UGE_ThresholdDebuff_CaloriesLow::StaticClass(),
		CaloriesState == EVitalState::Low);

	// CaloriesCritical: ConditionRegenRate = 0 when Critical or Empty
	ApplyOrRemoveDebuff(ASC, DebuffHandle_CaloriesCritical,
		UGE_ThresholdDebuff_CaloriesCritical::StaticClass(),
		CaloriesState == EVitalState::Critical || CaloriesState == EVitalState::Empty);

	// === Fatigue debuffs ===
	// FatigueHigh: StaminaRegenRate -30% when Exhausted or Critical
	ApplyOrRemoveDebuff(ASC, DebuffHandle_FatigueHigh,
		UGE_ThresholdDebuff_FatigueHigh::StaticClass(),
		FatigueState == EFatigueState::Exhausted || FatigueState == EFatigueState::Critical);

	// === Weight encumbrance debuffs ===
	// Tags set by InventoryComponent (State.Weight.Heavy, State.Weight.Overweight)
	const bool bIsHeavy = ASC->HasMatchingGameplayTag(ProjectTags::State_Weight_Heavy);
	const bool bIsOverweight = ASC->HasMatchingGameplayTag(ProjectTags::State_Weight_Overweight);

	// WeightHeavy: StaminaRegenRate -15% when Heavy (80-100% capacity)
	ApplyOrRemoveDebuff(ASC, DebuffHandle_WeightHeavy,
		UGE_ThresholdDebuff_WeightHeavy::StaticClass(),
		bIsHeavy);

	// WeightOverweight: StaminaRegenRate -30% when Overweight (>100% capacity)
	ApplyOrRemoveDebuff(ASC, DebuffHandle_WeightOverweight,
		UGE_ThresholdDebuff_WeightOverweight::StaticClass(),
		bIsOverweight);

	// NOTE: Condition drain when needs are critical is now computed inline in ApplyConditionDelta()
	// This avoids periodic GE "evaluation gotchas" and keeps TickVitals as single source of truth
}

void UProjectVitalsComponent::ApplyOrRemoveDebuff(
	UAbilitySystemComponent* ASC,
	FActiveGameplayEffectHandle& Handle,
	TSubclassOf<UGameplayEffect> EffectClass,
	bool bShouldBeActive)
{
	if (!ASC || !EffectClass)
	{
		return;
	}

	bool bIsCurrentlyActive = Handle.IsValid();

	if (bShouldBeActive && !bIsCurrentlyActive)
	{
		// Apply the debuff
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1, Context);
		if (Spec.IsValid())
		{
			Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	else if (!bShouldBeActive && bIsCurrentlyActive)
	{
		// Remove the debuff
		ASC->RemoveActiveGameplayEffect(Handle);
		Handle.Invalidate();
	}
}

void UProjectVitalsComponent::RemoveAllDebuffs()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		// Just invalidate handles if ASC is gone
		DebuffHandle_HydrationLow.Invalidate();
		DebuffHandle_HydrationCritical.Invalidate();
		DebuffHandle_CaloriesLow.Invalidate();
		DebuffHandle_CaloriesCritical.Invalidate();
		DebuffHandle_FatigueHigh.Invalidate();
		DebuffHandle_WeightHeavy.Invalidate();
		DebuffHandle_WeightOverweight.Invalidate();
		return;
	}

	// Remove each active debuff (these only modify multipliers)
	if (DebuffHandle_HydrationLow.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_HydrationLow);
		DebuffHandle_HydrationLow.Invalidate();
	}
	if (DebuffHandle_HydrationCritical.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_HydrationCritical);
		DebuffHandle_HydrationCritical.Invalidate();
	}
	if (DebuffHandle_CaloriesLow.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_CaloriesLow);
		DebuffHandle_CaloriesLow.Invalidate();
	}
	if (DebuffHandle_CaloriesCritical.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_CaloriesCritical);
		DebuffHandle_CaloriesCritical.Invalidate();
	}
	if (DebuffHandle_FatigueHigh.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_FatigueHigh);
		DebuffHandle_FatigueHigh.Invalidate();
	}
	if (DebuffHandle_WeightHeavy.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_WeightHeavy);
		DebuffHandle_WeightHeavy.Invalidate();
	}
	if (DebuffHandle_WeightOverweight.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DebuffHandle_WeightOverweight);
		DebuffHandle_WeightOverweight.Invalidate();
	}
}

// -------------------------------------------------------------------------
// Condition Regen/Drain (inline computation, single source of truth)
// -------------------------------------------------------------------------

void UProjectVitalsComponent::ApplyConditionDelta(UAbilitySystemComponent* ASC, float DeltaTime)
{
	if (!ASC)
	{
		return;
	}

	const UHealthAttributeSet* HealthSet = ASC->GetSet<UHealthAttributeSet>();
	if (!HealthSet)
	{
		return;
	}

	float Condition = HealthSet->GetCondition();
	float MaxCondition = HealthSet->GetMaxCondition();

	// === Compute Drain (per-vital, additive stacking) ===
	float Drain = 0.f;
	bool bAnyCritical = false;

	if (const USurvivalAttributeSet* SurvivalSet = ASC->GetSet<USurvivalAttributeSet>())
	{
		// Hydration drain (2 days TOTAL: 1 day to drain + 1 day Condition drain)
		float HydrationPercent = (SurvivalSet->GetMaxHydration() > 0.f)
			? (SurvivalSet->GetHydration() / SurvivalSet->GetMaxHydration()) : 0.f;
		if (HydrationPercent <= 0.0f)
		{
			Drain += DehydrationDrainPerSec * DeltaTime;
			bAnyCritical = true;
		}

		// Calorie drain (14 days TOTAL: 1 day to drain + 13 days Condition drain)
		float CaloriesPercent = (SurvivalSet->GetMaxCalories() > 0.f)
			? (SurvivalSet->GetCalories() / SurvivalSet->GetMaxCalories()) : 0.f;
		if (CaloriesPercent <= 0.0f)
		{
			Drain += StarvationDrainPerSec * DeltaTime;
			bAnyCritical = true;
		}

		// Fatigue drain (5 days TOTAL: 1 day to drain + 4 days Condition drain)
		float FatiguePercent = (SurvivalSet->GetMaxFatigue() > 0.f)
			? (SurvivalSet->GetFatigue() / SurvivalSet->GetMaxFatigue()) : 0.f;
		if (FatiguePercent >= 0.95f)
		{
			Drain += ExhaustionDrainPerSec * DeltaTime;
			bAnyCritical = true;
		}
	}

	// === Compute Regen (only when all vitals above critical) ===
	float Regen = 0.f;

	if (!bAnyCritical && Condition < MaxCondition)
	{
		// Gate 1: No bleeding
		bool bCanRegen = true;
		if (const UStatusAttributeSet* StatusSet = ASC->GetSet<UStatusAttributeSet>())
		{
			if (StatusSet->GetBleeding() > 0.f)
			{
				bCanRegen = false;
			}
		}

		// Gate 2: ConditionRegenRate > 0 (set to 0 by threshold debuffs if still active)
		float RegenRate = HealthSet->GetConditionRegenRate();
		if (bCanRegen && RegenRate > 0.f)
		{
			Regen = BaseConditionRegenPerSec * RegenRate * DeltaTime;
		}
	}

	// === Apply net delta ===
	float Delta = Regen - Drain;
	if (FMath::Abs(Delta) > KINDA_SMALL_NUMBER)
	{
		UProjectGASLibrary::ApplySingleMagnitude(
			ASC,
			ProjectTags::SetByCaller_Condition,
			Delta
		);
	}
}
