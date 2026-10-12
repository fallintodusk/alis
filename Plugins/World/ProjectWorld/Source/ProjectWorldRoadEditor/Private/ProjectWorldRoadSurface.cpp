// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace ProjectWorldRoadSurface
{
	bool Index(const FProjectWorldCanonicalBundle& Bundle, FContext& Context, FString& Error)
	{
		Context = FContext();
		for (const auto& Cell : Bundle.Cells)
		{
			const FIntPoint Key(Cell.CellX, Cell.CellY);
			if (Context.Cells.Contains(Key) || Cell.Bounds.ContainsNaN() || Cell.Bounds.Z <= Cell.Bounds.X || Cell.Bounds.W <= Cell.Bounds.Y)
			{ Error = TEXT("Road terrain context has invalid or duplicate cells."); return false; }
			Context.Cells.Add(Key, &Cell);
			Context.Domain += FVector2D(Cell.Bounds.X, Cell.Bounds.Y);
			Context.Domain += FVector2D(Cell.Bounds.Z, Cell.Bounds.W);
		}
		return true;
	}

	bool Conform(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const FContext& Context, const TArray<FProjectWorldSurfaceTriangle>& Ribbon,
		const FVector& Origin, double Lift, int32 Halo, FMeshDescription* Description,
		TSet<FString>& ConsumedTerrain, FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		FMeshDescription Candidate;
		FStaticMeshAttributes Attributes(Candidate);
		FPolygonGroupID Group;
		if (Description)
		{
			Attributes.Register();
			Attributes.GetVertexInstanceUVs().SetNumChannels(1);
			Group = Candidate.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Road");
		}
		TArray<const FProjectWorldCanonicalCell*> Neighbors;
		for (int32 Y = Cell.CellY - Halo; Y <= Cell.CellY + Halo; ++Y)
		for (int32 X = Cell.CellX - Halo; X <= Cell.CellX + Halo; ++X)
			if (const auto* Found = Context.Cells.Find(FIntPoint(X, Y))) Neighbors.Add(*Found);
		TArray<FProjectWorldSurfaceTriangle> Clipped;
		for (const auto& Source : Ribbon)
		{
			bool bInside = true;
			for (const auto& P : Source.Points)
			{
				if (P.ContainsNaN()) { Error = TEXT("Road footprint has non-finite coordinates."); return false; }
				bInside &= P.X >= Context.Domain.Min.X && P.X <= Context.Domain.Max.X && P.Y >= Context.Domain.Min.Y && P.Y <= Context.Domain.Max.Y;
			}
			if (bInside) { Clipped.Add(Source); continue; }
			const FVector A(Context.Domain.Min, 0), B(Context.Domain.Max.X, Context.Domain.Min.Y, 0);
			const FVector C(Context.Domain.Min.X, Context.Domain.Max.Y, 0), D(Context.Domain.Max, 0);
			for (const auto& Domain : {FProjectWorldSurfaceTriangle{{A, B, C}}, FProjectWorldSurfaceTriangle{{B, D, C}}})
			{
				TArray<FVector2D> Polygon;
				if (!IntersectXY(Source, Domain, Polygon, Error)) return false;
				for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
					Clipped.Add(FProjectWorldSurfaceTriangle{{FVector(Polygon[0], 0), FVector(Polygon[I], 0), FVector(Polygon[I + 1], 0)}});
			}
		}
		for (const auto& Source : Clipped)
		{
			FBox2D Bounds(ForceInit);
			TArray<FVector2D> Footprint;
			for (const auto& P : Source.Points) { Footprint.Add(FVector2D(P)); Bounds += Footprint.Last(); }
			const double ExpectedArea = Area(Footprint);
			if (ExpectedArea <= 1e-12) { Error = TEXT("Road ribbon has degenerate XY coverage."); return false; }
			double CoveredArea = 0;
			for (const auto* NeighborPointer : Neighbors)
			{
				const auto& Neighbor = *NeighborPointer;
				const FBox2D CellBounds(FVector2D(Neighbor.Bounds.X, Neighbor.Bounds.Y), FVector2D(Neighbor.Bounds.Z, Neighbor.Bounds.W));
				if (!CellBounds.Intersect(Bounds)) continue;
				TArray<FProjectWorldSurfaceTriangle> Terrain;
				if (Description)
				{
					if (!Triangles(Neighbor, Bounds, Terrain, Error)) return false;
				}
				else
				{
					const auto& T = Neighbor.Terrain;
					if (T.SamplesX < 2 || T.SamplesY < 2 || T.HeightsMeters.Num() != T.SamplesX * T.SamplesY ||
						T.SampleSpacing.ContainsNaN() || T.SampleSpacing.X <= 0 || T.SampleSpacing.Y <= 0)
					{ Error = TEXT("Road input terrain grid is invalid."); return false; }
					const FVector A(CellBounds.Min, 0), B(CellBounds.Max.X, CellBounds.Min.Y, 0);
					const FVector C(CellBounds.Min.X, CellBounds.Max.Y, 0), D(CellBounds.Max, 0);
					Terrain = {FProjectWorldSurfaceTriangle{{A, B, C}}, FProjectWorldSurfaceTriangle{{B, D, C}}};
				}
				bool bConsumed = false;
				for (const auto& Triangle : Terrain)
				{
					TArray<FVector2D> Polygon;
					if (!IntersectXY(Source, Triangle, Polygon, Error)) return false;
					const double IntersectionArea = Area(Polygon);
					if (IntersectionArea <= 1e-12) continue;
					if (Polygon.Num() > 6) { Error = TEXT("Triangle intersection exceeds its topology bound."); return false; }
					CoveredArea += IntersectionArea;
					bConsumed = true;
					if (!Description) continue;
					for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
					{
						FVector Positions[3];
						const FVector2D Points[3] = {Polygon[0], Polygon[I], Polygon[I + 1]};
						for (int32 J = 0; J < 3; ++J)
						{
							double Z;
							if (!Height(Triangle, Points[J], Z, Error)) return false;
							Positions[J] = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(Points[J], Z + Lift)) - Origin;
						}
						FVector Cross = FVector::CrossProduct(Positions[1] - Positions[0], Positions[2] - Positions[0]);
						if (Cross.Z > 0) { Swap(Positions[1], Positions[2]); Cross = -Cross; }
						const FVector Normal = -Cross.GetSafeNormal();
						const FVector Tangent = (FVector::XAxisVector - Normal * Normal.X).GetSafeNormal();
						TArray<FVertexInstanceID> Instances;
						for (const FVector& Position : Positions)
						{
							const auto Vertex = Candidate.CreateVertex();
							Attributes.GetVertexPositions()[Vertex] = FVector3f(Position);
							const auto Instance = Candidate.CreateVertexInstance(Vertex);
							Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
							Attributes.GetVertexInstanceTangents()[Instance] = FVector3f(Tangent);
							Attributes.GetVertexInstanceBinormalSigns()[Instance] = 1;
							Attributes.GetVertexInstanceColors()[Instance] = FVector4f(0.08f, 0.08f, 0.08f, 1);
							Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(Position.X, Position.Y) * 0.001f);
							Instances.Add(Instance);
						}
						Candidate.CreatePolygon(Group, Instances);
					}
				}
				if (bConsumed) ConsumedTerrain.Add(Neighbor.CellId);
			}
			if (FMath::Abs(CoveredArea - ExpectedArea) > FMath::Max(1e-8, ExpectedArea * 1e-7))
			{ Error = FString::Printf(TEXT("Road terrain coverage is incomplete or overlapping: %.17g / %.17g m2."), CoveredArea, ExpectedArea); return false; }
		}
		if (Description) *Description = MoveTemp(Candidate);
		return true;
	}
}
