// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAnchorPlacement.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"

namespace ProjectWorldAnchorPlacement
{
	namespace
	{
		bool SampleAcceptedTerrain(
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector2D& Point,
			FProjectWorldAnchorResolution& OutResolution,
			double& OutHeight,
			FString& OutError)
		{
			bool bSampled = false;
			const double SeamTolerance = FMath::Max(Bundle.HeightQuantizationMeters, 0.000001);
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				if (Point.X < Cell.Bounds.X || Point.X > Cell.Bounds.Z ||
					Point.Y < Cell.Bounds.Y || Point.Y > Cell.Bounds.W)
				{
					continue;
				}
				if (Cell.Terrain.VerticalProvenanceId.IsEmpty() ||
					Cell.Terrain.VerticalDatum.IsEmpty() ||
					Cell.Terrain.VerticalSourceAccuracyMeters < 0.0 ||
					Cell.Terrain.SamplingQuantizationResidualMeters < 0.0)
				{
					OutError = TEXT("Canonical terrain has no qualified vertical provenance.");
					return false;
				}
				const double Height = ProjectWorldGeneratedGeometry::SampleTerrain(
					Cell,
					Point.X,
					Point.Y);
				if (bSampled && FMath::Abs(Height - OutHeight) > SeamTolerance)
				{
					OutError = TEXT("Canonical terrain disagrees across a shared cell edge.");
					return false;
				}
				if (bSampled &&
					(Cell.Terrain.VerticalProvenanceId != OutResolution.VerticalProvenanceId ||
						Cell.Terrain.VerticalDatum != OutResolution.VerticalDatum ||
						!FMath::IsNearlyEqual(
							Cell.Terrain.VerticalSourceAccuracyMeters,
							OutResolution.VerticalSourceAccuracyMeters) ||
						!FMath::IsNearlyEqual(
							Cell.Terrain.SamplingQuantizationResidualMeters,
							OutResolution.VerticalResolverErrorMeters)))
				{
					OutError = TEXT("Canonical terrain provenance disagrees across a shared cell edge.");
					return false;
				}
				OutHeight = Height;
				OutResolution.VerticalProvenanceId = Cell.Terrain.VerticalProvenanceId;
				OutResolution.VerticalDatum = Cell.Terrain.VerticalDatum;
				OutResolution.VerticalSourceAccuracyMeters =
					Cell.Terrain.VerticalSourceAccuracyMeters;
				OutResolution.VerticalResolverErrorMeters =
					Cell.Terrain.SamplingQuantizationResidualMeters;
				bSampled = true;
			}
			if (!bSampled)
			{
				OutError = TEXT("Surface-snap anchor lies outside accepted canonical terrain.");
			}
			return bSampled;
		}
	}

	bool ResolveHeight(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		const FProjectWorldAnchorRecord& Anchor,
		const FVector2D& CanonicalPoint,
		double HorizontalDriftMeters,
		FProjectWorldAnchorResolution& OutResolution,
		double& OutHeightMeters,
		FString& OutError)
	{
		const FProjectWorldAnchorProvenance* Provenance =
			Set.Provenance.Find(Anchor.ProvenanceRef);
		if (Provenance == nullptr || !Provenance->bHasHorizontalAccuracy)
		{
			OutError = FString::Printf(
				TEXT("Anchor references unknown provenance: %s"),
				*Anchor.ProvenanceRef);
			return false;
		}

		const double ClassMaximum = Anchor.PlacementClass == EProjectWorldPlacementClass::Precision
			? 2.0
			: 5.0;
		OutResolution.HorizontalProvenanceId = Provenance->ProvenanceId;
		OutResolution.HorizontalSourceAccuracyMeters = Provenance->HorizontalAccuracyMeters;
		OutResolution.HorizontalFeatureDriftMeters = HorizontalDriftMeters;
		OutResolution.HorizontalResolverErrorMeters = Bundle.CoordinateQuantizationMeters;
		OutResolution.HorizontalTotalErrorMeters =
			OutResolution.HorizontalSourceAccuracyMeters +
			OutResolution.HorizontalFeatureDriftMeters +
			OutResolution.HorizontalResolverErrorMeters;
		if (Anchor.HorizontalToleranceMeters <= 0.0 ||
			Anchor.HorizontalToleranceMeters > ClassMaximum ||
			OutResolution.HorizontalTotalErrorMeters > Anchor.HorizontalToleranceMeters)
		{
			OutError = TEXT("Anchor exceeds its horizontal placement error budget.");
			return false;
		}

		if (Anchor.VerticalMode == EProjectWorldVerticalMode::SurfaceSnap)
		{
			if (!Anchor.VerticalDatum.IsEmpty() ||
				!Anchor.VerticalProvenanceRef.IsEmpty() ||
				!FMath::IsNearlyZero(Anchor.HeightMeters) ||
				Anchor.VerticalToleranceMeters <= 0.0)
			{
				OutError = TEXT("Surface-snap anchor requires a tolerance but no absolute-height fields.");
				return false;
			}
			if (!SampleAcceptedTerrain(
				Bundle,
				CanonicalPoint,
				OutResolution,
				OutHeightMeters,
				OutError))
			{
				return false;
			}
			if (OutResolution.VerticalDatum != Bundle.VerticalDatum)
			{
				OutError = TEXT("Surface-snap terrain uses an incompatible vertical datum.");
				return false;
			}
			OutResolution.VerticalTotalErrorMeters =
				OutResolution.VerticalSourceAccuracyMeters +
				OutResolution.VerticalResolverErrorMeters;
			if (OutResolution.VerticalTotalErrorMeters > Anchor.VerticalToleranceMeters)
			{
				OutError = TEXT("Surface-snap anchor exceeds its vertical placement error budget.");
				return false;
			}
			return true;
		}

		const FProjectWorldAnchorProvenance* VerticalProvenance =
			Set.Provenance.Find(Anchor.VerticalProvenanceRef);
		if (Anchor.VerticalDatum != Bundle.VerticalDatum ||
			VerticalProvenance == nullptr ||
			!VerticalProvenance->bHasVerticalAccuracy ||
			VerticalProvenance->VerticalDatum != Anchor.VerticalDatum)
		{
			OutError = TEXT("Absolute anchor has missing or incompatible vertical provenance.");
			return false;
		}
		OutResolution.VerticalProvenanceId = VerticalProvenance->ProvenanceId;
		OutResolution.VerticalDatum = VerticalProvenance->VerticalDatum;
		OutResolution.VerticalSourceAccuracyMeters = VerticalProvenance->VerticalAccuracyMeters;
		OutResolution.VerticalResolverErrorMeters = Bundle.CoordinateQuantizationMeters;
		OutResolution.VerticalTotalErrorMeters =
			OutResolution.VerticalSourceAccuracyMeters +
			OutResolution.VerticalResolverErrorMeters;
		if (Anchor.VerticalToleranceMeters <= 0.0 ||
			OutResolution.VerticalTotalErrorMeters > Anchor.VerticalToleranceMeters)
		{
			OutError = TEXT("Absolute anchor exceeds its vertical placement error budget.");
			return false;
		}
		OutHeightMeters = Anchor.HeightMeters;
		return true;
	}
}
