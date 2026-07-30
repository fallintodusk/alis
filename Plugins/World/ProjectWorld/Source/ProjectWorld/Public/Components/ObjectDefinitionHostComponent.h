// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IDefinitionIdProvider.h"
#include "Interfaces/IObjectDefinitionHost.h"
#include "ObjectDefinitionHostComponent.generated.h"

/**
 * Generic metadata host for ObjectDefinition-driven actors.
 *
 * Used when spawned actor class cannot or should not derive from AProjectWorldActor.
 */
UCLASS(ClassGroup = (ProjectWorld), meta = (BlueprintSpawnableComponent))
class PROJECTWORLD_API UObjectDefinitionHostComponent
	: public UActorComponent
	, public IObjectDefinitionHostInterface
	, public IDefinitionIdProvider
{
	GENERATED_BODY()

public:
	UObjectDefinitionHostComponent();

	// -------------------------------------------------------------------------
	// IObjectDefinitionHostInterface
	// -------------------------------------------------------------------------

	virtual FPrimaryAssetId GetHostedObjectDefinitionId_Implementation() const override;
	virtual void SetHostedObjectDefinitionId_Implementation(const FPrimaryAssetId& DefinitionId) override;
	virtual FSoftObjectPath GetHostedDefinitionAssetPath_Implementation() const override;
	virtual void SetHostedDefinitionAssetPath_Implementation(const FSoftObjectPath& AssetPath) override;
	virtual FString GetHostedAppliedStructureHash_Implementation() const override;
	virtual void SetHostedAppliedStructureHash_Implementation(const FString& StructureHash) override;
	virtual FString GetHostedAppliedContentHash_Implementation() const override;
	virtual void SetHostedAppliedContentHash_Implementation(const FString& ContentHash) override;

	// -------------------------------------------------------------------------
	// IDefinitionIdProvider
	// -------------------------------------------------------------------------

	virtual FPrimaryAssetId GetObjectDefinitionId_Implementation() const override;

	/** Stored ObjectDefinition id for this actor instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Definition")
	FPrimaryAssetId ObjectDefinitionId;

	/** Soft path to the ObjectDefinition asset (for Asset Registry ref tracking). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Definition")
	FSoftObjectPath DefinitionAssetPath;

	/** Stored structural hash for replace/reapply decisions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Definition")
	FString AppliedStructureHash;

	/** Stored content hash for idempotent reapply checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Definition")
	FString AppliedContentHash;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
