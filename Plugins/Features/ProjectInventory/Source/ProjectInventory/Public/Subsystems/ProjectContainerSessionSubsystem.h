// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Types/ContainerSessionTypes.h"
#include "ProjectContainerSessionSubsystem.generated.h"

class UProjectInventoryComponent;

/**
 * Local-player-owned world-container session manager.
 *
 * This subsystem owns the player-side runtime session handle and defers
 * authority-side single-opener enforcement to the world container source.
 */
UCLASS()
class PROJECTINVENTORY_API UProjectContainerSessionSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Inventory|ContainerSession")
	bool OpenWorldContainerSession(
		AActor* TargetActor,
		EContainerSessionMode Mode,
		FContainerSessionHandle& OutHandle,
		FText& OutError);

	bool RegisterOpenedSession(
		AActor* TargetActor,
		UObject* SourceObject,
		AActor* Instigator,
		const FContainerSessionHandle& Handle,
		FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|ContainerSession")
	bool CloseSession(const FContainerSessionHandle& Handle);

	bool CloseSessionLocal(const FContainerSessionHandle& Handle);
	void CloseAllSessions();

	UFUNCTION(BlueprintCallable, Category = "Inventory|ContainerSession")
	bool TakeAllFromWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|ContainerSession")
	bool TakeEntryFromWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|ContainerSession")
	bool StoreInventoryEntryInWorldContainerSession(
		const FContainerSessionHandle& Handle,
		UProjectInventoryComponent* Inventory,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);

	bool GetSessionContainerView(
		const FContainerSessionHandle& Handle,
		FText& OutLabel,
		FInventoryContainerView& OutContainerView,
		TArray<FInventoryEntryView>& OutEntries) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool HasAnyActiveSession() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool HasActiveFullOpenSession() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|ContainerSession")
	bool IsSessionActive(const FContainerSessionHandle& Handle) const;

private:
	struct FActiveContainerSession
	{
		FContainerSessionHandle Handle;
		TWeakObjectPtr<AActor> TargetActor;
		TWeakObjectPtr<UObject> SourceObject;
		TWeakObjectPtr<AActor> Instigator;
	};

	UObject* ResolveSessionSource(AActor* TargetActor) const;
	AActor* ResolveSessionInstigator() const;

	TMap<FGuid, FActiveContainerSession> ActiveSessions;
};
