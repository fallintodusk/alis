#include "ProjectWorldLayerPipeline.h"

#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

namespace
{
	const FProjectWorldRealizationLayer* FindLayer(const FProjectWorldRealizationProfile& Profile,
		const FString& LayerId)
	{
		return Profile.Layers.FindByPredicate([&LayerId](const FProjectWorldRealizationLayer& Layer)
		{
			return Layer.LayerId == LayerId;
		});
	}

	const IProjectWorldLayerProducer* FindModularProducer(
		const TArray<const IProjectWorldLayerProducer*>& Producers,
		const FProjectWorldRealizationLayer& Layer)
	{
		const IProjectWorldLayerProducer* const* Found = Producers.FindByPredicate(
			[&Layer](const IProjectWorldLayerProducer* Producer)
			{
				const FProjectWorldLayerProducerDeclaration& Declaration = Producer->GetDeclaration();
				return Declaration.GeneratorId == Layer.GeneratorId &&
					Declaration.GeneratorVersion == Layer.GeneratorVersion;
			});
		return Found != nullptr ? *Found : nullptr;
	}
}

namespace ProjectWorldLayerPipeline
{
	bool ApplyLayers(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlays,
		const TMap<FName, UMaterialInterface*>& PresentationMaterials,
		FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!ProjectWorldLayerProducerRegistry::GetAll(Producers, OutError))
		{
			return false;
		}
		for (const FString& LayerId : Profile.TopologicalLayerIds)
		{
			const FProjectWorldRealizationLayer* Layer = FindLayer(Profile, LayerId);
			if (Layer == nullptr || !Layer->IsGenerated())
			{
				OutError = FString::Printf(TEXT("Executable layer is missing: %s"), *LayerId);
				return false;
			}
			if (const IProjectWorldLayerProducer* Producer = FindModularProducer(Producers, *Layer))
			{
				FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
					[&LayerId](const FProjectWorldLayerInventory& Candidate)
					{ return Candidate.LayerId == LayerId; });
				if (Inventory == nullptr)
				{
					OutError = FString::Printf(TEXT("Layer inventory is missing: %s"), *LayerId);
					return false;
				}
				const FProjectWorldLayerInputs Inputs{Bundle, Profile, *Layer, AuthoredOverlays};
				const FProjectWorldLayerApplyContext Context{
					World, Inputs, Inventory->FinalDirtyUnits, PresentationMaterials};
				if (!Producer->Apply(Context, *Inventory, OutResult, OutError))
				{
					OutError = FString::Printf(TEXT("Layer %s (%s:v%d): %s"),
						*LayerId, *Layer->GeneratorId, Layer->GeneratorVersion, *OutError);
					return false;
				}
				continue;
			}
			OutError = FString::Printf(TEXT("Layer producer is missing: %s:v%d"),
				*Layer->GeneratorId, Layer->GeneratorVersion);
			return false;
		}
		return true;
	}

	bool DeleteLayers(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlays,
		FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!ProjectWorldLayerProducerRegistry::GetAll(Producers, OutError))
		{
			return false;
		}
		const TMap<FName, UMaterialInterface*> EmptyMaterials;
		const TArray<FString> EmptyDirtyUnits;
		for (int32 Index = Profile.TopologicalLayerIds.Num() - 1; Index >= 0; --Index)
		{
			const FString& LayerId = Profile.TopologicalLayerIds[Index];
			const FProjectWorldRealizationLayer* Layer = FindLayer(Profile, LayerId);
			if (Layer == nullptr || !Layer->IsGenerated())
			{
				OutError = FString::Printf(TEXT("Executable layer is missing: %s"), *LayerId);
				return false;
			}
			if (const IProjectWorldLayerProducer* Producer = FindModularProducer(Producers, *Layer))
			{
				const FProjectWorldLayerInputs Inputs{Bundle, Profile, *Layer, AuthoredOverlays};
				const FProjectWorldLayerApplyContext Context{World, Inputs, EmptyDirtyUnits, EmptyMaterials};
				if (!Producer->Delete(Context, OutResult, OutError))
				{
					return false;
				}
				continue;
			}
			OutError = FString::Printf(TEXT("Layer producer is missing: %s:v%d"),
				*Layer->GeneratorId, Layer->GeneratorVersion);
			return false;
		}
		return true;
	}
}
