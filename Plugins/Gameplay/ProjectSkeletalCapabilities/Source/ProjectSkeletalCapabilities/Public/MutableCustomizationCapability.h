// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/AssemblyTypes.h"
#include "MutableCustomizationCapability.generated.h"

class UCustomizableObject;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;
class USkeletalMeshComponent;
class IAssemblyCapability;

/**
 * Mutable customization adapter for the skeletal assembly framework.
 *
 * Orchestrates one CustomizableObject (CO) and its CustomizableObjectInstance (COI)
 * across multiple CustomizableSkeletalComponents (CSKs). Each CSK targets a
 * skeletal mesh component identified by AssemblyRole tag (e.g. BodyCustomization,
 * HeadCustomization).
 *
 * Capability ID: "MutableCustomization"
 * Scope: actor (one instance per actor, discovers targets via mesh role tags)
 *
 * ## Properties (set from JSON via SetPropertyByName)
 *
 * - MutableInstance: soft path to a pre-saved COI asset (preferred -- carries compiled model)
 * - MutableSource: soft path to the CustomizableObject asset (fallback when no saved COI)
 * - ComponentNameMapping: explicit role->CO component name mapping,
 *   format: "RoleA=CompNameA,RoleB=CompNameB" (e.g. "BodyCustomization=Body,HeadCustomization=Head")
 * - DefaultParameters: enum parameters to set after instance creation before first update,
 *   format: "ParamName=OptionName,..." (only works when CO is compiled)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTSKELETALCAPABILITIES_API UMutableCustomizationCapability : public UActorComponent
{
	GENERATED_BODY()

public:
	UMutableCustomizationCapability();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void DiscoverCustomizationTargets();
	void InitializeMutable();
	void TeardownMutable();
	void OnAssemblyStateChanged(EAssemblyState NewState);
	void OnMutableInstanceUpdated(UCustomizableObjectInstance* Instance);

	// Parse ComponentNameMapping string into the RoleToComponentName map
	void ParseComponentNameMapping();

	// Parse DefaultParameters string into the ParsedDefaultParameters array
	void ParseDefaultParameters();

	// Apply parsed default parameters to the COI before first async update
	void ApplyDefaultParameters();

	// Resolve CO component name for a given role
	FName ResolveComponentName(FName Role, const TArray<FName>& COComponentNames, int32 TargetIndex) const;

	// CO asset path (used when MutableInstance is not set)
	UPROPERTY(EditAnywhere, Category = "Mutable")
	TSoftObjectPtr<UCustomizableObject> MutableSource;

	// Pre-saved COI asset path. When set, clone this instance instead of
	// creating a fresh one from MutableSource. The saved COI carries the
	// compiled model and parameter selections from the editor.
	UPROPERTY(EditAnywhere, Category = "Mutable")
	TSoftObjectPtr<UCustomizableObjectInstance> MutableInstance;

	// Explicit role -> CO component name mapping.
	// Format: "BodyCustomization=Body,HeadCustomization=Head,LocalBodyCustomization=Body"
	// When set, each CSK gets the ComponentName matching its role.
	// When empty, falls back to ordinal assignment (fragile).
	UPROPERTY(EditAnywhere, Category = "Mutable")
	FString ComponentNameMapping;

	// Parsed mapping from ComponentNameMapping string
	TMap<FName, FName> RoleToComponentName;

	// Default enum parameters applied after COI creation.
	// Format: "Shirts=TShirt,Jacket=Jacket_A"
	UPROPERTY(EditAnywhere, Category = "Mutable")
	FString DefaultParameters;

	// Parsed default parameter pairs (ParamName, OptionName)
	TArray<TPair<FString, FString>> ParsedDefaultParameters;

	UPROPERTY()
	TObjectPtr<UCustomizableObjectInstance> CachedInstance;

	UPROPERTY()
	TArray<TObjectPtr<UCustomizableSkeletalComponent>> CreatedCSKs;

	TArray<TPair<TWeakObjectPtr<USkeletalMeshComponent>, FName>> CustomizationTargets;

	TWeakObjectPtr<UActorComponent> CachedAssemblyComponent;
	FDelegateHandle AssemblyStateHandle;

	bool bMutableInitialized = false;

	// True when DefaultParameters were set before CO compilation finished.
	// OnMutableInstanceUpdated will retry once after the first async update.
	bool bNeedsDeferredParamApply = false;
};
