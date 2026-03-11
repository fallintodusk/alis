// Copyright ALIS. All Rights Reserved.

#include "Services/DefaultInventoryMindThoughtSource.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "ProjectGameplayTags.h"

namespace
{
FMindThoughtCandidate MakeInventoryCandidate(
	const FName ThoughtId,
	const FText& ThoughtText,
	const int32 Priority,
	const EMindThoughtChannel Channel,
	const double CooldownSec,
	const double DedupeWindowSec,
	const FName DedupeKey)
{
	FMindThoughtCandidate Candidate;
	Candidate.ThoughtId = ThoughtId;
	Candidate.ThoughtText = ThoughtText;
	Candidate.Priority = Priority;
	Candidate.Channel = Channel;
	Candidate.SourceType = EMindThoughtSourceType::Inventory;
	Candidate.CooldownSec = CooldownSec;
	Candidate.DedupeWindowSec = DedupeWindowSec;
	Candidate.DedupeKey = DedupeKey;
	Candidate.TimeToLiveSec = 7.0f;
	return Candidate;
}
}

FDefaultInventoryMindThoughtSource::~FDefaultInventoryMindThoughtSource()
{
	ClearInventoryBinding();
}

void FDefaultInventoryMindThoughtSource::CollectThoughts(
	const FMindThoughtSourceContext& Context,
	TArray<FMindThoughtCandidate>& OutThoughts) const
{
	APawn* ContextPawn = Context.LocalPlayerPawn.Get();
	SyncInventoryBinding(ContextPawn);
	if (!ContextPawn)
	{
		return;
	}

	IInventoryReadOnly* InventoryReadOnly = ResolveInventoryReadOnly(ContextPawn);
	if (!InventoryReadOnly)
	{
		return;
	}

	const UAbilitySystemComponent* ASC = ContextPawn->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC)
	{
		return;
	}

	bool bHydrationCriticalNeed = false;
	const bool bNeedsHydration = HasVitalNeed(
		ASC,
		ProjectTags::State_Hydration_Low,
		ProjectTags::State_Hydration_Critical,
		ProjectTags::State_Hydration_Empty,
		bHydrationCriticalNeed);

	bool bCaloriesCriticalNeed = false;
	const bool bNeedsCalories = HasVitalNeed(
		ASC,
		ProjectTags::State_Calories_Low,
		ProjectTags::State_Calories_Critical,
		ProjectTags::State_Calories_Empty,
		bCaloriesCriticalNeed);

	bool bHasHydrationSupply = false;
	bool bHasCaloriesSupply = false;

	TArray<FInventoryEntryView> Entries;
	InventoryReadOnly->GetEntriesView(Entries);

	for (const FInventoryEntryView& Entry : Entries)
	{
		if (Entry.Quantity <= 0 || !Entry.bCanUse)
		{
			continue;
		}

		bHasHydrationSupply = bHasHydrationSupply || EntryHasPositiveMagnitude(Entry, ProjectTags::SetByCaller_Hydration);
		bHasCaloriesSupply = bHasCaloriesSupply || EntryHasPositiveMagnitude(Entry, ProjectTags::SetByCaller_Calories);

		if (bHasHydrationSupply && bHasCaloriesSupply)
		{
			break;
		}
	}

	if (bNeedsHydration && bHasHydrationSupply)
	{
		OutThoughts.Add(MakeInventoryCandidate(
			bHydrationCriticalNeed ? FName(TEXT("Mind.Inventory.UseHydrationNow")) : FName(TEXT("Mind.Inventory.UseHydrationSoon")),
			bHydrationCriticalNeed
				? NSLOCTEXT("ProjectMind", "InventoryHydrationNow", "I have water in my inventory. I should drink now.")
				: NSLOCTEXT("ProjectMind", "InventoryHydrationSoon", "I have water in my inventory. I should drink soon."),
			bHydrationCriticalNeed ? 230 : 190,
			bHydrationCriticalNeed ? EMindThoughtChannel::ToastAndJournal : EMindThoughtChannel::Toast,
			bHydrationCriticalNeed ? 18.0 : 24.0,
			12.0,
			FName(TEXT("Mind.Inventory.HydrationSupply"))));
	}

	if (bNeedsCalories && bHasCaloriesSupply)
	{
		OutThoughts.Add(MakeInventoryCandidate(
			bCaloriesCriticalNeed ? FName(TEXT("Mind.Inventory.UseFoodNow")) : FName(TEXT("Mind.Inventory.UseFoodSoon")),
			bCaloriesCriticalNeed
				? NSLOCTEXT("ProjectMind", "InventoryCaloriesNow", "I have food in my inventory. I should eat now.")
				: NSLOCTEXT("ProjectMind", "InventoryCaloriesSoon", "I have food in my inventory. I should eat soon."),
			bCaloriesCriticalNeed ? 225 : 185,
			bCaloriesCriticalNeed ? EMindThoughtChannel::ToastAndJournal : EMindThoughtChannel::Toast,
			bCaloriesCriticalNeed ? 18.0 : 24.0,
			12.0,
			FName(TEXT("Mind.Inventory.CaloriesSupply"))));
	}

	const float MaxWeight = InventoryReadOnly->GetMaxWeight();
	if (MaxWeight > KINDA_SMALL_NUMBER)
	{
		const float WeightRatio = InventoryReadOnly->GetCurrentWeight() / MaxWeight;
		const bool bOverweightTag = ASC->HasMatchingGameplayTag(ProjectTags::State_Weight_Overweight);
		if (bOverweightTag || WeightRatio >= 0.98f)
		{
			OutThoughts.Add(MakeInventoryCandidate(
				FName(TEXT("Mind.Inventory.Overweight")),
				NSLOCTEXT("ProjectMind", "InventoryOverweight", "I am overloaded. I should drop or move some items."),
				bOverweightTag ? 200 : 170,
				EMindThoughtChannel::Toast,
				20.0,
				14.0,
				FName(TEXT("Mind.Inventory.Weight"))));
		}
	}
}

FOnMindThoughtSourceChanged& FDefaultInventoryMindThoughtSource::OnThoughtSourceChanged()
{
	return SourceChangedDelegate;
}

void FDefaultInventoryMindThoughtSource::SyncInventoryBinding(APawn* LocalPawn) const
{
	UActorComponent* ResolvedInventoryComponent = ResolveInventoryComponent(LocalPawn);
	if (ResolvedInventoryComponent == BoundInventoryComponent.Get())
	{
		return;
	}

	ClearInventoryBinding();

	if (!ResolvedInventoryComponent)
	{
		return;
	}

	IInventoryReadOnly* InventoryReadOnly = Cast<IInventoryReadOnly>(ResolvedInventoryComponent);
	if (!InventoryReadOnly)
	{
		return;
	}

	FDefaultInventoryMindThoughtSource* MutableThis = const_cast<FDefaultInventoryMindThoughtSource*>(this);
	MutableThis->InventoryChangedHandle = InventoryReadOnly->OnInventoryViewChanged().AddRaw(
		MutableThis, &FDefaultInventoryMindThoughtSource::HandleInventoryViewChanged);
	MutableThis->InventoryErrorHandle = InventoryReadOnly->OnInventoryErrorNative().AddRaw(
		MutableThis, &FDefaultInventoryMindThoughtSource::HandleInventoryError);
	MutableThis->BoundInventoryComponent = ResolvedInventoryComponent;
}

void FDefaultInventoryMindThoughtSource::ClearInventoryBinding() const
{
	if (UActorComponent* BoundComponent = BoundInventoryComponent.Get())
	{
		if (IInventoryReadOnly* InventoryReadOnly = Cast<IInventoryReadOnly>(BoundComponent))
		{
			if (InventoryChangedHandle.IsValid())
			{
				InventoryReadOnly->OnInventoryViewChanged().Remove(InventoryChangedHandle);
			}

			if (InventoryErrorHandle.IsValid())
			{
				InventoryReadOnly->OnInventoryErrorNative().Remove(InventoryErrorHandle);
			}
		}
	}

	FDefaultInventoryMindThoughtSource* MutableThis = const_cast<FDefaultInventoryMindThoughtSource*>(this);
	MutableThis->InventoryChangedHandle.Reset();
	MutableThis->InventoryErrorHandle.Reset();
	MutableThis->BoundInventoryComponent.Reset();
}

UActorComponent* FDefaultInventoryMindThoughtSource::ResolveInventoryComponent(APawn* LocalPawn) const
{
	if (!LocalPawn)
	{
		return nullptr;
	}

	for (UActorComponent* Component : LocalPawn->GetComponents())
	{
		if (!Component)
		{
			continue;
		}

		if (Component->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
		{
			return Component;
		}
	}

	return nullptr;
}

IInventoryReadOnly* FDefaultInventoryMindThoughtSource::ResolveInventoryReadOnly(APawn* LocalPawn) const
{
	if (UActorComponent* BoundComponent = BoundInventoryComponent.Get())
	{
		if (BoundComponent->GetOwner() == LocalPawn)
		{
			return Cast<IInventoryReadOnly>(BoundComponent);
		}
	}

	return Cast<IInventoryReadOnly>(ResolveInventoryComponent(LocalPawn));
}

void FDefaultInventoryMindThoughtSource::HandleInventoryViewChanged()
{
	SourceChangedDelegate.Broadcast();
}

void FDefaultInventoryMindThoughtSource::HandleInventoryError(const FText& ErrorMessage)
{
	(void)ErrorMessage;
	SourceChangedDelegate.Broadcast();
}

bool FDefaultInventoryMindThoughtSource::HasVitalNeed(
	const UAbilitySystemComponent* ASC,
	const FGameplayTag& LowTag,
	const FGameplayTag& CriticalTag,
	const FGameplayTag& EmptyTag,
	bool& bOutCriticalNeed)
{
	bOutCriticalNeed = false;
	if (!ASC)
	{
		return false;
	}

	if (ASC->HasMatchingGameplayTag(EmptyTag) || ASC->HasMatchingGameplayTag(CriticalTag))
	{
		bOutCriticalNeed = true;
		return true;
	}

	return ASC->HasMatchingGameplayTag(LowTag);
}

bool FDefaultInventoryMindThoughtSource::EntryHasPositiveMagnitude(
	const FInventoryEntryView& Entry,
	const FGameplayTag& MagnitudeTag)
{
	if (!MagnitudeTag.IsValid())
	{
		return false;
	}

	if (const float* MagnitudeValue = Entry.UseMagnitudes.Find(MagnitudeTag))
	{
		return *MagnitudeValue > KINDA_SMALL_NUMBER;
	}

	return false;
}
