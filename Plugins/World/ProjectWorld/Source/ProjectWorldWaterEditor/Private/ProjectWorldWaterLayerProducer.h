#pragma once

#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldWaterLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldWaterLayerProducer& Get();
	virtual const FProjectWorldLayerProducerDeclaration& GetDeclaration() const override;
	virtual bool ValidateSettings(const FString& NormalizedSettings, FString& OutError) const override;
	virtual bool HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
		FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const override;
	virtual bool Apply(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
		FString& OutError) const override;
	virtual bool CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
		FString& OutError) const override;

private:
	FProjectWorldWaterLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
