#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshAttributeUtil.h"

namespace ProjectWorldMeshTerrainProducer
{
	bool BuildCellMesh(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		UE::Geometry::FDynamicMesh3& Mesh, FString& Error)
	{
		using namespace UE::Geometry;
		const auto& T = Cell.Terrain;
		TArray<FProjectWorldSurfaceTriangle> Faces;
		if (!ProjectWorldTerrainSurface::Triangles(Cell, FBox2D(FVector2D(T.Bounds.X, T.Bounds.Y), FVector2D(T.Bounds.Z, T.Bounds.W)), Faces, Error)) return false;
		Mesh = FDynamicMesh3();
		Mesh.EnableVertexUVs(FVector2f::ZeroVector);
		TMap<FVector, int32> Indices;
		TArray<FVector2f> Weights;
		auto Append = [&](const FVector& Point, const FVector2f& Weight, const FVector2f& UV) -> int32
		{
			if (const int32* Existing = Indices.Find(Point))
			{
				if (!Weights[*Existing].Equals(Weight, 1e-6f))
				{ Error = TEXT("Coincident canonical terrain vertices have conflicting semantic weights."); return INDEX_NONE; }
				return *Existing;
			}
			const int32 Id = Mesh.AppendVertex(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Point));
			Mesh.SetVertexUV(Id, UV);
			Indices.Add(Point, Id);
			Weights.Add(Weight);
			return Id;
		};
		for (int32 Row=0; Row<T.SamplesY; ++Row) for (int32 Column=0; Column<T.SamplesX; ++Column)
		{
			const int32 I = Row * T.SamplesX + Column;
			Append(FVector(T.Bounds.X + Column * T.SampleSpacing.X, FProjectWorldCanonicalLoader::TerrainRowNorthing(T, Row), T.HeightsMeters[I]),
				FVector2f(T.SurfaceWeights.FindChecked(GroundChannel)[I], T.SurfaceWeights.FindChecked(HydroTransitionChannel)[I]),
				FVector2f(static_cast<float>(Column) / static_cast<float>(T.SamplesX - 1),
					static_cast<float>(Row) / static_cast<float>(T.SamplesY - 1)));
		}
		for (const auto& Quad : T.RefinedQuads)
		{
			if (Quad.Weights.Num() != Quad.Vertices.Num()) { Error = TEXT("Refined terrain weights are missing."); return false; }
			for (int32 I=0; I<Quad.Vertices.Num(); ++I)
			{
				const auto& P = Quad.Vertices[I];
				if (Append(P, Quad.Weights[I], FVector2f(static_cast<float>((P.X-T.Bounds.X)/(T.Bounds.Z-T.Bounds.X)),
					static_cast<float>((T.Bounds.W-P.Y)/(T.Bounds.W-T.Bounds.Y)))) < 0) return false;
			}
		}
		for (const auto& Face : Faces)
		{
			const int32 *A = Indices.Find(Face.Points[0]), *B = Indices.Find(Face.Points[1]), *C = Indices.Find(Face.Points[2]);
			if (A == nullptr || B == nullptr || C == nullptr || Mesh.AppendTriangle(*A, *B, *C) < 0)
			{ Error = TEXT("Final canonical terrain cannot form a manifold source mesh."); return false; }
		}
		Mesh.EnableAttributes();
		CopyVertexUVsToOverlay(Mesh, *Mesh.Attributes()->PrimaryUV());
		Mesh.Attributes()->SetNumWeightLayers(2);
		for (int32 Layer=0; Layer<2; ++Layer)
		{
			auto* Attribute = Mesh.Attributes()->GetWeightLayer(Layer);
			Attribute->SetName(Layer == 0 ? GroundChannel : HydroTransitionChannel);
			Attribute->Initialize();
			for (int32 I=0; I<Weights.Num(); ++I) Attribute->SetValue(I, &Weights[I][Layer]);
		}
		if (!T.RefinedQuads.IsEmpty())
		{
			TArray<int32> Unused;
			for (int32 I : Mesh.VertexIndicesItr()) if (Mesh.GetVtxTriangleCount(I) == 0) Unused.Add(I);
			for (int32 I : Unused) Mesh.RemoveVertex(I);
			Mesh.CompactInPlace();
		}
		return true;
	}

	bool MatchesCellMesh(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const UE::Geometry::FDynamicMesh3& Actual, FString& Error)
	{
		UE::Geometry::FDynamicMesh3 Expected;
		if (!BuildCellMesh(Bundle, Cell, Expected, Error)) return false;
		if (Actual.VertexCount() != Expected.VertexCount() || Actual.TriangleCount() != Expected.TriangleCount())
		{ Error = TEXT("Terrain source omits final-surface topology."); return false; }
		for (int32 I : Expected.VertexIndicesItr())
			if (!Actual.IsVertex(I) || !Actual.GetVertex(I).Equals(Expected.GetVertex(I), 1e-6))
			{ Error = TEXT("Terrain source differs from final canonical vertices."); return false; }
		for (int32 I : Expected.TriangleIndicesItr())
			if (!Actual.IsTriangle(I) || Actual.GetTriangle(I) != Expected.GetTriangle(I))
			{ Error = TEXT("Terrain source differs from final canonical triangles."); return false; }
		if (Actual.Attributes() == nullptr || Actual.Attributes()->NumWeightLayers() != 2)
		{ Error = TEXT("Terrain source has missing semantic weights."); return false; }
		for (int32 Layer=0; Layer<2; ++Layer) for (int32 I : Expected.VertexIndicesItr())
		{
			float A=0, B=0;
			Actual.Attributes()->GetWeightLayer(Layer)->GetValue(I, &A);
			Expected.Attributes()->GetWeightLayer(Layer)->GetValue(I, &B);
			if (!FMath::IsNearlyEqual(A, B, 1e-6f))
			{ Error = TEXT("Terrain source semantic weights differ from canonical."); return false; }
		}
		return true;
	}
}
