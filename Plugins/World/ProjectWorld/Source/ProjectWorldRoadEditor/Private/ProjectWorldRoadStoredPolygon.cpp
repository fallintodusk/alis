// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadStoredPolygon.h"
#include "ProjectWorldTerrainSurface.h"
#include "CompGeom/ExactPredicates.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"
#include <limits>

namespace ProjectWorldRoadStoredPolygon
{
	namespace
	{
		double SignedArea(const TArray<FVector>& Points, double& Products)
		{
			double Sum = 0; Products = 0;
			for (int32 I = 1; I + 1 < Points.Num(); ++I)
			{
				const auto A = Points[I] - Points[0], B = Points[I + 1] - Points[0];
				Sum += A.X * B.Y - A.Y * B.X;
				Products += FMath::Abs(A.X * B.Y) + FMath::Abs(A.Y * B.X);
			}
			return Sum * 0.5;
		}
	}
	static bool Convert(const TArray<FVector>& Input, const FProjectWorldSurfaceTriangle& Plane, FVertexMap& Vertices, TArray<FVector>& Positions,
		TArray<FIntVector>& Faces, FString& Error, bool bAllowCollapsed)
	{
		auto Refuse = [&](const TCHAR* Message) { Error = Message; return false; };
		if (Input.Num() < 3 || Input.Num() > 6) return Refuse(TEXT("Road intersection polygon exceeds its convex bound."));
		for (const auto& P : Input)
			if (P.ContainsNaN() || FVector3f(P).ContainsNaN()) return Refuse(TEXT("Road elevation exceeds native mesh position storage."));
		double TestHeight;
		if (!ProjectWorldTerrainSurface::Height(Plane, FVector2D(Input[0]), TestHeight, Error)) return false;
		const FVector PlaneU = Plane.Points[1] - Plane.Points[0], PlaneV = Plane.Points[2] - Plane.Points[0];
		const double D = PlaneU.X * PlaneV.Y - PlaneU.Y * PlaneV.X;
		const double DX = (PlaneU.Z * PlaneV.Y - PlaneU.Y * PlaneV.Z) / D;
		const double DY = (PlaneU.X * PlaneV.Z - PlaneU.Z * PlaneV.X) / D;
		const double Gradient = FMath::Sqrt(DX * DX + DY * DY);
		if (!FMath::IsFinite(Gradient)) return Refuse(TEXT("Road source plane gradient is non-finite."));
		TArray<double> EvaluationBounds;
		for (int32 I = 0; I < Input.Num(); ++I)
		{
			for (int32 J = 0; J < I; ++J)
				if (Input[I].X == Input[J].X && Input[I].Y == Input[J].Y && Input[I].Z != Input[J].Z)
					return Refuse(TEXT("Road source has unequal heights at one XY location."));
			if (!ProjectWorldTerrainSurface::Height(Plane, FVector2D(Input[I]), TestHeight, Error)) return false;
			const double Bound = 64 * std::numeric_limits<double>::epsilon() * (FMath::Abs(Input[I].Z) +
				FMath::Abs(Plane.Points[0].Z) + FMath::Abs(DX * (Input[I].X - Plane.Points[0].X)) + FMath::Abs(DY * (Input[I].Y - Plane.Points[0].Y)));
			if (!FMath::IsFinite(Bound) || FMath::Abs(TestHeight - Input[I].Z) > Bound)
				return Refuse(TEXT("Road source polygon has no single affine height plane."));
			EvaluationBounds.Add(Bound);
		}
		TArray<FVector> Stored;
		FVertexMap Pending;
		double Displacement = 0, OriginalPerimeter = 0, StoredPerimeter = 0;
		for (int32 I = 0; I < Input.Num(); ++I)
		{
			const auto& P = Input[I];
			FVector3f Value(P);
			const FVector2f Key(Value.X, Value.Y);
			const auto* Previous = Vertices.Find(Key);
			if (!Previous) Previous = Pending.Find(Key);
			if (Previous)
			{
				const double Bound = FMath::Max(Gradient, Previous->Gradient) * FVector2D(P - Previous->Source).Length() +
					EvaluationBounds[I] + Previous->EvaluationRoundoff;
				if (!FMath::IsFinite(Bound) || FMath::Abs(P.Z - Previous->Source.Z) > Bound)
					return Refuse(TEXT("Road storage aliases conflicting source heights."));
				Value.Z = static_cast<float>(Previous->Stored.Z);
			}
			else Pending.Add(Key, {P, FVector(Value), Gradient, EvaluationBounds[I]});
			Stored.Add(FVector(Value));
			Displacement = FMath::Max(Displacement, FVector2D(Stored.Last() - P).Length());
		}
		for (int32 I = 0; I < Input.Num(); ++I)
		{
			OriginalPerimeter += FVector2D(Input[(I + 1) % Input.Num()] - Input[I]).Length();
			StoredPerimeter += FVector2D(Stored[(I + 1) % Stored.Num()] - Stored[I]).Length();
			for (int32 J = 0; J < I; ++J)
				if (Stored[I].X == Stored[J].X && Stored[I].Y == Stored[J].Y && Stored[I].Z != Stored[J].Z)
					return Refuse(TEXT("Road storage collapses unequal heights to one XY location."));
		}
		double OriginalProducts, StoredProducts;
		const double OriginalArea = SignedArea(Input, OriginalProducts);
		const double StoredArea = SignedArea(Stored, StoredProducts);
		if (!FMath::IsFinite(OriginalArea) || !FMath::IsFinite(StoredArea) || OriginalArea == 0 ||
			OriginalArea * StoredArea < 0 || (StoredArea == 0 && !bAllowCollapsed))
		{
			Error = FString::Printf(TEXT("Road storage loses or reverses polygon: %.17g -> %.17g cm2; %d points."), OriginalArea, StoredArea, Input.Num());
			Error += FString::Printf(TEXT(" Source plane XY area %.17g cm2."), D * 0.5);
			for (int32 I = 0; I < Input.Num(); ++I)
				Error += FString::Printf(TEXT(" [%.17g,%.17g,%.17g -> %.17g,%.17g,%.17g]"),
					Input[I].X, Input[I].Y, Input[I].Z, Stored[I].X, Stored[I].Y, Stored[I].Z);
			return false;
		}
		const double Roundoff = 64 * std::numeric_limits<double>::epsilon() * (OriginalProducts + StoredProducts);
		const double ConversionBound = 2 * FMath::Max(OriginalPerimeter, StoredPerimeter) * Displacement + 2 * UE_DOUBLE_PI * Displacement * Displacement;
		if (!FMath::IsFinite(ConversionBound) || FMath::Abs(OriginalArea - StoredArea) > ConversionBound + Roundoff)
			return Refuse(TEXT("Road stored coverage exceeds its measured conversion bound."));
		TArray<FVector> Candidate;
		for (const auto& P : Stored) if (Candidate.IsEmpty() || Candidate.Last() != P) Candidate.Add(P);
		if (Candidate.Num() > 1 && Candidate.Last() == Candidate[0]) Candidate.Pop();
		if (StoredArea == 0)
		{
			for (int32 I = 1; I + 1 < Candidate.Num(); ++I)
				if (!FVector::CrossProduct(Candidate[I] - Candidate[0], Candidate[I + 1] - Candidate[0]).IsNearlyZero(0))
					return Refuse(TEXT("Road collapsed split retains an incompatible vertical height face."));
			Positions = MoveTemp(Candidate); Faces.Reset();
			for (auto& Pair : Pending) Vertices.Add(Pair.Key, MoveTemp(Pair.Value));
			return true;
		}
		for (int32 I = 0; I < Candidate.Num(); ++I)
		{
			const auto A = Candidate[I], B = Candidate[(I + 1) % Candidate.Num()];
			for (const auto& P : Candidate)
				if (FVector2D::CrossProduct(FVector2D(B - A), FVector2D(P - A)) * StoredArea < 0)
					return Refuse(TEXT("Road storage changes convex polygon ordering."));
		}
		TArray<FIntVector> Indices;
		double FanArea = 0;
		for (int32 I = 1; I + 1 < Candidate.Num(); ++I)
		{
			const auto Cross = FVector::CrossProduct(Candidate[I] - Candidate[0], Candidate[I + 1] - Candidate[0]);
			if (Cross.ContainsNaN()) return Refuse(TEXT("Road stored triangle is non-finite."));
			if (Cross.IsNearlyZero(0)) continue;
			if (Cross.Z == 0 || Cross.Z * StoredArea <= 0) return Refuse(TEXT("Road storage creates a vertical or reversed height face."));
			FanArea += Cross.Z * 0.5;
			Indices.Add(FIntVector(0, I, I + 1));
		}
		if (Indices.IsEmpty() || FMath::Abs(FanArea - StoredArea) > Roundoff)
			return Refuse(TEXT("Road stored triangulation has incomplete coverage."));
		Positions = MoveTemp(Candidate); Faces = MoveTemp(Indices);
		for (auto& Pair : Pending) Vertices.Add(Pair.Key, MoveTemp(Pair.Value));
		return true;
	}
	bool Store(const TArray<FVector>& Input, const FProjectWorldSurfaceTriangle& Plane, FVertexMap& Vertices, TArray<FVector>& Positions,
		TArray<FIntVector>& Faces, FString& Error)
	{
		return Convert(Input, Plane, Vertices, Positions, Faces, Error, false);
	}

	bool StoreGroup(const TArray<FProjectWorldSurfaceTriangle>& Pieces, const TArray<FVector2D>& Domain,
		FVertexMap& Vertices, TArray<FProjectWorldSurfaceTriangle>& Faces, FString& Error, const FBox2D* ClipBounds)
	{
		using namespace UE::Geometry;
		auto Refuse = [&](const TCHAR* Message) { Error = Message; return false; };
		if (Domain.Num() < 3 || Domain.Num() > 7 || Pieces.IsEmpty()) return Refuse(TEXT("Road storage group has no bounded original domain."));
		const FVector2D Origin = Domain[0];
		FPolygon2d Expected, FloatDomain;
		double Perimeter = 0, Extent = 0;
		for (int32 I = 0; I < Domain.Num(); ++I)
		{
			if (Domain[I].ContainsNaN() || FVector2f(Domain[I]).ContainsNaN()) return Refuse(TEXT("Road storage domain is not finite."));
			Expected.AppendVertex(Domain[I] - Origin);
			FloatDomain.AppendVertex(FVector2D(FVector2f(Domain[I])) - Origin);
			Perimeter += (Domain[(I + 1) % Domain.Num()] - Domain[I]).Length();
			Extent = FMath::Max(Extent, (Domain[I] - Origin).Length());
		}
		const double ExpectedArea = FMath::Abs(Expected.SignedArea());
		if (!FMath::IsFinite(ExpectedArea) || ExpectedArea <= 0 || Expected.SignedArea() * FloatDomain.SignedArea() <= 0)
			return Refuse(TEXT("Road storage collapses or reverses the original ribbon domain."));
		if (Expected.IsClockwise()) Expected.Reverse();
		TArray<FGeneralPolygon2d> ExpectedPolygons{FGeneralPolygon2d(Expected)}, Polygons;
		if (ClipBounds)
		{
			const auto A = ClipBounds->Min - Origin, B = ClipBounds->Max - Origin;
			TArray<FGeneralPolygon2d> Rectangle{FGeneralPolygon2d(FPolygon2d({A, FVector2d(B.X, A.Y), B, FVector2d(A.X, B.Y)}))}, Clipped;
			if (!PolygonsIntersection(ExpectedPolygons, Rectangle, Clipped) || Clipped.Num() != 1 || !Clipped[0].GetHoles().IsEmpty())
				return Refuse(TEXT("Road storage original-domain clipping failed."));
			ExpectedPolygons = MoveTemp(Clipped); Expected = ExpectedPolygons[0].GetOuter();
			Perimeter = Expected.Perimeter();
		}
		auto Bindings = Vertices;
		TArray<FProjectWorldSurfaceTriangle> Candidate;
		double Radius = 0, TotalArea = 0;
		for (const auto& Piece : Pieces)
		{
			TArray<FVector> Raw{Piece.Points[0], Piece.Points[1], Piece.Points[2]}, Stored;
			TArray<FIntVector> Indices;
			if (!Convert(Raw, Piece, Bindings, Stored, Indices, Error, true)) return false;
			for (const auto& P : Raw)
			{
				const auto* Binding = Bindings.Find(FVector2f(P.X, P.Y));
				if (!Binding) return Refuse(TEXT("Road storage lost a converted vertex binding."));
				Radius = FMath::Max(Radius, FVector2D(Binding->Stored - P).Length());
			}
			for (const auto& Index : Indices)
			{
				FProjectWorldSurfaceTriangle Face{{Stored[Index.X], Stored[Index.Y], Stored[Index.Z]}};
				FPolygon2d Polygon({FVector2D(Face.Points[0]) - Origin, FVector2D(Face.Points[1]) - Origin, FVector2D(Face.Points[2]) - Origin});
				if (Polygon.IsClockwise()) Polygon.Reverse();
				TotalArea += Polygon.SignedArea();
				Polygons.Add(FGeneralPolygon2d(MoveTemp(Polygon))); Candidate.Add(Face);
			}
		}
		// Max displacement bounds boundary conversion; internal subdivisions never multiply the allowance.
		const double Conversion = 2 * Perimeter * Radius + 2 * UE_DOUBLE_PI * Radius * Radius;
		const double Arithmetic = 1024 * std::numeric_limits<double>::epsilon() * FMath::Max(1.0, Extent * Extent) * (Domain.Num() + Polygons.Num());
		if (!FMath::IsFinite(Conversion) || !FMath::IsFinite(Arithmetic) || !FMath::IsFinite(TotalArea) || Polygons.IsEmpty())
			return Refuse(TEXT("Road original domain has no finite stored coverage."));
		TArray<FGeneralPolygon2d> United, Missing, Exterior;
		if (!PolygonsUnion(Polygons, United, false)) return Refuse(TEXT("Native Road stored-domain union failed."));
		bool HasHoles = false;
		for (const auto& Shape : United) HasHoles |= !Shape.GetHoles().IsEmpty();
		if (United.IsEmpty() || HasHoles)
		{
			Error = FString::Printf(TEXT("Native Road stored-domain union: %d components, %d source pieces."), United.Num(), Pieces.Num());
			for (const auto& Shape : United)
			{
				Error += FString::Printf(TEXT(" [area %.17g cm2; %d holes"), Shape.SignedArea(), Shape.GetHoles().Num());
				for (const auto& Hole : Shape.GetHoles()) Error += FString::Printf(TEXT(" %.17g cm2"), Hole.SignedArea());
				Error += TEXT("]");
			}
			return false;
		}
		// Integer normalization inside polygon union can split an exact shared edge.
		// Certify connectivity on the actual float vertices with native exact predicates.
		TArray<TArray<FVector2d>> Actual;
		for (const auto& Face : Candidate)
		{
			TArray<FVector2d> Points{FVector2d(Face.Points[0]), FVector2d(Face.Points[1]), FVector2d(Face.Points[2])};
			if (ExactPredicates::Orient2(Points[0], Points[1], Points[2]) < 0) Swap(Points[1], Points[2]);
			Actual.Add(MoveTemp(Points));
		}
		TBitArray<> Seen(false, Polygons.Num());
		TArray<int32> Reached{0}; Seen[0] = true;
		for (int32 At = 0; At < Reached.Num(); ++At)
		{
			const auto& First = Actual[Reached[At]];
			for (int32 Other = 0; Other < Polygons.Num(); ++Other)
			{
				if (Seen[Other]) continue;
				const auto& Second = Actual[Other];
				bool Adjacent = false;
				for (int32 I = 0; I < First.Num() && !Adjacent; ++I)
				{
					const auto& FirstStart = First[I]; const auto& FirstEnd = First[(I + 1) % First.Num()];
					const int32 Axis = FirstStart.X != FirstEnd.X ? 0 : 1;
					for (int32 J = 0; J < Second.Num(); ++J)
					{
						const auto& SecondStart = Second[J]; const auto& SecondEnd = Second[(J + 1) % Second.Num()];
						if (ExactPredicates::Orient2(FirstStart, FirstEnd, SecondStart) != 0 ||
							ExactPredicates::Orient2(FirstStart, FirstEnd, SecondEnd) != 0) continue;
						if ((FirstEnd[Axis] > FirstStart[Axis]) == (SecondEnd[Axis] > SecondStart[Axis])) continue;
						Adjacent = FMath::Max(FMath::Min(FirstStart[Axis], FirstEnd[Axis]), FMath::Min(SecondStart[Axis], SecondEnd[Axis])) <
							FMath::Min(FMath::Max(FirstStart[Axis], FirstEnd[Axis]), FMath::Max(SecondStart[Axis], SecondEnd[Axis]));
						if (Adjacent) break;
					}
				}
				if (Adjacent) { Reached.Add(Other); Seen[Other] = true; }
			}
		}
		if (Reached.Num() != Polygons.Num()) return Refuse(TEXT("Road stored source faces are not one edge-connected surface."));
		if (!PolygonsDifference(ExpectedPolygons, United, Missing) || !PolygonsDifference(United, ExpectedPolygons, Exterior))
			return Refuse(TEXT("Native Road stored-domain difference failed."));
		auto Area = [](const TArray<FGeneralPolygon2d>& Shapes)
		{
			double Result = 0;
			for (const auto& Shape : Shapes) Result += FMath::Abs(Shape.SignedArea());
			return Result;
		};
		const double MissingArea = Area(Missing), ExteriorArea = Area(Exterior), UnitedArea = Area(United);
		if (!FMath::IsFinite(MissingArea) || !FMath::IsFinite(ExteriorArea) || !FMath::IsFinite(UnitedArea) ||
			MissingArea > Conversion + Arithmetic || ExteriorArea > Conversion + Arithmetic || FMath::Abs(TotalArea - UnitedArea) > Arithmetic)
			return Refuse(TEXT("Road stored domain loses coverage, leaves its original boundary or duplicates faces."));
		for (const auto& Polygon : Polygons) for (const auto& P : Polygon.GetOuter().GetVertices())
			if (!Expected.Contains(P) && Expected.DistanceSquared(P) > Radius * Radius + Arithmetic)
				return Refuse(TEXT("Road stored vertex leaves the measured boundary conversion envelope."));
		TArray<FGeneralPolygon2d> Inner, Uncovered;
		if (Radius == 0) Inner = ExpectedPolygons;
		else if (!PolygonsOffset(-Radius, ExpectedPolygons, Inner, false, 2, EPolygonOffsetJoinType::Miter))
			return Refuse(TEXT("Native Road boundary conversion inset failed."));
		if (Inner.IsEmpty() || !PolygonsDifference(Inner, United, Uncovered) || !FMath::IsFinite(Area(Uncovered)) || Area(Uncovered) > Arithmetic)
			return Refuse(TEXT("Road stored coverage leaves an unproven original-domain interior."));
		Vertices = MoveTemp(Bindings); Faces = MoveTemp(Candidate);
		return true;
	}
}
