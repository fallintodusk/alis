// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Components/ProjectDialogueComponent.h"
#include "Data/DialogueTreeDefinition.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IDialogueService.h"
#include "ProjectServiceLocator.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
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

	// BypassCondition: if met, skip dialogue and pass through to next capability.
	if (BypassCondition.IsValid())
	{
		if (CheckCondition(BypassCondition))
		{
			UE_LOG(LogDialogueComponent, Log,
				TEXT("[%s] Bypass condition met (type='%s', id='%s') - skipping dialogue"),
				*GetNameSafe(GetOwner()), *BypassCondition.Type, *BypassCondition.Id);
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
			TEXT("[%s::SelectOption] Option %d blocked by condition (type='%s', id='%s')"),
			*GetNameSafe(GetOwner()), RawOptionIndex, *Selected.Condition.Type, *Selected.Condition.Id);
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
		const bool bHasCondition = Option.Condition.IsValid();
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

bool UProjectDialogueComponent::CheckCondition(const FDialogueCondition& CondData) const
{
	AActor* InstigatorActor = Instigator.Get();
	if (!InstigatorActor)
	{
		return false;
	}

	if (CondData.Type == TEXT("inventory"))
	{
		return CheckInventoryCondition(*InstigatorActor, CondData);
	}

	if (CondData.Type == TEXT("tag"))
	{
		return CheckTagCondition(*InstigatorActor, CondData);
	}

	UE_LOG(LogDialogueComponent, Warning,
		TEXT("[%s::CheckCondition] Unknown condition type '%s' (id='%s')"),
		*GetNameSafe(GetOwner()), *CondData.Type, *CondData.Id);
	return false;
}

bool UProjectDialogueComponent::CheckInventoryCondition(AActor& InstigatorActor, const FDialogueCondition& CondData)
{
	TInlineComponentArray<UActorComponent*> Components;
	InstigatorActor.GetComponents(Components);

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

		if (CondData.Exact)
		{
			const FPrimaryAssetId TargetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*CondData.Id));
			if (Inventory->ContainsItem(TargetId, CondData.Quantity))
			{
				return true;
			}
		}
		else
		{
			if (InventoryContainsByObjectNamePrefix(Inventory, CondData.Id, CondData.Quantity))
			{
				return true;
			}
		}
	}

	return false;
}

bool UProjectDialogueComponent::CheckTagCondition(AActor& InstigatorActor, const FDialogueCondition& CondData)
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*CondData.Id), false);
	if (!Tag.IsValid())
	{
		return false;
	}

	const IGameplayTagAssetInterface* TagSource = Cast<IGameplayTagAssetInterface>(&InstigatorActor);
	if (!TagSource)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	TagSource->GetOwnedGameplayTags(OwnedTags);

	return CondData.Exact
		? OwnedTags.HasTagExact(Tag)
		: OwnedTags.HasTag(Tag);
}

bool UProjectDialogueComponent::IsOptionConditionMet(const FDialogueOption& Option) const
{
	if (Option.Condition.IsValid())
	{
		return CheckCondition(Option.Condition);
	}
	// No condition = always visible
	return true;
}

// Consumed by DLG_*.json action strings. Asset path must match a real DLG file.
// Validate: scripts/ue/check/data/validate_all.py
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
