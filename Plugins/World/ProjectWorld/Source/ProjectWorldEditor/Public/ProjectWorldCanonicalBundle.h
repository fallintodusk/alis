// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalPolygon
{
	TArray<FVector2D> Outer;
	TArray<TArray<FVector2D>> Holes;
};

struct FProjectWorldCanonicalWaterSurface
{
	FString SurfaceGroupId;
	TArray<FString> SurfaceGroupMembers;
	FString Geometry;
	FString Behavior;
	FString Role;
	FString FunctionId;
	int32 FunctionVersion = 0;
	double LevelMeters = 0.0;
	TArray<FVector> Knots;
	bool bValid = false;
};

struct FProjectWorldCanonicalRepresentation
{
	FString CellId;
	FString Kind;
	TArray<FVector2D> Points;
	TArray<TArray<FVector2D>> Parts;
	TArray<FProjectWorldCanonicalPolygon> Polygons;
};

struct FProjectWorldCanonicalFeature
{
	FString FeatureId;
	FString FeatureClass;
	FString OwnerCellId;
	FString GeometryType;
	TArray<FVector2D> GeometryPoints;
	TArray<TArray<FVector2D>> GeometryParts;
	TArray<FProjectWorldCanonicalPolygon> GeometryPolygons;
	TArray<FProjectWorldCanonicalRepresentation> Representations;
	FProjectWorldCanonicalWaterSurface WaterSurface;
	FString RoadClass;
	FString VegetationClass;
	FString FoliageClass;
	FString LeafType;
	FString LeafCycle;
	FString Species;
	double WidthMeters = 0.0;
	double HeightMeters = 0.0;
};

struct FProjectWorldCanonicalTerrain
{
	FString ArtifactHash;
	FString VerticalProvenanceId;
	FString VerticalDatum;
	FString VerticalConfidence;
	double VerticalSourceAccuracyMeters = 0.0;
	double SamplingQuantizationResidualMeters = 0.0;
	FVector4d Bounds = FVector4d(0.0, 0.0, 0.0, 0.0);
	FVector2D SampleSpacing = FVector2D::ZeroVector;
	int32 SamplesX = 0;
	int32 SamplesY = 0;
	TArray<double> HeightsMeters;
};

struct FProjectWorldCanonicalCell
{
	FString CellId;
	FString FeatureArtifactHash;
	FVector4d Bounds = FVector4d(0.0, 0.0, 0.0, 0.0);
	int32 CellX = 0;
	int32 CellY = 0;
	TArray<FString> OwnedFeatureIds;
	TArray<FString> ReferencedFeatureIds;
	FProjectWorldCanonicalTerrain Terrain;
};

struct FProjectWorldCanonicalBundle
{
	FString CompileResultPath;
	FString CompileResultHash;
	FString InputsHash;
	FString ProfileId;
	FString WorldDataPluginName;
	FString GridId;
	FString CanonicalCrs;
	FString VerticalDatum;
	FString CoordinateTransform;
	FVector2D LatticeOriginMeters = FVector2D::ZeroVector;
	FVector2D EngineGeoreferenceOriginMeters = FVector2D::ZeroVector;
	FVector2D SampleSpacingMeters = FVector2D::ZeroVector;
	FIntPoint CellQuads = FIntPoint::ZeroValue;
	double CoordinateQuantizationMeters = 0.0;
	double HeightQuantizationMeters = 0.0;
	double HeightOriginMeters = 0.0;
	TArray<FProjectWorldCanonicalCell> Cells;
	TMap<FString, FProjectWorldCanonicalFeature> Features;
	int32 VerifiedOutputCount = 0;
};

struct FProjectWorldLandscapeLayout
{
	bool bCompatible = false;
	FIntPoint TotalQuads = FIntPoint::ZeroValue;
	FIntPoint ComponentCount = FIntPoint::ZeroValue;
	int32 SectionsPerComponent = 0;
	int32 QuadsPerSection = 0;
	FString Reason;
};

struct FProjectWorldCanonicalValidation
{
	FString ErrorCode;
	FString Message;
	FString Detail;

	bool IsAccepted() const
	{
		return ErrorCode.IsEmpty();
	}
};

class PROJECTWORLDEDITOR_API FProjectWorldCanonicalLoader final
{
public:
	static bool Load(
		const FString& CompileResultPath,
		FProjectWorldCanonicalBundle& OutBundle,
		FProjectWorldCanonicalValidation& OutValidation);

	static bool ComputeFileSha256(const FString& Path, FString& OutHash);

	static bool ResolveOwnedOutputPath(
		const FString& OutputRoot,
		const FString& RelativePath,
		FString& OutPath);

	static FVector CanonicalToUnreal(
		const FProjectWorldCanonicalBundle& Bundle,
		const FVector& CanonicalMeters);

	static FVector UnrealToCanonical(
		const FProjectWorldCanonicalBundle& Bundle,
		const FVector& UnrealCentimeters);

	static double TerrainRowNorthing(
		const FProjectWorldCanonicalTerrain& Terrain,
		int32 Row);

	static double TerrainSampleRow(
		const FProjectWorldCanonicalTerrain& Terrain,
		double CanonicalNorthing);

	static FProjectWorldLandscapeLayout SelectLandscapeLayout(
		const FProjectWorldCanonicalBundle& Bundle);
};
