// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProjectUIDefinitions.h"
#include "ProjectUIRegistrySubsystem.generated.h"

class FJsonObject;

/**
 * Loads ui_definitions.json files and provides lookup by Id, slot, and layer.
 */
UCLASS()
class PROJECTUI_API UProjectUIRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Find a definition by Id. */
	const FProjectUIDefinition* FindDefinition(FName Id) const;

	/** Get definitions registered for a slot, sorted by priority (desc). */
	void GetDefinitionsForSlot(FGameplayTag SlotTag, TArray<const FProjectUIDefinition*>& OutDefinitions) const;

	/** Get definitions registered for a layer, sorted by priority (desc). */
	void GetDefinitionsForLayer(FGameplayTag LayerTag, TArray<const FProjectUIDefinition*>& OutDefinitions) const;

	/** Expose full map for diagnostics. */
	const TMap<FName, FProjectUIDefinition>& GetAllDefinitions() const { return DefinitionsById; }

private:
	void LoadDefinitions();
	bool LoadDefinitionsFromFile(const FString& FilePath, const FString& PluginName);
	bool ParseDefinitionObject(const TSharedPtr<FJsonObject>& Object, const FString& PluginName, const FString& SourceFilePath, FProjectUIDefinition& OutDefinition) const;

	FGameplayTag ResolveLayerTag(const FString& LayerString) const;
	FGameplayTag ResolveTag(const FString& TagString) const;
	EProjectWidgetInputMode ParseInputMode(const FString& InputString) const;
	EProjectUILoadPolicy ParseLoadPolicy(const FString& LoadString) const;
	EProjectUISpawnPolicy ParseSpawnPolicy(const FString& SpawnString) const;
	EProjectUIScope ParseScope(const FString& ScopeString) const;
	EProjectUISizePolicy ParseSizePolicy(const FString& SizeString) const;
	EProjectUIViewModelCreationPolicy ParseViewModelCreationPolicy(const FString& PolicyString) const;

	void SortDefinitionsByPriority(TArray<const FProjectUIDefinition*>& Definitions) const;

private:
	TMap<FName, FProjectUIDefinition> DefinitionsById;
	TMultiMap<FGameplayTag, FName> DefinitionIdsBySlot;
	TMultiMap<FGameplayTag, FName> DefinitionIdsByLayer;
};
