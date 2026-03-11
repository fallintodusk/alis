// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IMindThoughtSource.h"

class APawn;
class UAbilitySystemComponent;
class UActorComponent;
class IInventoryReadOnly;
struct FInventoryEntryView;
struct FGameplayTag;

/**
 * Built-in inventory source for ProjectMind.
 *
 * Keeps Inventory plugin decoupled from mind orchestration.
 */
class FDefaultInventoryMindThoughtSource : public IMindInventoryThoughtSource
{
public:
	virtual ~FDefaultInventoryMindThoughtSource() override;

	virtual void CollectThoughts(
		const FMindThoughtSourceContext& Context,
		TArray<FMindThoughtCandidate>& OutThoughts) const override;
	virtual FOnMindThoughtSourceChanged& OnThoughtSourceChanged() override;

private:
	void SyncInventoryBinding(APawn* LocalPawn) const;
	void ClearInventoryBinding() const;
	UActorComponent* ResolveInventoryComponent(APawn* LocalPawn) const;
	IInventoryReadOnly* ResolveInventoryReadOnly(APawn* LocalPawn) const;

	void HandleInventoryViewChanged();
	void HandleInventoryError(const FText& ErrorMessage);

	static bool HasVitalNeed(
		const UAbilitySystemComponent* ASC,
		const FGameplayTag& LowTag,
		const FGameplayTag& CriticalTag,
		const FGameplayTag& EmptyTag,
		bool& bOutCriticalNeed);
	static bool EntryHasPositiveMagnitude(const FInventoryEntryView& Entry, const FGameplayTag& MagnitudeTag);

private:
	mutable TWeakObjectPtr<UActorComponent> BoundInventoryComponent;
	mutable FDelegateHandle InventoryChangedHandle;
	mutable FDelegateHandle InventoryErrorHandle;
	FOnMindThoughtSourceChanged SourceChangedDelegate;
};
