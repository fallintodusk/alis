// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVegetationExclusions.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace
{
	void AppendToken(FString& Target, const FString& Value)
	{
		Target += FString::Printf(TEXT("|%d:%s"), Value.Len(), *Value);
	}

	void AppendNumber(FString& Target, double Value)
	{
		AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
	}

	bool HashText(const FString& Text, FString& OutHash)
	{
		FTCHARToUTF8 Utf8(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return FProjectSha256::HashBuffer(Bytes, OutHash);
	}

	double PointSegmentDistanceSquared(
		const FVector2D& Point,
		const FVector2D& Start,
		const FVector2D& End)
	{
		const FVector2D Delta = End - Start;
		const double LengthSquared = Delta.SquaredLength();
		const double Alpha = LengthSquared > UE_DOUBLE_SMALL_NUMBER
			? FMath::Clamp(FVector2D::DotProduct(Point - Start, Delta) / LengthSquared, 0.0, 1.0)
			: 0.0;
		return (Point - (Start + Delta * Alpha)).SquaredLength();
	}

	enum class ERingContainment : uint8
	{
		Outside,
		Inside,
		Boundary
	};

	ERingContainment ClassifyRing(const FVector2D& Point, const TArray<FVector2D>& Ring)
	{
		if (Ring.Num() < 3)
		{
			return ERingContainment::Outside;
		}
		bool bInside = false;
		for (int32 Index = 0, Previous = Ring.Num() - 1; Index < Ring.Num(); Previous = Index++)
		{
			const FVector2D& A = Ring[Previous];
			const FVector2D& B = Ring[Index];
			if (PointSegmentDistanceSquared(Point, A, B) <= UE_DOUBLE_SMALL_NUMBER)
			{
				return ERingContainment::Boundary;
			}
			if ((A.Y > Point.Y) != (B.Y > Point.Y) &&
				Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside ? ERingContainment::Inside : ERingContainment::Outside;
	}

	bool PointInWaterPolygon(const FVector2D& Point, const FProjectWorldCanonicalPolygon& Polygon)
	{
		const ERingContainment Outer = ClassifyRing(Point, Polygon.Outer);
		if (Outer == ERingContainment::Outside)
		{
			return false;
		}
		if (Outer == ERingContainment::Boundary)
		{
			return true;
		}
		for (const TArray<FVector2D>& Hole : Polygon.Holes)
		{
			const ERingContainment Inner = ClassifyRing(Point, Hole);
			if (Inner == ERingContainment::Boundary)
			{
				return true;
			}
			if (Inner == ERingContainment::Inside)
			{
				return false;
			}
		}
		return true;
	}

	bool BoundsOverlap(const FVector4d& Left, const FVector4d& Right)
	{
		return Left.X < Right.Z && Left.Z > Right.X && Left.Y < Right.W && Left.W > Right.Y;
	}

	const FProjectWorldRealizationLayer* FindDependency(
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldRealizationLayer& VegetationLayer,
		const TCHAR* GeneratorId)
	{
		for (const FString& DependencyId : VegetationLayer.DependsOn)
		{
			const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate(
				[&DependencyId](const auto& Candidate) { return Candidate.LayerId == DependencyId; });
			if (Layer != nullptr && Layer->GeneratorId == GeneratorId && Layer->GeneratorVersion == 1)
			{
				return Layer;
			}
		}
		return nullptr;
	}

	bool ReadRoadClasses(
		const FProjectWorldRealizationLayer& RoadLayer,
		TSet<FString>& OutClasses,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Settings;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RoadLayer.NormalizedSettings), Settings) ||
			!Settings.IsValid() || !Settings->TryGetArrayField(TEXT("selected_classes"), Values) || Values == nullptr)
		{
			OutError = TEXT("Vegetation road exclusion cannot read the road dependency contract.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString RoadClass;
			if (!Value.IsValid() || !Value->TryGetString(RoadClass) || RoadClass.IsEmpty())
			{
				OutError = TEXT("Vegetation road exclusion found an invalid selected road class.");
				return false;
			}
			OutClasses.Add(RoadClass);
		}
		return !OutClasses.IsEmpty();
	}

	TArray<FString> CellFeatureIds(
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalBundle& Bundle,
		const TSet<FString>& RoadClasses)
	{
		TSet<FString> Candidates;
		Candidates.Append(Cell.OwnedFeatureIds);
		Candidates.Append(Cell.ReferencedFeatureIds);
		TArray<FString> Result;
		for (const FString& FeatureId : Candidates)
		{
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
			if (Feature != nullptr &&
				(Feature->FeatureClass == TEXT("water") ||
					(Feature->FeatureClass == TEXT("road") && RoadClasses.Contains(Feature->RoadClass))))
			{
				Result.Add(FeatureId);
			}
		}
		Result.Sort();
		return Result;
	}
}

EProjectWorldVegetationExclusion FProjectWorldVegetationExclusionContext::Classify(
	const FVector2D& Point) const
{
	for (const FProjectWorldVegetationRoadSegment& Segment : RoadSegments)
	{
		if (PointSegmentDistanceSquared(Point, Segment.Start, Segment.End) <=
			FMath::Square(Segment.HalfWidthMeters))
		{
			return EProjectWorldVegetationExclusion::Road;
		}
	}
	for (const FProjectWorldCanonicalPolygon* Polygon : WaterPolygons)
	{
		if (Polygon != nullptr && PointInWaterPolygon(Point, *Polygon))
		{
			return EProjectWorldVegetationExclusion::Water;
		}
	}
	for (const FProjectWorldVegetationWaterSegment& Segment : WaterSegments)
	{
		if (PointSegmentDistanceSquared(Point, Segment.Start, Segment.End) <=
			FMath::Square(Segment.ClearanceMeters))
		{
			return EProjectWorldVegetationExclusion::Water;
		}
	}
	for (const FVector4d& Bounds : AuthoredMaskBounds)
	{
		if (Point.X >= Bounds.X && Point.X <= Bounds.Z && Point.Y >= Bounds.Y && Point.Y <= Bounds.W)
		{
			return EProjectWorldVegetationExclusion::AuthoredMask;
		}
	}
	return EProjectWorldVegetationExclusion::None;
}

namespace ProjectWorldVegetationExclusions
{
	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldRealizationLayer& VegetationLayer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldVegetationExclusionContext& OutContext,
		FString& OutError)
	{
		OutContext = FProjectWorldVegetationExclusionContext();
		const FProjectWorldRealizationLayer* RoadLayer = FindDependency(
			Profile, VegetationLayer, TEXT("project_road_mesh"));
		const FProjectWorldRealizationLayer* WaterLayer = FindDependency(
			Profile, VegetationLayer, TEXT("project_water_mesh"));
		if (RoadLayer == nullptr || WaterLayer == nullptr)
		{
			OutError = TEXT("Vegetation v1 requires road and water exclusion dependencies.");
			return false;
		}
		TSet<FString> RoadClasses;
		if (!ReadRoadClasses(*RoadLayer, RoadClasses, OutError))
		{
			return false;
		}
		FString Identity(TEXT("project_vegetation_exclusions_v1"));
		AppendToken(Identity, Cell.CellId);
		for (const FString& FeatureId : CellFeatureIds(Cell, Bundle, RoadClasses))
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			if (Feature.FeatureClass == TEXT("road"))
			{
				for (const FProjectWorldCanonicalRepresentation& Representation : Feature.Representations)
				{
					if (Representation.CellId != Cell.CellId || Representation.Kind != TEXT("road_fragment"))
					{
						continue;
					}
					for (const TArray<FVector2D>& Part : Representation.Parts)
					{
						for (int32 Index = 0; Index + 1 < Part.Num(); ++Index)
						{
							FProjectWorldVegetationRoadSegment& Segment = OutContext.RoadSegments.AddDefaulted_GetRef();
							Segment.Start = Part[Index];
							Segment.End = Part[Index + 1];
							Segment.HalfWidthMeters = Feature.WidthMeters * 0.5;
							AppendToken(Identity, Feature.FeatureId);
							AppendNumber(Identity, Segment.HalfWidthMeters);
							AppendNumber(Identity, Segment.Start.X);
							AppendNumber(Identity, Segment.Start.Y);
							AppendNumber(Identity, Segment.End.X);
							AppendNumber(Identity, Segment.End.Y);
						}
					}
				}
			}
			else if (!Feature.WaterSurface.bValid)
			{
				OutError = TEXT("Vegetation water exclusion found incomplete canonical surface authority.");
				return false;
			}
			else if (Feature.WaterSurface.Geometry == TEXT("polygon"))
			{
				AppendToken(Identity, Feature.FeatureId);
				AppendToken(Identity, Feature.WaterSurface.Geometry);
				for (const FProjectWorldCanonicalPolygon& Polygon : Feature.GeometryPolygons)
				{
					OutContext.WaterPolygons.Add(&Polygon);
					AppendToken(Identity, FString::FromInt(Polygon.Outer.Num()));
					for (const FVector2D& Point : Polygon.Outer)
					{
						AppendNumber(Identity, Point.X);
						AppendNumber(Identity, Point.Y);
					}
					AppendToken(Identity, FString::FromInt(Polygon.Holes.Num()));
					for (const TArray<FVector2D>& Hole : Polygon.Holes)
					{
						AppendToken(Identity, FString::FromInt(Hole.Num()));
						for (const FVector2D& Point : Hole)
						{
							AppendNumber(Identity, Point.X);
							AppendNumber(Identity, Point.Y);
						}
					}
				}
			}
			else if (Feature.WaterSurface.Geometry == TEXT("ribbon") && Feature.WidthMeters > 0.0)
			{
				AppendToken(Identity, Feature.FeatureId);
				AppendToken(Identity, Feature.WaterSurface.Geometry);
				AppendNumber(Identity, Feature.WidthMeters);
				for (const TArray<FVector2D>& Part : Feature.GeometryParts)
				{
					for (int32 Index = 0; Index + 1 < Part.Num(); ++Index)
					{
						FProjectWorldVegetationWaterSegment& Segment =
							OutContext.WaterSegments.AddDefaulted_GetRef();
						Segment.Start = Part[Index];
						Segment.End = Part[Index + 1];
						Segment.ClearanceMeters = Feature.WidthMeters * 0.5;
						AppendNumber(Identity, Segment.Start.X);
						AppendNumber(Identity, Segment.Start.Y);
						AppendNumber(Identity, Segment.End.X);
						AppendNumber(Identity, Segment.End.Y);
					}
				}
			}
			else
			{
				OutError = TEXT("Vegetation water exclusion found unsupported canonical geometry.");
				return false;
			}
		}

		TArray<const FProjectWorldAuthoredOverlay*> Masks;
		for (const FProjectWorldAuthoredOverlay& Overlay : AuthoredOverlaySet.Overlays)
		{
			if (Overlay.Anchor.Kind == EProjectWorldAnchorKind::Mask &&
				Overlay.Anchor.Excludes.Contains(TEXT("vegetation")) &&
				BoundsOverlap(Overlay.Anchor.BoundsMeters, Cell.Bounds))
			{
				Masks.Add(&Overlay);
			}
		}
		Masks.Sort([](const auto& Left, const auto& Right) { return Left.OverlayId < Right.OverlayId; });
		for (const FProjectWorldAuthoredOverlay* Mask : Masks)
		{
			OutContext.AuthoredMaskBounds.Add(Mask->Anchor.BoundsMeters);
			AppendToken(Identity, Mask->OverlayId);
			AppendNumber(Identity, Mask->Anchor.BoundsMeters.X);
			AppendNumber(Identity, Mask->Anchor.BoundsMeters.Y);
			AppendNumber(Identity, Mask->Anchor.BoundsMeters.Z);
			AppendNumber(Identity, Mask->Anchor.BoundsMeters.W);
		}
		if (!HashText(Identity, OutContext.InputHash))
		{
			OutError = TEXT("Cannot hash vegetation exclusion input.");
			return false;
		}
		return true;
	}
}
