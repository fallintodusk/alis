#pragma once

#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldBuildingLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldBuildingLayerProducer& Get();
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
	FProjectWorldBuildingLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
