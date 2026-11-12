// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedOutline.h"
#include "Algo/BinarySearch.h"
#include "CompGeom/ExactPredicates.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MeshBoundaryLoops.h"
#include "Selections/MeshConnectedComponents.h"

namespace ProjectWorldConstructedOutline
{
	using namespace UE::Geometry;
	bool Extract(const TArray<FGeneralPolygon2d>& Pieces,TArray<FGeneralPolygon2d>& Out,FString& Error)
	{
		Out.Reset(); Error.Reset();
		auto Refuse = [&](const TCHAR* Message) { Error=Message; return false; };
		if (Pieces.IsEmpty()) return true;
		// Each of 4096 source triangles clips to at most seven vertices and five stored faces.
		constexpr int32 MaximumClippedCoverPieces=5*4096;
		if (Pieces.Num()>MaximumClippedCoverPieces) return Refuse(TEXT("Constructed outline exceeds the bounded clipped-cover census."));
		TArray<FPolygon2d> Rings; TArray<FBox2D> Bounds;
		TMap<FVector2D,int32> IDs; FDynamicMesh3 Mesh;
		for (const auto& Piece : Pieces)
		{
			auto Ring=Piece.GetOuter();
			if (!Piece.GetHoles().IsEmpty() || Ring.VertexCount()<3 || Ring.VertexCount()>7)
				return Refuse(TEXT("Constructed outline requires bounded convex clipped pieces without holes."));
			if (Ring.IsClockwise()) Ring.Reverse();
			FBox2D Box(ForceInit);
			for (const auto& P : Ring.GetVertices())
			{
				if (P.ContainsNaN()) return Refuse(TEXT("Constructed outline has nonfinite source coordinates."));
				Box+=P;
				if (!IDs.Contains(P)) IDs.Add(P,Mesh.AppendVertex(FVector(P,0)));
			}
			for (int32 I=0; I<Ring.VertexCount(); ++I)
			{
				const auto& A=Ring[I]; const auto& B=Ring[(I+1)%Ring.VertexCount()]; bool Positive=false;
				if (A==B) return Refuse(TEXT("Constructed outline has a repeated source edge node."));
				for (const auto& P : Ring.GetVertices())
				{
					const double Side=ExactPredicates::Orient2(A,B,P);
					if (!FMath::IsFinite(Side) || Side<0) return Refuse(TEXT("Constructed outline piece is not consistently convex."));
					Positive|=Side>0;
				}
				if (!Positive) return Refuse(TEXT("Constructed outline piece has no exact positive interior."));
			}
			Rings.Add(MoveTemp(Ring)); Bounds.Add(Box);
		}
		auto Separates = [](const FPolygon2d& A,const FPolygon2d& B)
		{
			for (int32 I=0; I<A.VertexCount(); ++I)
			{
				bool Outside=true;
				for (const auto& P : B.GetVertices()) Outside &= ExactPredicates::Orient2(A[I],A[(I+1)%A.VertexCount()],P)<=0;
				if (Outside) return true;
			}
			return false;
		};
		for (int32 I=0; I<Rings.Num(); ++I) for (int32 J=I+1; J<Rings.Num(); ++J)
			if (Bounds[I].Intersect(Bounds[J]) && !Separates(Rings[I],Rings[J]) && !Separates(Rings[J],Rings[I]))
				return Refuse(TEXT("Constructed outline pieces overlap with exact positive area."));
		TArray<FVector2D> Sorted[2]; IDs.GenerateKeyArray(Sorted[0]); Sorted[1]=Sorted[0];
		for (int32 Axis=0; Axis<2; ++Axis) Sorted[Axis].Sort([Axis](const auto& A,const auto& B) { return A[Axis]<B[Axis]; });
		struct FUse { int32 Count=0,Direction=0; };
		TMap<FIndex2i,FUse> Uses; int64 Work=0;
		for (const auto& Ring : Rings)
		{
			FVector2D Center=FVector2D::ZeroVector;
			const auto Anchor=Ring[0];
			for (const auto& P : Ring.GetVertices()) Center+=P-Anchor;
			Center=Anchor+Center/Ring.VertexCount();
			if (Center.ContainsNaN()) return Refuse(TEXT("Constructed outline temporary fan is nonfinite."));
			for (int32 I=0; I<Ring.VertexCount(); ++I)
				if (ExactPredicates::Orient2(Ring[I],Ring[(I+1)%Ring.VertexCount()],Center)<=0)
					return Refuse(TEXT("Constructed outline temporary fan has no strict source interior."));
			const int32 CenterID=Mesh.AppendVertex(FVector(Center,0));
			for (int32 I=0; I<Ring.VertexCount(); ++I)
			{
				const auto A=Ring[I],B=Ring[(I+1)%Ring.VertexCount()];
				int32 Begin[2],End[2];
				for (int32 Axis=0; Axis<2; ++Axis)
				{
					auto Coordinate=[Axis](const auto& P) { return P[Axis]; };
					Begin[Axis]=Algo::LowerBoundBy(Sorted[Axis],FMath::Min(A[Axis],B[Axis]),Coordinate);
					End[Axis]=Algo::UpperBoundBy(Sorted[Axis],FMath::Max(A[Axis],B[Axis]),Coordinate);
				}
				const int32 Search=End[0]-Begin[0]<=End[1]-Begin[1] ? 0 : 1,Along=A.X!=B.X ? 0 : 1;
				TArray<FVector2D> Nodes;
				for (int32 At=Begin[Search]; At<End[Search]; ++At)
				{
					if (++Work>64*1024*1024) return Refuse(TEXT("Constructed outline exceeds its exact subdivision-work budget."));
					const auto& P=Sorted[Search][At];
					if (P.X>=FMath::Min(A.X,B.X) && P.X<=FMath::Max(A.X,B.X) &&
						P.Y>=FMath::Min(A.Y,B.Y) && P.Y<=FMath::Max(A.Y,B.Y) && ExactPredicates::Orient2(A,B,P)==0) Nodes.Add(P);
				}
				Nodes.Sort([Along,A,B](const auto& P,const auto& Q) { return B[Along]>A[Along] ? P[Along]<Q[Along] : P[Along]>Q[Along]; });
				if (Nodes.Num()<2 || Nodes[0]!=A || Nodes.Last()!=B) return Refuse(TEXT("Constructed exact source edge subdivision is incomplete."));
				for (int32 At=1; At<Nodes.Num(); ++At)
				{
					const int32 V0=IDs[Nodes[At-1]],V1=IDs[Nodes[At]];
					const FIndex2i Key(FMath::Min(V0,V1),FMath::Max(V0,V1)); auto& Use=Uses.FindOrAdd(Key);
					++Use.Count; Use.Direction+=V0<V1 ? 1 : -1;
					if (Use.Count>2 || (Use.Count==2 && Use.Direction!=0) || Mesh.AppendTriangle(CenterID,V0,V1)<0)
						return Refuse(TEXT("Constructed outline has duplicate, inconsistent or nonmanifold source edges."));
				}
			}
		}
		if (!Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(true,false),EValidityCheckFailMode::ReturnOnly))
			return Refuse(TEXT("Constructed native outline topology is invalid."));
		FMeshConnectedComponents Components(&Mesh); Components.FindConnectedTriangles();
		TArray<int32> TriangleOwner; TriangleOwner.Init(INDEX_NONE,Mesh.MaxTriangleID());
		for (int32 I=0; I<Components.Components.Num(); ++I)
			for (const int32 T : Components.Components[I].Indices) TriangleOwner[T]=I;
		FMeshBoundaryLoops Boundary(&Mesh,false);
		Boundary.SpanBehavior=FMeshBoundaryLoops::ESpanBehaviors::Abort;
		Boundary.FailureBehavior=FMeshBoundaryLoops::EFailureBehaviors::Abort;
		if (!Boundary.Compute() || Boundary.bAborted || Boundary.bSawOpenSpans || Boundary.bFellBackToSpansOnFailure ||
			!Boundary.Spans.IsEmpty() || !Boundary.FailureBowties.IsEmpty()) return Refuse(TEXT("Constructed native boundary loops are incomplete or ambiguous."));
		TArray<FGeneralPolygon2d> Candidate; Candidate.SetNum(Components.Components.Num());
		TArray<bool> HasOuter; HasOuter.Init(false,Candidate.Num());
		TArray<TPair<int32,FPolygon2d>> Holes; TSet<int32> AccountedEdges;
		for (const auto& Loop : Boundary.Loops)
		{
			if (Loop.Vertices.Num()<3 || Loop.Edges.Num()!=Loop.Vertices.Num()) return Refuse(TEXT("Constructed native boundary loop is open."));
			TArray<FVector2D> Points; int32 Owner=INDEX_NONE;
			for (int32 I=0; I<Loop.Edges.Num(); ++I)
			{
				const int32 E=Loop.Edges[I]; const auto EV=Mesh.GetEdgeV(E); const auto ET=Mesh.GetEdgeT(E);
				const auto* Use=Uses.Find(FIndex2i(FMath::Min(EV.A,EV.B),FMath::Max(EV.A,EV.B)));
				if (!Mesh.IsBoundaryEdge(E) || !Use || Use->Count!=1 || AccountedEdges.Contains(E) || ET.A<0 ||
					(Owner!=INDEX_NONE && Owner!=TriangleOwner[ET.A])) return Refuse(TEXT("Constructed boundary invents an edge or merges point-touching components."));
				Owner=TriangleOwner[ET.A]; AccountedEdges.Add(E); Points.Add(FVector2D(Mesh.GetVertex(Loop.Vertices[I])));
			}
			FPolygon2d Ring(Points); const double Area=Ring.SignedArea();
			if (!FMath::IsFinite(Area) || Area==0 || !Candidate.IsValidIndex(Owner)) return Refuse(TEXT("Constructed boundary has no finite oriented owner."));
			if (Area<0) Holes.Add({Owner,MoveTemp(Ring)});
			else
			{
				if (HasOuter[Owner]) return Refuse(TEXT("Constructed connected component has multiple exterior loops."));
				HasOuter[Owner]=true; Candidate[Owner]=FGeneralPolygon2d(MoveTemp(Ring));
			}
		}
		int32 RequiredEdges=0; for (const auto& Pair : Uses) RequiredEdges+=Pair.Value.Count==1;
		if (AccountedEdges.Num()!=RequiredEdges || HasOuter.Contains(false)) return Refuse(TEXT("Constructed boundary omits a source edge or component."));
		for (auto& Hole : Holes) if (!Candidate[Hole.Key].AddHole(MoveTemp(Hole.Value)))
			return Refuse(TEXT("Constructed boundary hole cannot be assigned to its exact connected owner."));
		Out=MoveTemp(Candidate); return true;
	}
}
