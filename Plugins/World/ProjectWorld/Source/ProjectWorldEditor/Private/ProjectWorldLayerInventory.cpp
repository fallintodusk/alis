// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLayerInventory.h"

#include "ProjectWorldBuildingInventory.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGameplayPlacement.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldVegetationRealization.h"
#include "ProjectWorldWaterRealization.h"
#include "Utilities/ProjectSha256.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "MaterialShared.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"

namespace ProjectWorldLayerInventory
{
	namespace
	{
		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("%d:"), Value.Len());
			Target += Value;
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		void AppendPoint(FString& Target, const FVector2D& Point)
		{
			AppendNumber(Target, Point.X);
			AppendNumber(Target, Point.Y);
		}

		void AppendPolygon(FString& Target, const FProjectWorldCanonicalPolygon& Polygon)
		{
			AppendToken(Target, FString::FromInt(Polygon.Outer.Num()));
			for (const FVector2D& Point : Polygon.Outer)
			{
				AppendPoint(Target, Point);
			}
			AppendToken(Target, FString::FromInt(Polygon.Holes.Num()));
			for (const TArray<FVector2D>& Hole : Polygon.Holes)
			{
				AppendToken(Target, FString::FromInt(Hole.Num()));
				for (const FVector2D& Point : Hole)
				{
					AppendPoint(Target, Point);
				}
			}
		}

		bool HashWaterCell(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			FString& OutHash)
		{
			TSet<FString> FeatureIds;
			for (const FString& FeatureId : Cell.OwnedFeatureIds)
			{
				FeatureIds.Add(FeatureId);
			}
			for (const FString& FeatureId : Cell.ReferencedFeatureIds)
			{
				FeatureIds.Add(FeatureId);
			}
			TArray<FString> SortedIds = FeatureIds.Array();
			SortedIds.Sort();
			FString Identity(TEXT("project_water_cell_input_v1"));
			AppendToken(Identity, Cell.CellId);
			AppendNumber(Identity, Cell.Bounds.X);
			AppendNumber(Identity, Cell.Bounds.Y);
			AppendNumber(Identity, Cell.Bounds.Z);
			AppendNumber(Identity, Cell.Bounds.W);
			AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.X);
			AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.Y);
			AppendNumber(Identity, Bundle.CoordinateQuantizationMeters);
			AppendNumber(Identity, Bundle.HeightQuantizationMeters);
			AppendNumber(Identity, Bundle.HeightOriginMeters);
			for (const FString& FeatureId : SortedIds)
			{
				const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
				if (Feature == nullptr || Feature->FeatureClass != TEXT("water"))
				{
					continue;
				}
				AppendToken(Identity, Feature->FeatureId);
				AppendNumber(Identity, Feature->WidthMeters);
				AppendToken(Identity, Feature->WaterSurface.bValid ? TEXT("1") : TEXT("0"));
				AppendToken(Identity, Feature->WaterSurface.SurfaceGroupId);
				AppendToken(Identity, Feature->WaterSurface.Geometry);
				AppendToken(Identity, Feature->WaterSurface.Behavior);
				AppendToken(Identity, Feature->WaterSurface.FunctionId);
				AppendToken(Identity, FString::FromInt(Feature->WaterSurface.FunctionVersion));
				AppendNumber(Identity, Feature->WaterSurface.LevelMeters);
				AppendToken(Identity, FString::FromInt(Feature->WaterSurface.Knots.Num()));
				for (const FVector& Knot : Feature->WaterSurface.Knots)
				{
					AppendNumber(Identity, Knot.X);
					AppendNumber(Identity, Knot.Y);
					AppendNumber(Identity, Knot.Z);
				}
				AppendToken(Identity, FString::FromInt(Feature->GeometryParts.Num()));
				for (const TArray<FVector2D>& Part : Feature->GeometryParts)
				{
					AppendToken(Identity, FString::FromInt(Part.Num()));
					for (const FVector2D& Point : Part)
					{
						AppendPoint(Identity, Point);
					}
				}
				AppendToken(Identity, FString::FromInt(Feature->GeometryPolygons.Num()));
				for (const FProjectWorldCanonicalPolygon& Polygon : Feature->GeometryPolygons)
				{
					AppendPolygon(Identity, Polygon);
				}
			}
			FTCHARToUTF8 Utf8(*Identity);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		FProjectWorldLayerInventory* FindInventory(
			FProjectWorldRealizationResult& Result,
			const TCHAR* GeneratorId)
		{
			return Result.LayerInventories.FindByPredicate([GeneratorId](const auto& Inventory)
			{
				return Inventory.GeneratorId == GeneratorId;
			});
		}

		bool ReadTag(const AActor* Actor, const FString& Prefix, FString& OutValue)
		{
			for (const FName& Tag : Actor->Tags)
			{
				const FString Text = Tag.ToString();
				if (Text.StartsWith(Prefix))
				{
					OutValue = Text.RightChop(Prefix.Len());
					return !OutValue.IsEmpty();
				}
			}
			return false;
		}

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		bool AddPackageArtifact(
			const FString& PackageName,
			const FString& Kind,
			const FString& SemanticHash,
			FProjectWorldLayerInventory& Inventory,
			FString& OutError)
		{
			FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName,
				FPackageName::GetAssetPackageExtension());
			Filename = FPaths::ConvertRelativePathToFull(Filename);
			if (!FPaths::FileExists(Filename))
			{
				OutError = FString::Printf(TEXT("Layer package was not saved: %s"), *PackageName);
				return false;
			}
			FString Relative = Filename;
			const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || Relative.StartsWith(TEXT("..")))
			{
				OutError = FString::Printf(TEXT("Layer package escapes the project: %s"), *Filename);
				return false;
			}
			Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
			FString Digest;
			if (!FProjectSha256::HashFile(Filename, Digest))
			{
				OutError = FString::Printf(TEXT("Cannot hash layer package: %s"), *Filename);
				return false;
			}
			Inventory.Artifacts.Add({Relative, Kind, Digest, SemanticHash});
			return true;
		}
	}

	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		bool bFirstApply,
		const FProjectWorldLayerDirtyInput* DirtyInput,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TMap<FString, TArray<FProjectWorldLayerInputInventory>> CurrentInputs;
		TMap<FString, TMap<FString, TSet<FString>>> DependencyUnitMappings;
		for (const FProjectWorldRealizationLayer& Layer : Profile.Layers)
		{
			if (!Layer.IsGenerated())
			{
				continue;
			}
			TArray<FProjectWorldLayerInputInventory>& LayerInputs = CurrentInputs.Add(Layer.LayerId);
			for (const FString& Selector : Layer.CanonicalSelectors)
			{
				if (Selector != TEXT("terrain") && Selector != TEXT("water") && Selector != TEXT("roads") &&
					Selector != TEXT("vegetation") && Selector != TEXT("buildings") &&
					Selector != TEXT("gameplay_placements"))
				{
					OutError = FString::Printf(TEXT("Generator selector is unsupported: %s"), *Selector);
					return false;
				}
				if (Selector == TEXT("gameplay_placements"))
				{
					FProjectWorldGameplayPlacementSet Set;
					if (!ProjectWorldGameplayPlacement::Load(Profile, Layer, Set, OutError)) return false;
					TMap<FString, TSet<FString>>& TerrainMapping =
						DependencyUnitMappings.FindOrAdd(Layer.LayerId + TEXT("|terrain"));
					for (const FProjectWorldGameplayPlacement& Placement : Set.Placements)
					{
						FString CellId;
						FTransform Transform;
						FString Hash;
						if (!ProjectWorldGameplayPlacement::BuildInput(
							Bundle, Layer, Placement, CellId, Transform, Hash, OutError)) return false;
						LayerInputs.Add({Placement.ObjectId, Hash});
						TerrainMapping.FindOrAdd(CellId).Add(Placement.ObjectId);
					}
					continue;
				}
				for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
				{
					FString Hash;
					if (Selector == TEXT("terrain"))
					{
						Hash = Cell.Terrain.ArtifactHash;
					}
					else if (Selector == TEXT("water") && !HashWaterCell(Bundle, Cell, Hash))
					{
						OutError = FString::Printf(TEXT("Cannot hash water input for cell: %s"), *Cell.CellId);
						return false;
					}
					else if (Selector == TEXT("roads") &&
						!ProjectWorldRoadRealization::HashCellInput(Bundle, Cell, Layer, Hash, OutError))
					{
						return false;
					}
					else if (Selector == TEXT("vegetation") &&
						!ProjectWorldVegetationRealization::HashCellInput(
							Bundle, Cell, Layer, Profile, AuthoredOverlaySet, Hash, OutError))
					{
						return false;
					}
					else if (Selector == TEXT("buildings") &&
						!ProjectWorldBuildingRealization::HashCellInput(
							Bundle, Cell, Layer, AuthoredOverlaySet, Hash, OutError))
					{
						return false;
					}
					if (!Hash.IsEmpty())
					{
						LayerInputs.Add({Cell.CellId, Hash});
					}
				}
			}
			LayerInputs.Sort([](const auto& Left, const auto& Right)
			{
				return Left.UnitId < Right.UnitId;
			});
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
			DirtyInputs.OperatorAdditions = DirtyInput->OperatorAdditions;
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
		FProjectWorldLayerInventory* Terrain = FindInventory(OutResult, TEXT("project_landscape"));
		FProjectWorldLayerInventory* Water = FindInventory(OutResult, TEXT("project_water_mesh"));
		FProjectWorldLayerInventory* Roads = FindInventory(OutResult, TEXT("project_road_mesh"));
		FProjectWorldLayerInventory* Vegetation = FindInventory(OutResult, TEXT("project_vegetation_instances"));
		FProjectWorldLayerInventory* Buildings = FindInventory(OutResult, TEXT("project_building_massing"));
		FProjectWorldLayerInventory* Gameplay = FindInventory(OutResult, TEXT("project_gameplay_placement"));
		if (Terrain == nullptr || Water == nullptr)
		{
			OutError = TEXT("Executable terrain/water layer inventories are incomplete.");
			return false;
		}
		Terrain->Artifacts.Reset();
		Water->Artifacts.Reset();
		if (Roads != nullptr)
		{
			Roads->Artifacts.Reset();
		}
		if (Vegetation != nullptr)
		{
			Vegetation->Artifacts.Reset();
		}
		if (Buildings != nullptr)
		{
			Buildings->Artifacts.Reset();
		}
		if (Gameplay != nullptr)
		{
			Gameplay->Artifacts.Reset();
		}

		TMap<FString, const FProjectWorldCanonicalCell*> CellsById;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			CellsById.Add(Cell.CellId, &Cell);
		}
		TSet<FString> TerrainCells;
		for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
		{
			FString CellId;
			if (!ReadTag(*It, TEXT("ProjectWorld.TerrainCell="), CellId))
			{
				continue;
			}
			const FProjectWorldCanonicalCell* const* Cell = CellsById.Find(CellId);
			if (Cell == nullptr || TerrainCells.Contains(CellId) || It->LandscapeComponents.Num() != 1 ||
				!It->GetIsSpatiallyLoaded() || It->bEnableAutoLODGeneration || It->GetHLODLayer() != nullptr)
			{
				OutError = FString::Printf(TEXT("Landscape proxy ownership is invalid for cell: %s"), *CellId);
				return false;
			}
			TerrainCells.Add(CellId);
			FString Semantic;
			if (!HashText(FString::Printf(
				TEXT("project_landscape_proxy_v1|%s|%s|%s|%d,%d"),
				*Profile.LogicalLandscapeId,
				*CellId,
				*(*Cell)->Terrain.ArtifactHash,
				It->GetSectionBase().X,
				It->GetSectionBase().Y), Semantic) ||
				!AddPackageArtifact(It->GetPackage()->GetName(), TEXT("external_actor"), Semantic, *Terrain, OutError))
			{
				return false;
			}
		}
		if (TerrainCells.Num() != Bundle.Cells.Num())
		{
			OutError = TEXT("Landscape proxy inventory does not exactly cover canonical cells.");
			return false;
		}
		OutResult.LandscapeProxyCount = TerrainCells.Num();

		FString MaterialSemantic;
		if (!HashText(TEXT("project_water_material_v1|") + Water->NormalizedLayerContractHash, MaterialSemantic) ||
			!AddPackageArtifact(
				Water->ArtifactRoot + TEXT("M_ProjectWorldWater"),
				TEXT("asset"),
				MaterialSemantic,
				*Water,
				OutError))
		{
			return false;
		}
		TSet<FString> WaterCells;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			FString CellId;
			FString MeshSemantic;
			if (!ProjectWorldWaterRealization::ReadActorIdentity(*It, CellId, MeshSemantic))
			{
				continue;
			}
			UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
			FString OwnershipFailure;
			if (!CellsById.Contains(CellId)) OwnershipFailure = TEXT("cell is outside the canonical target");
			else if (WaterCells.Contains(CellId)) OwnershipFailure = TEXT("cell actor is duplicated");
			else if (Mesh == nullptr) OwnershipFailure = TEXT("StaticMesh is missing");
			else if (Mesh->GetNaniteSettings().bEnabled) OwnershipFailure = TEXT("water mesh has Nanite enabled");
			else if (!It->GetIsSpatiallyLoaded()) OwnershipFailure = TEXT("actor is not spatially loaded");
			else if (It->bEnableAutoLODGeneration) OwnershipFailure = TEXT("actor permits HLOD generation");
			else if (It->GetHLODLayer() != nullptr) OwnershipFailure = TEXT("actor has an HLOD layer");
			else if (Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
				OwnershipFailure = TEXT("water collision is enabled");
			else if (Mesh->GetStaticMaterials().IsEmpty()) OwnershipFailure = TEXT("water material slot is missing");
			else if (Mesh->GetStaticMaterials()[0].MaterialInterface == nullptr)
				OwnershipFailure = TEXT("water material is missing");
			else if (!Mesh->GetStaticMaterials()[0].MaterialInterface->GetShadingModels().HasShadingModel(MSM_SingleLayerWater))
				OwnershipFailure = TEXT("material is not Single Layer Water");
			if (!OwnershipFailure.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Persistent water ownership is invalid for cell %s: %s"),
					*CellId,
					*OwnershipFailure);
				return false;
			}
			WaterCells.Add(CellId);
			if (!AddPackageArtifact(Mesh->GetOutermost()->GetName(), TEXT("asset"), MeshSemantic, *Water, OutError))
			{
				return false;
			}
			FString ActorSemantic;
			if (!HashText(FString::Printf(
				TEXT("project_water_actor_v1|%s|%s|%.17g,%.17g,%.17g"),
				*CellId,
				*MeshSemantic,
				It->GetActorLocation().X,
				It->GetActorLocation().Y,
				It->GetActorLocation().Z), ActorSemantic) ||
				!AddPackageArtifact(It->GetPackage()->GetName(), TEXT("external_actor"), ActorSemantic, *Water, OutError))
			{
				return false;
			}
		}
		OutResult.WaterCellActorCount = WaterCells.Num();
		OutResult.WaterMeshAssetCount = WaterCells.Num();
		if (Roads != nullptr)
		{
			const FProjectWorldRealizationLayer* RoadLayer = Profile.Layers.FindByPredicate([](const auto& Layer)
			{
				return Layer.GeneratorId == TEXT("project_road_mesh");
			});
			TSet<FString> ExpectedRoadCells;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				bool bExpected = false;
				if (RoadLayer == nullptr || !ProjectWorldRoadRealization::ExpectsCellOutput(
					Bundle, Cell, *RoadLayer, bExpected, OutError))
				{
					return false;
				}
				if (bExpected)
				{
					ExpectedRoadCells.Add(Cell.CellId);
				}
			}
			TSet<FString> RoadCells;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				FString CellId;
				FString MeshSemantic;
				if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, MeshSemantic))
				{
					continue;
				}
				UStaticMeshComponent* Component = It->GetStaticMeshComponent();
				UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
				FString OwnershipFailure;
				if (!ExpectedRoadCells.Contains(CellId)) OwnershipFailure = TEXT("cell has no selected canonical roads");
				else if (RoadCells.Contains(CellId)) OwnershipFailure = TEXT("cell actor is duplicated");
				else if (Mesh == nullptr) OwnershipFailure = TEXT("StaticMesh is missing");
				else if (!Mesh->GetNaniteSettings().bEnabled) OwnershipFailure = TEXT("road mesh has Nanite disabled");
				else if (Mesh->GetBodySetup() == nullptr || Mesh->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple)
					OwnershipFailure = TEXT("road collision is not complex-as-simple");
				else if (!It->GetIsSpatiallyLoaded()) OwnershipFailure = TEXT("actor is not spatially loaded");
				else if (It->bEnableAutoLODGeneration) OwnershipFailure = TEXT("actor permits HLOD generation");
				else if (It->GetHLODLayer() != nullptr) OwnershipFailure = TEXT("actor has an HLOD layer");
				else if (Component->CanEverAffectNavigation()) OwnershipFailure = TEXT("road affects navigation");
				if (!OwnershipFailure.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Persistent road ownership is invalid for cell %s: %s"), *CellId, *OwnershipFailure);
					return false;
				}
				RoadCells.Add(CellId);
				if (!AddPackageArtifact(Mesh->GetOutermost()->GetName(), TEXT("asset"), MeshSemantic, *Roads, OutError))
				{
					return false;
				}
				FString ActorSemantic;
				if (!HashText(TEXT("project_road_actor_v1|") + CellId + TEXT("|") + MeshSemantic, ActorSemantic) ||
					!AddPackageArtifact(It->GetPackage()->GetName(), TEXT("external_actor"), ActorSemantic, *Roads, OutError))
				{
					return false;
				}
			}
			if (RoadCells.Num() != ExpectedRoadCells.Num())
			{
				OutError = TEXT("Road cell inventory does not exactly cover selected canonical roads.");
				return false;
			}
			OutResult.RoadCellActorCount = RoadCells.Num();
			OutResult.RoadMeshAssetCount = RoadCells.Num();
		}
		if (Vegetation != nullptr)
		{
			OutResult.VegetationCandidateCount = 0;
			OutResult.VegetationRoadExcludedCount = 0;
			OutResult.VegetationWaterExcludedCount = 0;
			OutResult.VegetationAuthoredMaskExcludedCount = 0;
			const FProjectWorldRealizationLayer* VegetationLayer = Profile.Layers.FindByPredicate([](const auto& Layer)
			{
				return Layer.GeneratorId == TEXT("project_vegetation_instances");
			});
			TSet<FString> ExpectedCells;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				TArray<FProjectWorldVegetationInstance> Instances;
				FProjectWorldVegetationPlacementStats Stats;
				FString Semantic;
				if (VegetationLayer == nullptr || !ProjectWorldVegetationRealization::BuildCellInstances(
					Bundle, Cell, *VegetationLayer, Profile, AuthoredOverlaySet,
					Instances, &Stats, Semantic, OutError))
				{
					return false;
				}
				OutResult.VegetationCandidateCount += Stats.CandidateCount;
				OutResult.VegetationRoadExcludedCount += Stats.RoadExcludedCount;
				OutResult.VegetationWaterExcludedCount += Stats.WaterExcludedCount;
				OutResult.VegetationAuthoredMaskExcludedCount += Stats.AuthoredMaskExcludedCount;
				if (!Instances.IsEmpty())
				{
					ExpectedCells.Add(Cell.CellId);
				}
			}
			TSet<FString> ActualCells;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				FString CellId;
				FString Semantic;
				if (!ProjectWorldVegetationRealization::ReadActorIdentity(*It, CellId, Semantic))
				{
					continue;
				}
				TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
				It->GetComponents(Components);
				FString OwnershipFailure;
				int32 ActorInstances = 0;
				if (!ExpectedCells.Contains(CellId)) OwnershipFailure = TEXT("cell has no canonical vegetation");
				else if (ActualCells.Contains(CellId)) OwnershipFailure = TEXT("cell actor is duplicated");
				else if (Components.IsEmpty()) OwnershipFailure = TEXT("actor has no HISM components");
				else if (!It->GetIsSpatiallyLoaded()) OwnershipFailure = TEXT("actor is not spatially loaded");
				else if (It->bEnableAutoLODGeneration || It->GetHLODLayer() != nullptr)
					OwnershipFailure = TEXT("actor participates in HLOD");
				for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
				{
					UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
					if (Mesh == nullptr || !Mesh->GetNaniteSettings().bEnabled ||
						Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
						Component->CanEverAffectNavigation() || Component->GetInstanceCount() <= 0)
					{
						OwnershipFailure = TEXT("HISM component violates mesh, Nanite, collision, navigation, or population policy");
						break;
					}
					ActorInstances += Component->GetInstanceCount();
				}
				if (!OwnershipFailure.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Persistent vegetation ownership is invalid for cell %s: %s"),
						*CellId, *OwnershipFailure);
					return false;
				}
				ActualCells.Add(CellId);
				OutResult.VegetationComponentCount += Components.Num();
				OutResult.VegetationInstanceCount += ActorInstances;
				if (!AddPackageArtifact(It->GetPackage()->GetName(), TEXT("external_actor"), Semantic, *Vegetation, OutError))
				{
					return false;
				}
			}
			if (ActualCells.Num() != ExpectedCells.Num())
			{
				OutError = TEXT("Vegetation cell inventory does not exactly cover canonical vegetation.");
				return false;
			}
			OutResult.VegetationCellActorCount = ActualCells.Num();
		}
		if (Buildings != nullptr)
		{
			const FProjectWorldRealizationLayer* BuildingLayer = Profile.Layers.FindByPredicate([](const auto& Layer)
			{
				return Layer.GeneratorId == TEXT("project_building_massing");
			});
			if (BuildingLayer == nullptr || !ProjectWorldBuildingInventory::Capture(
				World, Bundle, *BuildingLayer, AuthoredOverlaySet, *Buildings, OutResult, OutError))
			{
				return false;
			}
		}
		if (Gameplay != nullptr && !ProjectWorldGameplayPlacement::Capture(
			World, Bundle, Profile, *Gameplay, OutResult, OutError))
		{
			return false;
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
