// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadSurface.h"
#include "ProjectWorldRoadStoredPolygon.h"
#include "ProjectWorldRoadReferencedSurface.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace ProjectWorldRoadSurface
{
	namespace
	{
		bool CheckStored(const FProjectWorldCanonicalBundle& Bundle, const FRibbonTriangle& Entry,
			const TArray<const FProjectWorldCanonicalCell*>& Neighbors, const FVector Positions[3],
			const FVector& Origin, TSet<FString>& ConsumedTerrain, FString& Error)
		{
			using namespace ProjectWorldTerrainSurface;
			FProjectWorldSurfaceTriangle Road;
			FBox2D Bounds(ForceInit); TArray<FVector2D> Footprint;
			for (int32 I = 0; I < 3; ++I)
			{
				Road.Points[I] = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Positions[I] + Origin);
				Footprint.Add(FVector2D(Road.Points[I])); Bounds += Footprint.Last();
			}
			double Covered = 0;
			for (const auto* Neighbor : Neighbors)
			{
				if (!FBox2D(FVector2D(Neighbor->Bounds.X, Neighbor->Bounds.Y), FVector2D(Neighbor->Bounds.Z, Neighbor->Bounds.W)).Intersect(Bounds)) continue;
				TArray<FProjectWorldSurfaceTriangle> Terrain;
				if (!Triangles(*Neighbor, Bounds, Terrain, Error)) return false;
				for (const auto& Ground : Terrain)
				{
					TArray<FVector2D> Polygon;
					if (!IntersectXY(Road, Ground, Polygon, Error)) return false;
					const double IntersectionArea = Area(Polygon);
					if (IntersectionArea <= 1e-12) continue;
					const auto* Support = Entry.Elevation ? &Entry.Elevation->TerrainArtifacts : Entry.Grade ? &Entry.Grade->TerrainArtifacts : nullptr;
					if (!Support || Support->FindRef(Neighbor->CellId) != ProjectWorldCivilGrade::BaseHash(Neighbor->Terrain))
					{ Error = TEXT("Stored Road consumes Terrain outside its authenticated profile support."); return false; }
					Covered += IntersectionArea; ConsumedTerrain.Add(Neighbor->CellId);
					for (const auto& P : Polygon)
					{
						double RoadZ, GroundZ;
						if (!Height(Road, P, RoadZ, Error) || !Height(Ground, P, GroundZ, Error)) return false;
						if (RoadZ < GroundZ - 1e-8)
						{ Error = TEXT("Stored Road geometry intersects its final canonical Terrain."); return false; }
					}
				}
			}
			const double Expected = Area(Footprint);
			if (Expected <= 1e-12 || FMath::Abs(Expected - Covered) > FMath::Max(1e-8, Expected * 1e-7))
			{ Error = TEXT("Stored Road geometry has missing or overlapping Terrain coverage."); return false; }
			return true;
		}
	}
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
		const FContext& Context, const TArray<FRibbonTriangle>& Ribbon,
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
		TArray<FRibbonTriangle> Clipped;
		TMap<const FProjectWorldCanonicalRoadElevation*, ProjectWorldRoadStoredPolygon::FVertexMap> StoredVertices;
		struct FReferencedStorage
		{
			TArray<FProjectWorldSurfaceTriangle> Field;
			TMap<FVector2f,float> Heights;
		};
		TMap<const FProjectWorldCanonicalRoadElevation*,FReferencedStorage> ReferencedStorage;
		struct FStorageGroup
		{
			const FRibbonTriangle* Example = nullptr;
			TArray<FProjectWorldSurfaceTriangle> Pieces;
		};
		TMap<int32, FStorageGroup> StorageGroups;
		int32 UngroupedId = -1;
		auto Emit = [&](const FRibbonTriangle& Entry, FVector Positions[3])
		{
			if (Entry.Grade && !Entry.bCanonicalHeight)
				for (int32 I = 0; I < 3; ++I) Positions[I] = FVector(FVector3f(Positions[I]));
			FVector Cross = FVector::CrossProduct(Positions[1] - Positions[0], Positions[2] - Positions[0]);
			if ((Entry.bCanonicalHeight || Entry.Grade) && !CheckStored(Bundle, Entry, Neighbors, Positions, Origin, ConsumedTerrain, Error)) return false;
			if (Cross.Z > 0) { Swap(Positions[1], Positions[2]); Cross = -Cross; }
			const FVector Normal = Entry.bCanonicalHeight ? -Cross.GetSafeNormal(0.0) : -Cross.GetSafeNormal();
			const FVector Tangent = (FVector::XAxisVector - Normal * Normal.X).GetSafeNormal();
			if (Entry.bCanonicalHeight && (Normal.ContainsNaN() || Normal.IsNearlyZero() || Tangent.ContainsNaN() || Tangent.IsNearlyZero()))
			{ Error = TEXT("Road elevation has no finite native shading frame."); return false; }
			if (!Description) return true;
			TArray<FVertexInstanceID> Instances;
			for (const FVector& Position : MakeArrayView(Positions, 3))
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
			return true;
		};
		for (const auto& Entry : Ribbon)
		{
			const auto& Source = Entry.Surface;
			bool bInside = true;
			for (const auto& P : Source.Points)
			{
				if (P.ContainsNaN()) { Error = TEXT("Road footprint has non-finite coordinates."); return false; }
				bInside &= P.X >= Context.Domain.Min.X && P.X <= Context.Domain.Max.X && P.Y >= Context.Domain.Min.Y && P.Y <= Context.Domain.Max.Y;
			}
			if (bInside) { Clipped.Add(Entry); continue; }
			const FVector A(Context.Domain.Min, 0), B(Context.Domain.Max.X, Context.Domain.Min.Y, 0);
			const FVector C(Context.Domain.Min.X, Context.Domain.Max.Y, 0), D(Context.Domain.Max, 0);
			for (const auto& Domain : {FProjectWorldSurfaceTriangle{{A, B, C}}, FProjectWorldSurfaceTriangle{{B, D, C}}})
			{
				TArray<FVector2D> Polygon;
				if (!IntersectXY(Source, Domain, Polygon, Error)) return false;
				for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
				{
					FRibbonTriangle Face;
					Face.bCanonicalHeight = Entry.bCanonicalHeight;
					Face.Abutments = Entry.Abutments;
					Face.Elevation = Entry.Elevation;
						Face.Grade = Entry.Grade;
					Face.StorageGroup = Entry.StorageGroup;
					Face.StorageDomain = Entry.StorageDomain;
					const FVector2D Points[3] = {Polygon[0], Polygon[I], Polygon[I + 1]};
					for (int32 J = 0; J < 3; ++J)
					{
						double Z = 0;
						if (Entry.bCanonicalHeight && !Height(Source, Points[J], Z, Error)) return false;
						Face.Surface.Points[J] = FVector(Points[J], Z);
					}
					Clipped.Add(Face);
				}
			}
		}
		for (const auto& Entry : Clipped)
		{
			const auto& Source = Entry.Surface;
			FBox2D Bounds(ForceInit);
			TArray<FVector2D> Footprint;
			for (const auto& P : Source.Points) { Footprint.Add(FVector2D(P)); Bounds += Footprint.Last(); }
			const double ExpectedArea = Area(Footprint);
			if (ExpectedArea <= 1e-12)
			{
				Error = FString::Printf(TEXT("Road ribbon has degenerate XY coverage: %.17g m2."),ExpectedArea);
				for (const auto& P : Source.Points) Error+=FString::Printf(TEXT(" [%.17g,%.17g,%.17g]"),P.X,P.Y,P.Z);
				return false;
			}
			double CoveredArea = 0;
			for (const auto* NeighborPointer : Neighbors)
			{
				const auto& Neighbor = *NeighborPointer;
				const FBox2D CellBounds(FVector2D(Neighbor.Bounds.X, Neighbor.Bounds.Y), FVector2D(Neighbor.Bounds.Z, Neighbor.Bounds.W));
				if (!CellBounds.Intersect(Bounds)) continue;
				TArray<FProjectWorldSurfaceTriangle> Terrain;
				if (Description || Entry.bCanonicalHeight || Entry.Grade)
				{
					if (!Triangles(Neighbor, Bounds, Terrain, Error, Entry.Grade && !Entry.bCanonicalHeight ?
						EProjectWorldTerrainQuery::ImmutableGrid : EProjectWorldTerrainQuery::FinalSurface)) return false;
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
					if (Entry.bCanonicalHeight)
					{
						for (const auto& P : Polygon)
						{
							double RoadZ, TerrainZ;
							if (!Height(Source, P, RoadZ, Error) || !Height(Triangle, P, TerrainZ, Error)) return false;
							if (RoadZ + Lift < TerrainZ - 1e-8)
							{ Error = TEXT("Road elevation intersects its authenticated final Terrain."); return false; }
						}
					}
					if ((!Description && !Entry.Grade) || Entry.bCanonicalHeight) continue;
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
						if (!Emit(Entry, Positions)) return false;
					}
				}
				if (bConsumed) ConsumedTerrain.Add(Neighbor.CellId);
				if (Entry.bCanonicalHeight && !Entry.Abutments.IsEmpty())
				{
					TArray<FProjectWorldSurfaceTriangle> Base;
					if (!Triangles(Neighbor, Bounds, Base, Error, EProjectWorldTerrainQuery::ImmutableGrid)) return false;
					for (const auto& Ground : Base)
					{
						TArray<FVector2D> Polygon;
						if (!IntersectXY(Source, Ground, Polygon, Error)) return false;
						for (const auto& P : Polygon) for (const auto& End : Entry.Abutments)
						{
							const FVector2D D = End.Value - End.Key;
							const double Alpha = FVector2D::DotProduct(P - End.Key, D) / D.SquaredLength();
							if (Alpha < -1e-8 || Alpha > 1 + 1e-8 || (P - End.Key - Alpha * D).Length() >= 1e-8) continue;
							double RoadZ, GroundZ;
							if (!Height(Source, P, RoadZ, Error) || !Height(Ground, P, GroundZ, Error)) return false;
							if (FMath::Abs(RoadZ - GroundZ) > 1e-7)
							{ Error = TEXT("Road elevation changes its immutable grid abutment section."); return false; }
						}
					}
				}
			}
			if (FMath::Abs(CoveredArea - ExpectedArea) > FMath::Max(1e-8, ExpectedArea * 1e-7))
			{ Error = FString::Printf(TEXT("Road terrain coverage is incomplete or overlapping: %.17g / %.17g m2."), CoveredArea, ExpectedArea); return false; }
			if (Entry.bCanonicalHeight)
			{
				auto& GroupStorage = StorageGroups.FindOrAdd(Entry.StorageGroup == INDEX_NONE ? UngroupedId-- : Entry.StorageGroup);
				if (!GroupStorage.Example) GroupStorage.Example = &Entry;
				if (GroupStorage.Example->Elevation != Entry.Elevation)
				{ Error = TEXT("Road storage group crosses source profiles."); return false; }
				auto& Piece = GroupStorage.Pieces.AddDefaulted_GetRef();
				for (int32 I = 0; I < 3; ++I)
					Piece.Points[I] = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
						FVector(Source.Points[I].X, Source.Points[I].Y, Source.Points[I].Z + Lift)) - Origin;
			}
		}
		FBox2D StorageClip(ForceInit);
		for (const auto& P : {Context.Domain.Min, Context.Domain.Max})
			StorageClip += FVector2D(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(P, Bundle.HeightOriginMeters)) - Origin);
		TArray<int32> GroupIds; StorageGroups.GetKeys(GroupIds); GroupIds.Sort();
		for (const auto Id : GroupIds)
		{
			const auto& Storage = StorageGroups[Id];
			const auto& Entry = *Storage.Example;
			const auto& Parent = Entry.StorageGroup == INDEX_NONE ? Entry.Surface : Entry.StorageDomain;
			TArray<FVector2D> Domain;
			for (const auto& P : Parent.Points)
				Domain.Add(FVector2D(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, P) - Origin));
			TArray<FProjectWorldSurfaceTriangle> Stored;
			if (!ProjectWorldRoadStoredPolygon::StoreGroup(Storage.Pieces, Domain, StoredVertices.FindOrAdd(Entry.Elevation), Stored, Error, &StorageClip))
			{ Error = FString::Printf(TEXT("Road storage group %d: %s"), Id, *Error); return false; }
			if (Entry.Elevation && Entry.Elevation->Domain==EProjectWorldRoadElevationDomain::ReferencedConstructedSurface)
			{
				auto* Cached=ReferencedStorage.Find(Entry.Elevation);
				if (!Cached)
				{
					FReferencedStorage New; FString Identity;
					if (!ProjectWorldRoadReferencedSurface::Read(Bundle,*Entry.Elevation,New.Field,Identity,Error)) return false;
					Cached=&ReferencedStorage.Add(Entry.Elevation,MoveTemp(New));
				}
				// Raw source aliases and XY coverage have already passed StoreGroup.
				// The referenced field is sampled at the XY the mesh actually stores.
				for (auto& Face : Stored) for (auto& P : Face.Points)
				{
					const FVector2f Key(P.X,P.Y);
					if (const auto* Z=Cached->Heights.Find(Key)) { P.Z=*Z; continue; }
					const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,P+Origin); double Height;
					if (!ProjectWorldConstructedSurface::Sample(Cached->Field,FVector2D(Canonical),Height,Error)) return false;
					const float Z=static_cast<float>((Height+Lift-Bundle.HeightOriginMeters)*100-Origin.Z);
					if (!FMath::IsFinite(Z)) { Error=TEXT("Referenced stored height exceeds native position storage."); return false; }
					Cached->Heights.Add(Key,Z); P.Z=Z;
				}
			}
			for (auto Face : Stored) if (!Emit(Entry, Face.Points)) return false;
		}
		if (Description) *Description = MoveTemp(Candidate);
		return true;
	}
}
