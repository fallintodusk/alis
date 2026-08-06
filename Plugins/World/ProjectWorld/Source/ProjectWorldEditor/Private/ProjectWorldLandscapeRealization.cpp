// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLandscapeRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"

namespace ProjectWorldLandscapeRealization
{
	const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));
	const FName LandscapeTag(TEXT("ProjectWorld.Landscape.v1"));
	const FName GeneratedBaseLayerName(TEXT("Generated Base"));
	const FName GeneratedRoadsLayerName(TEXT("Generated Roads"));
	const FName AuthoredCorrectionsLayerName(TEXT("Authored Corrections"));
	const FName TerrainRowOrderTag(TEXT("ProjectWorld.TerrainRows=north_to_south_v1"));

	FGuid StableGuid(const FString& Value)
	{
		FGuid Guid;
		FGuid::ParseExact(FMD5::HashAnsiString(*Value), EGuidFormats::Digits, Guid);
		return Guid;
	}

	void SetIdentityTag(AActor* Actor, const FString& Prefix, const FString& Value)
	{
		Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		});
		Actor->Tags.Add(FName(*(Prefix + Value)));
	}

	void RemoveIdentityTags(AActor* Actor, const FString& Prefix)
	{
		Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		});
	}

	bool HasIdentityTag(const AActor* Actor, const FString& Prefix, const FString& Value)
	{
		return Actor->Tags.Contains(FName(*(Prefix + Value)));
	}

	bool AddRequiredLayers(ALandscape* Landscape, FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(0);
		if (BaseLayer == nullptr)
		{
			OutError = TEXT("Landscape import created no default edit layer.");
			return false;
		}
		BaseLayer->SetName(GeneratedBaseLayerName, true);

		if (Landscape->CreateLayer(GeneratedRoadsLayerName) == INDEX_NONE ||
			Landscape->CreateLayer(AuthoredCorrectionsLayerName) == INDEX_NONE)
		{
			OutError = TEXT("Cannot create the required generated and authored Landscape layers.");
			return false;
		}

		const ULandscapeEditLayerBase* AuthoredLayer =
			Landscape->GetEditLayerConst(AuthoredCorrectionsLayerName);
		if (AuthoredLayer == nullptr)
		{
			OutError = TEXT("Authored Corrections Landscape layer was not created.");
			return false;
		}
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredLayer->GetGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		return true;
	}

	bool UpdateGeneratedBase(
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		const FProjectWorldLandscapeHeightfield& Heightfield,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(GeneratedBaseLayerName);
		ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (BaseLayer == nullptr || AuthoredLayer == nullptr)
		{
			OutError = TEXT("Existing generated Landscape is missing a required edit layer.");
			return false;
		}

		const FGuid AuthoredGuidBefore = AuthoredLayer->GetGuid();
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (LandscapeInfo == nullptr)
		{
			OutError = TEXT("Existing generated Landscape has no LandscapeInfo.");
			return false;
		}

		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}

		const int32 ComponentQuads = Layout.SectionsPerComponent * Layout.QuadsPerSection;
		const bool bTerrainRowOrderChanged = !Landscape->Tags.Contains(TerrainRowOrderTag);
		FScopedSetLandscapeEditingLayer LayerScope(
			Landscape,
			BaseLayer->GetGuid(),
			[Landscape]()
			{
				Landscape->RequestLayersContentUpdateForceAll();
			});
		FLandscapeEditDataInterface EditData(LandscapeInfo, BaseLayer->GetGuid());
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString TerrainTagPrefix = FString::Printf(
				TEXT("ProjectWorld.TerrainCell=%s="),
				*Cell.CellId);
			if (!bTerrainRowOrderChanged &&
				HasIdentityTag(Landscape, TerrainTagPrefix, Cell.Terrain.ArtifactHash))
			{
				continue;
			}

			const int32 CanonicalOffsetX = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
			const int32 CanonicalOffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
			const int32 RegionX1 = CanonicalOffsetX;
			const int32 RegionX2 = RegionX1 + Bundle.CellQuads.X;
			const int32 RegionY1 = CanonicalOffsetY;
			const int32 RegionY2 = RegionY1 + Bundle.CellQuads.Y;
			if (RegionX1 % ComponentQuads != 0 || RegionY1 % ComponentQuads != 0 ||
				Bundle.CellQuads.X % ComponentQuads != 0 || Bundle.CellQuads.Y % ComponentQuads != 0)
			{
				OutError = FString::Printf(
					TEXT("Changed terrain cell %s is not component-aligned for partial Landscape update."),
					*Cell.CellId);
				return false;
			}

			EditData.SetHeightData(
				RegionX1,
				RegionY1,
				RegionX2,
				RegionY2,
				Heightfield.EncodedHeights.GetData() +
					RegionY1 * Heightfield.VertexCount.X + RegionX1,
				Heightfield.VertexCount.X,
				true);
			SetIdentityTag(Landscape, TerrainTagPrefix, Cell.Terrain.ArtifactHash);
			OutResult.UpdatedLandscapeComponentCount +=
				(Bundle.CellQuads.X / ComponentQuads) * (Bundle.CellQuads.Y / ComponentQuads);
		}

		AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (AuthoredLayer == nullptr || AuthoredLayer->GetGuid() != AuthoredGuidBefore)
		{
			OutError = TEXT("Authored Corrections Landscape layer identity changed during regeneration.");
			return false;
		}
		OutResult.bAuthoredCorrectionLayerPreserved = true;
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredGuidBefore.ToString(EGuidFormats::DigitsWithHyphensLower);
		Landscape->Tags.AddUnique(TerrainRowOrderTag);
		return true;
	}

	bool IsGeneratedLandscape(const AActor* Actor)
	{
		return Actor != nullptr && Actor->IsA<ALandscape>() &&
			Actor->Tags.Contains(GeneratedTag) && Actor->Tags.Contains(LandscapeTag);
	}

	bool BuildHeightfield(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		FProjectWorldLandscapeHeightfield& OutHeightfield,
		FString& OutError)
	{
		if (!Layout.bCompatible || Bundle.Cells.IsEmpty())
		{
			OutError = TEXT("Canonical grid is not compatible with a stock Landscape layout.");
			return false;
		}

		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}

		OutHeightfield.VertexCount = Layout.TotalQuads + FIntPoint(1, 1);
		const int32 SampleCount = OutHeightfield.VertexCount.X * OutHeightfield.VertexCount.Y;
		TArray<double> CanonicalHeights;
		CanonicalHeights.SetNumUninitialized(SampleCount);
		TBitArray<> Written(false, SampleCount);
		OutHeightfield.MinimumHeightMeters = TNumericLimits<double>::Max();
		OutHeightfield.MaximumHeightMeters = TNumericLimits<double>::Lowest();

		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			if (Cell.Terrain.SamplesX != Bundle.CellQuads.X + 1 ||
				Cell.Terrain.SamplesY != Bundle.CellQuads.Y + 1 ||
				!Cell.Terrain.SampleSpacing.Equals(Bundle.SampleSpacingMeters, KINDA_SMALL_NUMBER))
			{
				OutError = FString::Printf(TEXT("Terrain sampling contract differs in %s."), *Cell.CellId);
				return false;
			}

			const int32 OffsetX = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
			const int32 OffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
			for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
			{
				for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
				{
					const int32 GlobalX = OffsetX + Column;
					const int32 GlobalY = OffsetY + Row;
					const int32 GlobalIndex = GlobalY * OutHeightfield.VertexCount.X + GlobalX;
					const double Height = Cell.Terrain.HeightsMeters[
						Row * Cell.Terrain.SamplesX + Column];
					if (Written[GlobalIndex] &&
						!FMath::IsNearlyEqual(CanonicalHeights[GlobalIndex], Height, Bundle.HeightQuantizationMeters))
					{
						OutError = FString::Printf(TEXT("Terrain seam differs at canonical sample %d,%d."), GlobalX, GlobalY);
						return false;
					}
					CanonicalHeights[GlobalIndex] = Height;
					Written[GlobalIndex] = true;
					OutHeightfield.MinimumHeightMeters = FMath::Min(OutHeightfield.MinimumHeightMeters, Height);
					OutHeightfield.MaximumHeightMeters = FMath::Max(OutHeightfield.MaximumHeightMeters, Height);
				}
			}
		}

		OutHeightfield.EncodedHeights.SetNumUninitialized(SampleCount);
		for (int32 LandscapeY = 0; LandscapeY < OutHeightfield.VertexCount.Y; ++LandscapeY)
		{
			for (int32 X = 0; X < OutHeightfield.VertexCount.X; ++X)
			{
				const int32 CanonicalIndex = LandscapeY * OutHeightfield.VertexCount.X + X;
				if (!Written[CanonicalIndex])
				{
					OutError = FString::Printf(TEXT("Terrain envelope has no sample at %d,%d."), X, LandscapeY);
					return false;
				}
				const float RelativeHeight = static_cast<float>(
					CanonicalHeights[CanonicalIndex] - Bundle.HeightOriginMeters);
				const uint16 Encoded = LandscapeDataAccess::GetTexHeight(RelativeHeight);
				const double Decoded = LandscapeDataAccess::GetLocalHeight(Encoded) + Bundle.HeightOriginMeters;
				if (FMath::Abs(Decoded - CanonicalHeights[CanonicalIndex]) > Bundle.HeightQuantizationMeters)
				{
					OutError = TEXT("Landscape 16-bit height encoding exceeds canonical height tolerance.");
					return false;
				}
				OutHeightfield.EncodedHeights[
					LandscapeY * OutHeightfield.VertexCount.X + X] = Encoded;
			}
		}
		return true;
	}

	bool CreateOrUpdate(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError,
		UMaterialInterface* LandscapeMaterial)
	{
		FProjectWorldLandscapeHeightfield Heightfield;
		if (!BuildHeightfield(Bundle, Layout, Heightfield, OutError))
		{
			return false;
		}

		ALandscape* Landscape = nullptr;
		bool bCreatedLandscape = false;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (!IsGeneratedLandscape(*It))
			{
				continue;
			}
			if (Landscape != nullptr)
			{
				OutError = TEXT("Generated map contains more than one owned Landscape.");
				return false;
			}
			Landscape = *It;
		}

		const int32 ComponentQuads = Layout.SectionsPerComponent * Layout.QuadsPerSection;
		if (Landscape != nullptr)
		{
			if (!HasIdentityTag(Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId) ||
				Landscape->ComponentSizeQuads != ComponentQuads ||
				Landscape->NumSubsections != Layout.SectionsPerComponent ||
				Landscape->SubsectionSizeQuads != Layout.QuadsPerSection ||
				Landscape->LandscapeComponents.Num() != Layout.ComponentCount.X * Layout.ComponentCount.Y)
			{
				OutError = TEXT("Existing generated Landscape identity or component layout differs from canonical input.");
				return false;
			}
			if (!UpdateGeneratedBase(Landscape, Bundle, Layout, Heightfield, OutResult, OutError))
			{
				return false;
			}
		}
		else
		{
			FVector4d Envelope = Bundle.Cells[0].Bounds;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				Envelope.X = FMath::Min(Envelope.X, Cell.Bounds.X);
				Envelope.Y = FMath::Min(Envelope.Y, Cell.Bounds.Y);
				Envelope.Z = FMath::Max(Envelope.Z, Cell.Bounds.Z);
				Envelope.W = FMath::Max(Envelope.W, Cell.Bounds.W);
			}
			const FVector ActorLocation = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Envelope.X, Envelope.W, Bundle.HeightOriginMeters));
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = FName(TEXT("ProjectWorld_Landscape"));
			SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
			SpawnParameters.OverrideActorGuid = StableGuid(Bundle.GridId + TEXT("|landscape"));
			Landscape = World->SpawnActor<ALandscape>(
				ALandscape::StaticClass(),
				ActorLocation,
				FRotator::ZeroRotator,
				SpawnParameters);
			if (Landscape == nullptr)
			{
				OutError = TEXT("Cannot create the generated Landscape actor.");
				return false;
			}
			bCreatedLandscape = true;

			Landscape->SetActorScale3D(FVector(
				Bundle.SampleSpacingMeters.X * 100.0,
				Bundle.SampleSpacingMeters.Y * 100.0,
				100.0));
			Landscape->Tags.Add(GeneratedTag);
			Landscape->Tags.Add(LandscapeTag);
			Landscape->Tags.Add(TerrainRowOrderTag);
			Landscape->SetActorLabel(TEXT("ProjectWorld Landscape"));
			Landscape->SetFolderPath(FName(TEXT("ProjectWorld/Generated")));
			Landscape->SetIsSpatiallyLoaded(false);
			TMap<FGuid, TArray<uint16>> HeightData;
			HeightData.Add(FGuid(), Heightfield.EncodedHeights);
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerData;
			MaterialLayerData.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
			Landscape->Import(
				StableGuid(Bundle.GridId + TEXT("|landscape-data")),
				0,
				0,
				Heightfield.VertexCount.X - 1,
				Heightfield.VertexCount.Y - 1,
				Layout.SectionsPerComponent,
				Layout.QuadsPerSection,
				HeightData,
				TEXT(""),
				MaterialLayerData,
				ELandscapeImportAlphamapType::Additive,
				TArrayView<const FLandscapeLayer>());
			if (!AddRequiredLayers(Landscape, OutResult, OutError))
			{
				return false;
			}
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				SetIdentityTag(
					Landscape,
					FString::Printf(TEXT("ProjectWorld.TerrainCell=%s="), *Cell.CellId),
					Cell.Terrain.ArtifactHash);
			}
			OutResult.UpdatedLandscapeComponentCount =
				Layout.ComponentCount.X * Layout.ComponentCount.Y;
			++OutResult.CreatedActorCount;
		}

		SetIdentityTag(Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId);
		SetIdentityTag(Landscape, TEXT("ProjectWorld.Input="), Bundle.InputsHash);
		UMaterialInterface* DesiredLandscapeMaterial = LandscapeMaterial != nullptr
			? LandscapeMaterial
			: LoadObject<UMaterialInterface>(
				nullptr,
				TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
		const bool bMaterialChanged = Landscape->LandscapeMaterial != DesiredLandscapeMaterial;
		Landscape->LandscapeMaterial = DesiredLandscapeMaterial;
		if (bCreatedLandscape || bMaterialChanged)
		{
			Landscape->UpdateAllComponentMaterialInstances(true);
		}
		Landscape->MarkPackageDirty();
		OutResult.LandscapeComponentCount = Landscape->LandscapeComponents.Num();
		return true;
	}

	bool ClearGeneratedLayers(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		ALandscape* Landscape = nullptr;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (IsGeneratedLandscape(*It))
			{
				Landscape = *It;
				break;
			}
		}
		if (Landscape == nullptr)
		{
			return true;
		}
		if (!HasIdentityTag(Landscape, TEXT("ProjectWorld.Grid="), Bundle.GridId))
		{
			OutError = TEXT("Generated Landscape grid identity differs from the deletion receipt.");
			return false;
		}

		ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(GeneratedBaseLayerName);
		ULandscapeEditLayerBase* RoadsLayer = Landscape->GetEditLayer(GeneratedRoadsLayerName);
		ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (BaseLayer == nullptr || RoadsLayer == nullptr || AuthoredLayer == nullptr)
		{
			OutError = TEXT("Generated Landscape is missing a required protected layer boundary.");
			return false;
		}

		const FGuid AuthoredGuid = AuthoredLayer->GetGuid();
		Landscape->ClearEditLayer(BaseLayer->GetGuid());
		Landscape->ClearEditLayer(RoadsLayer->GetGuid());
		AuthoredLayer = Landscape->GetEditLayer(AuthoredCorrectionsLayerName);
		if (AuthoredLayer == nullptr || AuthoredLayer->GetGuid() != AuthoredGuid)
		{
			OutError = TEXT("Authored Corrections identity changed while clearing generated layers.");
			return false;
		}

		RemoveIdentityTags(Landscape, TEXT("ProjectWorld.Input="));
		RemoveIdentityTags(Landscape, TEXT("ProjectWorld.TerrainCell="));
		Landscape->RequestLayersContentUpdateForceAll();
		Landscape->MarkPackageDirty();
		OutResult.bAuthoredCorrectionLayerPreserved = true;
		OutResult.AuthoredCorrectionLayerGuid =
			AuthoredGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		OutResult.LandscapeComponentCount = Landscape->LandscapeComponents.Num();
		OutResult.UpdatedLandscapeComponentCount = Landscape->LandscapeComponents.Num();
		return true;
	}
}
