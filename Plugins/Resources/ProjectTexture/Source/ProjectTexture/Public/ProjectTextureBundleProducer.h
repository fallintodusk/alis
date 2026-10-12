// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ProjectTextureBundleProducer.generated.h"

class UProjectTextureRuntimeCatalog;
class UTextureRenderTarget2D;

enum class EProjectTextureProducerState : uint8 { Pending, Ready, Failed };

// Optional execution adapter. ProjectTexture retains admission, sharing and output ownership.
// Implementations retain all in-flight resources and publish a complete immutable bundle.
UCLASS(Abstract)
class PROJECTTEXTURE_API UProjectTextureBundleProducer : public UObject
{
	GENERATED_BODY()
public:
	virtual FString GetStructuralIdentity() const PURE_VIRTUAL(UProjectTextureBundleProducer::GetStructuralIdentity, return FString(););
	// Includes all native intermediates, pools and the owner's stable full-mip outputs.
	virtual int64 GetPeakBytes() const PURE_VIRTUAL(UProjectTextureBundleProducer::GetPeakBytes, return 0;);
	virtual bool Validate(const UProjectTextureRuntimeCatalog* Catalog, FString& Error) const
		PURE_VIRTUAL(UProjectTextureBundleProducer::Validate, return false;);
	virtual bool Begin(FString& Error) PURE_VIRTUAL(UProjectTextureBundleProducer::Begin, return false;);
	virtual EProjectTextureProducerState Poll(float Delta, FString& Error)
		PURE_VIRTUAL(UProjectTextureBundleProducer::Poll, return EProjectTextureProducerState::Failed;);
	virtual TArray<UTextureRenderTarget2D*> GetOutputs() const
		PURE_VIRTUAL(UProjectTextureBundleProducer::GetOutputs, return {};);
	// Called outside callbacks. Drain unfinished work before releasing GPU resources.
	virtual void CancelAndDrain() PURE_VIRTUAL(UProjectTextureBundleProducer::CancelAndDrain, );
	virtual void ReleaseOutputs() PURE_VIRTUAL(UProjectTextureBundleProducer::ReleaseOutputs, );
};
