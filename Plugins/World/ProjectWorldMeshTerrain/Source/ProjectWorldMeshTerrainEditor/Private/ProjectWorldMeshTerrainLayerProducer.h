#pragma once

#include "ProjectWorldLayerProducerRegistry.h"

class FProjectWorldMeshTerrainLayerProducer final : public IProjectWorldLayerProducer
{
public:
	static FProjectWorldMeshTerrainLayerProducer& Get();
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
	virtual bool Delete(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldRealizationResult& OutResult, FString& OutError) const override;

private:
	FProjectWorldMeshTerrainLayerProducer();
	FProjectWorldLayerProducerDeclaration Declaration;
};
