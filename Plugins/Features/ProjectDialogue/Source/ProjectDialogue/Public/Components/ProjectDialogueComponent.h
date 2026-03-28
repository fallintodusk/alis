// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IActorWatchEventListener.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Interfaces/IDialogueService.h"
#include "Data/ProjectDialogueTypes.h"
#include "ProjectDialogueComponent.generated.h"

class UDialogueTreeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeChanged, const FString&, NodeId);

// Native C++ delegates for non-UObject subscribers (e.g. service impl)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeChangedNative, const FString& /*NodeId*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnDialogueOptionSelectedNative,
	const FString& /*FromNodeId*/,
	int32 /*OptionIndex*/,
	const FString& /*NextNodeId*/);
DECLARE_MULTICAST_DELEGATE(FOnDialogueEventNative);

/**
 * Universal dialogue component for any actor (NPCs, objects, terminals).
 * Navigates a tree-based dialogue loaded from UDialogueTreeDefinition (JSON-generated).
 *
 * Usage:
 * - Attach to any actor in editor
 * - Assign DialogueTreeAsset (soft ptr to auto-generated UAsset)
 * - Interaction system calls StartConversation() on press E
 * - UI reads state via IDialogueService (ServiceLocator bridge)
 */
UCLASS(ClassGroup=(Dialogue), meta=(BlueprintSpawnableComponent))
class PROJECTDIALOGUE_API UProjectDialogueComponent
	: public UActorComponent
	, public IInteractableComponentTargetInterface
	, public IProjectActionReceiver
	, public IActorWatchEventListener
{
	GENERATED_BODY()

public:
	UProjectDialogueComponent();

	// --- IInteractableComponentTargetInterface ---
	virtual int32 GetInteractPriority_Implementation() const override { return InteractPriority; }
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FInteractionPrompt GetInteractionPrompt_Implementation(AActor* Instigator) const override;
	virtual FText GetInteractionLabel_Implementation() const override;
	virtual FInteractionExecutionSpec GetInteractionExecutionSpec_Implementation(AActor* Instigator) const override;

	// Start dialogue from the tree's start node
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartConversation();

	// Select a player choice by visible-options index
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SelectOption(int32 OptionIndex);

	// Advance auto-advance node or end terminal node
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AdvanceOrEnd();

	// End conversation immediately
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsInConversation() const { return bInConversation; }

	// Frame number when current conversation started (0 when inactive).
	uint64 GetConversationStartFrame() const { return ConversationStartFrame; }

	// Current node accessors
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FString GetCurrentSpeaker() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FString GetCurrentText() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsCurrentNodeTerminal() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool CurrentNodeHasOptions() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FString GetCurrentNodeId() const { return CurrentNodeId; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FName GetCurrentTreeId() const;

	// Get visible options (filtered by GameplayTag conditions on instigator)
	void GetVisibleOptions(TArray<FDialogueOption>& OutOptions) const;

	// Get current node options for UI (includes condition-locked options).
	void GetOptionsForView(TArray<FDialogueOptionView>& OutOptions) const;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueEvent OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueEvent OnConversationEnded;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueNodeChanged OnNodeChanged;

	// Native delegates for C++ subscribers that are not UObjects
	FOnDialogueNodeChangedNative OnNodeChangedNative;
	FOnDialogueOptionSelectedNative OnOptionSelectedNative;
	FOnDialogueEventNative OnConversationEndedNative;

	// CapabilityRegistry discovers this via CDO scan
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// The pawn that initiated the dialogue (set by interaction handler)
	void SetInstigator(AActor* InInstigator) { Instigator = InInstigator; }

	/**
	 * Start conversation with explicit instigator and optional action target.
	 * ActionTarget receives actions (lock.unlock, motion.open) instead of owner.
	 * If null, falls back to owner (existing behavior).
	 */
	void StartConversationWithInstigator(AActor* InInstigator, AActor* InActionTarget = nullptr);

	// --- IProjectActionReceiver ---
	virtual void HandleAction(const FString& Context, const FString& Action) override;
	virtual void HandleActorWatchEvent(const FActorWatchEvent& Event) override;

	// The dialogue tree asset (set in editor or via code)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TSoftObjectPtr<UDialogueTreeDefinition> DialogueTreeAsset;

	/** Priority in interaction chain. Higher = runs first. Set from JSON properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 InteractPriority = 0;

	/**
	 * If enabled, watch events from UActorWatcherComponent on the same owner can
	 * auto-start this dialogue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Watch")
	bool bAutoStartFromWatchEvents = true;

	/**
	 * Optional event-name filter for auto-start. Empty means any watch event.
	 * Recommended for scenarios: "lock.access_denied".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Watch")
	FName AutoStartWatchEventName = FName(TEXT("lock.access_denied"));

	/**
	 * If instigator has this GameplayTag, skip dialogue and pass through to next
	 * capability in the chain. Used for "already completed" scenarios (e.g. door
	 * already unlocked -> dialogue skips -> door opens normally).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString BypassCondition;

private:
	bool bInConversation = false;
	FString CurrentNodeId;

	// Resolved (loaded) tree pointer
	UPROPERTY(Transient)
	TObjectPtr<UDialogueTreeDefinition> LoadedTree;

	// The actor that started this dialogue
	TWeakObjectPtr<AActor> Instigator;

	// Optional external action target (e.g. grandpa dispatches to door)
	// If valid, DispatchActions routes to this actor instead of owner.
	TWeakObjectPtr<AActor> ActionTarget;

	// Pending dialogue tree switch, applied after current conversation ends.
	TSoftObjectPtr<UDialogueTreeDefinition> PendingDialogueTreeAsset;

	// Used to guard against same-frame interaction rebroadcast closing dialogue immediately.
	uint64 ConversationStartFrame = 0;

	const FDialogueNode* GetCurrentNode() const;
	void NavigateToNode(const FString& NodeId);
	bool EnsureTreeLoaded();
	bool CheckCondition(const FString& ConditionStr) const;
	bool IsOptionConditionMet(const FDialogueOption& Option) const;
	void DispatchActions(const FString& Context, const TArray<FString>& Actions);
	void DispatchToActor(AActor* Actor, const FString& Context, const TArray<FString>& Actions);
};
