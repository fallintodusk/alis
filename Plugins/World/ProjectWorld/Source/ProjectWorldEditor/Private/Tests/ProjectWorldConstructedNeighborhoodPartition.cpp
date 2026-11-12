// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedNeighborhoodPartition.h"
#include "ProjectWorldConstructedMeshCoverage.h"
#include "ProjectWorldConstructedOutline.h"
#include "Dom/JsonObject.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedNeighborhood
{
	using namespace UE::Geometry;
	struct FSlot { FString Role; double Norm=0,Grade=0,Cross=0; };
	struct FMetrics { int32 Faces=0; double Norm=0,Grade=0,Cross=0; };
	bool BodySlots(const FScene& Scene,const FGroup& Body,TArray<FSlot>& Out,FString& Error)
	{
		Out.Reset(); FString Model; const TArray<TSharedPtr<FJsonValue>>* Regions=nullptr;
		if (!Scene.BodyQuality.IsValid() || !Scene.BodyQuality->TryGetStringField(TEXT("model"),Model) || Model!=TEXT("native_body_role_quality_v1") ||
			!Scene.BodyQuality->TryGetArrayField(TEXT("regions"),Regions) || !Regions || Regions->IsEmpty() || Regions->Num()>3 ||
			Scene.BodyAxis.ContainsNaN() || !FMath::IsFinite(Scene.BodyAxis.Length()) || Scene.BodyAxis.Length()==0)
		{ Error=TEXT("Shared partition requires the exact existing body source-role policy and finite axis."); return false; }
		TArray<FSlot> Slots; Slots.SetNum(Body.Field.Num()); TSet<FString> Roles;
		for (const auto& Value : *Regions)
		{
			if (!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Body role region is invalid."); return false; }
			const auto Object=Value->AsObject(); FSlot Slot; const TArray<TSharedPtr<FJsonValue>>* Indices=nullptr;
			if (!Object->TryGetStringField(TEXT("role"),Slot.Role) || Roles.Contains(Slot.Role) ||
				!Object->TryGetArrayField(TEXT("source_triangle_indices"),Indices) || !Indices || Indices->IsEmpty())
			{ Error=TEXT("Body role regions are repeated or incomplete."); return false; }
			if (Slot.Role==TEXT("gangway"))
			{
				if (!Object->TryGetNumberField(TEXT("maximum_grade"),Slot.Grade) || !FMath::IsFinite(Slot.Grade) || Slot.Grade<0 || Slot.Grade>.125 ||
					!Object->TryGetNumberField(TEXT("maximum_cross_slope"),Slot.Cross) || !FMath::IsFinite(Slot.Cross) || Slot.Cross<0 || Slot.Cross>.02)
				{ Error=TEXT("Shared partition cannot weaken the existing gangway limits."); return false; }
			}
			else if ((Slot.Role!=TEXT("fixed_pier") && Slot.Role!=TEXT("floating_deck")) ||
				!Object->TryGetNumberField(TEXT("maximum_gradient"),Slot.Norm) || !FMath::IsFinite(Slot.Norm) || Slot.Norm<0 || Slot.Norm>.02)
			{ Error=TEXT("Shared partition cannot reinterpret or weaken ordinary body limits."); return false; }
			for (const auto& Index : *Indices)
			{
				if (!Index.IsValid() || Index->Type!=EJson::Number || !FMath::IsFinite(Index->AsNumber()) || Index->AsNumber()<0 ||
					Index->AsNumber()>=Slots.Num() || Index->AsNumber()!=FMath::FloorToDouble(Index->AsNumber()))
				{ Error=TEXT("Body role policy source ordinal is invalid."); return false; }
				const int32 I=static_cast<int32>(Index->AsNumber());
				if (!Slots[I].Role.IsEmpty()) { Error=TEXT("Body role policy repeats a source face."); return false; }
				Slots[I]=Slot;
			}
			Roles.Add(Slot.Role);
		}
		for (const auto& Slot : Slots) if (Slot.Role.IsEmpty()) { Error=TEXT("Body role policy omits a source face."); return false; }
		Out=MoveTemp(Slots); return true;
	}
	bool OwnerCoverage(const FGroup& Group,const FProjectWorldCanonicalCell& Cell,
		const FUnit& Unit,double Radius,TSharedPtr<FJsonObject>& Receipt,FString& Error)
	{
		const FVector2D Anchor(Cell.Bounds.X,Cell.Bounds.Y);
		const FAxisAlignedBox2d Bounds(FVector2D::ZeroVector,FVector2D(Cell.Bounds.Z-Cell.Bounds.X,Cell.Bounds.W-Cell.Bounds.Y));
		TArray<FGeneralPolygon2d> Pieces,Outline,Expected;
		for (const auto& Face : Group.Field)
		{
			FPolygon2d Polygon({FVector2D(Face.Points[0])-Anchor,FVector2D(Face.Points[1])-Anchor,FVector2D(Face.Points[2])-Anchor});
			if (Polygon.IsClockwise()) Polygon.Reverse(); Polygon.ClipConvex(Bounds);
			const auto Result=ProjectWorldConstructedMeshCoverage::ClassifySourceClip(Polygon,Error);
			if (Result==ProjectWorldConstructedMeshCoverage::ESourceClip::Invalid) return false;
			if (Result==ProjectWorldConstructedMeshCoverage::ESourceClip::Area) Pieces.Add(FGeneralPolygon2d(MoveTemp(Polygon)));
		}
		if (!ProjectWorldConstructedOutline::Extract(Pieces,Outline,Error)) return false;
		auto Ring = [&](const FPolygon2d& Input,bool Hole)
		{
			TArray<FVector2D> Points; for (const auto& P : Input.GetVertices()) Points.Add(FVector2D(P.X*100,(Cell.Bounds.W-Cell.Bounds.Y-P.Y)*100));
			FPolygon2d Out(Points); if (Out.IsClockwise()!=Hole) Out.Reverse(); return Out;
		};
		for (const auto& Polygon : Outline)
		{
			FGeneralPolygon2d Converted(Ring(Polygon.GetOuter(),false));
			for (const auto& Hole : Polygon.GetHoles()) if (!Converted.AddHole(Ring(Hole,true))) { Error=TEXT("Owner coverage frame conversion lost a hole."); return false; }
			Expected.Add(MoveTemp(Converted));
		}
		ProjectWorldConstructedMeshCoverage::FMetrics Metrics;
		if (!ProjectWorldConstructedMeshCoverage::Validate(Expected,Unit.Surface.Triangles,Radius,Metrics,Error)) return false;
		Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("group_id"),Group.Id); Receipt->SetStringField(TEXT("cell_id"),Cell.CellId);
		Receipt->SetNumberField(TEXT("faces"),Unit.Surface.Triangles.Num()); Receipt->SetNumberField(TEXT("maximum_storage_displacement_cm"),Radius);
		Receipt->SetNumberField(TEXT("source_area_cm2"),Metrics.OriginalArea); Receipt->SetNumberField(TEXT("stored_area_cm2"),Metrics.StoredArea);
		Receipt->SetNumberField(TEXT("missing_area_cm2"),Metrics.MissingArea); Receipt->SetNumberField(TEXT("exterior_area_cm2"),Metrics.ExteriorArea);
		Receipt->SetNumberField(TEXT("conversion_bound_cm2"),Metrics.ConversionBound); Receipt->SetNumberField(TEXT("arithmetic_bound_cm2"),Metrics.ArithmeticBound);
		return true;
	}
	bool BuildSharedPartition(const FScene& Scene,FPartitionOutput& Out,FString& Error)
	{
		Out=FPartitionOutput(); Error.Reset(); FPartitionOutput Candidate; Candidate.Receipt=MakeShared<FJsonObject>();
		const int32 BodyIndex=Scene.Groups.IndexOfByPredicate([](const auto& G){ return G.Role==TEXT("constructed_body"); }); TArray<FSlot> Slots;
		if (!Scene.SharedPartition || !Scene.Groups.IsValidIndex(BodyIndex) || !BodySlots(Scene,Scene.Groups[BodyIndex],Slots,Error)) return false;
		const FVector2D Along=Scene.BodyAxis.GetSafeNormal(0.0),Across(-Along.Y,Along.X);
		TMap<FString,FMetrics> Measurements; TSet<int32> EmittedBody; TArray<TSharedPtr<FJsonValue>> Coverage,Cells;
		for (const auto& Cell : Scene.Frame.Cells)
		{
			struct FReference { int32 Group,Source; }; TArray<FProjectWorldSurfaceTriangle> Field; TArray<FReference> References;
			for (int32 G=0; G<Scene.Groups.Num(); ++G) for (int32 I=0; I<Scene.Groups[G].Field.Num(); ++I)
			{
				const auto& Face=Scene.Groups[G].Field[I]; FBox2D Box(ForceInit); for (const auto& P : Face.Points) Box+=FVector2D(P);
				if (Box.Max.X<=Cell.Bounds.X || Box.Min.X>=Cell.Bounds.Z || Box.Max.Y<=Cell.Bounds.Y || Box.Min.Y>=Cell.Bounds.W) continue;
				Field.Add(Face); References.Add({G,I});
			}
			if (Field.IsEmpty() || Field.Num()>4096) { Error=TEXT("Shared physical context exceeds the unchanged source-field budget."); return false; }
			FMeshDescription Mesh; FVector Origin; TArray<int32> Trace; double Radius;
			if (!ProjectWorldConstructedMesh::Build(Scene.Frame,Cell,Field,.001,Mesh,Origin,Error,&Trace,&Radius))
			{ Error=Cell.CellId+TEXT(" shared physical partition: ")+Error; return false; }
			TArray<FUnit> Split; TMap<int32,int32> ByGroup; TArray<FIntPoint> Rejoin;
			for (int32 G=0; G<Scene.Groups.Num(); ++G) if (Scene.Groups[G].RequiredCells.Contains(Cell.CellId))
			{ ByGroup.Add(G,Split.Num()); Split.Add({Scene.Groups[G].Id,Cell.CellId,{Origin,{}},{}}); }
			const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions(); int32 Ordinal=0;
			for (const auto Id : Mesh.Triangles().GetElementIDs())
			{
				if (!Trace.IsValidIndex(Ordinal) || !References.IsValidIndex(Trace[Ordinal])) { Error=TEXT("Shared physical output lost its immutable source map."); return false; }
				const auto Ref=References[Trace[Ordinal++]]; const int32* UnitIndex=ByGroup.Find(Ref.Group);
				if (!UnitIndex) { Error=TEXT("Shared physical output invents an undeclared owner/cell contribution."); return false; }
				auto& Unit=Split[*UnitIndex]; FProjectWorldSurfaceTriangle Face;
				for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id,I))]);
				const auto U=Face.Points[1]-Face.Points[0],V=Face.Points[2]-Face.Points[0]; const double D=U.X*V.Y-U.Y*V.X;
				const FVector2D Gradient((U.Z*V.Y-U.Y*V.Z)/D,(U.X*V.Z-U.Z*V.X)/D);
				const double Norm=Gradient.Length(),Grade=FMath::Abs(FVector2D::DotProduct(Gradient,Along)),Cross=FMath::Abs(FVector2D::DotProduct(Gradient,Across));
				FSlot Slot; Slot.Role=Scene.Groups[Ref.Group].Role; Slot.Norm=.02;
				if (Ref.Group==BodyIndex) { Slot=Slots[Ref.Source]; EmittedBody.Add(Ref.Source); }
				const bool Accepted=Slot.Role==TEXT("gangway") ? Grade<=Slot.Grade && Cross<=Slot.Cross : Norm<=Slot.Norm;
				if (!FMath::IsFinite(Norm) || !FMath::IsFinite(Grade) || !FMath::IsFinite(Cross) || !Accepted)
				{ Error=FString::Printf(TEXT("Shared physical role quality refuses group=%s role=%s source=%d norm=%.17g grade=%.17g cross=%.17g"),*Unit.GroupId,*Slot.Role,Ref.Source,Norm,Grade,Cross); return false; }
				if (Slot.Role!=TEXT("gangway")) Candidate.MaximumOrdinaryNorm=FMath::Max(Candidate.MaximumOrdinaryNorm,Norm);
				auto& M=Measurements.FindOrAdd(Unit.GroupId+TEXT(":")+Slot.Role); ++M.Faces;
				M.Norm=FMath::Max(M.Norm,Norm); M.Grade=FMath::Max(M.Grade,Grade); M.Cross=FMath::Max(M.Cross,Cross);
				Rejoin.Add(FIntPoint(*UnitIndex,Unit.Surface.Triangles.Num())); Unit.Surface.Triangles.Add(Face); Unit.Sources.Add(Ref.Source);
			}
			if (Rejoin.Num()!=Mesh.Triangles().Num() || Trace.Num()!=Rejoin.Num()) { Error=TEXT("Splitting changes the complete output census."); return false; }
			Ordinal=0; TSet<uint64> Consumed;
			for (const auto Id : Mesh.Triangles().GetElementIDs())
			{
				const auto Location=Rejoin[Ordinal]; const auto& Unit=Split[Location.X]; const auto& Ref=References[Trace[Ordinal++]];
				const uint64 Key=(static_cast<uint64>(Location.X)<<32)|static_cast<uint32>(Location.Y);
				if (Consumed.Contains(Key) || Unit.GroupId!=Scene.Groups[Ref.Group].Id || Unit.Sources[Location.Y]!=Ref.Source)
				{ Error=TEXT("Splitting loses, duplicates or reassigns actual source provenance."); return false; }
				Consumed.Add(Key);
				for (int32 I=0; I<3; ++I) if (Unit.Surface.Triangles[Location.Y].Points[I]!=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id,I))]))
				{ Error=TEXT("Splitting changes actual ordered native XYZ."); return false; }
			}
			for (auto& Unit : Split)
			{
				const auto* Group=Scene.Groups.FindByPredicate([&](const auto& G){ return G.Id==Unit.GroupId; });
				if (!Group) { Error=TEXT("Split owner is absent from the frozen source manifest."); return false; }
				TSharedPtr<FJsonObject> Receipt;
				if (!OwnerCoverage(*Group,Cell,Unit,Radius,Receipt,Error)) { Error=Unit.GroupId+TEXT(" owner coverage: ")+Error; return false; }
				Coverage.Add(MakeShared<FJsonValueObject>(Receipt));
				if (!CaptureBuildOutput(Unit,Candidate.Expected,Error)) return false;
				Candidate.Units.Add(MoveTemp(Unit));
			}
			auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("cell_id"),Cell.CellId); Receipt->SetNumberField(TEXT("source_context_faces"),Field.Num());
			Receipt->SetNumberField(TEXT("stored_faces"),Mesh.Triangles().Num()); Receipt->SetNumberField(TEXT("maximum_xy_displacement_cm"),Radius);
			Cells.Add(MakeShared<FJsonValueObject>(Receipt));
		}
		for (int32 I=0; I<Slots.Num(); ++I)
		{
			if (!Measurements.Contains(Scene.Groups[BodyIndex].Id+TEXT(":")+Slots[I].Role) || (Slots[I].Role==TEXT("gangway") && !EmittedBody.Contains(I)))
			{ Error=TEXT("Fresh shared body output omits a required role or connector source."); return false; }
		}
		if (!ValidateCensus(Scene,Candidate.Units,Candidate.Expected,Error)) return false;
		TArray<FProjectWorldSurfaceTriangle> Complete;
		for (const auto& Unit : Candidate.Units)
		{
			TArray<FProjectWorldSurfaceTriangle> Converted;
			if (!ProjectWorldStoredSurface::Reframe(Unit.Surface,Candidate.Units[0].Surface.Origin,Converted,Error)) return false;
			Complete.Append(Converted);
		}
		if (!ProjectWorldConstructedMesh::ValidateStored(Complete,.1,Error)) return false;
		TArray<TSharedPtr<FJsonValue>> Roles;
		for (const auto& Pair : Measurements)
		{
			auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner_role"),Pair.Key); Row->SetNumberField(TEXT("faces"),Pair.Value.Faces);
			Row->SetNumberField(TEXT("maximum_norm"),Pair.Value.Norm); Row->SetNumberField(TEXT("maximum_grade"),Pair.Value.Grade); Row->SetNumberField(TEXT("maximum_cross_slope"),Pair.Value.Cross);
			Roles.Add(MakeShared<FJsonValueObject>(Row));
		}
		Candidate.Receipt->SetArrayField(TEXT("actual_role_metrics"),Roles); Candidate.Receipt->SetArrayField(TEXT("per_owner_coverage"),Coverage);
		Candidate.Receipt->SetArrayField(TEXT("cell_partitions"),Cells); Out=MoveTemp(Candidate); return true;
	}
}
#endif
