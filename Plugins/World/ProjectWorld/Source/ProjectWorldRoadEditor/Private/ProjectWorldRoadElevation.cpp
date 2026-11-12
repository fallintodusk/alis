// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadElevation.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldRoadReferencedSurface.h"
#include "ProjectWorldRoadFormation.h"

namespace ProjectWorldRoadElevation
{
	namespace
	{
		FProjectWorldSurfaceTriangle Triangle(const FProjectWorldCanonicalRoadElevationTriangle& Face)
		{ return FProjectWorldSurfaceTriangle{{Face.UVZ[0], Face.UVZ[1], Face.UVZ[2]}}; }
		FBox2D Bounds(const FProjectWorldSurfaceTriangle& Face)
		{
			FBox2D Result(ForceInit);
			for (const auto& P : Face.Points) Result += FVector2D(P);
			return Result;
		}
		double Area(const FProjectWorldSurfaceTriangle& Face)
		{
			return ProjectWorldTerrainSurface::Area({FVector2D(Face.Points[0]), FVector2D(Face.Points[1]), FVector2D(Face.Points[2])});
		}
		bool Map(const FProjectWorldSurfaceTriangle& Parameter, const FProjectWorldSurfaceTriangle& XY,
			const FVector2D& P, double Z, FVector& Out, FString& Error)
		{
			const FVector A = Parameter.Points[1] - Parameter.Points[0], B = Parameter.Points[2] - Parameter.Points[0];
			const double D = A.X * B.Y - A.Y * B.X;
			if (!FMath::IsFinite(D) || FMath::Abs(D) <= 1e-12)
			{ Error = TEXT("Road station mapping is degenerate."); return false; }
			const FVector2D Delta = P - FVector2D(Parameter.Points[0]);
			const double U = (Delta.X * B.Y - Delta.Y * B.X) / D;
			const double V = (A.X * Delta.Y - A.Y * Delta.X) / D;
			Out = XY.Points[0] + (XY.Points[1] - XY.Points[0]) * U + (XY.Points[2] - XY.Points[0]) * V;
			Out.Z = Z;
			return !Out.ContainsNaN();
		}
	}

	bool Validate(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature,
		FString& ConsumedIdentity, FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		if (!Feature.RoadElevation.IsSet()) return true;
		const auto& Profile = Feature.RoadElevation.GetValue();
		if (Profile.EndpointRelation == EProjectWorldRoadEndpointRelation::ConstructedSurface)
		{
			bool bHash = Profile.ConstructedSurfaceHash.Len() == 64;
			for (const TCHAR C : Profile.ConstructedSurfaceHash)
				bHash &= (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f');
			if (!bHash)
			{ Error = TEXT("Constructed Road endpoints require their exact supporting model identity."); return false; }
		}
		else if (Profile.EndpointRelation != EProjectWorldRoadEndpointRelation::TerrainAbutment || !Profile.ConstructedSurfaceHash.IsEmpty())
		{ Error = TEXT("Road endpoint support is unknown or inconsistent with its model."); return false; }
		const bool bReferenced = Profile.Domain == EProjectWorldRoadElevationDomain::ReferencedConstructedSurface;
		const bool bParameter = Profile.Domain == EProjectWorldRoadElevationDomain::SourceParameter;
		if ((!bReferenced && !bParameter) ||
			(bReferenced && (Profile.EndpointRelation != EProjectWorldRoadEndpointRelation::ConstructedSurface ||
				!Profile.Triangles.IsEmpty() || Profile.ReferencedSurfaceModels.IsEmpty())) ||
			(bParameter && (Profile.Triangles.Num() < 2 || Profile.Triangles.Num() > 4096 || !Profile.ReferencedSurfaceModels.IsEmpty())) ||
			Feature.GeometryType != TEXT("LineString") || Feature.GeometryPoints.Num() < 2 ||
			Profile.SourceFeatureId != Feature.FeatureId || Profile.SourcePoints != Feature.GeometryPoints ||
			!FMath::IsFinite(Profile.LengthMeters) || Profile.LengthMeters <= 0 ||
			!FMath::IsFinite(Profile.WidthMeters) || Profile.WidthMeters != Feature.WidthMeters ||
			Profile.TerrainArtifacts.IsEmpty())
		{ Error = TEXT("Road elevation requires a finite, qualified single source line and width."); return false; }
		double Length = 0;
		for (int32 I = 0; I + 1 < Feature.GeometryPoints.Num(); ++I)
		{
			const double Segment = (Feature.GeometryPoints[I + 1] - Feature.GeometryPoints[I]).Length();
			if (!FMath::IsFinite(Segment) || Segment <= 1e-8)
			{ Error = TEXT("Road elevation source has a repeated or unknown station."); return false; }
			Length += Segment;
		}
		if (FMath::Abs(Length - Profile.LengthMeters) > 1e-8)
		{ Error = TEXT("Road elevation length differs from its original source direction."); return false; }
		FString Identity = TEXT("road_affine_elevation_v1|");
		if (Profile.EndpointRelation == EProjectWorldRoadEndpointRelation::ConstructedSurface)
			Identity += TEXT("constructed_surface|") + Profile.ConstructedSurfaceHash + TEXT("|");
		Identity += FString::Printf(TEXT("%.17g|%.17g|"), Profile.LengthMeters, Profile.WidthMeters);
		for (const auto& P : Feature.GeometryPoints) Identity += FString::Printf(TEXT("%.17g,%.17g;"), P.X, P.Y);
		TArray<FString> Keys;
		TSet<FString> Support;
		for (int32 I = 0; I + 1 < Feature.GeometryPoints.Num(); ++I)
		{
			FBox2D Domain(ForceInit);
			Domain += Feature.GeometryPoints[I]; Domain += Feature.GeometryPoints[I + 1];
			Domain = Domain.ExpandBy(Feature.WidthMeters * 0.5);
			for (const auto& C : Bundle.Cells)
				if (Domain.Intersect(FBox2D(FVector2D(C.Bounds.X, C.Bounds.Y), FVector2D(C.Bounds.Z, C.Bounds.W)))) Support.Add(C.CellId);
		}
		if (Support.Num() != Profile.TerrainArtifacts.Num())
		{ Error = TEXT("Road elevation omits its finite source-support census."); return false; }
		Profile.TerrainArtifacts.GetKeys(Keys);
		Keys.Sort();
		for (const auto& Key : Keys)
		{
			const auto* Cell = Bundle.Cells.FindByPredicate([&](const auto& C) { return C.CellId == Key; });
			if (!Support.Contains(Key) || !Cell || ProjectWorldCivilGrade::BaseHash(Cell->Terrain) != Profile.TerrainArtifacts[Key])
			{ Error = TEXT("Road elevation support Terrain is missing or stale."); return false; }
			Identity += FString::Printf(TEXT("%d:%s%s|"), Key.Len(), *Key, *Profile.TerrainArtifacts[Key]);
		}
		if (bReferenced)
		{
			TArray<FProjectWorldSurfaceTriangle> Field; FString ReferencedIdentity;
			if (!ProjectWorldRoadReferencedSurface::Read(Bundle,Profile,Field,ReferencedIdentity,Error)) return false;
			ConsumedIdentity = Identity+ReferencedIdentity; return true;
		}
		double Total = 0;
		for (int32 I = 0; I < Profile.Triangles.Num(); ++I)
		{
			const auto Face = Triangle(Profile.Triangles[I]);
			double Dummy;
			if (!Height(Face, FVector2D(Face.Points[0]), Dummy, Error)) return false;
			for (const auto& P : Face.Points)
			{
				if (P.X < -1e-8 || P.X > Profile.LengthMeters + 1e-8 || FMath::Abs(P.Y) > Profile.WidthMeters * 0.5 + 1e-8)
				{ Error = TEXT("Road elevation leaves its declared source domain."); return false; }
				Identity += FString::Printf(TEXT("%.17g,%.17g,%.17g;"), P.X, P.Y, P.Z);
			}
			Total += Area(Face);
			for (int32 J = 0; J < I; ++J)
			{
				const auto Other = Triangle(Profile.Triangles[J]);
				if (!Bounds(Face).Intersect(Bounds(Other))) continue;
				TArray<FVector2D> Intersection;
				if (!IntersectXY(Face, Other, Intersection, Error)) return false;
				if (ProjectWorldTerrainSurface::Area(Intersection) > 1e-9)
				{ Error = TEXT("Road elevation has overlapping parameter faces."); return false; }
				// Vertices on a shared edge must agree, including T-junctions.
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const auto& Source = Side ? Other : Face;
					const auto& Target = Side ? Face : Other;
					for (const auto& P : Source.Points)
					{
						bool Positive = false, Negative = false;
						for (int32 K = 0; K < 3; ++K)
						{
							const FVector2D A(Target.Points[K]), B(Target.Points[(K + 1) % 3]);
							const double D = FVector2D::CrossProduct(B - A, FVector2D(P) - A);
							Positive |= D > 1e-8; Negative |= D < -1e-8;
						}
						if (!(Positive && Negative))
						{
							double Z;
							if (!Height(Target, FVector2D(P), Z, Error)) return false;
							if (FMath::Abs(Z - P.Z) > 1e-7)
							{ Error = TEXT("Road elevation is discontinuous at a shared parameter edge."); return false; }
						}
					}
				}
			}
		}
		if (FMath::Abs(Total - Profile.LengthMeters * Profile.WidthMeters) > FMath::Max(1e-8, Total * 1e-9))
		{ Error = TEXT("Road elevation has missing parameter coverage."); return false; }
		ConsumedIdentity = MoveTemp(Identity);
		return true;
	}

	bool Station(const FProjectWorldCanonicalFeature& Feature, const FVector2D& XY, double Tolerance,
		double& Out, FString& Error)
	{
		bool Found = false;
		double Cumulative = 0;
		for (int32 I = 0; I + 1 < Feature.GeometryPoints.Num(); ++I)
		{
			const auto& A = Feature.GeometryPoints[I];
			const auto D = Feature.GeometryPoints[I + 1] - A;
			const double Length = D.Length();
			if (Length <= 1e-8) { Error = TEXT("Road station source is degenerate."); return false; }
			const double Alpha = FMath::Clamp(FVector2D::DotProduct(XY - A, D) / D.SquaredLength(), 0.0, 1.0);
			if ((XY - A - Alpha * D).Length() <= Tolerance)
			{
				const double Candidate = Cumulative + Alpha * Length;
				if (Found && FMath::Abs(Candidate - Out) > Tolerance)
				{ Error = TEXT("Road source station is ambiguous."); return false; }
				Out = Candidate;
				Found = true;
			}
			Cumulative += Length;
		}
		if (!Found) Error = TEXT("Road fragment cannot map to its qualified source line.");
		return Found;
	}

	bool Append(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature, const FVector2D& A, const FVector2D& B,
		const FVector2D& SideA, const FVector2D& SideB, double Tolerance, double Offset,
		TArray<ProjectWorldRoadSurface::FRibbonTriangle>& Out, FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		const auto Formed=ProjectWorldRoadFormation::Faces({A,B,SideA,SideB},Offset);
		const FProjectWorldSurfaceTriangle XY[2]={Formed[0],Formed[1]};
		if (Feature.RoadElevation.IsSet() && Feature.RoadElevation->Domain == EProjectWorldRoadElevationDomain::ReferencedConstructedSurface)
			return ProjectWorldRoadReferencedSurface::Append(Bundle,Feature,XY,Out,Error);
		if (!Feature.RoadElevation.IsSet())
		{
			for (const auto& Face : XY)
			{
				auto& Entry = Out.AddDefaulted_GetRef(); Entry.Surface = Face;
				Entry.Grade = Feature.CivilGrade.IsSet() ? &Feature.CivilGrade.GetValue() : nullptr;
			}
			return true;
		}
		double SA, SB;
		if (!Station(Feature, A, Tolerance, SA, Error) || !Station(Feature, B, Tolerance, SB, Error)) return false;
		const double Half = Feature.WidthMeters * 0.5;
		const FProjectWorldSurfaceTriangle UV[2] = {
			{{FVector(SA, Half, 0), FVector(SA, -Half, 0), FVector(SB, Half, 0)}},
			{{FVector(SA, -Half, 0), FVector(SB, -Half, 0), FVector(SB, Half, 0)}}};
		for (int32 I = 0; I < 2; ++I)
		{
			const int32 StorageGroup = Out.Num();
			double Covered = 0;
			for (const auto& Elevation : Feature.RoadElevation->Triangles)
			{
				const auto Field = Triangle(Elevation);
				if (!Bounds(UV[I]).Intersect(Bounds(Field))) continue;
				TArray<FVector2D> Polygon;
				if (!IntersectXY(UV[I], Field, Polygon, Error)) return false;
				const double IntersectionArea = ProjectWorldTerrainSurface::Area(Polygon);
				if (IntersectionArea <= 1e-12) continue;
				Covered += IntersectionArea;
				for (int32 J = 1; J + 1 < Polygon.Num(); ++J)
				{
					ProjectWorldRoadSurface::FRibbonTriangle Face;
					Face.bCanonicalHeight = true;
					Face.StorageGroup = StorageGroup;
					Face.StorageDomain = XY[I];
					Face.Elevation = &Feature.RoadElevation.GetValue();
					const bool bTerrainAbutment = Feature.RoadElevation->EndpointRelation == EProjectWorldRoadEndpointRelation::TerrainAbutment;
					if (bTerrainAbutment && (FMath::Abs(SA) < 1e-8 || FMath::Abs(SA - Feature.RoadElevation->LengthMeters) < 1e-8))
						Face.Abutments.Add({A + SideA, A - SideA});
					if (bTerrainAbutment && (FMath::Abs(SB) < 1e-8 || FMath::Abs(SB - Feature.RoadElevation->LengthMeters) < 1e-8))
						Face.Abutments.Add({B + SideB, B - SideB});
					const FVector2D Points[3] = {Polygon[0], Polygon[J], Polygon[J + 1]};
					for (int32 K = 0; K < 3; ++K)
					{
						double Z;
						if (!Height(Field, Points[K], Z, Error) || !Map(UV[I], XY[I], Points[K], Z, Face.Surface.Points[K], Error)) return false;
					}
					// Mapping can make a redundant clipping vertex exactly identical at projected double precision.
					if (Face.Surface.Points[0] == Face.Surface.Points[1] || Face.Surface.Points[1] == Face.Surface.Points[2] ||
						Face.Surface.Points[2] == Face.Surface.Points[0]) continue;
					Out.Add(Face);
				}
			}
			if (FMath::Abs(Covered - Area(UV[I])) > FMath::Max(1e-8, Area(UV[I]) * 1e-8))
			{ Error = TEXT("Road elevation does not cover the unchanged native ribbon."); return false; }
			if (Out.Num() == StorageGroup)
			{ Error = TEXT("Road elevation mapping collapses the complete native ribbon domain."); return false; }
		}
		return true;
	}
}
