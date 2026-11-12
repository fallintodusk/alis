// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldConstructedLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldConstructedLayerProducer& Get();
	virtual const FProjectWorldLayerProducerDeclaration& GetDeclaration() const override {return Declaration;}
	virtual bool ValidateSettings(const FString& Json,FString& Error) const override;
	virtual bool HashUnitInputs(const FProjectWorldLayerInputs& Inputs,FProjectWorldLayerUnitInputs& Units,FString& Error) const override;
	virtual bool Apply(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error) const override;
	virtual bool CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error) const override;
private:
	FProjectWorldConstructedLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
