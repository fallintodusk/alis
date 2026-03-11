// Copyright ALIS. All Rights Reserved.

#include "Support/ObjectParentGeneralizationTestDoubles.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataAsset.h"

UProjectInteractionCounterCapabilityComponent::UProjectInteractionCounterCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InteractionLabel = FText::FromString(TEXT("ComponentFallback"));
}

int32 UProjectInteractionCounterCapabilityComponent::GetInteractPriority_Implementation() const
{
	return InteractPriority;
}

bool UProjectInteractionCounterCapabilityComponent::OnComponentInteract_Implementation(AActor* Instigator)
{
	++InteractionCallCount;
	LastInstigator = Instigator;
	return bOnInteractReturnValue;
}

FInteractionPrompt UProjectInteractionCounterCapabilityComponent::GetInteractionPrompt_Implementation(AActor* Instigator) const
{
	return FInteractionPrompt();
}

FText UProjectInteractionCounterCapabilityComponent::GetInteractionLabel_Implementation() const
{
	return InteractionLabel;
}

UPrimitiveComponent* UProjectInteractionCounterCapabilityComponent::GetInteractTargetMesh_Implementation() const
{
	return BoundTargetMesh;
}

void UProjectInteractionCounterCapabilityComponent::SetInteractTargetMesh_Implementation(UPrimitiveComponent* TargetMesh)
{
	BoundTargetMesh = TargetMesh;
}

AProjectInteractionActorInterfaceTestActor::AProjectInteractionActorInterfaceTestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestMesh"));
	TestMesh->SetupAttachment(Root);

	FocusLabel = FText::FromString(TEXT("ActorInterface"));
}

bool AProjectInteractionActorInterfaceTestActor::OnInteract_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent)
{
	++OnInteractCallCount;
	LastInstigator = InteractInstigator;
	LastHitComponent = HitComponent;
	return bOnInteractReturnValue;
}

FInteractionFocusInfo AProjectInteractionActorInterfaceTestActor::GetFocusInfo_Implementation(UPrimitiveComponent* HitComponent) const
{
	FInteractionFocusInfo Result;
	Result.Label = FocusLabel;
	Result.HighlightMesh = HitComponent ? HitComponent : TestMesh;
	return Result;
}

AProjectSyncDefinitionApplicableTestActor::AProjectSyncDefinitionApplicableTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool AProjectSyncDefinitionApplicableTestActor::ApplyDefinition_Implementation(UPrimaryDataAsset* Definition)
{
	++ApplyDefinitionCallCount;
	LastAppliedDefinition = Definition;
	return bApplyDefinitionResult;
}

UProjectTestInventoryComponent::UProjectTestInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProjectTestInventoryComponent::SetItemQuantity(const FPrimaryAssetId& ItemId, int32 Quantity)
{
	if (Quantity > 0)
	{
		Quantities.FindOrAdd(ItemId) = Quantity;
	}
	else
	{
		Quantities.Remove(ItemId);
	}

	InventoryChangedDelegate.Broadcast();
}

int32 UProjectTestInventoryComponent::GetItemQuantity(const FPrimaryAssetId& ItemId) const
{
	const int32* Found = Quantities.Find(ItemId);
	return Found ? *Found : 0;
}

void UProjectTestInventoryComponent::GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const
{
	OutEntries.Reset();
	int32 InstanceCounter = 1;
	for (const TPair<FPrimaryAssetId, int32>& Pair : Quantities)
	{
		FInventoryEntryView View;
		View.ItemId = Pair.Key;
		View.Quantity = Pair.Value;
		View.InstanceId = InstanceCounter++;
		View.DisplayName = FText::FromName(Pair.Key.PrimaryAssetName);
		OutEntries.Add(View);
	}
}

void UProjectTestInventoryComponent::GetContainersView(TArray<FInventoryContainerView>& OutContainers) const
{
	OutContainers.Reset();
}

bool UProjectTestInventoryComponent::ContainsItem(FPrimaryAssetId ItemId, int32 MinQuantity) const
{
	return GetItemQuantity(ItemId) >= MinQuantity;
}

void UProjectTestInventoryComponent::HandleAction(const FString& Context, const FString& Action)
{
	if (!Action.StartsWith(TEXT("inventory.consume:")))
	{
		return;
	}

	FString ItemIdStr = Action.Mid(18);
	ItemIdStr.TrimStartAndEndInline();
	const bool bPrefixMatch = ItemIdStr.RemoveFromEnd(TEXT("*"));
	ItemIdStr.TrimStartAndEndInline();

	const FPrimaryAssetId TargetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*ItemIdStr));
	FPrimaryAssetId ResolvedId = TargetId;
	int32 CurrentQuantity = GetItemQuantity(ResolvedId);
	if (CurrentQuantity <= 0 && bPrefixMatch)
	{
		for (const TPair<FPrimaryAssetId, int32>& Pair : Quantities)
		{
			if (Pair.Value <= 0 || Pair.Key.PrimaryAssetType != FPrimaryAssetType(TEXT("ObjectDefinition")))
			{
				continue;
			}

			if (Pair.Key.PrimaryAssetName.ToString().StartsWith(ItemIdStr, ESearchCase::IgnoreCase))
			{
				ResolvedId = Pair.Key;
				CurrentQuantity = Pair.Value;
				break;
			}
		}

		if (CurrentQuantity <= 0)
		{
			return;
		}
	}

	SetItemQuantity(ResolvedId, CurrentQuantity - 1);
	++ConsumedActionCount;
	LastConsumedItemId = ResolvedId;
}
