// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMeshCoverage.h"
#include "ProjectWorldConstructedOutline.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"
#include "CompGeom/ExactPredicates.h"
#include <limits>

namespace ProjectWorldConstructedMeshCoverage
{
	using namespace UE::Geometry;
	ESourceClip ClassifySourceClip(const FPolygon2d& Ring,FString& Error)
	{
		Error.Reset();
		for (const auto& P : Ring.GetVertices()) if (P.ContainsNaN())
		{ Error=TEXT("Constructed source clip has nonfinite vertices."); return ESourceClip::Invalid; }
		int32 Direction=0;
		for (int32 I=1; I+1<Ring.VertexCount(); ++I)
		{
			const double Side=ExactPredicates::Orient2(Ring[0],Ring[I],Ring[I+1]);
			if (!FMath::IsFinite(Side)) { Error=TEXT("Constructed source clip area is unknown."); return ESourceClip::Invalid; }
			if (Side==0) continue;
			const int32 Sign=Side>0 ? 1 : -1;
			if (Direction!=0 && Direction!=Sign)
			{ Error=TEXT("Constructed source convex clipping has inconsistent orientation."); return ESourceClip::Invalid; }
			Direction=Sign;
		}
		if (Direction==0) return ESourceClip::Empty;
		const double Area=Ring.SignedArea();
		if (!FMath::IsFinite(Area) || Area*Direction<=0)
		{ Error=TEXT("Constructed source clip has exact positive area but unknown numerical area."); return ESourceClip::Invalid; }
		return ESourceClip::Area;
	}
	namespace
	{
		double Area(const TArray<FGeneralPolygon2d>& Polygons)
		{ double Result=0; for (const auto& P : Polygons) Result+=FMath::Abs(P.SignedArea()); return Result; }
		bool RetainedRing(const FPolygon2d& Ring)
		{
			TArray<FVector2D> Points;
			for (const auto& P : Ring.GetVertices())
			{
				const FVector2D Stored{FVector2f(P)};
				if (Stored.ContainsNaN()) return false;
				if (Points.IsEmpty() || Points.Last()!=Stored) Points.Add(Stored);
			}
			if (Points.Num()>1 && Points.Last()==Points[0]) Points.Pop();
			return Points.Num()>=3 && Ring.SignedArea()*FPolygon2d(Points).SignedArea()>0;
		}
		bool MatchCores(const TArray<FGeneralPolygon2d>& Original,const TArray<FGeneralPolygon2d>& Actual,
			double Radius,double Arithmetic,FString& Error)
		{
			TArray<int32> Matches; Matches.Init(0,Actual.Num());
			for (const auto& Component : Original)
			{
				const TArray<FGeneralPolygon2d> Source={Component};
				TArray<FGeneralPolygon2d> Core;
				if (Radius==0) Core={Component};
				else if (!PolygonsOffset(-Radius,Source,Core,false,2,EPolygonOffsetJoinType::Miter))
				{ Error=TEXT("Native constructed component or hole inset failed."); return false; }
				if (Core.Num()!=1 || !FMath::IsFinite(Area(Core)) || Area(Core)<=Arithmetic)
				{ Error=TEXT("Constructed component or hole has no independently certifiable interior."); return false; }
				int32 Owner=INDEX_NONE;
				for (int32 I=0; I<Actual.Num(); ++I)
				{
					TArray<FGeneralPolygon2d> Common;
					const TArray<FGeneralPolygon2d> Candidate={Actual[I]};
					if (!PolygonsIntersection(Core,Candidate,Common) || !FMath::IsFinite(Area(Common)))
					{ Error=TEXT("Native constructed component correspondence is unknown."); return false; }
					if (Area(Common)<=Arithmetic) continue;
					if (Owner!=INDEX_NONE)
					{
						Error=FString::Printf(TEXT("A constructed component or hole was split: source_count=%d actual_count=%d source_area=%.17g first_area=%.17g second_area=%.17g common_area=%.17g."),
							Original.Num(),Actual.Num(),FMath::Abs(Component.SignedArea()),FMath::Abs(Actual[Owner].SignedArea()),FMath::Abs(Actual[I].SignedArea()),Area(Common));
						return false;
					}
					Owner=I;
				}
				if (Owner==INDEX_NONE || ++Matches[Owner]!=1)
				{ Error=TEXT("Constructed component or hole correspondence is missing or merged."); return false; }
				TArray<FGeneralPolygon2d> Uncovered;
				const TArray<FGeneralPolygon2d> Matched={Actual[Owner]};
				if (!PolygonsDifference(Core,Matched,Uncovered) || !FMath::IsFinite(Area(Uncovered)) || Area(Uncovered)>Arithmetic)
				{ Error=TEXT("A constructed component or hole loses its independently protected interior."); return false; }
			}
			for (const int32 Count : Matches) if (Count!=1)
			{ Error=TEXT("Stored constructed geometry invents a component or hole."); return false; }
			return true;
		}
	}
	bool Validate(const TArray<FGeneralPolygon2d>& Original,const TArray<FProjectWorldSurfaceTriangle>& Stored,
		double RadiusCentimeters,FMetrics& Metrics,FString& Error)
	{
		Metrics=FMetrics();
		if (!FMath::IsFinite(RadiusCentimeters) || RadiusCentimeters<0)
		{ Error=TEXT("Constructed coverage has no finite measured conversion radius."); return false; }
		if (Original.IsEmpty() && Stored.IsEmpty()) return true;
		if (Original.IsEmpty() || Stored.IsEmpty())
		{ Error=TEXT("Constructed coverage loses or invents the complete body domain."); return false; }
		double Perimeter=0,Extent=0; int32 BoundaryVertices=0;
		for (const auto& P : Original)
		{
			auto Ring = [&](const FPolygon2d& R)
			{
				if (!RetainedRing(R)) return false;
				Perimeter+=R.Perimeter(); BoundaryVertices+=R.VertexCount();
				for (const auto& V : R.GetVertices()) Extent=FMath::Max(Extent,V.Length());
				return true;
			};
			if (!Ring(P.GetOuter())) { Error=TEXT("A complete constructed component cannot survive native storage."); return false; }
			for (const auto& Hole : P.GetHoles()) if (!Ring(Hole))
			{ Error=TEXT("A declared constructed hole cannot survive native storage."); return false; }
		}
		TArray<FGeneralPolygon2d> Actual;
		double Total=0;
		for (const auto& T : Stored)
		{
			FPolygon2d P({FVector2D(T.Points[0]),FVector2D(T.Points[1]),FVector2D(T.Points[2])});
			if (P.IsClockwise()) P.Reverse();
			Total+=P.SignedArea(); Actual.Add(FGeneralPolygon2d(MoveTemp(P)));
		}
		TArray<FGeneralPolygon2d> United,Missing,Exterior,Inner,Uncovered;
		if (!ProjectWorldConstructedOutline::Extract(Actual,United,Error)) return false;
		if (United.Num()!=Original.Num() ||
			!PolygonsDifference(Original,United,Missing) || !PolygonsDifference(United,Original,Exterior))
		{ Error=TEXT("Constructed whole-domain native coverage/components cannot be certified."); return false; }
		Metrics.OriginalArea=Area(Original); Metrics.StoredArea=Area(United);
		Metrics.MissingArea=Area(Missing); Metrics.ExteriorArea=Area(Exterior);
		Metrics.ConversionBound=2*Perimeter*RadiusCentimeters+2*UE_DOUBLE_PI*RadiusCentimeters*RadiusCentimeters;
		Metrics.ArithmeticBound=1024*std::numeric_limits<double>::epsilon()*FMath::Max(1.0,Extent*Extent)*(BoundaryVertices+Stored.Num());
		if (!FMath::IsFinite(Metrics.OriginalArea) || !FMath::IsFinite(Metrics.StoredArea) || !FMath::IsFinite(Total) ||
			!FMath::IsFinite(Metrics.MissingArea) || !FMath::IsFinite(Metrics.ExteriorArea) ||
			!FMath::IsFinite(Metrics.ConversionBound) || !FMath::IsFinite(Metrics.ArithmeticBound) ||
			Metrics.MissingArea>Metrics.ConversionBound+Metrics.ArithmeticBound ||
			Metrics.ExteriorArea>Metrics.ConversionBound+Metrics.ArithmeticBound ||
			FMath::Abs(Total-Metrics.StoredArea)>Metrics.ArithmeticBound)
		{ Error=TEXT("Constructed stored coverage exceeds measured conversion or duplicates faces."); return false; }
		if (RadiusCentimeters==0) Inner=Original;
		else if (!PolygonsOffset(-RadiusCentimeters,Original,Inner,false,2,EPolygonOffsetJoinType::Miter))
		{ Error=TEXT("Native constructed coverage inset failed."); return false; }
		if (Inner.Num()!=Original.Num() || !PolygonsDifference(Inner,United,Uncovered) ||
			!FMath::IsFinite(Area(Uncovered)) || Area(Uncovered)>Metrics.ArithmeticBound)
		{ Error=TEXT("Constructed stored coverage leaves an unproved original-domain interior."); return false; }
		if (!MatchCores(Original,United,RadiusCentimeters,Metrics.ArithmeticBound,Error)) return false;
		auto Holes = [](const TArray<FGeneralPolygon2d>& Polygons)
		{
			TArray<FGeneralPolygon2d> Result;
			for (const auto& P : Polygons) for (auto H : P.GetHoles())
			{ if (H.IsClockwise()) H.Reverse(); Result.Add(FGeneralPolygon2d(MoveTemp(H))); }
			return Result;
		};
		if (!MatchCores(Holes(Original),Holes(United),RadiusCentimeters,Metrics.ArithmeticBound,Error)) return false;
		return true;
	}
}
