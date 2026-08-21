// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldWaterRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldWaterMeshBuilder.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldWaterRealization, Log, All);

namespace ProjectWorldWaterRealization
{
	namespace
	{
		const FString WaterCellTagPrefix(TEXT("ProjectWorld.WaterCell="));
		const FString WaterSemanticTagPrefix(TEXT("ProjectWorld.WaterSemantic="));
		const FName WaterTag(TEXT("ProjectWorld.Water.v1"));

		struct FWaterSettings
		{
			FLinearColor Scattering = FLinearColor::Black;
			FLinearColor Absorption = FLinearColor::Black;
		};

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

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		bool ReadRgb(
			const TSharedPtr<FJsonObject>& Settings,
			const TCHAR* Field,
			FLinearColor& OutColor)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Settings->TryGetArrayField(Field, Values) || Values == nullptr || Values->Num() != 3)
			{
				return false;
			}
			double Channels[3] = {};
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (!(*Values)[Index]->TryGetNumber(Channels[Index]) ||
					!FMath::IsFinite(Channels[Index]) || Channels[Index] < 0.0)
				{
					return false;
				}
			}
			OutColor = FLinearColor(Channels[0], Channels[1], Channels[2]);
			return true;
		}

		bool ResolveContract(
			const FProjectWorldRealizationProfile& Profile,
			const FProjectWorldRealizationLayer*& OutLayer,
			FWaterSettings& OutSettings,
			FString& OutError)
		{
			OutLayer = Profile.Layers.FindByPredicate([](const FProjectWorldRealizationLayer& Layer)
			{
				return Layer.GeneratorId == TEXT("project_water_mesh") && Layer.GeneratorVersion == 1;
			});
			TSharedPtr<FJsonObject> Settings;
			if (OutLayer == nullptr ||
				!FJsonSerializer::Deserialize(
					TJsonReaderFactory<>::Create(OutLayer->NormalizedSettings), Settings) ||
				!Settings.IsValid() ||
				!ReadRgb(Settings, TEXT("scattering_coefficients"), OutSettings.Scattering) ||
				!ReadRgb(Settings, TEXT("absorption_coefficients"), OutSettings.Absorption))
			{
				OutError = TEXT("Water layer has no executable material contract.");
				return false;
			}
			return true;
		}

		FString ObjectPath(const FString& PackageName)
		{
			return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
		}

		bool SaveAsset(UPackage* Package, UObject* Asset)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(),
				FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.TopLevelFlags = RF_Public | RF_Standalone;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, Asset, *Filename, Arguments);
		}

		bool BuildMaterial(
			const FProjectWorldRealizationLayer& Layer,
			const FWaterSettings& Settings,
			bool bRebuild,
			UMaterial*& OutMaterial,
			FString& OutError)
		{
			const FString PackageName = Layer.ArtifactRoot + TEXT("M_ProjectWorldWater");
			OutMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath(PackageName));
			if (OutMaterial != nullptr && !bRebuild)
			{
				if (!OutMaterial->GetShadingModels().HasShadingModel(MSM_SingleLayerWater))
				{
					OutError = TEXT("Existing generated water material has the wrong shading model.");
					return false;
				}
				return true;
			}

			UPackage* Package = OutMaterial != nullptr
				? OutMaterial->GetOutermost()
				: CreatePackage(*PackageName);
			if (OutMaterial == nullptr)
			{
				OutMaterial = NewObject<UMaterial>(
					Package,
					*FPackageName::GetLongPackageAssetName(PackageName),
					RF_Public | RF_Standalone);
			}
			for (UMaterialExpression* Expression : OutMaterial->GetExpressionCollection().Expressions)
			{
				if (Expression != nullptr)
				{
					Expression->MarkAsGarbage();
				}
			}
			OutMaterial->GetExpressionCollection().Empty();
			UMaterialExpressionConstant3Vector* Scattering =
				NewObject<UMaterialExpressionConstant3Vector>(OutMaterial, TEXT("Scattering"));
			UMaterialExpressionConstant3Vector* Absorption =
				NewObject<UMaterialExpressionConstant3Vector>(OutMaterial, TEXT("Absorption"));
			// LEGIBILITY PLACEHOLDER, not art. Scattering and absorption alone describe the
			// water VOLUME; with a metre of depth over a valley floor there is almost no light
			// path to tint, so the surface reads as invisible and an operator cannot see where
			// water was placed. A flat blue base colour and low roughness make the blockout
			// visible without touching geometry or elevation. Real water appearance - surface
			// normals, waves, foam, flow - belongs to the presentation slice, which is where
			// these constants should become profile-driven settings.
			UMaterialExpressionConstant3Vector* BaseColor =
				NewObject<UMaterialExpressionConstant3Vector>(OutMaterial, TEXT("BaseColor"));
			UMaterialExpressionConstant* Roughness =
				NewObject<UMaterialExpressionConstant>(OutMaterial, TEXT("Roughness"));
			BaseColor->Constant = FLinearColor(0.012f, 0.075f, 0.150f);
			Roughness->R = 0.06f;
			UMaterialExpressionSingleLayerWaterMaterialOutput* Output =
				NewObject<UMaterialExpressionSingleLayerWaterMaterialOutput>(OutMaterial, TEXT("WaterOutput"));
			Scattering->Constant = Settings.Scattering;
			Absorption->Constant = Settings.Absorption;
			Output->ScatteringCoefficients.Connect(0, Scattering);
			Output->AbsorptionCoefficients.Connect(0, Absorption);
			OutMaterial->GetExpressionCollection().AddExpression(Scattering);
			OutMaterial->GetExpressionCollection().AddExpression(Absorption);
			OutMaterial->GetExpressionCollection().AddExpression(BaseColor);
			OutMaterial->GetExpressionCollection().AddExpression(Roughness);
			OutMaterial->GetExpressionCollection().AddExpression(Output);
			UMaterialEditorOnlyData* EditorOnly = OutMaterial->GetEditorOnlyData();
			EditorOnly->BaseColor.Connect(0, BaseColor);
			EditorOnly->Roughness.Connect(0, Roughness);
			OutMaterial->SetShadingModel(MSM_SingleLayerWater);
			OutMaterial->PostEditChange();
			FAssetCompilingManager::Get().FinishAllCompilation();
			FAssetRegistryModule::AssetCreated(OutMaterial);
			if (!SaveAsset(Package, OutMaterial))
			{
				OutError = TEXT("Cannot save the generated Single Layer Water material.");
				return false;
			}
			return true;
		}

		TArray<FString> WaterFeatureIds(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell)
		{
			TSet<FString> UniqueIds;
			UniqueIds.Append(Cell.OwnedFeatureIds);
			UniqueIds.Append(Cell.ReferencedFeatureIds);
			TArray<FString> Result;
			for (const FString& FeatureId : UniqueIds)
			{
				const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
				if (Feature != nullptr && Feature->FeatureClass == TEXT("water") && Feature->WaterSurface.bValid)
				{
					Result.Add(FeatureId);
				}
			}
			Result.Sort();
			return Result;
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
					OutError = FString::Printf(TEXT("Water cell actor identity is duplicated: %s"), *CellId);
					return nullptr;
				}
				Result = *It;
			}
			return Result;
		}

		bool BuildCellMesh(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const TMap<FString, FProjectWorldPreparedWaterSurface>& PreparedSurfaces,
			FMeshDescription& OutDescription,
			FVector& OutOrigin,
			FString& OutSemantic,
			int32& OutTriangles,
			FString& OutError)
		{
			TArray<FString> SemanticParts;
			bool bHasMesh = false;
			for (const FString& FeatureId : WaterFeatureIds(Bundle, Cell))
			{
				const FProjectWorldPreparedWaterSurface* PreparedSurface = PreparedSurfaces.Find(FeatureId);
				if (PreparedSurface == nullptr)
				{
					OutError = FString::Printf(TEXT("Water feature was not prepared: %s"), *FeatureId);
					return false;
				}
				FProjectWorldWaterMeshBuildResult Surface;
				if (!ProjectWorldWaterMeshBuilder::BuildCellSurface(
					Bundle,
					Cell,
					Bundle.Features.FindChecked(FeatureId),
					*PreparedSurface,
					Surface,
					OutError))
				{
					return false;
				}
				if (Surface.TriangleCount == 0)
				{
					continue;
				}
				if (!bHasMesh)
				{
					OutDescription = MoveTemp(Surface.MeshDescription);
					OutOrigin = Surface.ActorOrigin;
					bHasMesh = true;
				}
				else
				{
					FStaticMeshOperations::AppendMeshDescription(
						Surface.MeshDescription,
						OutDescription,
						FStaticMeshOperations::FAppendSettings());
				}
				OutTriangles += Surface.TriangleCount;
				SemanticParts.Add(FeatureId + TEXT(":") + Surface.SemanticDigest);
			}
			return !bHasMesh || HashText(
				TEXT("project_water_cell_mesh_v1|") + Cell.CellId + TEXT("|") +
				FString::Join(SemanticParts, TEXT("|")),
				OutSemantic);
		}

		bool RemoveCellOutput(UWorld* World, AStaticMeshActor* Actor, const FString& PackageName)
		{
			if (Actor != nullptr && !World->EditorDestroyActor(Actor, true))
			{
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName,
				FPackageName::GetAssetPackageExtension());
			return !IFileManager::Get().FileExists(*Filename) ||
				IFileManager::Get().Delete(*Filename, false, true);
		}
	}

	bool ReadActorIdentity(
		const AActor* Actor,
		FString& OutCellId,
		FString& OutSemanticHash)
	{
		return Actor != nullptr && Actor->Tags.Contains(WaterTag) &&
			ReadTag(Actor, WaterCellTagPrefix, OutCellId) &&
			ReadTag(Actor, WaterSemanticTagPrefix, OutSemanticHash);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = nullptr;
		FWaterSettings Settings;
		if (!ResolveContract(Profile, Layer, Settings, OutError))
		{
			return false;
		}
		FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
			[Layer](const FProjectWorldLayerInventory& Candidate)
			{
				return Candidate.LayerId == Layer->LayerId;
			});
		if (Inventory == nullptr)
		{
			OutError = TEXT("Water realization has no authenticated dirty inventory.");
			return false;
		}
		const bool bWholeDirty = Inventory->FinalDirtyUnits.Contains(TEXT("*"));
		UMaterial* WaterMaterial = nullptr;
		if (!BuildMaterial(*Layer, Settings, bWholeDirty, WaterMaterial, OutError))
		{
			return false;
		}
		TMap<FString, AStaticMeshActor*> CellActors;
		TSet<FString> CellsWithAssets;
		TSet<FString> DirtyCells;
		TSet<FString> DirtyFeatureIds;
		TMap<FString, FVector4d> FeatureDomainBounds;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString AssetName = TEXT("SM_ProjectWorldWater_") + SanitizeToken(Cell.CellId);
			const FString PackageName = Layer->ArtifactRoot + TEXT("Cells/") + AssetName;
			AStaticMeshActor* Actor = FindCellActor(World, Cell.CellId, OutError);
			if (!OutError.IsEmpty())
			{
				return false;
			}
			CellActors.Add(Cell.CellId, Actor);
			const TArray<FString> CellFeatureIds = WaterFeatureIds(Bundle, Cell);
			for (const FString& FeatureId : CellFeatureIds)
			{
				FVector4d* Bounds = FeatureDomainBounds.Find(FeatureId);
				if (Bounds == nullptr)
				{
					FeatureDomainBounds.Add(FeatureId, Cell.Bounds);
					continue;
				}
				Bounds->X = FMath::Min(Bounds->X, Cell.Bounds.X);
				Bounds->Y = FMath::Min(Bounds->Y, Cell.Bounds.Y);
				Bounds->Z = FMath::Max(Bounds->Z, Cell.Bounds.Z);
				Bounds->W = FMath::Max(Bounds->W, Cell.Bounds.W);
			}
			const bool bExpectedSurface = !CellFeatureIds.IsEmpty();
			const bool bAssetExists = IFileManager::Get().FileExists(*FPackageName::LongPackageNameToFilename(
				PackageName,
				FPackageName::GetAssetPackageExtension()));
			if (bAssetExists)
			{
				CellsWithAssets.Add(Cell.CellId);
			}
			if (!bWholeDirty && !Inventory->FinalDirtyUnits.Contains(Cell.CellId) &&
				!(bExpectedSurface && (Actor == nullptr || !bAssetExists)))
			{
				continue;
			}
			DirtyCells.Add(Cell.CellId);
			DirtyFeatureIds.Append(CellFeatureIds);
		}
		TArray<FString> SortedFeatureIds;
		SortedFeatureIds = DirtyFeatureIds.Array();
		SortedFeatureIds.Sort();
		TMap<FString, FProjectWorldPreparedWaterSurface> PreparedSurfaces;
		const double PrepareStart = FPlatformTime::Seconds();
		for (const FString& FeatureId : SortedFeatureIds)
		{
			FProjectWorldPreparedWaterSurface Surface;
			if (!ProjectWorldWaterMeshBuilder::PrepareSurface(
				Bundle.Features.FindChecked(FeatureId), FeatureDomainBounds.FindChecked(FeatureId), Surface, OutError))
			{
				return false;
			}
			PreparedSurfaces.Add(FeatureId, MoveTemp(Surface));
		}
		UE_LOG(
			LogProjectWorldWaterRealization,
			Display,
			TEXT("[ProjectWorldWaterRealization::Apply] Prepared surfaces - features=%d dirty_cells=%d duration_s=%.3f"),
			PreparedSurfaces.Num(),
			DirtyCells.Num(),
			FPlatformTime::Seconds() - PrepareStart);

		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString AssetName = TEXT("SM_ProjectWorldWater_") + SanitizeToken(Cell.CellId);
			const FString PackageName = Layer->ArtifactRoot + TEXT("Cells/") + AssetName;
			AStaticMeshActor* Actor = CellActors.FindRef(Cell.CellId);
			const bool bAssetExists = CellsWithAssets.Contains(Cell.CellId);
			if (!DirtyCells.Contains(Cell.CellId))
			{
				continue;
			}

			FMeshDescription Description;
			FVector ActorOrigin = FVector::ZeroVector;
			FString Semantic;
			int32 TriangleCount = 0;
			if (!BuildCellMesh(
				Bundle, Cell, PreparedSurfaces, Description, ActorOrigin, Semantic, TriangleCount, OutError))
			{
				return false;
			}
			FString ExistingCellId;
			FString ExistingSemantic;
			if (TriangleCount > 0 && Actor != nullptr && bAssetExists &&
				ReadActorIdentity(Actor, ExistingCellId, ExistingSemantic) &&
				ExistingCellId == Cell.CellId && ExistingSemantic == Semantic)
			{
				continue;
			}
			if (TriangleCount == 0)
			{
				if (!RemoveCellOutput(World, Actor, PackageName))
				{
					OutError = FString::Printf(TEXT("Cannot retire water output for cell: %s"), *Cell.CellId);
					return false;
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
			Mesh->GetStaticMaterials().Add(FStaticMaterial(WaterMaterial, TEXT("Water"), TEXT("Water")));
			FMeshNaniteSettings NaniteSettings = Mesh->GetNaniteSettings();
			NaniteSettings.bEnabled = false;
			Mesh->SetNaniteSettings(NaniteSettings);
			Mesh->bGenerateMeshDistanceField = false;
			Mesh->bHasNavigationData = false;
			Mesh->SetNumSourceModels(1);
			Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale = 0.0f;
			UStaticMesh::FBuildMeshDescriptionsParams BuildParameters;
			BuildParameters.bUseHashAsGuid = true;
			if (!Mesh->BuildFromMeshDescriptions({&Description}, BuildParameters))
			{
				OutError = FString::Printf(TEXT("Cannot build persistent water mesh for cell: %s"), *Cell.CellId);
				return false;
			}
			FAssetRegistryModule::AssetCreated(Mesh);
			if (!SaveAsset(Package, Mesh))
			{
				OutError = FString::Printf(TEXT("Cannot save persistent water mesh for cell: %s"), *Cell.CellId);
				return false;
			}

			const bool bUpdatingActor = Actor != nullptr;
			if (Actor == nullptr)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*SanitizeToken(TEXT("ProjectWorld_Water_") + Cell.CellId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|water|") + Cell.CellId);
				Actor = World->SpawnActor<AStaticMeshActor>(
					AStaticMeshActor::StaticClass(), ActorOrigin, FRotator::ZeroRotator, SpawnParameters);
			}
			if (Actor == nullptr || Actor->GetStaticMeshComponent() == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create water cell actor: %s"), *Cell.CellId);
				return false;
			}
			Actor->Modify();
			Actor->Tags.Reset();
			Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.Add(WaterTag);
			SetTag(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			SetTag(Actor, WaterCellTagPrefix, Cell.CellId);
			SetTag(Actor, WaterSemanticTagPrefix, Semantic);
			Actor->SetActorLabel(TEXT("ProjectWorld Water ") + Cell.CellId);
			Actor->SetActorLocation(ActorOrigin, false, nullptr, ETeleportType::TeleportPhysics);
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->SetHLODLayer(nullptr);
			UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
			Component->Modify();
			Component->SetStaticMesh(Mesh);
			Component->SetMobility(EComponentMobility::Static);
			Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCanEverAffectNavigation(false);
			Actor->MarkPackageDirty();
			OutResult.WaterTriangleCount += TriangleCount;
			if (bUpdatingActor)
			{
				++OutResult.UpdatedActorCount;
			}
			else
			{
				++OutResult.CreatedActorCount;
			}
		}
		return true;
	}
}
