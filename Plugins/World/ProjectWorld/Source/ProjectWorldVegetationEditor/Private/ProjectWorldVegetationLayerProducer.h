#pragma once

#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldVegetationLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldVegetationLayerProducer& Get();
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
	FProjectWorldVegetationLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
