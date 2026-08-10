// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"

FVector FProjectWorldCanonicalLoader::CanonicalToUnreal(
	const FProjectWorldCanonicalBundle& Bundle,
	const FVector& CanonicalMeters)
{
	return FVector(
		(CanonicalMeters.X - Bundle.EngineGeoreferenceOriginMeters.X) * 100.0,
		-(CanonicalMeters.Y - Bundle.EngineGeoreferenceOriginMeters.Y) * 100.0,
		(CanonicalMeters.Z - Bundle.HeightOriginMeters) * 100.0);
}

FVector FProjectWorldCanonicalLoader::UnrealToCanonical(
	const FProjectWorldCanonicalBundle& Bundle,
	const FVector& UnrealCentimeters)
{
	return FVector(
		UnrealCentimeters.X * 0.01 + Bundle.EngineGeoreferenceOriginMeters.X,
		-UnrealCentimeters.Y * 0.01 + Bundle.EngineGeoreferenceOriginMeters.Y,
		UnrealCentimeters.Z * 0.01 + Bundle.HeightOriginMeters);
}

double FProjectWorldCanonicalLoader::TerrainRowNorthing(
	const FProjectWorldCanonicalTerrain& Terrain,
	int32 Row)
{
	return Terrain.Bounds.W - Row * Terrain.SampleSpacing.Y;
}

double FProjectWorldCanonicalLoader::TerrainSampleRow(
	const FProjectWorldCanonicalTerrain& Terrain,
	double CanonicalNorthing)
{
	return (Terrain.Bounds.W - CanonicalNorthing) / Terrain.SampleSpacing.Y;
}

FProjectWorldLandscapeLayout FProjectWorldCanonicalLoader::SelectLandscapeLayout(
	const FProjectWorldCanonicalBundle& Bundle)
{
	FProjectWorldLandscapeLayout Layout;
	if (Bundle.Cells.IsEmpty())
	{
		Layout.Reason = TEXT("No canonical cells are available.");
		return Layout;
	}

	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = MIN_int32;
	int32 MaxY = MIN_int32;
	TSet<FIntPoint> Coordinates;
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		MinX = FMath::Min(MinX, Cell.CellX);
		MinY = FMath::Min(MinY, Cell.CellY);
		MaxX = FMath::Max(MaxX, Cell.CellX);
		MaxY = FMath::Max(MaxY, Cell.CellY);
		Coordinates.Add(FIntPoint(Cell.CellX, Cell.CellY));
	}

	const FIntPoint CellCount(MaxX - MinX + 1, MaxY - MinY + 1);
	if (Coordinates.Num() != CellCount.X * CellCount.Y)
	{
		Layout.Reason = TEXT("Canonical cells do not form one rectangular Landscape envelope.");
		return Layout;
	}

	Layout.TotalQuads = FIntPoint(
		CellCount.X * Bundle.CellQuads.X,
		CellCount.Y * Bundle.CellQuads.Y);
	constexpr int32 SectionSizes[] = {255, 127, 63, 31, 15, 7};
	constexpr int32 SubsectionCounts[] = {2, 1};
	for (int32 QuadsPerSection : SectionSizes)
	{
		for (int32 SectionsPerComponent : SubsectionCounts)
		{
			const int32 ComponentQuads = QuadsPerSection * SectionsPerComponent;
			if (Layout.TotalQuads.X % ComponentQuads != 0 ||
				Layout.TotalQuads.Y % ComponentQuads != 0)
			{
				continue;
			}

			Layout.bCompatible = true;
			Layout.QuadsPerSection = QuadsPerSection;
			Layout.SectionsPerComponent = SectionsPerComponent;
			Layout.ComponentCount = FIntPoint(
				Layout.TotalQuads.X / ComponentQuads,
				Layout.TotalQuads.Y / ComponentQuads);
			Layout.Reason = TEXT("Canonical samples align exactly with stock Landscape components.");
			return Layout;
		}
	}

	Layout.Reason = FString::Printf(
		TEXT("Canonical %dx%d quad envelope cannot map to stock Landscape sections without resampling."),
		Layout.TotalQuads.X,
		Layout.TotalQuads.Y);
	return Layout;
}
