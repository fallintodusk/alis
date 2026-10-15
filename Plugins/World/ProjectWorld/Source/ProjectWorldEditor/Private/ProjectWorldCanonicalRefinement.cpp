#include "ProjectWorldCanonicalRefinement.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "Algo/AllOf.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectWorldCanonicalRefinement
{
	namespace
	{
		constexpr double Epsilon = 1e-8;
		const FString Schema = TEXT("https://alis.world/schemas/world-compiler/terrain-refinement-v1.json");
		bool Reject(FProjectWorldCanonicalValidation& V, const FString& Message)
		{
			if (V.ErrorCode.IsEmpty()) { V.ErrorCode = TEXT("terrain-refinement"); V.Message = Message; }
			return false;
		}
		bool Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double& Out)
		{ return O.IsValid() && O->TryGetNumberField(Key, Out) && FMath::IsFinite(Out); }
		bool Array(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const TArray<TSharedPtr<FJsonValue>>*& Out)
		{ return O.IsValid() && O->TryGetArrayField(Key, Out) && Out != nullptr; }
		bool Hash(const FString& Value)
		{
			return Value.Len() == 64 && Algo::AllOf(Value, [](TCHAR C) { return FChar::IsHexDigit(C); });
		}
	}

	bool ResolveBase(const FString& Root, TSharedPtr<FJsonObject>& TerrainObject,
		TSharedPtr<FJsonObject>& RefinementObject,
		TFunctionRef<bool(const FString&, const FString&, int64)> IsVerifiedOutput,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Identity;
		if (!TerrainObject->TryGetStringField(TEXT("$schema"), Identity) || Identity != Schema) return true;
		const TSharedPtr<FJsonObject>* Base = nullptr;
		FString Path, Digest, FullPath, Text;
		double Bytes = 0, Version = 0;
		if (!Number(TerrainObject, TEXT("schema_version"), Version) || Version != 1 ||
			!TerrainObject->TryGetObjectField(TEXT("base"), Base) || Base == nullptr ||
			!(*Base)->TryGetStringField(TEXT("path"), Path) || !Path.StartsWith(TEXT("canonical/terrain/")) ||
			!(*Base)->TryGetStringField(TEXT("sha256"), Digest) || !Hash(Digest) ||
			!Number(*Base, TEXT("byte_size"), Bytes) || Bytes <= 0 || Bytes != FMath::FloorToDouble(Bytes) ||
			!IsVerifiedOutput(Path, Digest, static_cast<int64>(Bytes)) ||
			!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root, Path, FullPath) ||
			!FFileHelper::LoadFileToString(Text, *FullPath))
			return Reject(Validation, TEXT("Refinement base is absent, unverified or malformed."));
		TSharedPtr<FJsonObject> GridObject;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), GridObject) ||
			!GridObject.IsValid() || !GridObject->TryGetStringField(TEXT("$schema"), Identity) ||
			Identity != TEXT("https://alis.world/schemas/world-compiler/terrain-cell-v2.json"))
			return Reject(Validation, TEXT("Refinement requires one regular-grid base, without nested overlays."));
		RefinementObject = TerrainObject;
		TerrainObject = GridObject;
		return true;
	}

	bool Parse(const TSharedPtr<FJsonObject>& Object, const FString& Grid, const FString& Cell,
		FProjectWorldCanonicalTerrain& Terrain, FProjectWorldCanonicalValidation& Validation)
	{
		if (!Object.IsValid()) return true;
		FString G, C, Rule;
		const TArray<TSharedPtr<FJsonValue>>* Quads = nullptr;
		const TSharedPtr<FJsonObject>* Consumed = nullptr;
		if (!Object->TryGetStringField(TEXT("grid_id"), G) || G != Grid ||
			!Object->TryGetStringField(TEXT("cell_id"), C) || C != Cell ||
			!Object->TryGetStringField(TEXT("coordinate_rule"), Rule) || Rule != TEXT("canonical_boundary_intersections_float64") ||
			!Object->TryGetObjectField(TEXT("consumed"), Consumed) || Consumed == nullptr ||
			!Array(Object, TEXT("quads"), Quads) || Quads->IsEmpty())
			return Reject(Validation, TEXT("Refinement identity or geometry is incomplete."));
		for (const TCHAR* Field : {TEXT("water_sha256"), TEXT("policy_sha256"), TEXT("implementation_sha256"), TEXT("execution_identity"), TEXT("domain_sha256")})
		{
			FString Digest;
			if (!(*Consumed)->TryGetStringField(Field, Digest) || !Hash(Digest))
				return Reject(Validation, TEXT("Refinement consumed identity is invalid."));
		}
		TSet<FIntPoint> Coordinates;
		for (const auto& Value : *Quads)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			const TArray<TSharedPtr<FJsonValue>> *Coordinate = nullptr, *Vertices = nullptr, *Faces = nullptr;
			if (!Value->TryGetObject(O) || O == nullptr || !Array(*O, TEXT("coordinate"), Coordinate) || Coordinate->Num() != 2 ||
				!Array(*O, TEXT("vertices"), Vertices) || !Array(*O, TEXT("triangles"), Faces) || Faces->IsEmpty() || Faces->Num() > 4096)
				return Reject(Validation, TEXT("Refinement quad is malformed."));
			FProjectWorldCanonicalTerrainQuad Quad;
			for (int32 I = 0; I < 2; ++I)
			{
				double N = 0;
				if (!(*Coordinate)[I]->TryGetNumber(N) || !FMath::IsFinite(N) || N < 0 || N != FMath::FloorToDouble(N) ||
					N >= (I == 0 ? Terrain.SamplesX - 1 : Terrain.SamplesY - 1))
					return Reject(Validation, TEXT("Refinement quad coordinate is invalid."));
				Quad.Coordinate[I] = static_cast<int32>(N);
			}
			if (Coordinates.Contains(Quad.Coordinate)) return Reject(Validation, TEXT("Refinement repeats a quad."));
			Coordinates.Add(Quad.Coordinate);
			const double West = Terrain.Bounds.X + Quad.Coordinate.X * Terrain.SampleSpacing.X;
			const double North = FProjectWorldCanonicalLoader::TerrainRowNorthing(Terrain, Quad.Coordinate.Y);
			TSet<FVector2D> VertexXY;
			for (const auto& Vertex : *Vertices)
			{
				const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
				if (!Vertex->TryGetArray(Row) || Row == nullptr || Row->Num() != 5)
					return Reject(Validation, TEXT("Refinement vertex layout is invalid."));
				double V[5];
				for (int32 I = 0; I < 5; ++I)
					if (!(*Row)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I]))
						return Reject(Validation, TEXT("Refinement vertex is nonfinite."));
				if (V[0] < West - Epsilon || V[0] > West + Terrain.SampleSpacing.X + Epsilon ||
					V[1] > North + Epsilon || V[1] < North - Terrain.SampleSpacing.Y - Epsilon ||
					V[3] < 0 || V[4] < 0 || FMath::Abs(V[3] + V[4] - 1) > Epsilon)
					return Reject(Validation, TEXT("Refinement vertex leaves its quad or has invalid weights."));
				if (VertexXY.Contains(FVector2D(V[0],V[1]))) return Reject(Validation, TEXT("Refinement repeats an XY vertex."));
				VertexXY.Add(FVector2D(V[0],V[1]));
				Quad.Vertices.Add(FVector(V[0], V[1], V[2]));
				Quad.Weights.Add(FVector2f(V[3], V[4]));
			}
			double TotalArea = 0;
			TMap<FIntPoint,FIntPoint> EdgeUses;
			TSet<int32> UsedVertices;
			TArray<FProjectWorldSurfaceTriangle> Triangles;
			for (const auto& Face : *Faces)
			{
				const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
				if (!Face->TryGetArray(Row) || Row == nullptr || Row->Num() != 3)
					return Reject(Validation, TEXT("Refinement face layout is invalid."));
				FIntVector Indices;
				for (int32 I = 0; I < 3; ++I)
				{
					double N = 0;
					if (!(*Row)[I]->TryGetNumber(N) || !FMath::IsFinite(N) || N < 0 || N >= Quad.Vertices.Num() || N != FMath::FloorToDouble(N))
						return Reject(Validation, TEXT("Refinement face index is invalid."));
					Indices[I] = static_cast<int32>(N);
				}
				FProjectWorldSurfaceTriangle Triangle{{Quad.Vertices[Indices.X], Quad.Vertices[Indices.Y], Quad.Vertices[Indices.Z]}};
				const FVector U = Triangle.Points[1] - Triangle.Points[0], V = Triangle.Points[2] - Triangle.Points[0];
				const double Area = (U.X * V.Y - U.Y * V.X) * 0.5;
				if (Area <= 1e-12) return Reject(Validation, TEXT("Refinement face is collapsed or reversed."));
				for (const auto& Other : Triangles)
				{
					TArray<FVector2D> Intersection; FString Error;
					if (!ProjectWorldTerrainSurface::IntersectXY(Triangle, Other, Intersection, Error) ||
						ProjectWorldTerrainSurface::Area(Intersection) > Epsilon)
						return Reject(Validation, TEXT("Refinement faces overlap or cannot be checked."));
				}
				TotalArea += Area;
				Triangles.Add(Triangle);
				Quad.Triangles.Add(Indices);
				for (int32 I=0; I<3; ++I)
				{
					const int32 A=Indices[I], B=Indices[(I+1)%3];
					UsedVertices.Add(A);
					auto& Uses = EdgeUses.FindOrAdd(FIntPoint(FMath::Min(A,B),FMath::Max(A,B)));
					++Uses.X; Uses.Y += A < B ? 1 : -1;
				}
			}
			if (UsedVertices.Num() != Quad.Vertices.Num()) return Reject(Validation, TEXT("Refinement contains unused vertices."));
			for (const auto& Pair : EdgeUses)
			{
				const FVector A=Quad.Vertices[Pair.Key.X], B=Quad.Vertices[Pair.Key.Y];
				const bool Outer = (FMath::Abs(A.X-West)<Epsilon && FMath::Abs(B.X-West)<Epsilon) ||
					(FMath::Abs(A.X-West-Terrain.SampleSpacing.X)<Epsilon && FMath::Abs(B.X-West-Terrain.SampleSpacing.X)<Epsilon) ||
					(FMath::Abs(A.Y-North)<Epsilon && FMath::Abs(B.Y-North)<Epsilon) ||
					(FMath::Abs(A.Y-North+Terrain.SampleSpacing.Y)<Epsilon && FMath::Abs(B.Y-North+Terrain.SampleSpacing.Y)<Epsilon);
				if ((Outer && Pair.Value.X != 1) || (!Outer && (Pair.Value.X != 2 || Pair.Value.Y != 0)))
					return Reject(Validation,TEXT("Refinement has an open, nonconforming or nonmanifold interior edge."));
			}
			if (FMath::Abs(TotalArea - Terrain.SampleSpacing.X * Terrain.SampleSpacing.Y) > Epsilon)
				return Reject(Validation, TEXT("Refinement has missing quad coverage."));
			Terrain.RefinedQuads.Add(MoveTemp(Quad));
		}
		return true;
	}

	bool ValidateSeams(const FProjectWorldCanonicalBundle& Bundle, FProjectWorldCanonicalValidation& Validation)
	{
		struct FEdgePoint { FVector Position; FVector2f Weight; };
		struct FEdge { TArray<FEdgePoint> Points; FVector A; FVector B; FVector2f WeightA; FVector2f WeightB; };
		TMap<FString, TArray<FEdge>> Edges;
		for (const auto& Cell : Bundle.Cells) for (const auto& Quad : Cell.Terrain.RefinedQuads)
		{
			const auto& T = Cell.Terrain;
			const int32 X = Quad.Coordinate.X, Y = Quad.Coordinate.Y;
			auto Point = [&](int32 C, int32 R) { return FVector(T.Bounds.X + C * T.SampleSpacing.X,
				FProjectWorldCanonicalLoader::TerrainRowNorthing(T, R), T.HeightsMeters[R * T.SamplesX + C]); };
			const FVector Corners[]{Point(X,Y), Point(X+1,Y), Point(X+1,Y+1), Point(X,Y+1)};
			for (int32 I = 0; I < 4; ++I)
			{
				FEdge Edge; Edge.A = Corners[I]; Edge.B = Corners[(I+1)%4];
				auto Weight = [&](const FVector& P)
				{
					const int32 C = FMath::RoundToInt((P.X-T.Bounds.X)/T.SampleSpacing.X);
					const int32 R = FMath::RoundToInt(FProjectWorldCanonicalLoader::TerrainSampleRow(T, P.Y));
					return FVector2f(T.SurfaceWeights.FindChecked(TEXT("ground"))[R*T.SamplesX+C],
						T.SurfaceWeights.FindChecked(TEXT("hydro_transition"))[R*T.SamplesX+C]);
				};
				if (Edge.A.X > Edge.B.X || (Edge.A.X == Edge.B.X && Edge.A.Y > Edge.B.Y)) Swap(Edge.A, Edge.B);
				Edge.WeightA = Weight(Edge.A); Edge.WeightB = Weight(Edge.B);
				for (int32 Index=0; Index<Quad.Vertices.Num(); ++Index)
				{
					const FVector& V = Quad.Vertices[Index];
					const FVector2D D(Edge.B - Edge.A), P(V - Edge.A);
					const double Fraction = FVector2D::DotProduct(P, D) / D.SizeSquared();
					if (FMath::Abs(P.X * D.Y - P.Y * D.X) <= Epsilon * D.Size() && Fraction >= -Epsilon && Fraction <= 1 + Epsilon)
						Edge.Points.Add({V, Quad.Weights[Index]});
				}
				Edge.Points.Sort([](const FEdgePoint& A, const FEdgePoint& B) { return A.Position.X < B.Position.X || (A.Position.X == B.Position.X && A.Position.Y < B.Position.Y); });
				Edges.FindOrAdd(FString::Printf(TEXT("%.17g,%.17g:%.17g,%.17g"), Edge.A.X, Edge.A.Y, Edge.B.X, Edge.B.Y)).Add(MoveTemp(Edge));
			}
		}
		for (const auto& Pair : Edges)
		{
			const auto& Entries = Pair.Value;
			if (Entries.Num() > 2 || Entries[0].Points.Num() < 2) return Reject(Validation, TEXT("Invalid refinement boundary."));
			if (Entries.Num() == 2)
			{
				if (Entries[0].Points.Num() != Entries[1].Points.Num()) return Reject(Validation, TEXT("Refinement seam topology differs."));
				for (int32 I=0; I<Entries[0].Points.Num(); ++I)
					if (!Entries[0].Points[I].Position.Equals(Entries[1].Points[I].Position, Epsilon) ||
						!Entries[0].Points[I].Weight.Equals(Entries[1].Points[I].Weight, 1e-6f))
						return Reject(Validation, TEXT("Refinement seam heights or weights differ."));
			}
			else if (Entries[0].Points.Num() != 2 || !Entries[0].Points[0].Position.Equals(Entries[0].A, Epsilon) ||
				!Entries[0].Points[1].Position.Equals(Entries[0].B, Epsilon) ||
				!Entries[0].Points[0].Weight.Equals(Entries[0].WeightA, 1e-6f) || !Entries[0].Points[1].Weight.Equals(Entries[0].WeightB, 1e-6f))
				return Reject(Validation, TEXT("Refinement needs an expanded neighbor domain or original outer edge."));
		}
		return true;
	}
}
