// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/ProjectDialogueTypes.h"
#include "DialogueTreeDefinition.generated.h"

/**
 * Auto-generated UAsset from dialogue JSON files.
 * Created by ProjectDefinitionGenerator from Data/Sources/*.json.
 *
 * Maps directly to dialogue JSON schema:
 *   { "id": "...", "startNode": "...", "nodes": { "id": { ... } } }
 */
UCLASS(BlueprintType)
class PROJECTDIALOGUE_API UDialogueTreeDefinition : public UObject
{
	GENERATED_BODY()

public:
	// Unique tree identifier (maps to JSON "id")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName TreeId;

	// First node to display
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FString StartNode;

	// All nodes keyed by semantic ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TMap<FString, FDialogueNode> Nodes;

	// --- Generator metadata (set by ProjectDefinitionGenerator) ---

	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	bool bGenerated = false;

	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	int32 GeneratorVersion = 0;

	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonPath;

	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonHash;

	// --- Helpers ---

	const FDialogueNode* FindNode(const FString& NodeId) const
	{
		return Nodes.Find(NodeId);
	}

	bool IsValid() const
	{
		return !StartNode.IsEmpty() && Nodes.Contains(StartNode);
	}
};
