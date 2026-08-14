// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGeometryParsing.h"

#include "ProjectWorldCanonicalBundle.h"

#include "Dom/JsonObject.h"

namespace ProjectWorldGeometryParsing
{
	void Reject(
		FProjectWorldCanonicalValidation& Validation,
		const TCHAR* Code,
		const FString& Message,
		const FString& Detail = FString())
	{
		Validation.ErrorCode = Code;
		Validation.Message = Message;
		Validation.Detail = Detail;
	}

	bool ReadPoint(
		const TSharedPtr<FJsonValue>& Value,
		FVector2D& OutPoint,
		FProjectWorldCanonicalValidation& Validation)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (!Value->TryGetArray(Pair) || Pair == nullptr || Pair->Num() != 2)
		{
			Reject(Validation, TEXT("geometry-shape"), TEXT("Geometry point is not a coordinate pair."));
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!(*Pair)[0]->TryGetNumber(X) || !(*Pair)[1]->TryGetNumber(Y) ||
			!FMath::IsFinite(X) || !FMath::IsFinite(Y))
		{
			Reject(Validation, TEXT("geometry-value"), TEXT("Geometry coordinate is not finite."));
			return false;
		}
		OutPoint = FVector2D(X, Y);
		return true;
	}

	bool ReadLinePoints(
		const TArray<TSharedPtr<FJsonValue>>& Values,
		TArray<FVector2D>& OutPoints,
		FProjectWorldCanonicalValidation& Validation)
	{
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			FVector2D Point;
			if (!ReadPoint(Value, Point, Validation))
			{
				return false;
			}
			OutPoints.Add(Point);
		}
		return OutPoints.Num() >= 2;
	}

	bool ReadPolygon(
		const TArray<TSharedPtr<FJsonValue>>& Rings,
		FProjectWorldCanonicalPolygon& OutPolygon,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (Rings.IsEmpty())
		{
			Reject(Validation, TEXT("geometry-shape"), TEXT("Polygon has no outer ring."));
			return false;
		}

		for (int32 RingIndex = 0; RingIndex < Rings.Num(); ++RingIndex)
		{
			const TArray<TSharedPtr<FJsonValue>>* RingValues = nullptr;
			TArray<FVector2D> Ring;
			if (!Rings[RingIndex]->TryGetArray(RingValues) || RingValues == nullptr ||
				!ReadLinePoints(*RingValues, Ring, Validation) || Ring.Num() < 4 ||
				!Ring[0].Equals(Ring.Last(), KINDA_SMALL_NUMBER))
			{
				Reject(Validation, TEXT("geometry-shape"), TEXT("Polygon ring is invalid or not closed."));
				return false;
			}
			if (RingIndex == 0)
			{
				OutPolygon.Outer = MoveTemp(Ring);
			}
			else
			{
				OutPolygon.Holes.Add(MoveTemp(Ring));
			}
		}
		return !OutPolygon.Outer.IsEmpty();
	}

	bool ReadGeometry(
		const TSharedPtr<FJsonObject>& Geometry,
		FString& OutType,
		TArray<FVector2D>& OutOuterPoints,
		FProjectWorldCanonicalValidation& OutValidation,
		TArray<TArray<FVector2D>>* OutParts,
		TArray<FProjectWorldCanonicalPolygon>* OutPolygons)
	{
		const TArray<TSharedPtr<FJsonValue>>* Coordinates = nullptr;
		if (!Geometry->TryGetStringField(TEXT("type"), OutType) || OutType.IsEmpty() ||
			!Geometry->TryGetArrayField(TEXT("coordinates"), Coordinates) || Coordinates == nullptr)
		{
			Reject(OutValidation, TEXT("geometry-shape"), TEXT("Geometry type or coordinates are missing."));
			return false;
		}

		if (OutType == TEXT("LineString"))
		{
			if (!ReadLinePoints(*Coordinates, OutOuterPoints, OutValidation))
			{
				return false;
			}
			if (OutParts != nullptr)
			{
				OutParts->Add(OutOuterPoints);
			}
			return true;
		}
		if (OutType == TEXT("Point"))
		{
			FVector2D Point;
			const TSharedPtr<FJsonValue> PointValue = MakeShared<FJsonValueArray>(*Coordinates);
			if (!ReadPoint(PointValue, Point, OutValidation))
			{
				return false;
			}
			OutOuterPoints.Add(Point);
			if (OutParts != nullptr)
			{
				OutParts->Add({Point});
			}
			return true;
		}
		if (OutType == TEXT("MultiPoint"))
		{
			if (Coordinates->IsEmpty())
			{
				Reject(OutValidation, TEXT("geometry-shape"), TEXT("MultiPoint has no points."));
				return false;
			}
			for (const TSharedPtr<FJsonValue>& PointValue : *Coordinates)
			{
				FVector2D Point;
				if (!ReadPoint(PointValue, Point, OutValidation))
				{
					return false;
				}
				OutOuterPoints.Add(Point);
				if (OutParts != nullptr)
				{
					OutParts->Add({Point});
				}
			}
			return true;
		}
		if (OutType == TEXT("MultiLineString"))
		{
			if (Coordinates->IsEmpty())
			{
				Reject(OutValidation, TEXT("geometry-shape"), TEXT("MultiLineString has no lines."));
				return false;
			}
			for (const TSharedPtr<FJsonValue>& LineValue : *Coordinates)
			{
				const TArray<TSharedPtr<FJsonValue>>* LineValues = nullptr;
				TArray<FVector2D> Line;
				if (!LineValue->TryGetArray(LineValues) || LineValues == nullptr ||
					!ReadLinePoints(*LineValues, Line, OutValidation))
				{
					return false;
				}
				OutOuterPoints.Append(Line);
				if (OutParts != nullptr)
				{
					OutParts->Add(MoveTemp(Line));
				}
			}
			return true;
		}
		if (OutType == TEXT("Polygon"))
		{
			FProjectWorldCanonicalPolygon Polygon;
			if (!ReadPolygon(*Coordinates, Polygon, OutValidation))
			{
				return false;
			}
			OutOuterPoints.Append(Polygon.Outer);
			if (OutParts != nullptr)
			{
				OutParts->Add(Polygon.Outer);
			}
			if (OutPolygons != nullptr)
			{
				OutPolygons->Add(MoveTemp(Polygon));
			}
			return true;
		}
		if (OutType == TEXT("MultiPolygon"))
		{
			if (Coordinates->IsEmpty())
			{
				Reject(OutValidation, TEXT("geometry-shape"), TEXT("MultiPolygon has no polygons."));
				return false;
			}
			for (const TSharedPtr<FJsonValue>& PolygonValue : *Coordinates)
			{
				const TArray<TSharedPtr<FJsonValue>>* Rings = nullptr;
				FProjectWorldCanonicalPolygon Polygon;
				if (!PolygonValue->TryGetArray(Rings) || Rings == nullptr ||
					!ReadPolygon(*Rings, Polygon, OutValidation))
				{
					return false;
				}
				OutOuterPoints.Append(Polygon.Outer);
				if (OutParts != nullptr)
				{
					OutParts->Add(Polygon.Outer);
				}
				if (OutPolygons != nullptr)
				{
					OutPolygons->Add(MoveTemp(Polygon));
				}
			}
			return !OutOuterPoints.IsEmpty();
		}

		Reject(OutValidation, TEXT("geometry-type"), TEXT("Unsupported canonical geometry type."), OutType);
		return false;
	}
}
