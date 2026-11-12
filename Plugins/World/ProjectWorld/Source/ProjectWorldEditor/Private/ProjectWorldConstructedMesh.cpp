// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldConstructedMeshCoverage.h"
#include "ProjectWorldConstructedBoundary.h"
#include "ProjectWorldConstructedOutline.h"
#include "ConstrainedDelaunay2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include <limits>

namespace ProjectWorldConstructedMesh
{
	using namespace UE::Geometry;
	namespace
	{
		FGeneralPolygon2d Polygon(const FProjectWorldSurfaceTriangle& T, const FVector2D& Anchor)
		{
			FPolygon2d P({FVector2D(T.Points[0])-Anchor,FVector2D(T.Points[1])-Anchor,FVector2D(T.Points[2])-Anchor});
			if (P.SignedArea()<0) P.Reverse();
			return FGeneralPolygon2d(MoveTemp(P));
		}
		bool StoredPolygon(const FGeneralPolygon2d& Source,FGeneralPolygon2d& Out,
			TMap<FVector2D,FVector2D>& OriginalXY,double HeightMeters,double& MaximumRadius,FString& Error,
			ProjectWorldConstructedBoundary::FConversion* Conversion=nullptr)
		{
			auto Ring = [&](const FPolygon2d& Input, FPolygon2d& Output,bool Hole)
			{
				TArray<FVector2D> Raw,Stored; double Radius=0,Extent=0;
				for (const auto& P : Input.GetVertices())
				{
					const FVector2D Before(P.X*100,(HeightMeters-P.Y)*100);
					if (Before.ContainsNaN()) { Error=TEXT("Constructed raw ring has nonfinite XY."); return false; }
					Raw.Add(Before); Extent=FMath::Max(Extent,Before.Length());
				}
				for (int32 I=0; I<Raw.Num(); ++I)
				{
					const auto& P=Input.GetVertices()[I]; const auto& Before=Raw[I];
					FVector2f StoredPoint(Before);
					if (Conversion)
					{
						FVector2f Selected;
						if (!Conversion->Select(StoredPoint,Selected,Error)) return false;
						StoredPoint=Selected;
					}
					const FVector2D After(StoredPoint);
					if (Before.ContainsNaN() || After.ContainsNaN()) return false;
					Radius=FMath::Max(Radius,(After-Before).Length());
					if (!OriginalXY.Contains(After)) OriginalXY.Add(After,P);
					if (Stored.IsEmpty() || Stored.Last()!=After) Stored.Add(After);
				}
				MaximumRadius=FMath::Max(MaximumRadius,Radius);
				if (Stored.Num()>1 && Stored.Last()==Stored[0]) Stored.Pop();
				if (Stored.Num()<3) { Error=TEXT("collapsed ring"); return false; }
				const FPolygon2d Original(Raw); Output=FPolygon2d(Stored);
				const double Bound=2*FMath::Max(Original.Perimeter(),Output.Perimeter())*Radius+2*UE_DOUBLE_PI*Radius*Radius+
					1024*std::numeric_limits<double>::epsilon()*FMath::Max(1.0,Extent*Extent)*Raw.Num();
				if (!FMath::IsFinite(Bound) || Original.SignedArea()*Output.SignedArea()<=0 ||
					FMath::Abs(Original.SignedArea()-Output.SignedArea())>Bound)
				{ Error=FString::Printf(TEXT("ring area %.17g -> %.17g, bound %.17g"),Original.SignedArea(),Output.SignedArea(),Bound); return false; }
				for (int32 I=0; I<Stored.Num(); ++I) for (int32 J=I+1; J<Stored.Num(); ++J)
				{
					if (J==I+1 || (I==0 && J==Stored.Num()-1)) continue;
					if (FSegment2d(Stored[I],Stored[(I+1)%Stored.Num()]).Intersects(
						FSegment2d(Stored[J],Stored[(J+1)%Stored.Num()]),0,0))
					{ Error=TEXT("self-intersecting stored ring"); return false; }
				}
				if (Output.IsClockwise()!=Hole) Output.Reverse();
				return true;
			};
			FPolygon2d Outer;
			if (!Ring(Source.GetOuter(),Outer,false))
			{
				Error=TEXT("Constructed body exterior cannot retain topology at native float storage: ")+Error;
				for (int32 I=0; I<FMath::Min(16,Source.GetOuter().VertexCount()); ++I)
				{ const auto& P=Source.GetOuter()[I]; Error+=FString::Printf(TEXT(" [%.17g,%.17g]"),P.X,P.Y); }
				return false;
			}
			Out=FGeneralPolygon2d(Outer);
			for (const auto& Hole : Source.GetHoles())
			{
				FPolygon2d Stored;
				if (!Ring(Hole,Stored,true) || !Out.AddHole(Stored))
				{ Error=TEXT("Constructed body hole cannot retain topology at native float storage."); return false; }
			}
			return true;
		}
		bool Emit(FMeshDescription& Mesh, const FVector In[3], FString& Error,FTriangleID* EmittedId=nullptr)
		{
			FVector3f P[3]={FVector3f(In[0]),FVector3f(In[1]),FVector3f(In[2])};
			const double Before=FVector::CrossProduct(In[1]-In[0],In[2]-In[0]).Z;
			const double After=FVector::CrossProduct(FVector(P[1])-FVector(P[0]),FVector(P[2])-FVector(P[0])).Z;
			if (!FMath::IsFinite(After) || Before*After<=0)
			{
				Error=FString::Printf(TEXT("Constructed body float collapse: area2 %.17g -> %.17g cm2."),Before,After);
				for (int32 I=0; I<3; ++I) Error+=FString::Printf(TEXT(" [%.17g,%.17g -> %.17g,%.17g]"),In[I].X,In[I].Y,P[I].X,P[I].Y);
				return false;
			}
			if (After>0) Swap(P[1],P[2]);
			const FVector Normal=-FVector::CrossProduct(FVector(P[1]-P[0]),FVector(P[2]-P[0])).GetSafeNormal(0.0);
			const FVector Tangent=(FVector::XAxisVector-Normal*Normal.X).GetSafeNormal(0.0);
			if (Normal.ContainsNaN() || Tangent.ContainsNaN() || Normal.IsNearlyZero(0) || Tangent.IsNearlyZero(0))
			{ Error=TEXT("Constructed body has no finite native shading frame."); return false; }
			FStaticMeshAttributes A(Mesh); TArray<FVertexInstanceID> Instances;
			for (int32 I=0; I<3; ++I)
			{
				const auto V=Mesh.CreateVertex(); A.GetVertexPositions()[V]=P[I];
				const auto VI=Mesh.CreateVertexInstance(V); Instances.Add(VI);
				A.GetVertexInstanceNormals()[VI]=FVector3f(Normal);
				A.GetVertexInstanceTangents()[VI]=FVector3f(Tangent);
				A.GetVertexInstanceBinormalSigns()[VI]=1;
				A.GetVertexInstanceColors()[VI]=FVector4f(1,1,1,1);
				A.GetVertexInstanceUVs().Set(VI,0,FVector2f(P[I].X/100,P[I].Y/100));
			}
			const auto Triangle=Mesh.CreateTriangle(FPolygonGroupID(0),Instances);
			if (EmittedId) *EmittedId=Triangle;
			return true;
		}
	}
	bool Build(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const TArray<FProjectWorldSurfaceTriangle>& Field, double MaximumJoinStepMeters, FMeshDescription& Out, FVector& Origin, FString& Error,TArray<int32>* OutSourceTriangleIndices,
		double* OutMaximumXYDisplacementCentimeters)
	{
		Out=FMeshDescription(); Origin=FVector::ZeroVector; Error.Reset();
		if (OutSourceTriangleIndices) OutSourceTriangleIndices->Reset();
		if (OutMaximumXYDisplacementCentimeters) *OutMaximumXYDisplacementCentimeters=std::numeric_limits<double>::quiet_NaN();
		if (!FMath::IsFinite(MaximumJoinStepMeters) || MaximumJoinStepMeters<0 ||
			!ProjectWorldConstructedSurface::ValidateField(Field,Error))
		{ if (Error.IsEmpty()) Error=TEXT("Constructed mesh requires a finite explicit source agreement and join policy."); return false; }
		const FVector2D Anchor(Cell.Bounds.X,Cell.Bounds.Y);
		auto LocalFace = [&](FProjectWorldSurfaceTriangle T)
		{ for (auto& P : T.Points) { P.X-=Anchor.X; P.Y-=Anchor.Y; } return T; };
		if (Cell.Bounds.ContainsNaN() || Cell.Bounds.Z<=Cell.Bounds.X || Cell.Bounds.W<=Cell.Bounds.Y)
		{ Error=TEXT("Constructed mesh cell bounds are invalid."); return false; }
		const FVector CellOrigin=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
			FVector(Cell.Bounds.X,Cell.Bounds.W,Bundle.HeightOriginMeters));
		const FVector CandidateOrigin=CellOrigin;
		FMeshDescription Candidate; FStaticMeshAttributes Attributes(Candidate); Attributes.Register();
		TMap<FTriangleID,int32> SourceByTriangle;
		struct FHeightBinding { FVector Source; double StoredZ,Gradient,Roundoff; };
		TMap<FVector2D,FHeightBinding> HeightBindings;
		double MaximumRadius=0;
		Attributes.GetVertexInstanceUVs().SetNumChannels(1);
		Attributes.GetPolygonGroupMaterialSlotNames()[Candidate.CreatePolygonGroup()]=TEXT("ConstructedSurface");
		const FAxisAlignedBox2d LocalCell(FVector2D::ZeroVector,FVector2D(Cell.Bounds.Z-Cell.Bounds.X,Cell.Bounds.W-Cell.Bounds.Y));
		ProjectWorldConstructedBoundary::FConversion Boundary([&](const FVector2f& P,FString& DomainError)
		{
			const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(P.X,P.Y,0)+CandidateOrigin); double Height;
			return ProjectWorldConstructedSurface::TrySample(Field,FVector2D(Canonical),Height,DomainError);
		});
		TArray<FGeneralPolygon2d> SourcePieces,SourceFootprint,ClippedSourcePieces;
		for (const auto& T : Field) SourcePieces.Add(Polygon(T,Anchor));
		if (!ProjectWorldConstructedOutline::Extract(SourcePieces,SourceFootprint,Error))
		{ Error=TEXT("Complete constructed source footprint cannot be read: ")+Error; return false; }
		for (const auto& Component : SourceFootprint)
		{
			FGeneralPolygon2d Stored; TMap<FVector2D,FVector2D> Bindings;
			if (!StoredPolygon(Component,Stored,Bindings,Cell.Bounds.W-Cell.Bounds.Y,MaximumRadius,Error)) return false;
		}
		for (int32 SourceIndex=0; SourceIndex<Field.Num(); ++SourceIndex)
		{
			const auto& Face=Field[SourceIndex];
			const auto Plane=LocalFace(Face);
			const FVector U=Plane.Points[1]-Plane.Points[0],V=Plane.Points[2]-Plane.Points[0];
			const double D=U.X*V.Y-U.Y*V.X,DX=(U.Z*V.Y-U.Y*V.Z)/D,DY=(U.X*V.Z-U.Z*V.X)/D;
			const double Gradient=FMath::Sqrt(DX*DX+DY*DY);
			if (!FMath::IsFinite(Gradient)) { Error=TEXT("Constructed body source gradient is nonfinite."); return false; }
			TArray<FGeneralPolygon2d> Clipped;
			auto SourcePolygon=Polygon(Face,Anchor).GetOuter(); SourcePolygon.ClipConvex(LocalCell);
			const auto Clip=ProjectWorldConstructedMeshCoverage::ClassifySourceClip(SourcePolygon,Error);
			if (Clip==ProjectWorldConstructedMeshCoverage::ESourceClip::Invalid) return false;
			if (Clip==ProjectWorldConstructedMeshCoverage::ESourceClip::Empty) continue;
			Clipped.Add(FGeneralPolygon2d(MoveTemp(SourcePolygon)));
			ClippedSourcePieces.Append(Clipped);
			for (const auto& Part : Clipped)
			{
				FGeneralPolygon2d Stored;
				TMap<FVector2D,FVector2D> OriginalXY;
				if (!StoredPolygon(Part,Stored,OriginalXY,Cell.Bounds.W-Cell.Bounds.Y,MaximumRadius,Error,&Boundary)) return false;
				TConstrainedDelaunay2<double> CDT; CDT.FillRule=TConstrainedDelaunay2<double>::EFillRule::Positive; CDT.bOutputCCW=true;
				CDT.Add(Stored); const auto StoredBoundary=CDT.Vertices;
				if (!CDT.Triangulate() || CDT.Triangles.IsEmpty() || CDT.Vertices!=StoredBoundary || CDT.AddedVerticesStartIndex>=0)
				{ Error=TEXT("Native constructed body triangulation failed."); return false; }
				double Area=0;
				for (const auto& T : CDT.Triangles)
				{
					const double TwiceArea=FVector2D::CrossProduct(CDT.Vertices[T.B]-CDT.Vertices[T.A],CDT.Vertices[T.C]-CDT.Vertices[T.A]);
					if (!FMath::IsFinite(TwiceArea) || TwiceArea<=0)
					{ Error=TEXT("Native constructed body triangulation returned a reversed or empty face."); return false; }
					Area+=TwiceArea*.5;
					const int32 Indices[3]={T.A,T.B,T.C}; FVector P[3];
					for (int32 I=0; I<3; ++I)
					{
						const FVector2D Local=CDT.Vertices[Indices[I]];
						const FVector2D* SourceXY=OriginalXY.Find(Local); double Z;
						if (!SourceXY || !ProjectWorldTerrainSurface::Height(Plane,*SourceXY,Z,Error)) return false;
						const FVector Source(SourceXY->X,SourceXY->Y,Z);
						const double Evaluation=64*std::numeric_limits<double>::epsilon()*(FMath::Abs(Z)+FMath::Abs(Plane.Points[0].Z)+
							FMath::Abs(DX*(Source.X-Plane.Points[0].X))+FMath::Abs(DY*(Source.Y-Plane.Points[0].Y)));
						double StoredZ=static_cast<float>((Z-Bundle.HeightOriginMeters)*100-CandidateOrigin.Z);
						if (const auto* Previous=HeightBindings.Find(Local))
						{
							const double Bound=FMath::Max(Gradient,Previous->Gradient)*FVector2D(Source-Previous->Source).Length()+Evaluation+Previous->Roundoff;
							if (!FMath::IsFinite(Bound) || FMath::Abs(Z-Previous->Source.Z)>Bound)
							{
								Error=FString::Printf(TEXT("Constructed body source-height alias: delta %.17g, bound %.17g, XY [%.17g,%.17g], source [%.17g,%.17g,%.17g], previous [%.17g,%.17g,%.17g]."),
									FMath::Abs(Z-Previous->Source.Z),Bound,Local.X,Local.Y,Source.X,Source.Y,Source.Z,
									Previous->Source.X,Previous->Source.Y,Previous->Source.Z); return false;
							}
							StoredZ=Previous->StoredZ;
						}
						else
						{
							const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,FVector(Local,0)+CandidateOrigin); double Height;
							if (!ProjectWorldConstructedSurface::Sample(Field,FVector2D(Canonical),Height,Error)) return false;
							StoredZ=static_cast<float>((Height-Bundle.HeightOriginMeters)*100-CandidateOrigin.Z);
							if (!FMath::IsFinite(StoredZ)) { Error=TEXT("Constructed body height exceeds finite native float storage."); return false; }
							HeightBindings.Add(Local,{Source,StoredZ,Gradient,Evaluation});
						}
						P[I]=FVector(Local.X,Local.Y,StoredZ);
						const FVector Actual(P[I].X*.01,(Cell.Bounds.W-Cell.Bounds.Y)-P[I].Y*.01,
							(P[I].Z+CandidateOrigin.Z)*.01+Bundle.HeightOriginMeters); double Expected;
						if (!ProjectWorldTerrainSurface::Height(Plane,FVector2D(Actual),Expected,Error)) return false;
						if (FMath::Abs(Actual.Z-Expected)>MaximumJoinStepMeters)
						{ Error=TEXT("Constructed body's stored height exceeds the explicit source-field agreement limit."); return false; }
					}
					FTriangleID Emitted;
					if (!Emit(Candidate,P,Error,OutSourceTriangleIndices ? &Emitted : nullptr)) return false;
					if (OutSourceTriangleIndices) SourceByTriangle.Add(Emitted,SourceIndex);
				}
				const double Roundoff=1024*std::numeric_limits<double>::epsilon()*FMath::Max(1.0,Area)*CDT.Triangles.Num();
				if (FMath::Abs(Area-Stored.SignedArea())>Roundoff)
				{ Error=TEXT("Native constructed body triangulation does not cover its complete stored polygon."); return false; }
			}
		}
		TArray<FProjectWorldSurfaceTriangle> StoredBody;
		const auto Positions=Attributes.GetVertexPositions();
		for (const auto Face : Candidate.Triangles().GetElementIDs())
		{
			FProjectWorldSurfaceTriangle T;
			for (int32 I=0; I<3; ++I) T.Points[I]=FVector(Positions[Candidate.GetVertexInstanceVertex(Candidate.GetTriangleVertexInstance(Face,I))]);
			StoredBody.Add(T);
		}
		if (!ValidateStored(StoredBody,MaximumJoinStepMeters*100,Error)) return false;
		auto Centimeters = [&](const TArray<FGeneralPolygon2d>& Input)
		{
			TArray<FGeneralPolygon2d> Result;
			auto Ring = [&](const FPolygon2d& R,bool Hole)
			{
				TArray<FVector2D> V;
				for (const auto& P : R.GetVertices()) V.Add(FVector2D(P.X*100,(Cell.Bounds.W-Cell.Bounds.Y-P.Y)*100));
				FPolygon2d OutRing(V); if (OutRing.IsClockwise()!=Hole) OutRing.Reverse(); return OutRing;
			};
			for (const auto& P : Input)
			{
				FGeneralPolygon2d OutPolygon(Ring(P.GetOuter(),false));
				for (const auto& Hole : P.GetHoles()) if (!OutPolygon.AddHole(Ring(Hole,true))) return TArray<FGeneralPolygon2d>();
				Result.Add(MoveTemp(OutPolygon));
			}
			return Result;
		};
		TArray<FGeneralPolygon2d> ClippedFootprint,Expected;
		if (!ProjectWorldConstructedOutline::Extract(ClippedSourcePieces,ClippedFootprint,Error))
		{ Error=TEXT("Complete constructed source cell footprint cannot be read: ")+Error; return false; }
		Expected=Centimeters(ClippedFootprint);
		if (Expected.Num()!=ClippedFootprint.Num()) { Error=TEXT("Constructed coverage frame conversion loses holes."); return false; }
		ProjectWorldConstructedMeshCoverage::FMetrics Coverage;
		if (!ProjectWorldConstructedMeshCoverage::Validate(Expected,StoredBody,MaximumRadius,Coverage,Error)) return false;
		TArray<int32> SourceOrder;
		if (OutSourceTriangleIndices)
		{
			SourceOrder.Reserve(Candidate.Triangles().Num());
			for (const auto Triangle : Candidate.Triangles().GetElementIDs())
			{
				const int32* Source=SourceByTriangle.Find(Triangle);
				if (!Source || !Field.IsValidIndex(*Source))
				{ Error=TEXT("Constructed output has no original source-triangle provenance."); return false; }
				SourceOrder.Add(*Source);
			}
		}
		Out=MoveTemp(Candidate); Origin=CandidateOrigin;
		if (OutSourceTriangleIndices) *OutSourceTriangleIndices=MoveTemp(SourceOrder);
		if (OutMaximumXYDisplacementCentimeters) *OutMaximumXYDisplacementCentimeters=MaximumRadius;
		return true;
	}
}
