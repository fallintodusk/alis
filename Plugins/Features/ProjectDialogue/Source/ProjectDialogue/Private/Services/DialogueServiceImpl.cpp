// Copyright ALIS. All Rights Reserved.

#include "Services/DialogueServiceImpl.h"
#include "Components/ProjectDialogueComponent.h"
#include "Data/ProjectDialogueTypes.h"

void FDialogueServiceImpl::SetActiveComponent(UProjectDialogueComponent* Component)
{
	if (ActiveComponent.Get() == Component)
	{
		return;
	}

	UnbindFromComponent();

	ActiveComponent = Component;

	if (Component)
	{
		NodeChangedHandle = Component->OnNodeChangedNative.AddRaw(
			this, &FDialogueServiceImpl::HandleNodeChanged);
		OptionSelectedHandle = Component->OnOptionSelectedNative.AddRaw(
			this, &FDialogueServiceImpl::HandleOptionSelected);
		ConversationEndedHandle = Component->OnConversationEndedNative.AddRaw(
			this, &FDialogueServiceImpl::HandleConversationEnded);
	}

	StateChangedDelegate.Broadcast();
}

void FDialogueServiceImpl::ClearActiveComponent()
{
	UnbindFromComponent();
	ActiveComponent.Reset();
	StateChangedDelegate.Broadcast();
}

void FDialogueServiceImpl::UnbindFromComponent()
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return;
	}

	if (NodeChangedHandle.IsValid())
	{
		Comp->OnNodeChangedNative.Remove(NodeChangedHandle);
		NodeChangedHandle.Reset();
	}

	if (OptionSelectedHandle.IsValid())
	{
		Comp->OnOptionSelectedNative.Remove(OptionSelectedHandle);
		OptionSelectedHandle.Reset();
	}

	if (ConversationEndedHandle.IsValid())
	{
		Comp->OnConversationEndedNative.Remove(ConversationEndedHandle);
		ConversationEndedHandle.Reset();
	}
}

// --- IDialogueService ---

bool FDialogueServiceImpl::IsDialogueActive() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	return Comp && Comp->IsInConversation();
}

FName FDialogueServiceImpl::GetCurrentTreeId() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return NAME_None;
	}

	return Comp->GetCurrentTreeId();
}

FName FDialogueServiceImpl::GetCurrentNodeId() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return NAME_None;
	}

	const FString NodeId = Comp->GetCurrentNodeId();
	return NodeId.IsEmpty() ? NAME_None : FName(*NodeId);
}

FText FDialogueServiceImpl::GetCurrentSpeaker() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(Comp->GetCurrentSpeaker());
}

FText FDialogueServiceImpl::GetCurrentText() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(Comp->GetCurrentText());
}

void FDialogueServiceImpl::GetCurrentOptions(TArray<FDialogueOptionView>& OutOptions) const
{
	OutOptions.Reset();

	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (!Comp)
	{
		return;
	}

	Comp->GetOptionsForView(OutOptions);
}

bool FDialogueServiceImpl::IsCurrentNodeTerminal() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	return !Comp || Comp->IsCurrentNodeTerminal();
}

bool FDialogueServiceImpl::CurrentNodeHasOptions() const
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	return Comp && Comp->CurrentNodeHasOptions();
}

void FDialogueServiceImpl::SelectOption(int32 OptionIndex)
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (Comp)
	{
		Comp->SelectOption(OptionIndex);
	}
}

void FDialogueServiceImpl::AdvanceOrEnd()
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (Comp)
	{
		Comp->AdvanceOrEnd();
	}
}

void FDialogueServiceImpl::EndDialogue()
{
	UProjectDialogueComponent* Comp = ActiveComponent.Get();
	if (Comp)
	{
		Comp->EndConversation();
	}
}

bool FDialogueServiceImpl::IsDialogueActiveForActor(const AActor* Actor) const
{
	const UProjectDialogueComponent* Comp = ActiveComponent.Get();
	return Comp && Comp->IsInConversation() && Comp->GetOwner() == Actor;
}

const AActor* FDialogueServiceImpl::GetActiveDialogueOwner() const
{
	const UProjectDialogueComponent* Comp = ActiveComponent.Get();
	return Comp ? Comp->GetOwner() : nullptr;
}

void FDialogueServiceImpl::ActivateDialogueComponent(UActorComponent* Component)
{
	UProjectDialogueComponent* DialogueComp = Cast<UProjectDialogueComponent>(Component);
	if (!DialogueComp)
	{
		return;
	}

	SetActiveComponent(DialogueComp);
}

FOnDialogueStateChanged& FDialogueServiceImpl::OnDialogueStateChanged()
{
	return StateChangedDelegate;
}

FOnDialogueSignal& FDialogueServiceImpl::OnDialogueSignal()
{
	return SignalDelegate;
}

// --- Event handlers ---

void FDialogueServiceImpl::HandleNodeChanged(const FString& NodeId)
{
	const FName SignalTag = BuildSignalTagForNode(NodeId);
	if (!SignalTag.IsNone())
	{
		SignalDelegate.Broadcast(SignalTag);
	}

	StateChangedDelegate.Broadcast();
}

void FDialogueServiceImpl::HandleOptionSelected(const FString& FromNodeId, int32 /*OptionIndex*/, const FString& NextNodeId)
{
	const FName SignalTag = BuildSignalTagForOptionSelection(FromNodeId, NextNodeId);
	if (!SignalTag.IsNone())
	{
		SignalDelegate.Broadcast(SignalTag);
	}
}

void FDialogueServiceImpl::HandleConversationEnded()
{
	ClearActiveComponent();
}

FName FDialogueServiceImpl::BuildSignalTagForNode(const FString& NodeId) const
{
	if (NodeId.IsEmpty())
	{
		return NAME_None;
	}

	const FName TreeId = GetCurrentTreeId();
	if (TreeId.IsNone())
	{
		return NAME_None;
	}

	FString SanitizedTreeId = TreeId.ToString();
	SanitizedTreeId.ReplaceInline(TEXT(" "), TEXT("_"));

	FString SanitizedNodeId = NodeId;
	SanitizedNodeId.ReplaceInline(TEXT(" "), TEXT("_"));

	const FString SignalTagString = FString::Printf(
		TEXT("Dialogue.%s.%s"),
		*SanitizedTreeId,
		*SanitizedNodeId);
	return FName(*SignalTagString);
}

FName FDialogueServiceImpl::BuildSignalTagForOptionSelection(const FString& FromNodeId, const FString& NextNodeId) const
{
	if (FromNodeId.IsEmpty())
	{
		return NAME_None;
	}

	const FName TreeId = GetCurrentTreeId();
	if (TreeId.IsNone())
	{
		return NAME_None;
	}

	FString SanitizedTreeId = TreeId.ToString();
	SanitizedTreeId.ReplaceInline(TEXT(" "), TEXT("_"));

	FString SanitizedFromNodeId = FromNodeId;
	SanitizedFromNodeId.ReplaceInline(TEXT(" "), TEXT("_"));

	FString SanitizedNextNodeId = NextNodeId.IsEmpty() || NextNodeId == TEXT("$end")
		? FString(TEXT("end"))
		: NextNodeId;
	SanitizedNextNodeId.ReplaceInline(TEXT(" "), TEXT("_"));

	const FString SignalTagString = FString::Printf(
		TEXT("Dialogue.Option.%s.%s.Next.%s"),
		*SanitizedTreeId,
		*SanitizedFromNodeId,
		*SanitizedNextNodeId);
	return FName(*SignalTagString);
}
