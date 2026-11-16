// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include <limits>

namespace ProjectWorldConstructedSurface
{
	namespace
	{
		bool Hash(const FString& Value)
		{
			if (Value.Len() != 64) return false;
			for (const TCHAR C : Value) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
			return true;
		}

	}
	bool ValidateField(const TArray<FProjectWorldSurfaceTriangle>& Triangles,FString& Error)
    {
        if (Triangles.IsEmpty() || Triangles.Num()>4096)
        {Error=TEXT("Constructed surface needs a bounded finite triangle field.");return false;}
        return ProjectWorldTerrainSurface::ValidateField(Triangles,4096,Error);
    }
    ESampleResult TrySample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,const FVector2D& Point,double& Out,FString& Error)
    {
        if (Triangles.IsEmpty() || Triangles.Num()>4096 || Point.ContainsNaN())
        {Out=std::numeric_limits<double>::quiet_NaN();Error=TEXT("Constructed sampling requires a bounded validated field and finite point.");return ESampleResult::Invalid;}
        return ProjectWorldTerrainSurface::TrySampleField(Triangles,Point,4096,Out,Error);
    }
    bool Sample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,const FVector2D& Point,double& Out,FString& Error)
    {return TrySample(Triangles,Point,Out,Error)==ESampleResult::Sampled;}
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
