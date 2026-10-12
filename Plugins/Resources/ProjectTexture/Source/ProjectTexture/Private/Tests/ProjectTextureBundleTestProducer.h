// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectTextureBundleProducer.h"
#include "RenderCommandFence.h"
#include "ProjectTextureBundleTestProducer.generated.h"

UCLASS()
class UProjectTextureBundleTestProducer final : public UProjectTextureBundleProducer
{
	GENERATED_BODY()
public:
	FString Identity;
	int32 Starts = 0;
	bool bOmitSibling = false;
	virtual FString GetStructuralIdentity() const override { return Identity; }
	virtual int64 GetPeakBytes() const override { return 1024 * 1024; }
	virtual bool Validate(const UProjectTextureRuntimeCatalog* Catalog, FString& Error) const override;
	virtual bool Begin(FString& Error) override;
	virtual EProjectTextureProducerState Poll(float Delta, FString& Error) override;
	virtual TArray<UTextureRenderTarget2D*> GetOutputs() const override;
	virtual void CancelAndDrain() override;
	virtual void ReleaseOutputs() override;
private:
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> Outputs;
	FRenderCommandFence Fence;
};
