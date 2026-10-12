// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "RHIResources.h"
#include "ProjectTextureAsyncBundle.generated.h"

class UProjectTextureBundleProducer;
class UProjectTextureRuntimeCatalog;
class UTextureRenderTarget2D;

USTRUCT()
struct FProjectTextureAsyncBundle
{
	GENERATED_BODY()
	UPROPERTY(Transient) TObjectPtr<UProjectTextureBundleProducer> Producer;
	UPROPERTY(Transient) TObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;
	TArray<FString> ReadyKeys;
	FString DescriptorIdentity;
	FString Error;
	int64 PeakBytes = 0;
	bool bCopying = false;
	bool bCancelled = false;
	FGPUFenceRHIRef Fence;
};
