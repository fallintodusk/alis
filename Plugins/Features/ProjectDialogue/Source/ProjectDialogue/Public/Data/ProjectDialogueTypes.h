// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectDialogueTypes.generated.h"

/**
 * Structured condition for dialogue option visibility.
 *
 * Inventory example:
 *   "condition": { "type": "inventory", "id": "Cigarette", "quantity": 3, "exact": false }
 *
 * Tag example:
 *   "condition": { "type": "tag", "id": "Quest.ElderTrust", "exact": true }
 *
 * Fields:
 *   type     -- "inventory" or "tag"
 *   id       -- item ObjectDefinition id (inventory) or gameplay tag name (tag)
 *   quantity -- minimum required count (inventory only; ignored for tag)
 *   exact    -- inventory: exact id vs prefix/family match
 *              tag: exact tag vs hierarchy-aware match
 */
USTRUCT(BlueprintType)
struct PROJECTDIALOGUE_API FDialogueCondition
{
	GENERATED_BODY()

	// Condition type: "inventory" or "tag"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Type;

	// Item ObjectDefinition id (inventory) or gameplay tag name (tag)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Id;

	// Minimum quantity required (inventory only; ignored for tag conditions)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	int32 Quantity = 1;

	// inventory: true = exact id, false = prefix/family match
	// tag: true = exact tag only, false = hierarchy-aware parent match
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool Exact = true;

	bool IsValid() const { return !Type.IsEmpty() && !Id.IsEmpty(); }
};

/**
 * A single player choice within a dialogue node.
 *
 * JSON example:
 *   {
 *     "text": "I have water.",
 *     "next": "give_water",
 *     "condition": { "type": "inventory", "id": "WaterBottle" }
 *   }
 */
USTRUCT(BlueprintType)
struct PROJECTDIALOGUE_API FDialogueOption
{
	GENERATED_BODY()

	// Choice display text
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Text;

	// Target node ID. "$end" or empty = end dialogue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Next;

	// Structured condition for option visibility (empty = always visible)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueCondition Condition;

	bool IsEndOption() const
	{
		return Next.IsEmpty() || Next == TEXT("$end");
	}
};

/**
 * A single node in the dialogue tree.
 *
 * Nodes without options use "next" for auto-advance.
 * Nodes with neither options nor next are terminal (end dialogue).
 */
USTRUCT(BlueprintType)
struct PROJECTDIALOGUE_API FDialogueNode
{
	GENERATED_BODY()

	// Speaker display name (empty for objects/narration)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Speaker;

	// Dialogue text to display
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Text;

	// Player choices (empty = use Next for auto-advance, or terminal)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueOption> Options;

	// Auto-advance target (used when Options is empty)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString Next;

	// Generic actions dispatched on node entry (e.g. "audio.play:katyusha")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FString> Actions;

	bool IsTerminal() const
	{
		return Options.Num() == 0 && (Next.IsEmpty() || Next == TEXT("$end"));
	}

	bool HasOptions() const
	{
		return Options.Num() > 0;
	}
};
