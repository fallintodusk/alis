// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLayerInventory.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

namespace ProjectWorldLayerInventory
{
	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		bool bFirstApply,
		const FProjectWorldLayerDirtyInput* DirtyInput,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (!ProjectWorldLayerProducerRegistry::GetAll(Producers, OutError))
		{
			return false;
		}
		TMap<FString, TArray<FProjectWorldLayerInputInventory>> CurrentInputs;
		TMap<FString, TMap<FString, TSet<FString>>> DependencyUnitMappings;
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			if (!Layer.IsGenerated())
			{
				continue;
			}
			TArray<FProjectWorldLayerInputInventory>& LayerInputs = CurrentInputs.Add(Layer.LayerId);
			const IProjectWorldLayerProducer* const* Producer = Producers.FindByPredicate(
				[&Layer](const IProjectWorldLayerProducer* Candidate)
				{
					const FProjectWorldLayerProducerDeclaration& Declaration = Candidate->GetDeclaration();
					return Declaration.GeneratorId == Layer.GeneratorId &&
						Declaration.GeneratorVersion == Layer.GeneratorVersion;
				});
			if (Producer != nullptr)
			{
				const FProjectWorldLayerInputs Inputs{Bundle, Profile, Layer, AuthoredOverlaySet};
				FProjectWorldLayerUnitInputs UnitInputs;
				if (!(*Producer)->HashUnitInputs(Inputs, UnitInputs, OutError))
				{
					return false;
				}
				for (const TPair<FString, FString>& Unit : UnitInputs.HashByUnit)
				{
					LayerInputs.Add({Unit.Key, Unit.Value});
				}
				LayerInputs.Sort([](const auto& Left, const auto& Right)
				{
					return Left.UnitId < Right.UnitId;
				});
				for (const FString& DependencyId : Layer.DependsOn)
				{
					const FProjectWorldRealizationLayer* Dependency = Profile.Layers.FindByPredicate(
						[&DependencyId](const FProjectWorldRealizationLayer& Candidate)
						{ return Candidate.LayerId == DependencyId; });
					if (Dependency != nullptr && Dependency->DirtyGranularity != Layer.DirtyGranularity)
					{
						DependencyUnitMappings.Add(Layer.LayerId + TEXT("|") + DependencyId,
							UnitInputs.UnitsByCanonicalCell);
					}
				}
				continue;
			}
			OutError = FString::Printf(TEXT("Layer producer is missing: %s:v%d"),
				*Layer.GeneratorId, Layer.GeneratorVersion);
			return false;
		}

		FProjectWorldDirtyInputs DirtyInputs;
		DirtyInputs.bFirstApply = bFirstApply;
		DirtyInputs.DependencyUnitMappings = MoveTemp(DependencyUnitMappings);
		for (const TPair<FString, TArray<FProjectWorldLayerInputInventory>>& LayerInputs : CurrentInputs)
		{
			for (const FProjectWorldLayerInputInventory& Input : LayerInputs.Value)
			{
				DirtyInputs.ValidUnits.FindOrAdd(LayerInputs.Key).Add(Input.UnitId);
				DirtyInputs.OperatorValidUnits.FindOrAdd(LayerInputs.Key).Add(Input.UnitId);
			}
		}
		if (DirtyInput != nullptr)
		{
			if (DirtyInput->RealizationProfileId != Profile.ProfileId)
			{
				OutError = TEXT("Dirty input targets another realization profile.");
				return false;
			}
			if (DirtyInput->ProducerFingerprints.Num() != CurrentInputs.Num())
			{
				OutError = TEXT("Layer request producer fingerprints do not cover exactly the generated layers.");
				return false;
			}
			for (const TPair<FString, TArray<FProjectWorldLayerInputInventory>>& Pair : CurrentInputs)
			{
				if (!DirtyInput->ProducerFingerprints.Contains(Pair.Key))
				{
					OutError = TEXT("Layer request producer fingerprints do not cover exactly the generated layers.");
					return false;
				}
			}
			DirtyInputs.OperatorAdditions = DirtyInput->OperatorAdditions;
			DirtyInputs.IdentityDirtyLayers = DirtyInput->IdentityDirtyLayers;
			if (!bFirstApply)
			{
				for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
				{
					if (!Layer.IsGenerated())
					{
						continue;
					}
					const FProjectWorldLayerBaseIdentity* Base = DirtyInput->BaseLayers.Find(Layer.LayerId);
					if (Base == nullptr || Base->NormalizedLayerContractHash != Layer.ContractHash)
					{
						DirtyInputs.ComputedUnits.FindOrAdd(Layer.LayerId).Add(TEXT("*"));
						continue;
					}
					for (const TPair<FString, FString>& Prior : Base->CanonicalInputs)
					{
						DirtyInputs.ValidUnits.FindOrAdd(Layer.LayerId).Add(Prior.Key);
					}
					TMap<FString, FString> CurrentByUnit;
					for (const FProjectWorldLayerInputInventory& Input : CurrentInputs.FindChecked(Layer.LayerId))
					{
						CurrentByUnit.Add(Input.UnitId, Input.Hash);
						if (Base->CanonicalInputs.FindRef(Input.UnitId) != Input.Hash)
						{
							DirtyInputs.ComputedUnits.FindOrAdd(Layer.LayerId).Add(Input.UnitId);
						}
					}
					for (const TPair<FString, FString>& Prior : Base->CanonicalInputs)
					{
						if (!CurrentByUnit.Contains(Prior.Key))
						{
							DirtyInputs.ComputedUnits.FindOrAdd(Layer.LayerId).Add(Prior.Key);
						}
					}
				}
			}
		}
		TArray<FProjectWorldLayerDirtyPlan> DirtyPlan;
		if (!ProjectWorldRealizationProfile::BuildDirtyPlan(
			Profile,
			DirtyInputs,
			DirtyPlan,
			OutError))
		{
			return false;
		}

		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			if (!Layer.IsGenerated())
			{
				continue;
			}
			FProjectWorldLayerInventory Inventory;
			Inventory.LayerId = Layer.LayerId;
			Inventory.ScopeId = TEXT("layer_") + Profile.ProfileId + TEXT("_") + Layer.LayerId;
			Inventory.NormalizedLayerContractHash = Layer.ContractHash;
			Inventory.GeneratorId = Layer.GeneratorId;
			Inventory.GeneratorVersion = Layer.GeneratorVersion;
			if (DirtyInput != nullptr)
			{
				Inventory.GeneratorFingerprint = DirtyInput->ProducerFingerprints.FindChecked(Layer.LayerId);
			}
			Inventory.ArtifactRoot = Layer.ArtifactRoot;
			if (const FProjectWorldLayerDirtyPlan* LayerPlan = DirtyPlan.FindByPredicate(
				[&Layer](const FProjectWorldLayerDirtyPlan& Entry)
				{
					return Entry.LayerId == Layer.LayerId;
				}))
			{
				Inventory.FinalDirtyUnits = LayerPlan->DirtyUnits;
			}

			Inventory.CanonicalInputs = CurrentInputs.FindChecked(Layer.LayerId);
			for (const FString& DependencyId : Layer.DependsOn)
			{
				if (const FProjectWorldRealizationLayer* Dependency = Profile.Layers.FindByPredicate(
					[&DependencyId](const FProjectWorldRealizationLayer& Candidate)
					{
						return Candidate.LayerId == DependencyId;
					}))
				{
					Inventory.DependencyInputs.Add({
						FString::Printf(TEXT("layer:%s:contract"), *DependencyId),
						Dependency->ContractHash});
				}
			}
			OutResult.LayerInventories.Add(MoveTemp(Inventory));
		}
		return true;
	}

	bool CaptureArtifacts(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TArray<const IProjectWorldLayerProducer*> Producers;
		if (World == nullptr || !ProjectWorldLayerProducerRegistry::GetAll(Producers, OutError))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Cannot capture layers without an editor world.");
			return false;
		}
		const TMap<FName, UMaterialInterface*> EmptyMaterials;
		for (const FString& LayerId : Profile.TopologicalLayerIds)
		{
			const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate(
				[&LayerId](const FProjectWorldRealizationLayer& Candidate)
				{ return Candidate.LayerId == LayerId; });
			if (Layer == nullptr || !Layer->IsGenerated())
			{
				OutError = FString::Printf(TEXT("Executable layer is missing: %s"), *LayerId);
				return false;
			}
			const IProjectWorldLayerProducer* const* Producer = Producers.FindByPredicate(
				[Layer](const IProjectWorldLayerProducer* Candidate)
				{ return Candidate->GetDeclaration().GeneratorId == Layer->GeneratorId &&
					Candidate->GetDeclaration().GeneratorVersion == Layer->GeneratorVersion; });
			if (Producer == nullptr)
			{
				OutError = FString::Printf(TEXT("Layer producer is missing: %s:v%d"),
					*Layer->GeneratorId, Layer->GeneratorVersion);
				return false;
			}
			FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
				[&LayerId](const FProjectWorldLayerInventory& Candidate)
				{ return Candidate.LayerId == LayerId; });
			if (Inventory == nullptr)
			{
				OutError = FString::Printf(TEXT("Layer inventory is missing: %s"), *LayerId);
				return false;
			}
			Inventory->Artifacts.Reset();
			const FProjectWorldLayerInputs Inputs{Bundle, Profile, *Layer, AuthoredOverlaySet};
			const FProjectWorldLayerApplyContext Context{
				*World, Inputs, Inventory->FinalDirtyUnits, EmptyMaterials};
			if (!(*Producer)->CaptureArtifacts(Context, *Inventory, OutResult, OutError))
			{
				return false;
			}
			Inventory->PostApplyBuilder = (*Producer)->GetDeclaration().PostApplyBuilder;
		}
		for (FProjectWorldLayerInventory& Inventory : OutResult.LayerInventories)
		{
			Inventory.Artifacts.Sort([](const auto& Left, const auto& Right)
			{
				return Left.Path < Right.Path;
			});
		}
		return true;
	}
}
