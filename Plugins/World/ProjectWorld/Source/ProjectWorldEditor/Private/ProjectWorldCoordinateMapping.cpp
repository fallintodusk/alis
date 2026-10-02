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
