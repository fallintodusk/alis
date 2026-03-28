// Copyright ALIS. All Rights Reserved.

#include "Components/ProjectDialogueComponent.h"
#include "Data/DialogueTreeDefinition.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IDialogueService.h"
#include "ProjectServiceLocator.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Engine/StreamableManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogueComponent, Log, All);

namespace
{
bool InventoryContainsByObjectNamePrefix(const IInventoryReadOnly* Inventory, const FString& ObjectNamePrefix, int32 MinQuantity = 1)
{
	if (!Inventory || ObjectNamePrefix.IsEmpty())
	{
		return false;
	}

	TArray<FInventoryEntryView> Entries;
	Inventory->GetEntriesView(Entries);
	for (const FInventoryEntryView& Entry : Entries)
	{
		if (Entry.Quantity < MinQuantity || Entry.ItemId.PrimaryAssetType != FPrimaryAssetType(TEXT("ObjectDefinition")))
		{
			continue;
		}

		if (Entry.ItemId.PrimaryAssetName.ToString().StartsWith(ObjectNamePrefix, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}
}

UProjectDialogueComponent::UProjectDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FPrimaryAssetId UProjectDialogueComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Dialogue")));
}

bool UProjectDialogueComponent::OnComponentInteract_Implementation(AActor* InInstigator)
{
	if (bInConversation)
	{
		return true;
	}

	// BypassCondition: if instigator has this tag, skip dialogue entirely.
	// Returns true to pass through to next capability in the chain
	// (e.g. door opens normally after scenario is complete).
	if (!BypassCondition.IsEmpty())
	{
		if (CheckCondition(BypassCondition))
		{
			UE_LOG(LogDialogueComponent, Log,
				TEXT("[%s] Bypass condition '%s' met - skipping dialogue"),
				*GetNameSafe(GetOwner()), *BypassCondition);
			return true;
		}
	}

	SetInstigator(InInstigator);
	StartConversation();
	// Return false to block chain while in dialogue
	return false;
}

FInteractionPrompt UProjectDialogueComponent::GetInteractionPrompt_Implementation(AActor* InInstigator) const
{
	// No confirmation prompt - start dialogue immediately
	return FInteractionPrompt();
}

FText UProjectDialogueComponent::GetInteractionLabel_Implementation() const
{
	return NSLOCTEXT("Dialogue", "InteractLabel", "Talk");
}

FInteractionExecutionSpec UProjectDialogueComponent::GetInteractionExecutionSpec_Implementation(AActor* InInstigator) const
{
	(void)InInstigator;
	return FInteractionExecutionSpec();
}

void UProjectDialogueComponent::StartConversation()
{
	if (bInConversation)
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::StartConversation] Already in conversation"), *GetNameSafe(GetOwner()));
		return;
	}

	if (!EnsureTreeLoaded())
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::StartConversation] No dialogue tree loaded"), *GetNameSafe(GetOwner()));
		return;
	}

	if (const TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>())
	{
		DialogueService->ActivateDialogueComponent(this);
	}
	else
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::StartConversation] IDialogueService is unavailable; UI will not reflect this conversation"),
			*GetNameSafe(GetOwner()));
	}

	bInConversation = true;
	ConversationStartFrame = GFrameCounter;
	NavigateToNode(LoadedTree->StartNode);
	OnConversationStarted.Broadcast();
	UE_LOG(LogDialogueComponent, Log,
		TEXT("[%s::StartConversation] Started dialogue tree '%s' at node '%s'"),
		*GetNameSafe(GetOwner()),
		*LoadedTree->TreeId.ToString(),
		*CurrentNodeId);
}

void UProjectDialogueComponent::StartConversationWithInstigator(AActor* InInstigator, AActor* InActionTarget)
{
	SetInstigator(InInstigator);
	ActionTarget = InActionTarget;
	StartConversation();
}

void UProjectDialogueComponent::HandleActorWatchEvent(const FActorWatchEvent& Event)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogDialogueComponent, Verbose,
			TEXT("[%s::HandleActorWatchEvent] Ignored on non-authority or missing owner"),
			*GetNameSafe(OwnerActor));
		return;
	}

	if (!bAutoStartFromWatchEvents || bInConversation)
	{
		UE_LOG(LogDialogueComponent, Verbose,
			TEXT("[%s::HandleActorWatchEvent] Ignored (AutoStart=%d, InConversation=%d)"),
			*GetNameSafe(OwnerActor),
			bAutoStartFromWatchEvents ? 1 : 0,
			bInConversation ? 1 : 0);
		return;
	}

	if (!AutoStartWatchEventName.IsNone() && Event.EventName != AutoStartWatchEventName)
	{
		UE_LOG(LogDialogueComponent, Verbose,
			TEXT("[%s::HandleActorWatchEvent] Ignored event '%s' (expects '%s')"),
			*GetNameSafe(OwnerActor),
			*Event.EventName.ToString(),
			*AutoStartWatchEventName.ToString());
		return;
	}

	if (!Event.Instigator || !Event.SourceActor)
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::HandleActorWatchEvent] Missing instigator/source (Instigator=%s, Source=%s)"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(Event.Instigator),
			*GetNameSafe(Event.SourceActor));
		return;
	}

	UE_LOG(LogDialogueComponent, Log,
		TEXT("[%s::HandleActorWatchEvent] Auto-start from event (Name=%s, Tag=%s, Instigator=%s, Source=%s)"),
		*GetNameSafe(OwnerActor),
		*Event.EventName.ToString(),
		*Event.EventTag.ToString(),
		*GetNameSafe(Event.Instigator),
		*GetNameSafe(Event.SourceActor));

	StartConversationWithInstigator(Event.Instigator.Get(), Event.SourceActor.Get());
}

void UProjectDialogueComponent::SelectOption(int32 OptionIndex)
{
	if (!bInConversation)
	{
		return;
	}

	const FDialogueNode* Node = GetCurrentNode();
	if (!Node)
	{
		return;
	}

	// Primary path: OptionIndex is a raw node option index (UI view contract).
	int32 RawOptionIndex = INDEX_NONE;
	if (Node->Options.IsValidIndex(OptionIndex))
	{
		RawOptionIndex = OptionIndex;
	}
	else
	{
		// Compatibility path for older callers using "visible options" indices.
		TArray<int32> VisibleRawIndices;
		VisibleRawIndices.Reserve(Node->Options.Num());
		for (int32 i = 0; i < Node->Options.Num(); ++i)
		{
			if (IsOptionConditionMet(Node->Options[i]))
			{
				VisibleRawIndices.Add(i);
			}
		}

		if (VisibleRawIndices.IsValidIndex(OptionIndex))
		{
			RawOptionIndex = VisibleRawIndices[OptionIndex];
		}
	}

	if (!Node->Options.IsValidIndex(RawOptionIndex))
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::SelectOption] Invalid option index %d (raw options: %d)"),
			*GetNameSafe(GetOwner()), OptionIndex, Node->Options.Num());
		return;
	}

	const FDialogueOption& Selected = Node->Options[RawOptionIndex];
	if (!IsOptionConditionMet(Selected))
	{
		UE_LOG(LogDialogueComponent, Log,
			TEXT("[%s::SelectOption] Option %d blocked by condition '%s'"),
			*GetNameSafe(GetOwner()), RawOptionIndex, *Selected.Condition);
		return;
	}

	const FString FromNodeId = CurrentNodeId;
	OnOptionSelectedNative.Broadcast(FromNodeId, RawOptionIndex, Selected.Next);

	if (Selected.IsEndOption())
	{
		EndConversation();
		return;
	}

	NavigateToNode(Selected.Next);
}

void UProjectDialogueComponent::AdvanceOrEnd()
{
	if (!bInConversation)
	{
		return;
	}

	const FDialogueNode* Node = GetCurrentNode();
	if (!Node)
	{
		EndConversation();
		return;
	}

	if (Node->HasOptions())
	{
		// Node has choices - caller should use SelectOption() instead
		return;
	}

	if (Node->IsTerminal())
	{
		EndConversation();
		return;
	}

	// Auto-advance via "next"
	NavigateToNode(Node->Next);
}

void UProjectDialogueComponent::EndConversation()
{
	if (!bInConversation)
	{
		return;
	}

	// Notify action receivers that conversation ended
	TArray<FString> EndActions;
	EndActions.Add(TEXT("$end"));
	DispatchActions(TEXT("$conversation_end"), EndActions);

	if (!PendingDialogueTreeAsset.IsNull())
	{
		DialogueTreeAsset = PendingDialogueTreeAsset;
		PendingDialogueTreeAsset.Reset();
		LoadedTree = nullptr;
		UE_LOG(LogDialogueComponent, Log,
			TEXT("[%s::EndConversation] Applied pending dialogue tree switch to '%s'"),
			*GetNameSafe(GetOwner()),
			*DialogueTreeAsset.ToString());
	}

	bInConversation = false;
	ConversationStartFrame = 0;
	CurrentNodeId.Empty();
	Instigator.Reset();
	ActionTarget.Reset();

	OnConversationEnded.Broadcast();
	OnConversationEndedNative.Broadcast();
}

FString UProjectDialogueComponent::GetCurrentSpeaker() const
{
	const FDialogueNode* Node = GetCurrentNode();
	return Node ? Node->Speaker : FString();
}

FString UProjectDialogueComponent::GetCurrentText() const
{
	const FDialogueNode* Node = GetCurrentNode();
	return Node ? Node->Text : FString();
}

FName UProjectDialogueComponent::GetCurrentTreeId() const
{
	if (LoadedTree && !LoadedTree->TreeId.IsNone())
	{
		return LoadedTree->TreeId;
	}

	if (DialogueTreeAsset.IsNull())
	{
		return NAME_None;
	}

	const FString AssetName = DialogueTreeAsset.GetAssetName();
	return AssetName.IsEmpty() ? NAME_None : FName(*AssetName);
}

bool UProjectDialogueComponent::IsCurrentNodeTerminal() const
{
	const FDialogueNode* Node = GetCurrentNode();
	return Node ? Node->IsTerminal() : true;
}

bool UProjectDialogueComponent::CurrentNodeHasOptions() const
{
	const FDialogueNode* Node = GetCurrentNode();
	return Node && Node->HasOptions();
}

void UProjectDialogueComponent::GetVisibleOptions(TArray<FDialogueOption>& OutOptions) const
{
	OutOptions.Reset();

	const FDialogueNode* Node = GetCurrentNode();
	if (!Node)
	{
		return;
	}

	for (const FDialogueOption& Option : Node->Options)
	{
		if (IsOptionConditionMet(Option))
		{
			OutOptions.Add(Option);
		}
	}
}

void UProjectDialogueComponent::GetOptionsForView(TArray<FDialogueOptionView>& OutOptions) const
{
	OutOptions.Reset();

	const FDialogueNode* Node = GetCurrentNode();
	if (!Node)
	{
		return;
	}

	OutOptions.Reserve(Node->Options.Num());
	for (int32 i = 0; i < Node->Options.Num(); ++i)
	{
		const FDialogueOption& Option = Node->Options[i];
		const bool bHasCondition = !Option.Condition.IsEmpty();
		const bool bConditionMet = IsOptionConditionMet(Option);

		FDialogueOptionView View;
		View.Index = i; // raw node option index
		View.Text = FText::FromString(Option.Text);
		View.bLocked = !bConditionMet;
		View.bHasCondition = bHasCondition;
		View.bConditionSatisfied = bHasCondition && bConditionMet;
		OutOptions.Add(View);
	}
}

// --- Private ---

const FDialogueNode* UProjectDialogueComponent::GetCurrentNode() const
{
	if (!LoadedTree || CurrentNodeId.IsEmpty())
	{
		return nullptr;
	}

	return LoadedTree->FindNode(CurrentNodeId);
}

void UProjectDialogueComponent::NavigateToNode(const FString& NodeId)
{
	if (!LoadedTree)
	{
		return;
	}

	const FDialogueNode* Node = LoadedTree->FindNode(NodeId);
	if (!Node)
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::NavigateToNode] Node '%s' not found in tree '%s'"),
			*GetNameSafe(GetOwner()), *NodeId, *LoadedTree->TreeId.ToString());
		EndConversation();
		return;
	}

	CurrentNodeId = NodeId;
	OnNodeChanged.Broadcast(CurrentNodeId);
	OnNodeChangedNative.Broadcast(CurrentNodeId);

	// Dispatch node actions to ActionTarget (or owner) and instigator
	UE_LOG(LogDialogueComponent, Verbose,
		TEXT("[%s::NavigateToNode] Node '%s' has %d actions"),
		*GetNameSafe(GetOwner()), *NodeId, Node->Actions.Num());

	if (Node->Actions.Num() > 0)
	{
		DispatchActions(CurrentNodeId, Node->Actions);
	}
}

bool UProjectDialogueComponent::EnsureTreeLoaded()
{
	if (LoadedTree)
	{
		return LoadedTree->IsValid();
	}

	if (DialogueTreeAsset.IsNull())
	{
		return false;
	}

	// Synchronous load for now (dialogue assets are small)
	LoadedTree = DialogueTreeAsset.LoadSynchronous();
	if (!LoadedTree)
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::EnsureTreeLoaded] Failed to load '%s'"),
			*GetNameSafe(GetOwner()), *DialogueTreeAsset.ToString());
		return false;
	}

	if (!LoadedTree->IsValid())
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::EnsureTreeLoaded] Tree '%s' is invalid (bad StartNode)"),
			*GetNameSafe(GetOwner()), *LoadedTree->TreeId.ToString());
		return false;
	}

	return true;
}

bool UProjectDialogueComponent::CheckCondition(const FString& ConditionStr) const
{
	if (ConditionStr.IsEmpty())
	{
		return true;
	}

	// Inventory condition: "inventory:<ObjectId>" checks if instigator has the item
	if (ConditionStr.StartsWith(TEXT("inventory:")))
	{
		FString ItemIdStr = ConditionStr.Mid(10);
		ItemIdStr.TrimStartAndEndInline();
		const bool bPrefixMatch = ItemIdStr.RemoveFromEnd(TEXT("*"));
		ItemIdStr.TrimStartAndEndInline();

		AActor* InstigatorActor = Instigator.Get();
		if (!InstigatorActor)
		{
			return false;
		}

		// IInventoryReadOnly is on UProjectInventoryComponent, NOT on the actor.
		// Search components -- same pattern as DispatchActions.
		TInlineComponentArray<UActorComponent*> Components;
		InstigatorActor->GetComponents(Components);

		const FPrimaryAssetId TargetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*ItemIdStr));
		bool bFoundInventorySource = false;

		for (UActorComponent* Comp : Components)
		{
			if (!Comp || !Comp->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
			{
				continue;
			}

			const IInventoryReadOnly* Inventory = Cast<IInventoryReadOnly>(Comp);
			if (!Inventory)
			{
				continue;
			}
			bFoundInventorySource = true;

			// 1) Exact object id match (existing behavior).
			if (Inventory->ContainsItem(TargetId))
			{
				return true;
			}

			// 2) Explicit family fallback for content variants:
			//    inventory:WaterBottle* matches WaterBottleBig/WaterBottleSmall/etc.
			if (bPrefixMatch && InventoryContainsByObjectNamePrefix(Inventory, ItemIdStr))
			{
				UE_LOG(LogDialogueComponent, Verbose,
					TEXT("[%s::CheckCondition] inventory prefix match '%s*' satisfied"),
					*GetNameSafe(GetOwner()), *ItemIdStr);
				return true;
			}
		}

		if (!bFoundInventorySource)
		{
			UE_LOG(LogDialogueComponent, Warning,
				TEXT("[%s::CheckCondition] No IInventoryReadOnly found on instigator '%s'"),
				*GetNameSafe(GetOwner()), *GetNameSafe(InstigatorActor));
		}
		else
		{
			UE_LOG(LogDialogueComponent, Verbose,
				TEXT("[%s::CheckCondition] Inventory condition '%s' not satisfied for instigator '%s'"),
				*GetNameSafe(GetOwner()), *ConditionStr, *GetNameSafe(InstigatorActor));
		}
		return false;
	}

	// GameplayTag condition (existing behavior)
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*ConditionStr), /*bErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::CheckCondition] Unknown condition '%s' (not a tag, not inventory:)"),
			*GetNameSafe(GetOwner()), *ConditionStr);
		return false;
	}

	AActor* InstigatorActor = Instigator.Get();
	if (!InstigatorActor)
	{
		return true;
	}

	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InstigatorActor))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			return ASC->HasMatchingGameplayTag(Tag);
		}
	}

	return true;
}

bool UProjectDialogueComponent::IsOptionConditionMet(const FDialogueOption& Option) const
{
	return Option.Condition.IsEmpty() || CheckCondition(Option.Condition);
}

void UProjectDialogueComponent::HandleAction(const FString& Context, const FString& Action)
{
	if (!Action.StartsWith(TEXT("dialogue.set_tree:")))
	{
		return;
	}

	FString AssetPath = Action.Mid(18).TrimStartAndEnd();
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[%s::HandleAction] dialogue.set_tree has empty asset path (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		return;
	}

	// Normalize short object path: /Mount/Path/Asset -> /Mount/Path/Asset.Asset
	int32 LastSlash = INDEX_NONE;
	int32 LastDot = INDEX_NONE;
	AssetPath.FindLastChar(TEXT('/'), LastSlash);
	AssetPath.FindLastChar(TEXT('.'), LastDot);
	if (LastSlash != INDEX_NONE && (LastDot == INDEX_NONE || LastDot < LastSlash))
	{
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		AssetPath += TEXT(".") + AssetName;
	}

	const TSoftObjectPtr<UDialogueTreeDefinition> TargetTree{FSoftObjectPath(AssetPath)};
	if (bInConversation)
	{
		PendingDialogueTreeAsset = TargetTree;
		UE_LOG(LogDialogueComponent, Log,
			TEXT("[%s::HandleAction] Queued dialogue tree switch to '%s' after conversation end"),
			*GetNameSafe(GetOwner()), *AssetPath);
		return;
	}

	DialogueTreeAsset = TargetTree;
	LoadedTree = nullptr;
	CurrentNodeId.Empty();
	UE_LOG(LogDialogueComponent, Log,
		TEXT("[%s::HandleAction] Switched dialogue tree to '%s'"),
		*GetNameSafe(GetOwner()), *AssetPath);
}

void UProjectDialogueComponent::DispatchActions(const FString& Context, const TArray<FString>& Actions)
{
	// Authority guard: all actions must dispatch atomically on server.
	// Without this, some receivers (inventory.consume) would fire via Server RPC
	// while others (lock.unlock, motion.open) no-op on client -- partial execution.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[DispatchActions] Skipping on non-authority (Context='%s', %d actions)"),
			*Context, Actions.Num());
		return;
	}

	// Actions route to ActionTarget (e.g. door) if set, otherwise to owner (e.g. gramophone)
	AActor* Target = ActionTarget.IsValid() ? ActionTarget.Get() : GetOwner();
	if (!Target)
	{
		UE_LOG(LogDialogueComponent, Warning,
			TEXT("[DispatchActions] No target actor (owner or ActionTarget)"));
		return;
	}

	DispatchToActor(Target, Context, Actions);

	// Also dispatch to owner when target is external (ActionTarget path), but
	// keep owner fallback strictly dialogue-scoped to avoid accidental side effects.
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor != Target)
	{
		TArray<FString> OwnerOnlyActions;
		OwnerOnlyActions.Reserve(Actions.Num());
		for (const FString& Action : Actions)
		{
			if (Action.StartsWith(TEXT("dialogue.")))
			{
				OwnerOnlyActions.Add(Action);
			}
		}

		if (OwnerOnlyActions.Num() > 0)
		{
			DispatchToActor(OwnerActor, Context, OwnerOnlyActions);
		}
	}

	// Also dispatch to instigator (player) for inventory.consume, etc.
	AActor* InstigatorActor = Instigator.Get();
	if (InstigatorActor && InstigatorActor != Target && InstigatorActor != OwnerActor)
	{
		DispatchToActor(InstigatorActor, Context, Actions);
	}
}

void UProjectDialogueComponent::DispatchToActor(AActor* Actor, const FString& Context, const TArray<FString>& Actions)
{
	if (!Actor)
	{
		return;
	}

	// Collect receivers once
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	struct FReceiverEntry
	{
		IProjectActionReceiver* Receiver;
		UActorComponent* Component;
	};
	TArray<FReceiverEntry, TInlineAllocator<4>> Receivers;

	for (UActorComponent* Comp : Components)
	{
		if (!Comp || !Comp->GetClass()->ImplementsInterface(UProjectActionReceiver::StaticClass()))
		{
			continue;
		}

		IProjectActionReceiver* Receiver = Cast<IProjectActionReceiver>(Comp);
		if (!Receiver)
		{
			UE_LOG(LogDialogueComponent, Warning,
				TEXT("[DispatchToActor] %s on '%s' implements IProjectActionReceiver but Cast failed"),
				*Comp->GetClass()->GetName(), *GetNameSafe(Actor));
			continue;
		}

		Receivers.Add({Receiver, Comp});
	}

	if (Receivers.Num() == 0)
	{
		UE_LOG(LogDialogueComponent, Verbose,
			TEXT("[DispatchToActor] No IProjectActionReceiver found on '%s' (%d components)"),
			*GetNameSafe(Actor), Components.Num());
		return;
	}

	// Action-first loop: preserves JSON action order across all receivers.
	// e.g. lock.unlock runs on ALL receivers before motion.open runs on any.
	for (const FString& Action : Actions)
	{
		for (const FReceiverEntry& Entry : Receivers)
		{
			UE_LOG(LogDialogueComponent, Verbose,
				TEXT("[DispatchToActor] -> %s::%s::HandleAction(Context='%s', Action='%s')"),
				*GetNameSafe(Actor), *Entry.Component->GetClass()->GetName(), *Context, *Action);
			Entry.Receiver->HandleAction(Context, Action);
		}
	}
}
