// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadReferencedSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedSurface.h"
#include <limits>

namespace ProjectWorldRoadReferencedSurface
{
	bool Read(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalRoadElevation& Profile,
		TArray<FProjectWorldSurfaceTriangle>& Field, FString& Identity, FString& Error)
	{
		Field.Reset(); Identity.Reset();
		if (Profile.Domain != EProjectWorldRoadElevationDomain::ReferencedConstructedSurface ||
			Profile.EndpointRelation != EProjectWorldRoadEndpointRelation::ConstructedSurface ||
			!Profile.Triangles.IsEmpty() || Profile.ReferencedSurfaceModels.IsEmpty() || Profile.ReferencedSurfaceModels.Num() > 8)
		{ Error = TEXT("Referenced Road requires one closed structural-height representation."); return false; }
		TArray<FString> Keys; Profile.ReferencedSurfaceModels.GetKeys(Keys); Keys.Sort();
		TArray<FProjectWorldSurfaceTriangle> Candidate; FString Consumed(TEXT("referenced_constructed_xyz_v1|"));
		for (const auto& Key : Keys)
		{
			const auto* Source = Bundle.Features.Find(Key);
			if (!Source || !Source->ConstructedSurface.IsSet() || Source->ConstructedSurface->ModelHash != Profile.ReferencedSurfaceModels[Key])
			{ Error = TEXT("Referenced constructed source is missing or its exact model pin is stale."); return false; }
			TArray<FProjectWorldSurfaceTriangle> Faces; FString SourceIdentity;
			if (!ProjectWorldConstructedSurface::Read(Bundle,*Source,Faces,SourceIdentity,Error)) return false;
			if (Candidate.Num()+Faces.Num() > 4096)
			{ Error = TEXT("Referenced Road exceeds its complete structural field budget."); return false; }
			Candidate.Append(Faces); Consumed += SourceIdentity;
		}
		if (!ProjectWorldConstructedSurface::ValidateField(Candidate,Error)) return false;
		Field = MoveTemp(Candidate); Identity = MoveTemp(Consumed); return true;
	}
	bool Append(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Road,
		const FProjectWorldSurfaceTriangle (&Domain)[2], TArray<ProjectWorldRoadSurface::FRibbonTriangle>& Out, FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		TArray<FProjectWorldSurfaceTriangle> Field; FString Identity;
		if (!Road.RoadElevation.IsSet()) { Error=TEXT("Referenced Road has no elevation contract."); return false; }
		if (!Read(Bundle,Road.RoadElevation.GetValue(),Field,Identity,Error)) return false;
		TArray<ProjectWorldRoadSurface::FRibbonTriangle> Candidate;
		for (int32 Side=0; Side<2; ++Side)
		{
			double Covered = 0; FBox2D Bounds(ForceInit);
			for (const auto& P : Domain[Side].Points) Bounds += FVector2D(P);
			const int32 Group = Out.Num()+Candidate.Num();
			for (const auto& Surface : Field)
			{
				FBox2D SourceBounds(ForceInit); for (const auto& P : Surface.Points) SourceBounds += FVector2D(P);
				if (!Bounds.Intersect(SourceBounds)) continue;
				TArray<FVector2D> Polygon;
				if (!IntersectXY(Domain[Side],Surface,Polygon,Error)) return false;
				const double Common = Area(Polygon); if (Common <= 1e-12) continue;
				Covered += Common;
				double Tessellated=0;
				for (int32 I=1; I+1<Polygon.Num(); ++I)
				{
					ProjectWorldRoadSurface::FRibbonTriangle Entry; Entry.bCanonicalHeight = true;
					Entry.Elevation = &Road.RoadElevation.GetValue(); Entry.StorageGroup = Group; Entry.StorageDomain = Domain[Side];
					const FVector2D Points[3] = {Polygon[0],Polygon[I],Polygon[I+1]};
					const double FaceArea=Area({Points[0],Points[1],Points[2]});
					if (FaceArea==0) continue;
					Tessellated+=FaceArea;
					for (int32 J=0; J<3; ++J)
					{
						double Z; if (!ProjectWorldConstructedSurface::Sample(Field,Points[J],Z,Error)) return false;
						Entry.Surface.Points[J] = FVector(Points[J],Z);
					}
					Candidate.Add(Entry);
				}
				const double Arithmetic=64*std::numeric_limits<double>::epsilon()*FMath::Max(1.0,Common)*Polygon.Num();
				if (!FMath::IsFinite(Tessellated) || FMath::Abs(Tessellated-Common)>Arithmetic)
				{ Error=TEXT("Referenced surface fan does not retain its complete native intersection."); return false; }
			}
			const double Expected = Area({FVector2D(Domain[Side].Points[0]),FVector2D(Domain[Side].Points[1]),FVector2D(Domain[Side].Points[2])});
			if (Expected <= 1e-12 || FMath::Abs(Covered-Expected) > FMath::Max(1e-8,Expected*1e-8))
			{ Error = TEXT("Referenced surfaces do not cover the complete unchanged native Road width."); return false; }
		}
		Out.Append(Candidate); return true;
	}
}
