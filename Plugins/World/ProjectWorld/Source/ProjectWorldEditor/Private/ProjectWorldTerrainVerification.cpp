// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldTerrainVerification.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainWaterConformance.h"
#include "Utilities/ProjectSha256.h"

#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"

namespace
{
	const FName GeneratedBaseLayerName(TEXT("Generated Base"));
}

bool FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
	ALandscape* Landscape,
	const FProjectWorldCanonicalBundle& Bundle,
	FProjectWorldTerrainHeightComparison& OutComparison,
	FString& OutError)
{
	OutComparison = FProjectWorldTerrainHeightComparison();
	if (Landscape == nullptr)
	{
		OutError = TEXT("Terrain verification received no Landscape.");
		return false;
	}
	if (Bundle.Cells.IsEmpty())
	{
		OutError = TEXT("Terrain verification received no canonical cells.");
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	const ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(GeneratedBaseLayerName);
	if (LandscapeInfo == nullptr || BaseLayer == nullptr)
	{
		OutError = TEXT("Landscape is missing LandscapeInfo or the Generated Base edit layer.");
		return false;
	}

	int32 MinimumX = 0;
	int32 MinimumY = 0;
	int32 MaximumX = 0;
	int32 MaximumY = 0;
	if (!LandscapeInfo->GetLandscapeExtent(MinimumX, MinimumY, MaximumX, MaximumY))
	{
		OutError = TEXT("Cannot read the Landscape extent for terrain verification.");
		return false;
	}

	const int32 Width = MaximumX - MinimumX + 1;
	const int32 Height = MaximumY - MinimumY + 1;
	TArray<uint16> LayerHeights;
	LayerHeights.SetNumUninitialized(Width * Height);
	// bInUploadTextureChangesToGPU = false keeps this read CPU-side, so the value observed in a
	// -NullRHI commandlet is the value observed in the interactive editor.
	FLandscapeEditDataInterface LayerData(LandscapeInfo, BaseLayer->GetGuid(), false);
	LayerData.GetHeightDataFast(
		MinimumX, MinimumY, MaximumX, MaximumY, LayerHeights.GetData(), Width);

	TArray<uint8> HeightBytes;
	HeightBytes.Append(
		reinterpret_cast<const uint8*>(LayerHeights.GetData()),
		LayerHeights.Num() * sizeof(uint16));
	if (!FProjectSha256::HashBuffer(HeightBytes, OutComparison.RealizedHeightHash))
	{
		OutError = TEXT("Cannot hash the realized Generated Base heights.");
		return false;
	}

	int32 MinimumCellX = MAX_int32;
	int32 MaximumCellY = MIN_int32;
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
		MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		OutComparison.ExpectedSampleCount +=
			Cell.Terrain.SamplesX * Cell.Terrain.SamplesY;
	}

	// Tolerance is derived, not invented: the coarser of the canonical height quantization and
	// UE's own 1/128 m heightmap step.
	OutComparison.ToleranceMeters =
		FMath::Max(Bundle.HeightQuantizationMeters, 1.0 / 128.0);
	double MinimumRealized = 1.0e18;
	double MaximumRealized = -1.0e18;
	FProjectWorldTerrainWaterConformanceContext WaterContext;
	if (!ProjectWorldTerrainWaterConformance::Prepare(Bundle, WaterContext, OutError))
	{
		return false;
	}

	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		TArray<double> ExpectedHeights;
		FProjectWorldTerrainWaterConformanceStats WaterStats;
		if (!ProjectWorldTerrainWaterConformance::BuildCellHeights(
			Bundle, WaterContext, Cell, ExpectedHeights, WaterStats, OutError))
		{
			return false;
		}
		const int32 OffsetX = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
		const int32 OffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
		for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
		{
			for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
			{
				const int32 LayerIndex =
					(OffsetY + Row - MinimumY) * Width + (OffsetX + Column - MinimumX);
				if (!LayerHeights.IsValidIndex(LayerIndex))
				{
					OutError = FString::Printf(
						TEXT("Canonical sample (%d,%d) of %s falls outside the Landscape extent."),
						Column,
						Row,
						*Cell.CellId);
					return false;
				}
				const double Expected =
					ExpectedHeights[Row * Cell.Terrain.SamplesX + Column];
				const double Realized =
					LandscapeDataAccess::GetLocalHeight(LayerHeights[LayerIndex]) +
					Bundle.HeightOriginMeters;
				++OutComparison.SampleCount;
				MinimumRealized = FMath::Min(MinimumRealized, Realized);
				MaximumRealized = FMath::Max(MaximumRealized, Realized);
				const double ErrorMeters = FMath::Abs(Realized - Expected);
				OutComparison.MaximumErrorMeters =
					FMath::Max(OutComparison.MaximumErrorMeters, ErrorMeters);
				if (ErrorMeters > OutComparison.ToleranceMeters)
				{
					++OutComparison.MismatchCount;
					if (OutComparison.FirstMismatch.IsEmpty())
					{
						OutComparison.FirstMismatch = FString::Printf(
							TEXT("%s sample (%d,%d): expected projection %.3f m, realized %.3f m"),
							*Cell.CellId,
							Column,
							Row,
							Expected,
							Realized);
					}
				}
			}
		}
	}

	OutComparison.MinimumHeightMeters = MinimumRealized;
	OutComparison.MaximumHeightMeters = MaximumRealized;
	OutComparison.ReliefMeters = MaximumRealized - MinimumRealized;
	return true;
}

bool FProjectWorldTerrainVerification::CompareFinalHeightmapToCanonical(
	ALandscape* Landscape,
	const FProjectWorldCanonicalBundle& Bundle,
	FProjectWorldTerrainHeightComparison& OutComparison,
	FString& OutError)
{
	OutComparison = FProjectWorldTerrainHeightComparison();
	if (Landscape == nullptr || Bundle.Cells.IsEmpty())
	{
		OutError = TEXT("Final terrain verification received no Landscape or no canonical cells.");
		return false;
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (LandscapeInfo == nullptr)
	{
		OutError = TEXT("Landscape has no LandscapeInfo for final heightmap verification.");
		return false;
	}

	int32 MinimumCellX = MAX_int32;
	int32 MaximumCellY = MIN_int32;
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		MinimumCellX = FMath::Min(MinimumCellX, Cell.CellX);
		MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		OutComparison.ExpectedSampleCount += Cell.Terrain.SamplesX * Cell.Terrain.SamplesY;
	}
	OutComparison.ToleranceMeters =
		FMath::Max(Bundle.HeightQuantizationMeters, 1.0 / 128.0);

	double MinimumRealized = 1.0e18;
	double MaximumRealized = -1.0e18;
	TArray<uint8> IdentityBytes;
	FProjectWorldTerrainWaterConformanceContext WaterContext;
	if (!ProjectWorldTerrainWaterConformance::Prepare(Bundle, WaterContext, OutError))
	{
		return false;
	}

	// Cache one data interface per component; constructing it locks the heightmap mip.
	TMap<ULandscapeComponent*, TSharedPtr<FLandscapeComponentDataInterface>> Interfaces;
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		TArray<double> ExpectedHeights;
		FProjectWorldTerrainWaterConformanceStats WaterStats;
		if (!ProjectWorldTerrainWaterConformance::BuildCellHeights(
			Bundle, WaterContext, Cell, ExpectedHeights, WaterStats, OutError))
		{
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
				const int32 ComponentQuads = Landscape->ComponentSizeQuads;
				const FIntPoint ComponentIndex(
					GlobalX == 0 ? 0 : (GlobalX - 1) / ComponentQuads,
					GlobalY == 0 ? 0 : (GlobalY - 1) / ComponentQuads);
				ULandscapeComponent* const* Found =
					LandscapeInfo->XYtoComponentMap.Find(ComponentIndex);
				if (Found == nullptr || *Found == nullptr)
				{
					OutError = FString::Printf(
						TEXT("No Landscape component covers canonical sample %d,%d of %s."),
						Column, Row, *Cell.CellId);
					return false;
				}
				ULandscapeComponent* Component = *Found;
				TSharedPtr<FLandscapeComponentDataInterface>& Interface =
					Interfaces.FindOrAdd(Component);
				if (!Interface.IsValid())
				{
					// bWorkOnEditingLayer = false -> GetHeightmap(false) -> final/base heightmap.
					Interface = MakeShared<FLandscapeComponentDataInterface>(Component, 0, false);
				}
				const FIntPoint SectionBase = Component->GetSectionBase();
				const int32 LocalX = GlobalX - SectionBase.X;
				const int32 LocalY = GlobalY - SectionBase.Y;
				if (LocalX < 0 || LocalY < 0 || LocalX > ComponentQuads || LocalY > ComponentQuads)
				{
					OutError = FString::Printf(
						TEXT("Canonical sample %d,%d of %s maps outside its Landscape component."),
						Column, Row, *Cell.CellId);
					return false;
				}
				const uint16 Encoded = Interface->GetHeight(LocalX, LocalY);
				IdentityBytes.Append(
					reinterpret_cast<const uint8*>(&Encoded), sizeof(uint16));
				const double Expected =
					ExpectedHeights[Row * Cell.Terrain.SamplesX + Column];
				const double Realized =
					LandscapeDataAccess::GetLocalHeight(Encoded) + Bundle.HeightOriginMeters;
				++OutComparison.SampleCount;
				MinimumRealized = FMath::Min(MinimumRealized, Realized);
				MaximumRealized = FMath::Max(MaximumRealized, Realized);
				const double ErrorMeters = FMath::Abs(Realized - Expected);
				OutComparison.MaximumErrorMeters =
					FMath::Max(OutComparison.MaximumErrorMeters, ErrorMeters);
				if (ErrorMeters > OutComparison.ToleranceMeters)
				{
					++OutComparison.MismatchCount;
					if (OutComparison.FirstMismatch.IsEmpty())
					{
						OutComparison.FirstMismatch = FString::Printf(
							TEXT("%s sample (%d,%d): expected projection %.3f m, final %.3f m"),
							*Cell.CellId, Column, Row, Expected, Realized);
					}
				}
			}
		}
	}

	if (!FProjectSha256::HashBuffer(IdentityBytes, OutComparison.RealizedHeightHash))
	{
		OutError = TEXT("Cannot hash the final Landscape heights.");
		return false;
	}
	OutComparison.MinimumHeightMeters = MinimumRealized;
	OutComparison.MaximumHeightMeters = MaximumRealized;
	OutComparison.ReliefMeters = MaximumRealized - MinimumRealized;
	return true;
}
