// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ObjectDefinition.h"
#include "DefinitionCapabilityContractTestDoubles.generated.h"

UCLASS()
class PROJECTINTEGRATIONTESTS_API UProjectDefinitionCapabilityContractTestComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSoftObjectPtr<UObject> PrimaryAsset;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<TSoftObjectPtr<UObject>> RelatedAssets;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSoftClassPtr<UObject> GeneratedClass;

	UPROPERTY(EditAnywhere, Category = "Test")
	FString PathLookingText;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bEnabled = false;
};

UCLASS()
class PROJECTINTEGRATIONTESTS_API UProjectCapabilityBundleTestDefinition
	: public UObjectDefinition
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	FAssetBundleData& GetMutableAssetBundleData()
	{
		return AssetBundleData;
	}
#endif
};
