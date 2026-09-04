// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldBuildingRealization.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldBuildingRealization, Log, All);

namespace ProjectWorldBuildingRealization
{
	namespace
	{
		const FString CellTagPrefix(TEXT("ProjectWorld.BuildingCell="));
		const FString SemanticTagPrefix(TEXT("ProjectWorld.BuildingSemantic="));
		const FName BuildingTagV1(TEXT("ProjectWorld.BuildingMassing.v1"));
		const FName BuildingTagV2(TEXT("ProjectWorld.BuildingMassing.v2"));

		FString SanitizeToken(const FString& Value)
		{
			FString Result = Value;
			for (TCHAR& Character : Result)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
				{
					Character = TEXT('_');
				}
			}
			return Result.Left(96);
		}

		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("%d:"), Value.Len());
			Target += Value;
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		void SetTag(AActor* Actor, const FString& Prefix, const FString& Value)
		{
			Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
			{
				return Tag.ToString().StartsWith(Prefix);
			});
			Actor->Tags.Add(FName(*(Prefix + Value)));
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

		bool ResolveSettings(
			const FProjectWorldRealizationLayer& Layer,
			FProjectWorldBuildingSettings& OutSettings,
			FString& OutError)
		{
			OutSettings.GeneratorVersion = Layer.GeneratorVersion;
			TSharedPtr<FJsonObject> Settings;
			FString AnchorPolicy;
			FString TopologyPolicy;
			FString DuplicatePolicy;
			FString ContainedPolicy;
			FString ConflictPolicy;
			FString CollisionPolicy;
			FString NavigationPolicy;
			bool bNanite = false;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) ||
				!Settings.IsValid() ||
				!Settings->TryGetNumberField(TEXT("maximum_height_m"), OutSettings.MaximumHeightMeters) ||
				!Settings->TryGetStringField(TEXT("terrain_anchor_policy"), AnchorPolicy) ||
				AnchorPolicy != TEXT("owner_cell_clamped_bounds_center") ||
				!Settings->TryGetStringField(TEXT("topology_policy"), TopologyPolicy) ||
				TopologyPolicy != (Layer.GeneratorVersion == 2
					? TEXT("logical_building_classify_v2")
					: TEXT("cell_local_classify_v1")) ||
				!Settings->TryGetStringField(TEXT("duplicate_policy"), DuplicatePolicy) ||
				DuplicatePolicy != TEXT("stable_feature_id") ||
				!Settings->TryGetStringField(TEXT("contained_policy"), ContainedPolicy) ||
				ContainedPolicy != TEXT("associate_with_container") ||
				!Settings->TryGetStringField(TEXT("conflict_policy"), ConflictPolicy) ||
				ConflictPolicy != TEXT("reject_affected_fragments") ||
				!Settings->TryGetBoolField(TEXT("nanite"), bNanite) || !bNanite ||
				!Settings->TryGetStringField(TEXT("collision"), CollisionPolicy) ||
				CollisionPolicy != TEXT("complex_as_simple") ||
				!Settings->TryGetStringField(TEXT("navigation"), NavigationPolicy) ||
				NavigationPolicy != TEXT("no_navigation") ||
				!FMath::IsFinite(OutSettings.MaximumHeightMeters) ||
				OutSettings.MaximumHeightMeters < 50.0 || OutSettings.MaximumHeightMeters > 1000.0)
			{
				OutError = TEXT("Building layer has no executable settings contract.");
				return false;
			}
			return true;
		}

		bool BoundsOverlap(const FVector4d& Left, const FVector4d& Right)
		{
			return Left.X < Right.Z && Left.Z > Right.X && Left.Y < Right.W && Left.W > Right.Y;
		}

		TArray<const FProjectWorldAuthoredOverlay*> CellBuildingMasks(
			const FProjectWorldCanonicalCell& Cell,
			const FProjectWorldAuthoredOverlaySet& Set)
		{
			TArray<const FProjectWorldAuthoredOverlay*> Result;
			for (const FProjectWorldAuthoredOverlay& Overlay : Set.Overlays)
			{
				if (Overlay.Anchor.Kind == EProjectWorldAnchorKind::Mask &&
					Overlay.Anchor.Excludes.Contains(TEXT("buildings")) &&
					BoundsOverlap(Cell.Bounds, Overlay.Anchor.BoundsMeters))
				{
					Result.Add(&Overlay);
				}
			}
			Result.Sort([](const FProjectWorldAuthoredOverlay& Left, const FProjectWorldAuthoredOverlay& Right)
			{
				return Left.OverlayId < Right.OverlayId;
			});
			return Result;
		}

		const FProjectWorldCanonicalCell* FindOwnerCell(
			const FProjectWorldCanonicalBundle& Bundle,
			const FString& CellId)
		{
			return Bundle.Cells.FindByPredicate([&CellId](const FProjectWorldCanonicalCell& Cell)
			{
				return Cell.CellId == CellId;
			});
		}

		AStaticMeshActor* FindCellActor(UWorld* World, const FString& CellId, FString& OutError)
		{
			AStaticMeshActor* Result = nullptr;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				FString CandidateCell;
				FString Semantic;
				if (!ReadActorIdentity(*It, CandidateCell, Semantic) || CandidateCell != CellId)
				{
					continue;
				}
				if (Result != nullptr)
				{
					OutError = FString::Printf(TEXT("Building cell actor identity is duplicated: %s"), *CellId);
					return nullptr;
				}
				Result = *It;
			}
			if (Result == nullptr)
			{
				const FString ActorName = SanitizeToken(TEXT("ProjectWorld_Buildings_") + CellId);
				AStaticMeshActor* NamedActor = FindObject<AStaticMeshActor>(World->PersistentLevel, *ActorName);
				FString NamedCell;
				FString NamedSemantic;
				if (ReadActorIdentity(NamedActor, NamedCell, NamedSemantic) && NamedCell == CellId)
				{
					Result = NamedActor;
				}
			}
			return Result;
		}

		bool SaveAsset(UPackage* Package, UObject* Asset)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.TopLevelFlags = RF_Public | RF_Standalone;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, Asset, *Filename, Arguments);
		}

		bool SaveExternalActor(AActor* Actor)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Package == nullptr)
			{
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, nullptr, *Filename, Arguments);
		}

		bool RemoveCellOutput(UWorld* World, AStaticMeshActor* Actor, const FString& PackageName)
		{
			if (Actor != nullptr && !World->EditorDestroyActor(Actor, true))
			{
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			return !IFileManager::Get().FileExists(*Filename) ||
				IFileManager::Get().Delete(*Filename, false, true);
		}

		void LogRejections(
			const FProjectWorldCanonicalCell& Cell,
			const FProjectWorldBuildingMeshBuildResult& Build)
		{
			for (const FProjectWorldBuildingRejection& Rejection : Build.Rejections)
			{
				UE_LOG(
					LogProjectWorldBuildingRealization,
					Warning,
					TEXT("[ProjectWorldBuildingRealization::Apply] Building fragment rejected - cell=%s reason=%s features=%s"),
					*Cell.CellId,
					*Rejection.Reason,
					*FString::Join(Rejection.FeatureIds, TEXT(",")));
			}
		}
	}

	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FString& OutHash,
		FString& OutError)
	{
		FProjectWorldBuildingSettings Settings;
			if (!ResolveSettings(Layer, Settings, OutError))
		{
			return false;
		}
		FString Identity = FString::Printf(TEXT("project_building_cell_input_v%d"), Layer.GeneratorVersion);
		AppendToken(Identity, Cell.CellId);
		AppendToken(Identity, Layer.ContractHash);
		AppendNumber(Identity, Bundle.CoordinateQuantizationMeters);
		AppendNumber(Identity, Bundle.HeightQuantizationMeters);
		AppendNumber(Identity, Bundle.HeightOriginMeters);
		for (const FString& FeatureId : ProjectWorldBuildingMeshBuilder::CellBuildingFeatureIds(Bundle, Cell))
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			if (Layer.GeneratorVersion == 2 && Feature.BuildingVolumes.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Building v2 feature has no effective volumes: %s"), *FeatureId);
				return false;
			}
			const FProjectWorldCanonicalCell* Owner = FindOwnerCell(Bundle, Feature.OwnerCellId);
			if (Owner == nullptr)
			{
				OutError = FString::Printf(TEXT("Building owner cell is unavailable: %s"), *FeatureId);
				return false;
			}
			AppendToken(Identity, Feature.FeatureId);
			AppendToken(Identity, Feature.OwnerCellId);
			AppendToken(Identity, Owner->Terrain.ArtifactHash);
			AppendToken(Identity, Feature.GeometryType);
			AppendNumber(Identity, Feature.HeightMeters);
			if (Layer.GeneratorVersion == 2)
			{
				for (const FProjectWorldCanonicalBuildingVolume& Volume : Feature.BuildingVolumes)
				{
					AppendToken(Identity, Volume.VolumeId);
					AppendToken(Identity, Volume.SourceFeatureId);
					AppendToken(Identity, Volume.GeometryType);
					AppendNumber(Identity, Volume.MinHeightMeters);
					AppendNumber(Identity, Volume.HeightMeters);
					for (const FProjectWorldCanonicalPolygon& Polygon : Volume.GeometryPolygons)
					{
						AppendToken(Identity, FString::FromInt(Polygon.Outer.Num()));
						for (const FVector2D& Point : Polygon.Outer)
						{
							AppendNumber(Identity, Point.X);
							AppendNumber(Identity, Point.Y);
						}
						AppendToken(Identity, FString::FromInt(Polygon.Holes.Num()));
						for (const TArray<FVector2D>& Hole : Polygon.Holes)
						{
							AppendToken(Identity, FString::FromInt(Hole.Num()));
							for (const FVector2D& Point : Hole)
							{
								AppendNumber(Identity, Point.X);
								AppendNumber(Identity, Point.Y);
							}
						}
					}
				}
			}
			for (const FProjectWorldCanonicalPolygon& Polygon : Feature.GeometryPolygons)
			{
				AppendToken(Identity, FString::FromInt(Polygon.Outer.Num()));
				for (const FVector2D& Point : Polygon.Outer)
				{
					AppendNumber(Identity, Point.X);
					AppendNumber(Identity, Point.Y);
				}
				AppendToken(Identity, FString::FromInt(Polygon.Holes.Num()));
				for (const TArray<FVector2D>& Hole : Polygon.Holes)
				{
					AppendToken(Identity, FString::FromInt(Hole.Num()));
					for (const FVector2D& Point : Hole)
					{
						AppendNumber(Identity, Point.X);
						AppendNumber(Identity, Point.Y);
					}
				}
			}
		}
		for (const FProjectWorldAuthoredOverlay* Overlay : CellBuildingMasks(Cell, AuthoredOverlaySet))
		{
			AppendToken(Identity, Overlay->OverlayId);
			AppendNumber(Identity, Overlay->Anchor.BoundsMeters.X);
			AppendNumber(Identity, Overlay->Anchor.BoundsMeters.Y);
			AppendNumber(Identity, Overlay->Anchor.BoundsMeters.Z);
			AppendNumber(Identity, Overlay->Anchor.BoundsMeters.W);
		}
		return HashText(Identity, OutHash);
	}

	bool BuildCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldBuildingMeshBuildResult& OutBuild,
		FString& OutError)
	{
		FProjectWorldBuildingSettings Settings;
		return ResolveSettings(Layer, Settings, OutError) &&
			ProjectWorldBuildingMeshBuilder::BuildCell(
				Bundle, Cell, AuthoredOverlaySet, Settings, OutBuild, OutError);
	}

	bool ReadActorIdentity(const AActor* Actor, FString& OutCellId, FString& OutSemanticHash)
	{
		return Actor != nullptr &&
			(Actor->Tags.Contains(BuildingTagV1) || Actor->Tags.Contains(BuildingTagV2)) &&
			ReadTag(Actor, CellTagPrefix, OutCellId) &&
			ReadTag(Actor, SemanticTagPrefix, OutSemanticHash);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		UMaterialInterface* BuildingMaterial,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([](const auto& Candidate)
		{
			return Candidate.GeneratorId == TEXT("project_building_massing") &&
				(Candidate.GeneratorVersion == 1 || Candidate.GeneratorVersion == 2);
		});
		if (Layer == nullptr)
		{
			return true;
		}
		FProjectWorldBuildingSettings Settings;
		if (BuildingMaterial == nullptr || !ResolveSettings(*Layer, Settings, OutError))
		{
			OutError = BuildingMaterial == nullptr ? TEXT("Building material is unavailable.") : OutError;
			return false;
		}
		FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
			[Layer](const auto& Candidate) { return Candidate.LayerId == Layer->LayerId; });
		if (Inventory == nullptr)
		{
			OutError = TEXT("Building realization has no authenticated dirty inventory.");
			return false;
		}
		const bool bWholeDirty = Inventory->FinalDirtyUnits.Contains(TEXT("*"));
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString AssetName = TEXT("SM_ProjectWorldBuildings_") + SanitizeToken(Cell.CellId);
			const FString PackageName = Layer->ArtifactRoot + TEXT("Cells/") + AssetName;
			AStaticMeshActor* Actor = FindCellActor(World, Cell.CellId, OutError);
			if (!OutError.IsEmpty())
			{
				return false;
			}
			const bool bAssetExists = IFileManager::Get().FileExists(*FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension()));
			FProjectWorldBuildingMeshBuildResult Build;
			if (!ProjectWorldBuildingMeshBuilder::BuildCell(
				Bundle, Cell, AuthoredOverlaySet, Settings, Build, OutError))
			{
				OutError = FString::Printf(TEXT("Cannot build massing for building cell %s: %s"), *Cell.CellId, *OutError);
				return false;
			}
			const bool bExpected = Build.TriangleCount > 0;
			if (!bWholeDirty && !Inventory->FinalDirtyUnits.Contains(Cell.CellId) &&
				!(bExpected && (Actor == nullptr || !bAssetExists)))
			{
				continue;
			}
			LogRejections(Cell, Build);

			FString ExistingCell;
			FString ExistingSemantic;
			if (bExpected && Actor != nullptr && bAssetExists &&
				ReadActorIdentity(Actor, ExistingCell, ExistingSemantic) &&
				ExistingSemantic == Build.SemanticDigest)
			{
				continue;
			}
			if (!bExpected)
			{
				const bool bRemovedActor = Actor != nullptr;
				if (!RemoveCellOutput(World, Actor, PackageName))
				{
					OutError = FString::Printf(TEXT("Cannot retire building output for cell: %s"), *Cell.CellId);
					return false;
				}
				if (bRemovedActor)
				{
					++OutResult.RemovedActorCount;
					++OutResult.SelfSavedActorMutationCount;
				}
				continue;
			}

			UPackage* Package = CreatePackage(*PackageName);
			UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, *AssetName);
			if (Mesh == nullptr)
			{
				Mesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
			}
			Mesh->Modify();
			Mesh->GetStaticMaterials().Reset();
			Mesh->GetStaticMaterials().Add(FStaticMaterial(BuildingMaterial, TEXT("Building"), TEXT("Building")));
			FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
			Nanite.bEnabled = true;
			Mesh->SetNaniteSettings(Nanite);
			Mesh->bGenerateMeshDistanceField = false;
			Mesh->bHasNavigationData = false;
			Mesh->SetNumSourceModels(1);
			Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale = 0.0f;
			UStaticMesh::FBuildMeshDescriptionsParams BuildParameters;
			BuildParameters.bUseHashAsGuid = true;
			if (!Mesh->BuildFromMeshDescriptions({&Build.MeshDescription}, BuildParameters))
			{
				OutError = FString::Printf(TEXT("Cannot build persistent building mesh for cell: %s"), *Cell.CellId);
				return false;
			}
			Mesh->CreateBodySetup();
			Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
			Mesh->GetBodySetup()->bDoubleSidedGeometry = true;
			FAssetRegistryModule::AssetCreated(Mesh);
			if (!SaveAsset(Package, Mesh))
			{
				OutError = FString::Printf(TEXT("Cannot save persistent building mesh for cell: %s"), *Cell.CellId);
				return false;
			}

			const bool bUpdating = Actor != nullptr;
			if (Actor == nullptr)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*SanitizeToken(TEXT("ProjectWorld_Buildings_") + Cell.CellId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|buildings|") + Cell.CellId);
				Actor = World->SpawnActor<AStaticMeshActor>(
					AStaticMeshActor::StaticClass(), Build.ActorOrigin, FRotator::ZeroRotator, SpawnParameters);
				if (Actor != nullptr)
				{
					Actor->SetPackageExternal(true);
				}
			}
			if (Actor == nullptr || Actor->GetStaticMeshComponent() == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create building cell actor: %s"), *Cell.CellId);
				return false;
			}
			Actor->Modify();
			Actor->Tags.Reset();
			Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.Add(Layer->GeneratorVersion == 2 ? BuildingTagV2 : BuildingTagV1);
			SetTag(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			SetTag(Actor, TEXT("ProjectWorld.Cell="), Cell.CellId);
			SetTag(Actor, CellTagPrefix, Cell.CellId);
			SetTag(Actor, SemanticTagPrefix, Build.SemanticDigest);
			Actor->SetActorLabel(TEXT("ProjectWorld Buildings ") + Cell.CellId);
			Actor->SetActorLocation(Build.ActorOrigin, false, nullptr, ETeleportType::TeleportPhysics);
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->SetHLODLayer(nullptr);
			UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
			Component->Modify();
			Component->SetStaticMesh(Mesh);
			Component->SetMobility(EComponentMobility::Static);
			Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Component->SetCanEverAffectNavigation(false);
			Actor->MarkPackageDirty();
			const bool bMapPackageIsTemporary =
				World->PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
			if (!bMapPackageIsTemporary && !SaveExternalActor(Actor))
			{
				OutError = FString::Printf(TEXT("Cannot save building cell actor: %s"), *Cell.CellId);
				return false;
			}
			OutResult.BuildingTriangleRewriteCount += Build.TriangleCount;
			++OutResult.SelfSavedActorMutationCount;
			if (bUpdating)
			{
				++OutResult.UpdatedActorCount;
			}
			else
			{
				++OutResult.CreatedActorCount;
			}
		}
		FAssetCompilingManager::Get().FinishAllCompilation();
		return true;
	}
}
