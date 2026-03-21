// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Types/LootEntryTypes.h"
#include "Types/ContainerSessionTypes.h"
#include "UObject/Interface.h"
#include "IWorldContainerSessionSource.generated.h"

class AActor;

/**
 * Thin world-side session contract for meaningful storage containers.
 *
 * This interface does not own inventory orchestration. It only exposes:
 * - stable world container identity
 * - support for session modes
 * - authority-side claim/release for single-opener FullOpen behavior
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UWorldContainerSessionSource : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IWorldContainerSessionSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	FText GetContainerDisplayLabel() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	FInventoryContainerView GetContainerView() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	TArray<FInventoryEntryView> GetContainerEntryViews() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	FWorldContainerKey GetWorldContainerKey() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool SupportsContainerSession(EContainerSessionMode Mode) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool TryBeginContainerSession(AActor* Instigator, FGuid SessionId, EContainerSessionMode Mode, FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool ConsumeContainerEntries(FGuid SessionId, const TArray<FContainerEntryTransfer>& Entries, FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool CanStoreContainerEntries(FGuid SessionId, const TArray<FContainerEntryTransfer>& Entries, FText& OutError) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool StoreContainerEntries(FGuid SessionId, const TArray<FContainerEntryTransfer>& Entries, FText& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool EndContainerSession(FGuid SessionId);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ContainerSession")
	bool HasActiveFullOpenSession() const;
};
