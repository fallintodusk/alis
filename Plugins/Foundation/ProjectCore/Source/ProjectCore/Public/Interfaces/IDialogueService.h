// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDialogueService.generated.h"

class AActor;

/**
 * Read-only snapshot of a dialogue option for UI consumption.
 * Created by IDialogueService from the active component's data.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FDialogueOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	// True if condition prevents selection (greyed out in UI)
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bLocked = false;

	// True when option has any condition string.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bHasCondition = false;

	// True when option has condition and condition currently passes.
	// Useful for subtle highlight of newly available choices.
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bConditionSatisfied = false;

	bool operator==(const FDialogueOptionView& Other) const
	{
		return Index == Other.Index
			&& Text.EqualTo(Other.Text)
			&& bLocked == Other.bLocked
			&& bHasCondition == Other.bHasCondition
			&& bConditionSatisfied == Other.bConditionSatisfied;
	}
};

// Broadcast when dialogue state changes (node changed, started, ended)
DECLARE_MULTICAST_DELEGATE(FOnDialogueStateChanged);
// Broadcast semantic dialogue signal tag for current node (for data-driven integrations).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueSignal, const FName& /*SignalTag*/);

/**
 * Read-only dialogue service interface for UI consumers.
 *
 * Registered via FProjectServiceLocator by ProjectDialogue module.
 * Resolved by ProjectDialogueUI ViewModel.
 * Wraps the currently active UProjectDialogueComponent.
 *
 * Usage:
 *   if (auto Service = FProjectServiceLocator::Resolve<IDialogueService>())
 *   {
 *       Service->OnDialogueStateChanged().AddUObject(this, &UMyVM::HandleStateChanged);
 *       Service->OnDialogueSignal().AddRaw(this, &FMySystem::HandleDialogueSignal);
 *   }
 */
class PROJECTCORE_API IDialogueService : public TSharedFromThis<IDialogueService>
{
public:
	virtual ~IDialogueService() = default;

	static FName ServiceKey() { return FName(TEXT("IDialogueService")); }

	// Query state
	virtual bool IsDialogueActive() const = 0;
	virtual FName GetCurrentTreeId() const = 0;
	virtual FName GetCurrentNodeId() const = 0;
	virtual FText GetCurrentSpeaker() const = 0;
	virtual FText GetCurrentText() const = 0;
	virtual void GetCurrentOptions(TArray<FDialogueOptionView>& OutOptions) const = 0;
	virtual bool IsCurrentNodeTerminal() const = 0;
	virtual bool CurrentNodeHasOptions() const = 0;

	// Commands
	virtual void SelectOption(int32 OptionIndex) = 0;
	virtual void AdvanceOrEnd() = 0;
	virtual void EndDialogue() = 0;
	virtual bool IsDialogueActiveForActor(const AActor* /*Actor*/) const { return false; }
	virtual const AActor* GetActiveDialogueOwner() const { return nullptr; }

	/**
	 * Optional producer-side hook: set currently active dialogue source component.
	 * Default no-op keeps custom implementations source-agnostic.
	 */
	virtual void ActivateDialogueComponent(class UActorComponent* /*Component*/) {}

	// Events
	virtual FOnDialogueStateChanged& OnDialogueStateChanged() = 0;
	virtual FOnDialogueSignal& OnDialogueSignal() = 0;
};
