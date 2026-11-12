// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "CompGeom/ExactPredicates.h"
#include <limits>

namespace ProjectWorldConstructedSurface
{
	namespace
	{
		constexpr double ContinuityTolerance=1e-8;
		bool Hash(const FString& Value)
		{
			if (Value.Len() != 64) return false;
			for (const TCHAR C : Value) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
			return true;
		}
		bool Inside(const FProjectWorldSurfaceTriangle& T, const FVector& P)
		{
			const FVector A = T.Points[1]-T.Points[0], B = T.Points[2]-T.Points[0], D = P-T.Points[0];
			const double Determinant = A.X*B.Y-A.Y*B.X;
			const double U = (D.X*B.Y-D.Y*B.X)/Determinant, V = (A.X*D.Y-A.Y*D.X)/Determinant;
			return U >= -1e-9 && V >= -1e-9 && 1-U-V >= -1e-9;
		}
	}
	bool ValidateField(const TArray<FProjectWorldSurfaceTriangle>& Triangles, FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		if (Triangles.IsEmpty() || Triangles.Num() > 4096)
		{ Error = TEXT("Constructed surface needs a bounded finite triangle field."); return false; }
		TArray<FBox2D> Bounds;
		for (const auto& Face : Triangles)
		{
			double Z;
			if (!Height(Face,FVector2D(Face.Points[0]),Z,Error)) return false;
			FBox2D Box(ForceInit); for (const auto& P : Face.Points) Box += FVector2D(P);
			Bounds.Add(Box);
		}
		for (int32 I=0; I<Triangles.Num(); ++I) for (int32 J=I+1; J<Triangles.Num(); ++J)
		{
			if (!Bounds[I].Intersect(Bounds[J])) continue;
			TArray<FVector2D> Overlap;
			if (!IntersectXY(Triangles[I],Triangles[J],Overlap,Error)) return false;
			if (Area(Overlap) > 1e-9)
			{ Error = TEXT("Constructed height fields overlap in XY."); return false; }
			for (int32 Side=0; Side<2; ++Side)
			{
				const auto& A = Triangles[Side ? J : I]; const auto& B = Triangles[Side ? I : J];
				for (const auto& P : A.Points) if (Inside(B,P))
				{
					double Z;
					if (!Height(B,FVector2D(P),Z,Error)) return false;
					if (FMath::Abs(Z-P.Z) > ContinuityTolerance)
					{ Error = TEXT("Constructed height field has a discontinuous edge or T junction."); return false; }
				}
			}
		}
		return true;
	}
	ESampleResult TrySample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,const FVector2D& Point,double& Out,FString& Error)
	{
		Out=std::numeric_limits<double>::quiet_NaN(); Error.Reset();
		if (Triangles.IsEmpty() || Triangles.Num()>4096 || Point.ContainsNaN())
		{ Error=TEXT("Constructed sampling requires a bounded validated field and finite point."); return ESampleResult::Invalid; }
		bool Found=false; double Candidate=0;
		for (const auto& T : Triangles)
		{
			for (const auto& P : T.Points) if (P.ContainsNaN())
			{ Error=TEXT("Constructed sampling received a nonfinite field."); return ESampleResult::Invalid; }
			using UE::Geometry::ExactPredicates::Orient2;
			const double Direction=Orient2(FVector2D(T.Points[0]),FVector2D(T.Points[1]),FVector2D(T.Points[2]));
			if (Direction==0 || !FMath::IsFinite(Direction))
			{ Error=TEXT("Constructed sampling received a degenerate field face."); return ESampleResult::Invalid; }
			bool Contains=true;
			for (int32 I=0; I<3; ++I)
				Contains &= Orient2(FVector2D(T.Points[I]),FVector2D(T.Points[(I+1)%3]),Point)*Direction>=0;
			if (!Contains) continue;
			double Height;
			if (!ProjectWorldTerrainSurface::Height(T,Point,Height,Error)) return ESampleResult::Invalid;
			if (Found && FMath::Abs(Candidate-Height)>ContinuityTolerance)
			{ Error=TEXT("Constructed sampling has contradictory containing faces."); return ESampleResult::Invalid; }
			if (!Found) Candidate=Height;
			Found=true;
		}
		if (!Found) { Error=TEXT("Constructed sample is outside the complete source field."); return ESampleResult::Outside; }
		Out=Candidate; return ESampleResult::Sampled;
	}
	bool Sample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,const FVector2D& Point,double& Out,FString& Error)
	{
		return TrySample(Triangles,Point,Out,Error)==ESampleResult::Sampled;
	}
	bool Read(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature,
		TArray<FProjectWorldSurfaceTriangle>& Triangles, FString& Identity, FString& Error)
	{
		Triangles.Reset(); Identity.Reset();
		if (Feature.FeatureClass != TEXT("constructed_surface") || !Feature.ConstructedSurface.IsSet())
		{ Error = TEXT("Referenced feature has no typed constructed surface."); return false; }
		const auto& Surface = Feature.ConstructedSurface.GetValue();
		if (Surface.Triangles.IsEmpty() || Surface.Triangles.Num() > 4096 ||
			Surface.SourceFeatureId != Feature.FeatureId || !Hash(Surface.ModelHash) ||
			Bundle.CanonicalCrs.IsEmpty() || Surface.CanonicalCrs != Bundle.CanonicalCrs ||
			Surface.VerticalDatum != TEXT("EPSG:3855") || Surface.VerticalDatum != Bundle.VerticalDatum ||
			(Surface.Basis != TEXT("observed") && Surface.Basis != TEXT("identified_model")) ||
			(Surface.Role != TEXT("fixed_pier") && Surface.Role != TEXT("floating_assembly") &&
				!(Surface.Basis == TEXT("identified_model") && (Surface.Role == TEXT("identified_assembly") ||
				Surface.Role == TEXT("inferred_apron") || Surface.Role == TEXT("inferred_gate_apron")))))
		{ Error = TEXT("Constructed source identity, frame, basis or structural role is unknown."); return false; }
		TArray<FProjectWorldSurfaceTriangle> Candidate;
		FString Consumed = Surface.SourceFeatureId+TEXT("|")+Surface.ModelHash+TEXT("|")+Surface.CanonicalCrs+
			TEXT("|")+Surface.VerticalDatum+TEXT("|")+Surface.Basis+TEXT("|")+Surface.Role+TEXT("|");
		for (const auto& Face : Surface.Triangles)
		{
			Candidate.Add(FProjectWorldSurfaceTriangle{{Face.XYZ[0],Face.XYZ[1],Face.XYZ[2]}});
			for (const auto& P : Face.XYZ) Consumed += FString::Printf(TEXT("%.17g,%.17g,%.17g;"),P.X,P.Y,P.Z);
		}
		if (!ValidateField(Candidate,Error)) return false;
		Triangles = MoveTemp(Candidate); Identity = MoveTemp(Consumed); return true;
	}
}
