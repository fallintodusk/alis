// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;

// Authored (hand-made) content placed above replaceable generated geography.
// Regeneration replaces generated layers wholesale, so authored packages are
// never touched; they are re-attached through these anchor records instead.
// Canonical coordinates are the durable authority - an Unreal transform is
// always DERIVED here and never stored as the source of truth.
// Contract: Plugins/World/ProjectWorld/docs/territory_generation.md,
// "Authored anchor semantics".
enum class EProjectWorldAnchorKind : uint8
{
	Coordinate,
	Feature,
	Mask
};

enum class EProjectWorldPlacementClass : uint8
{
	Precision,
	Standard
};

enum class EProjectWorldVerticalMode : uint8
{
	SurfaceSnap,
	Absolute
};

struct FProjectWorldAnchorProvenance
{
	FString ProvenanceId;
	double HorizontalAccuracyMeters = 0.0;
	bool bHasHorizontalAccuracy = false;
	FString VerticalDatum;
	double VerticalAccuracyMeters = 0.0;
	bool bHasVerticalAccuracy = false;
};

struct FProjectWorldAnchorRecord
{
	EProjectWorldAnchorKind Kind = EProjectWorldAnchorKind::Coordinate;
	EProjectWorldPlacementClass PlacementClass = EProjectWorldPlacementClass::Precision;
	EProjectWorldVerticalMode VerticalMode = EProjectWorldVerticalMode::SurfaceSnap;
	FString ProvenanceRef;
	FString VerticalProvenanceRef;
	double HorizontalToleranceMeters = 0.0;

	// Coordinate anchors.
	FString CanonicalCrs;
	double EastingMeters = 0.0;
	double NorthingMeters = 0.0;
	FString VerticalDatum;
	double HeightMeters = 0.0;
	double VerticalToleranceMeters = 0.0;

	// Feature anchors.
	FString FeatureId;
	FString ExpectedFeatureClass;
	FString ExpectedGeometryType;
	double ExpectedEastingMeters = 0.0;
	double ExpectedNorthingMeters = 0.0;

	// Shared optional placement.
	double YawDegrees = 0.0;
	double OffsetEastMeters = 0.0;
	double OffsetNorthMeters = 0.0;
	double OffsetUpMeters = 0.0;

	// Mask anchors.
	FVector4d BoundsMeters = FVector4d::Zero();
	TArray<FString> Excludes;
};

struct FProjectWorldAuthoredOverlay
{
	FString OverlayId;
	FString AuthoredPackage;
	FProjectWorldAnchorRecord Anchor;
};

struct FProjectWorldAuthoredOverlaySet
{
	FString OverlaySetId;
	FString SetHash;
	FString WorldDataPluginName;
	FString GridId;
	int32 ResolverVersion = 0;
	TMap<FString, FProjectWorldAnchorProvenance> Provenance;
	TArray<FProjectWorldAuthoredOverlay> Overlays;
};

struct FProjectWorldAnchorResolution
{
	FString OverlayId;
	// Derived, never authoritative.
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	// Distance the resolved canonical point moved from its authored
	// expectation. Meaningful only for feature anchors.
	double DriftMeters = 0.0;
	FString HorizontalProvenanceId;
	FString VerticalProvenanceId;
	FString VerticalDatum;
	double HorizontalSourceAccuracyMeters = 0.0;
	double HorizontalFeatureDriftMeters = 0.0;
	double HorizontalResolverErrorMeters = 0.0;
	double HorizontalTotalErrorMeters = 0.0;
	double VerticalSourceAccuracyMeters = 0.0;
	double VerticalResolverErrorMeters = 0.0;
	double VerticalTotalErrorMeters = 0.0;
	bool bSurfaceSnapped = false;
	bool bPlaces = false;
};

namespace ProjectWorldAuthoredOverlay
{
	// The resolver semantics this module implements. An overlay set frozen
	// against a different version is refused rather than silently re-placed.
	constexpr int32 SupportedResolverVersion = 3;

	bool Load(
		const FString& Path,
		FProjectWorldAuthoredOverlaySet& OutSet,
		FString& OutErrorCode,
		FString& OutError);

	// Projects the authored expected point onto the matching canonical
	// feature. Additive geometry growth cannot relocate the anchor, while
	// actual movement remains measurable and subject to tolerance.
	bool FeatureAnchorPoint(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAnchorRecord& Anchor,
		FVector2D& OutPoint,
		FString& OutError);

	// Resolves one overlay to a world transform, or fails CLOSED. A feature
	// that disappeared, changed class, or moved beyond its declared tolerance
	// is reported - never silently re-anchored.
	bool Resolve(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		const FProjectWorldAuthoredOverlay& Overlay,
		FProjectWorldAnchorResolution& OutResolution,
		FString& OutError);
}
