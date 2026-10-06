// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizationGeneratorRegistry.h"

#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldLayerProducerRegistry.h"


namespace ProjectWorldRealizationGeneratorRegistry
{
	bool IsRegistered(const FString& GeneratorId, int32 GeneratorVersion, EProjectWorldLayerKind LayerKind)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		FString RegistryError;
		if (!ProjectWorldLayerProducerRegistry::GetAll(Producers, RegistryError))
		{
			return false;
		}
		for (const IProjectWorldLayerProducer* Producer : Producers)
		{
			const FProjectWorldLayerProducerDeclaration& Declaration = Producer->GetDeclaration();
			if (Declaration.GeneratorId == GeneratorId && Declaration.GeneratorVersion == GeneratorVersion)
			{
				return Declaration.LayerKind == LayerKind;
			}
		}
		return false;
	}

	bool ValidateSettings(const FProjectWorldRealizationLayer& Layer, FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!ProjectWorldLayerProducerRegistry::GetAll(Producers, OutError))
		{
			return false;
		}
		for (const IProjectWorldLayerProducer* Producer : Producers)
		{
			const FProjectWorldLayerProducerDeclaration& Declaration = Producer->GetDeclaration();
			if (Declaration.GeneratorId == Layer.GeneratorId && Declaration.GeneratorVersion == Layer.GeneratorVersion)
			{
				return ProjectWorldLayerProducerRegistry::ValidateLayer(Layer, OutError);
			}
		}
		OutError = FString::Printf(TEXT("Generator settings are not registered for: %s:v%d"),
			*Layer.GeneratorId, Layer.GeneratorVersion);
		return false;
	}
}
