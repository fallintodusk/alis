// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ProjectMenuExperienceLaunchBinding.generated.h"

class UButton;
class UMenuMainComposerSubsystem;

/**
 * Carries one configured experience id through UButton::OnClicked.
 *
 * OnClicked is a no-argument dynamic delegate, so a parameterized launch needs a small
 * bound object per button. That is the whole purpose of this class: it exists so the menu
 * needs no per-experience UFUNCTION, and adding an experience stays a config change.
 *
 * Deliberately not an action framework - it launches experiences and nothing else.
 * Unrelated menu actions keep their own handlers.
 */
UCLASS()
class PROJECTMENUMAIN_API UProjectMenuExperienceLaunchBinding : public UObject
{
	GENERATED_BODY()

public:
	/** Bind this carrier to one button. The button is retained weakly for later unbinding. */
	void BindTo(
		UButton& InButton,
		UMenuMainComposerSubsystem* InComposer,
		const FString& InExperienceId,
		const FString& InMode);

	/**
	 * Remove this carrier's delegate from its button.
	 *
	 * Required before the owner drops its reference: dropping the array entry alone leaves
	 * the delegate on the button, and AddUniqueDynamic cannot dedupe a freshly created
	 * carrier, so a rebound menu would fire one click twice.
	 */
	void Unbind();

	/** Bound to one button's OnClicked. */
	UFUNCTION()
	void HandleClicked();

	const FString& GetExperienceId() const { return ExperienceId; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMenuMainComposerSubsystem> Composer;

	UPROPERTY(Transient)
	FString ExperienceId;

	UPROPERTY(Transient)
	FString Mode;
};
