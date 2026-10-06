#pragma once

#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldRoadLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldRoadLayerProducer& Get();
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
	FProjectWorldRoadLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
