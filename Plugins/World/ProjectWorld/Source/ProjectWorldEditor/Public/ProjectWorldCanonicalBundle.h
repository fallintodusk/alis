// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalPolygon
{
	TArray<FVector2D> Outer;
	TArray<TArray<FVector2D>> Holes;
};

struct FProjectWorldCanonicalBasinArtifact
{
	FString Path, Hash;
	int64 ByteSize=0;
};

struct FProjectWorldCanonicalBasinAdmission
{
	FString CanonicalCrs, SourceFeatureHash, ModelHash, BoundaryEncodingHash, AdmissionHash;
	FProjectWorldCanonicalBasinArtifact Input;
	TMap<FString,FProjectWorldCanonicalBasinArtifact> TerrainSupport;
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
	TOptional<double> ContainmentSpillMeters;
	TArray<FVector> Knots;
	TOptional<FProjectWorldCanonicalBasinAdmission> BasinAdmission;
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

struct FProjectWorldCanonicalRoadElevationTriangle
{
	FVector UVZ[3];
};

enum class EProjectWorldRoadEndpointRelation : uint8
{
	TerrainAbutment,
	ConstructedSurface
};

enum class EProjectWorldRoadElevationDomain : uint8
{
	SourceParameter,
	ReferencedConstructedSurface
};

struct FProjectWorldCanonicalSurfaceTriangle
{
	FVector XYZ[3];
};

struct FProjectWorldCanonicalRetainedSource
{
	FString Path;
	FString Hash;
	int64 ByteSize=0;
	bool bNative=false;
	FString ContextId;
};

struct FProjectWorldCanonicalConstructedRegion
{
	FString Role;
	TArray<FString> SourceIds;
	TArray<int32> SourceTriangles;
	FString AxisSourceId;
	FVector2D AxisStart=FVector2D::ZeroVector;
	FVector2D AxisEnd=FVector2D::ZeroVector;
};

struct FProjectWorldCanonicalConstructedPolicy
{
	int32 MaximumTriangles=0;
	double MaximumOrdinaryGradient=0;
	double MaximumGangwayGrade=0;
	double MaximumCrossSlope=0;
	double MaximumNumericErrorMeters=0;
};

struct FProjectWorldCanonicalConstructedSurface
{
	FString SourceFeatureId;
	FString ModelHash;
	FString CanonicalCrs;
	FString VerticalDatum;
	FString Basis;
	FString Role;
	TArray<FProjectWorldCanonicalSurfaceTriangle> Triangles;
	FString PolicyHash;
	FString AdmissionHash;
	TArray<FString> SourceProviderIds;
	TArray<FProjectWorldCanonicalRetainedSource> SourceSnapshots;
	TArray<FProjectWorldCanonicalConstructedRegion> FaceRegions;
	TOptional<FProjectWorldCanonicalConstructedPolicy> OperationalPolicy;
};

struct FProjectWorldCanonicalRoadElevation
{
	FString SourceFeatureId;
	TArray<FVector2D> SourcePoints;
	double LengthMeters = 0;
	double WidthMeters = 0;
	TArray<FProjectWorldCanonicalRoadElevationTriangle> Triangles;
	TMap<FString, FString> TerrainArtifacts;
	EProjectWorldRoadEndpointRelation EndpointRelation = EProjectWorldRoadEndpointRelation::TerrainAbutment;
	FString ConstructedSurfaceHash;
	EProjectWorldRoadElevationDomain Domain = EProjectWorldRoadElevationDomain::SourceParameter;
	TMap<FString, FString> ReferencedSurfaceModels;
};

struct FProjectWorldCanonicalCivilGrade
{
	TMap<FString, FString> TerrainArtifacts;
};
// PROJECTWORLD_PRODUCER_BEGIN project_building_massing
struct FProjectWorldCanonicalBuildingVolume
{
	FString VolumeId;
	FString SourceFeatureId;
	FString GeometryType;
	TArray<FProjectWorldCanonicalPolygon> GeometryPolygons;
	double MinHeightMeters = 0.0;
	double HeightMeters = 0.0;
};
// PROJECTWORLD_PRODUCER_END project_building_massing

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
	TOptional<FProjectWorldCanonicalRoadElevation> RoadElevation;
	TOptional<FProjectWorldCanonicalCivilGrade> CivilGrade;
	TOptional<FProjectWorldCanonicalConstructedSurface> ConstructedSurface;
	FString RoadClass;
	FString VegetationClass;
	FString FoliageClass;
	FString LeafType;
	FString LeafCycle;
	FString Species;
	double WidthMeters = 0.0;
	double HeightMeters = 0.0;
	// PROJECTWORLD_PRODUCER_BEGIN project_building_massing
	TArray<FProjectWorldCanonicalBuildingVolume> BuildingVolumes;
	// PROJECTWORLD_PRODUCER_END project_building_massing
};

struct FProjectWorldCanonicalTerrainQuad
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	TArray<FVector> Vertices;
	TArray<FVector2f> Weights;
	TArray<FIntVector> Triangles;
};

struct FProjectWorldCanonicalBasinConstruction
{
	FString CanonicalCrs, SourceFeatureHash, ModelHash, InputHash;
	int32 MaximumTriangles=0;
	TMap<FString,double> Parameters;
	TMap<FString,FString> GroundEvidence;
	double SpillMarginMeters=0;
};

struct FProjectWorldCanonicalTerrain
{
	FString ArtifactHash;
	FString BaseArtifactHash;
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
	FString SurfaceContractId;
	int32 SurfaceContractVersion = 0;
	FString SurfaceContractHash;
	TArray<FName> SurfaceRoles;
	TMap<FName, TArray<float>> SurfaceWeights;
	TArray<FProjectWorldCanonicalTerrainQuad> RefinedQuads;
	TOptional<FProjectWorldCanonicalBasinConstruction> BasinConstruction;
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

};
