// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/ContainerSessionTypes.h"
#include "UObject/Interface.h"
#include "IInventoryWorldContainerTransferBridge.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInventoryWorldContainerTransferBridge : public UInterface
{
	GENERATED_BODY()
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnInventoryWorldContainerSessionOpenedNative,
	UObject*,
	const FContainerSessionHandle&);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnInventoryWorldContainerSessionClosedNative,
	const FContainerSessionHandle&);

/**
 * Player-inventory-side bridge for moving items between inventory and an
 * already-opened world-container session.
 *
 * UI consumers use this semantic contract instead of calling feature-module
 * internals or sequencing remove/add mutations on their own.
 */
class PROJECTCORE_API IInventoryWorldContainerTransferBridge
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool RequestOpenWorldContainerSession(
		AActor* TargetActor,
		EContainerSessionMode Mode,
		FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool RequestCloseWorldContainerSession(
		const FContainerSessionHandle& SessionHandle,
		FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool GetActiveWorldContainerSession(
		AActor*& OutTargetActor,
		FContainerSessionHandle& OutSessionHandle) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool TransferWorldContainerEntryToInventory(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool StoreInventoryEntryInWorldContainer(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory|WorldContainer")
	bool TakeAllFromWorldContainer(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		FText& OutError);

	virtual FOnInventoryWorldContainerSessionOpenedNative& OnWorldContainerSessionOpenedNative() = 0;
	virtual FOnInventoryWorldContainerSessionClosedNative& OnWorldContainerSessionClosedNative() = 0;
};
