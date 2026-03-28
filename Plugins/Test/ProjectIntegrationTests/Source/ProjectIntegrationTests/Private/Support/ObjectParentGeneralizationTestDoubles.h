// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "IDefinitionApplicable.h"
#include "ObjectParentGeneralizationTestDoubles.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPrimaryDataAsset;

UCLASS()
class PROJECTINTEGRATIONTESTS_API UProjectInteractionCounterCapabilityComponent
	: public UActorComponent
	, public IInteractableComponentTargetInterface
{
	GENERATED_BODY()

public:
	UProjectInteractionCounterCapabilityComponent();

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 InteractPriority = 0;

	UPROPERTY(EditAnywhere, Category = "Test")
	FText InteractionLabel;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bOnInteractReturnValue = true;

	UPROPERTY(EditAnywhere, Category = "Test")
	float HoldDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Test")
	FText ActiveInteractionLabel;

	UPROPERTY(Transient)
	int32 InteractionCallCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LastInstigator = nullptr;

	// IInteractableComponentTargetInterface
	virtual int32 GetInteractPriority_Implementation() const override;
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FInteractionPrompt GetInteractionPrompt_Implementation(AActor* Instigator) const override;
	virtual FText GetInteractionLabel_Implementation() const override;
	virtual FInteractionExecutionSpec GetInteractionExecutionSpec_Implementation(AActor* Instigator) const override;
	virtual UPrimitiveComponent* GetInteractTargetMesh_Implementation() const override;
	virtual void SetInteractTargetMesh_Implementation(UPrimitiveComponent* TargetMesh) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BoundTargetMesh = nullptr;
};

UCLASS()
class PROJECTINTEGRATIONTESTS_API AProjectInteractionActorInterfaceTestActor
	: public AActor
	, public IInteractableTargetInterface
{
	GENERATED_BODY()

public:
	AProjectInteractionActorInterfaceTestActor();

	UPROPERTY(EditAnywhere, Category = "Test")
	FText FocusLabel;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bOnInteractReturnValue = true;

	UPROPERTY(EditAnywhere, Category = "Test")
	float HoldDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Test")
	FText ActiveInteractionLabel;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bReturnHighlightMesh = true;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bReturnValidFocus = true;

	UPROPERTY(Transient)
	int32 OnInteractCallCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LastInstigator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> LastHitComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UStaticMeshComponent> TestMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UProjectInteractionCounterCapabilityComponent> FallbackCapability = nullptr;

	// IInteractableTargetInterface
	virtual bool OnInteract_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent) override;
	virtual FInteractionFocusInfo GetFocusInfo_Implementation(UPrimitiveComponent* HitComponent) const override;
	virtual FInteractionExecutionSpec GetInteractionExecutionSpec_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent) const override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root = nullptr;
};

UCLASS()
class PROJECTINTEGRATIONTESTS_API AProjectSyncDefinitionApplicableTestActor
	: public AActor
	, public IDefinitionApplicable
{
	GENERATED_BODY()

public:
	AProjectSyncDefinitionApplicableTestActor();

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bApplyDefinitionResult = true;

	UPROPERTY(Transient)
	int32 ApplyDefinitionCallCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPrimaryDataAsset> LastAppliedDefinition = nullptr;

	// IDefinitionApplicable
	virtual bool ApplyDefinition_Implementation(UPrimaryDataAsset* Definition) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root = nullptr;
};

UCLASS()
class PROJECTINTEGRATIONTESTS_API UProjectTestInventoryComponent
	: public UActorComponent
	, public IInventoryReadOnly
	, public IProjectActionReceiver
{
	GENERATED_BODY()

public:
	UProjectTestInventoryComponent();

	void SetItemQuantity(const FPrimaryAssetId& ItemId, int32 Quantity);
	int32 GetItemQuantity(const FPrimaryAssetId& ItemId) const;

	UPROPERTY(Transient)
	int32 ConsumedActionCount = 0;

	UPROPERTY(Transient)
	FPrimaryAssetId LastConsumedItemId;

	// IInventoryReadOnly
	virtual void GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const override;
	virtual void GetContainersView(TArray<FInventoryContainerView>& OutContainers) const override;
	virtual float GetCurrentWeight() const override { return 0.0f; }
	virtual float GetMaxWeight() const override { return 1000.0f; }
	virtual float GetCurrentVolume() const override { return 0.0f; }
	virtual float GetMaxVolume() const override { return 1000.0f; }
	virtual int32 GetMaxSlots() const override { return 100; }
	virtual FOnInventoryViewChanged& OnInventoryViewChanged() override { return InventoryChangedDelegate; }
	virtual FOnInventoryErrorNative& OnInventoryErrorNative() override { return InventoryErrorDelegate; }
	virtual bool ContainsItem(FPrimaryAssetId ItemId, int32 MinQuantity = 1) const override;

	// IProjectActionReceiver
	virtual void HandleAction(const FString& Context, const FString& Action) override;

private:
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, int32> Quantities;

	FOnInventoryViewChanged InventoryChangedDelegate;
	FOnInventoryErrorNative InventoryErrorDelegate;
};
