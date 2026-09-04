// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class ALandscape;
struct FProjectWorldCanonicalBundle;

// Terrain correctness evidence. Structural counts, proxy identity, georeference XY error, and
// semantic hashes all answer only "did Unreal produce the same terrain again"; a completely flat
// Landscape satisfies every one of them. This answers the different question: "did Unreal produce
// the deterministic Terrain-plus-Water Landscape projection", by reading the realized Generated
// Base edit layer back and comparing it against the canonical inputs that produced it.
struct PROJECTWORLDEDITOR_API FProjectWorldTerrainHeightComparison
{
	int32 SampleCount = 0;
	int32 ExpectedSampleCount = 0;
	int32 MismatchCount = 0;
	double MaximumErrorMeters = 0.0;
	double MinimumHeightMeters = 0.0;
	double MaximumHeightMeters = 0.0;
	double ReliefMeters = 0.0;
	double ToleranceMeters = 0.0;
	// Deterministic identity of the realized Generated Base heights in Landscape coordinate
	// order. Reconstruction/no-op identity only; the canonical-derived comparison above is the
	// correctness authority.
	FString RealizedHeightHash;
	FString FirstMismatch;
};

class PROJECTWORLDEDITOR_API FProjectWorldTerrainVerification final
{
public:
	// SOURCE surface. Reads the Generated Base edit layer, which is one input to UE's
	// edit-layer blend. Diagnostic only: a correct source layer does not imply correct
	// rendered terrain. Uses FLandscapeEditDataInterface(info, guid), documented as
	// "the specified edit layer".
	static bool CompareGeneratedBaseToCanonical(
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldTerrainHeightComparison& OutComparison,
		FString& OutError);

	// FINAL surface. Reads the blended final/base heightmap that actually renders, collides,
	// and drives cached bounds. This is the acceptance surface.
	//
	// Decode path verified against UE 5.8 source rather than inferred:
	//   FLandscapeComponentDataInterface(component, mip, bWorkOnEditingLayer)
	//       -> InComponent->GetHeightmap(bWorkOnEditingLayer)
	//   ULandscapeComponent::GetHeightmap(bool InReturnEditingHeightmap)
	//       -> editing-layer texture when true, final/base heightmap otherwise.
	// Note: FLandscapeEditDataInterface with an invalid GUID is NOT a final-heightmap
	// accessor; its contract is "current/specified edit layer".
	static bool CompareFinalHeightmapToCanonical(
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldTerrainHeightComparison& OutComparison,
		FString& OutError);
};
