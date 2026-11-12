// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedNeighborhoodInput.h"
#include "ProjectWorldConstructedNeighborhoodPartition.h"
#include "ProjectWorldConstructedSurface.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedNeighborhood
{
	TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out; for (const double V : Values) Out.Add(MakeShared<FJsonValueNumber>(V)); return Out;
	}
	TArray<TSharedPtr<FJsonValue>> JsonField(const TArray<FProjectWorldSurfaceTriangle>& Field)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const auto& Face : Field)
		{
			TArray<TSharedPtr<FJsonValue>> Points;
			for (const auto& P : Face.Points) Points.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
			Out.Add(MakeShared<FJsonValueArray>(Points));
		}
		return Out;
	}
	bool SmallScene(FScene& Out,FAutomationTestBase& Test,FString& Error)
	{
		Out=FScene(); Out.Frame.CanonicalCrs=TEXT("EPSG:32618"); Out.Frame.VerticalDatum=TEXT("EPSG:3855");
		FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("cell"); Cell.Bounds=FVector4(0,0,8,4); Out.Frame.Cells.Add(Cell);
		const TArray<FProjectWorldSurfaceTriangle> Body={{{FVector(0,0,1.25),FVector(4,0,1.25),FVector(0,4,1.25)}},
			{{FVector(4,0,1.25),FVector(4,4,1.25),FVector(0,4,1.25)}}};
		const TArray<FProjectWorldSurfaceTriangle> Land={{{FVector(4,0,1.25),FVector(8,0,1.25),FVector(4,4,1.25)}},
			{{FVector(8,0,1.25),FVector(8,4,1.25),FVector(4,4,1.25)}}};
		auto Surfaces=MakeShared<FJsonObject>(); Surfaces->SetArrayField(TEXT("body_field"),JsonField(Body)); Surfaces->SetArrayField(TEXT("land_field"),JsonField(Land));
		auto Manifest=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Groups;
		for (const auto& Pair : {TPair<FString,FString>(TEXT("body"),TEXT("constructed_body")),TPair<FString,FString>(TEXT("land"),TEXT("inferred_ground"))})
		{
			auto Group=MakeShared<FJsonObject>(); Group->SetStringField(TEXT("id"),Pair.Key); Group->SetStringField(TEXT("role"),Pair.Value);
			Group->SetArrayField(TEXT("source_surfaces"),{MakeShared<FJsonValueString>(Pair.Key+TEXT("_field"))});
			Group->SetArrayField(TEXT("required_cells"),{MakeShared<FJsonValueString>(TEXT("cell"))}); Group->SetNumberField(TEXT("source_triangle_count"),2);
			Groups.Add(MakeShared<FJsonValueObject>(Group));
		}
		Manifest->SetArrayField(TEXT("required_groups"),Groups);
		if (!Test.TestTrue(TEXT("Small distinct owner groups bind every source and canonical cell."),ParseGroups(*Manifest,*Surfaces,Out.Frame,Out.Groups,Error))) return false;
		TArray<FGroup> Refused; auto Missing=Groups; Missing.Pop(); Manifest->SetArrayField(TEXT("required_groups"),Missing);
		Test.TestFalse(TEXT("Omitting an owner cannot omit its required source field."),ParseGroups(*Manifest,*Surfaces,Out.Frame,Refused,Error));
		Manifest->SetArrayField(TEXT("required_groups"),Groups); Groups[1]->AsObject()->SetStringField(TEXT("id"),TEXT("body"));
		Test.TestFalse(TEXT("Repeated owner identities cannot alias source ordinals."),ParseGroups(*Manifest,*Surfaces,Out.Frame,Refused,Error));
		Groups[1]->AsObject()->SetStringField(TEXT("id"),TEXT("land")); Groups[1]->AsObject()->SetStringField(TEXT("role"),TEXT("unknown"));
		Test.TestFalse(TEXT("Unknown owner roles refuse instead of being treated as Ground."),ParseGroups(*Manifest,*Surfaces,Out.Frame,Refused,Error));
		Test.TestTrue(TEXT("Group refusal exposes no partial source authority."),Refused.IsEmpty());
		Out.BodyAxis=FVector2D(1,0); Out.BodyQuality=MakeShared<FJsonObject>();
		Out.BodyQuality->SetStringField(TEXT("model"),TEXT("native_body_role_quality_v1"));
		auto Region=MakeShared<FJsonObject>(); Region->SetStringField(TEXT("role"),TEXT("fixed_pier"));
		Region->SetNumberField(TEXT("maximum_gradient"),.02); Region->SetArrayField(TEXT("source_triangle_indices"),Numbers({0,1}));
		Out.BodyQuality->SetArrayField(TEXT("regions"),{MakeShared<FJsonValueObject>(Region)}); return true;
	}
	bool Build(const FScene& Scene,TArray<FUnit>& Out,FExpectedOutputs& Expected,double& MaximumOrdinaryNorm,FString& Error)
	{
		Out.Reset(); Expected.Reset(); MaximumOrdinaryNorm=0; TArray<FUnit> Candidate; FExpectedOutputs Built;
		auto Add = [&](FUnit Unit)
		{
			if (!CaptureBuildOutput(Unit,Built,Error)) return false;
			Candidate.Add(MoveTemp(Unit)); return true;
		};
		for (const auto& Group : Scene.Groups)
		{
			if (Group.Role==TEXT("constructed_body") && !Scene.Body.Units.IsEmpty())
			{
				for (int32 I=0; I<Scene.Body.Units.Num(); ++I)
					if (!Add({Group.Id,Scene.BodyCells[I],Scene.Body.Units[I],Scene.Body.UnitSources[I]})) return false;
				continue;
			}
			for (const auto& CellId : Group.RequiredCells)
			{
				const auto* Cell=Scene.Frame.Cells.FindByPredicate([&](const auto& C){ return C.CellId==CellId; });
				if (!Cell) { Error=TEXT("Required group cell cannot be realized."); return false; }
				FMeshDescription Mesh; FUnit Unit; Unit.GroupId=Group.Id; Unit.CellId=CellId;
				if (!ProjectWorldConstructedMesh::Build(Scene.Frame,*Cell,Group.Field,.001,Mesh,Unit.Surface.Origin,Error,&Unit.Sources))
				{ Error=Group.Id+TEXT("/")+CellId+TEXT(": ")+Error; return false; }
				const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
				for (const auto Id : Mesh.Triangles().GetElementIDs())
				{
					FProjectWorldSurfaceTriangle Face;
					for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id,I))]);
					const auto U=Face.Points[1]-Face.Points[0],V=Face.Points[2]-Face.Points[0]; const double D=U.X*V.Y-U.Y*V.X;
					const double Norm=FVector2D((U.Z*V.Y-U.Y*V.Z)/D,(U.X*V.Z-U.Z*V.X)/D).Length();
					if (!FMath::IsFinite(Norm) || Norm>.02) { Error=Group.Id+TEXT(": actual ordinary stored gradient exceeds 0.02."); return false; }
					MaximumOrdinaryNorm=FMath::Max(MaximumOrdinaryNorm,Norm); Unit.Surface.Triangles.Add(Face);
				}
				if (!Add(MoveTemp(Unit))) return false;
			}
		}
		if (!ValidateCensus(Scene,Candidate,Built,Error)) return false;
		TArray<FProjectWorldSurfaceTriangle> Complete;
		for (const auto& Unit : Candidate)
		{
			TArray<FProjectWorldSurfaceTriangle> Converted;
			if (!ProjectWorldStoredSurface::Reframe(Unit.Surface,Candidate[0].Surface.Origin,Converted,Error)) return false;
			Complete.Append(Converted);
		}
		if (!ProjectWorldConstructedMesh::ValidateStored(Complete,.1,Error)) return false;
		Out=MoveTemp(Candidate); Expected=MoveTemp(Built); return true;
	}
	bool Save(const FScene& Scene,const TArray<FUnit>& Units,double MaximumNorm,const TSharedPtr<FJsonObject>& PartitionReceipt,FString& Error)
	{
		if (Scene.OutputPath.IsEmpty()) return true;
		if (!FPaths::IsUnderDirectory(Scene.OutputPath,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) || FPaths::FileExists(Scene.OutputPath))
		{ Error=TEXT("Grouped evidence requires a fresh project-scratch output."); return false; }
		auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("physical_owner"),TEXT("grouped_neighborhood_diagnostic_v1"));
		Root->SetStringField(TEXT("manifest_sha256"),Scene.Hash); Root->SetStringField(TEXT("source_geometry_sha256"),Scene.GeometryHash);
		Root->SetNumberField(TEXT("maximum_stored_ordinary_gradient"),MaximumNorm);
		if (PartitionReceipt.IsValid()) Root->SetObjectField(TEXT("shared_partition_receipt"),PartitionReceipt);
		Root->SetStringField(TEXT("scope"),TEXT("Actual F32 grouped scene and cross-owner relationships only; no Road width, gameplay, streaming, survey or production admission claim."));
		TArray<TSharedPtr<FJsonValue>> JsonUnits;
		for (const auto& Unit : Units)
		{
			auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("group_id"),Unit.GroupId); Row->SetStringField(TEXT("cell_id"),Unit.CellId);
			const auto& O=Unit.Surface.Origin; Row->SetArrayField(TEXT("origin_cm"),Numbers({O.X,O.Y,O.Z}));
			TArray<TSharedPtr<FJsonValue>> Vertices,Faces,Sources; int32 Index=0;
			for (const auto& Face : Unit.Surface.Triangles)
			{
				for (const auto& P : Face.Points) Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
				Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(Index),double(Index+1),double(Index+2)}))); Index+=3;
			}
			for (const int32 Source : Unit.Sources) Sources.Add(MakeShared<FJsonValueNumber>(Source));
			Row->SetArrayField(TEXT("vertices_cm"),Vertices); Row->SetArrayField(TEXT("triangles"),Faces); Row->SetArrayField(TEXT("source_triangle_ordinals"),Sources);
			JsonUnits.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("native_physical_units"),JsonUnits); FString Text;
		if (!FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Scene.OutputPath))
		{ Error=TEXT("Grouped native output could not be written."); return false; }
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedNeighborhoodTest,
	"Project.World.Realization.Constructed.NeighborhoodSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedNeighborhoodTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedNeighborhood;
	ProjectWorldConstructedNeighborhood::FScene Scene; FString Error; const FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_NEIGHBORHOOD_MANIFEST"));
	if (Path.IsEmpty()) { if (!SmallScene(Scene,*this,Error)) { AddError(Error); return false; } }
	else if (!TestTrue(TEXT("The complete scene uses its exact frozen source/cell/owner manifest."),
		Load(Path,FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_NEIGHBORHOOD_SHA256")),Scene,Error))) { AddError(Error); return false; }
	TArray<FUnit> Units; FExpectedOutputs Expected; double MaximumNorm; TSharedPtr<FJsonObject> PartitionReceipt;
	if (Scene.SharedPartition)
	{
		FPartitionOutput Partition;
		if (!TestTrue(TEXT("The shared physical partition preserves all source owners, actual roles and original frames."),BuildSharedPartition(Scene,Partition,Error))) { AddError(Error); return false; }
		Units=MoveTemp(Partition.Units); Expected=MoveTemp(Partition.Expected); MaximumNorm=Partition.MaximumOrdinaryNorm; PartitionReceipt=MoveTemp(Partition.Receipt);
	}
	else if (!TestTrue(TEXT("Every required source owner realizes in its original frame with exact cross-owner stored relationships."),Build(Scene,Units,Expected,MaximumNorm,Error))) { AddError(Error); return false; }
	auto Missing=Units; Missing.Pop();
	TestFalse(TEXT("Omitting actual owner output refuses the full-scene census."),ValidateCensus(Scene,Missing,Expected,Error));
	auto Partial=Units; Partial[0].Surface.Triangles.Pop(); Partial[0].Sources.Pop();
	TestTrue(TEXT("The partial-body control retains a positive body and every required owner/cell."),!Partial[0].Surface.Triangles.IsEmpty() && Partial.Num()==Units.Num());
	TestFalse(TEXT("Removing one positive face cannot pass while its group remains present."),ValidateCensus(Scene,Partial,Expected,Error));
	auto Wrong=Units; const auto* FirstGroup=Scene.Groups.FindByPredicate([&](const auto& G){ return G.Id==Wrong[0].GroupId; });
	Wrong[0].Sources[0]=(Wrong[0].Sources[0]+1)%FirstGroup->Field.Num();
	TestFalse(TEXT("A wrong in-range source ordinal cannot authenticate an actual stored face."),ValidateCensus(Scene,Wrong,Expected,Error));
	Wrong=Units;
	Wrong[0].Sources[0]=FirstGroup->Field.Num();
	TestFalse(TEXT("Native source ordinals cannot escape their declared group."),ValidateCensus(Scene,Wrong,Expected,Error));
	const auto* Other=Scene.Groups.FindByPredicate([&](const auto& G){ return G.Id!=Units[0].GroupId && G.RequiredCells.Contains(Units[0].CellId); });
	if (!TestNotNull(TEXT("The wrong-group control has another owner in the same cell."),Other)) return false;
	Wrong=Units; Wrong[0].GroupId=Other->Id; Wrong[0].Sources.Init(0,Wrong[0].Surface.Triangles.Num());
	TestFalse(TEXT("A numerically valid ordinal from another group cannot authenticate this geometry."),ValidateCensus(Scene,Wrong,Expected,Error));
	TestTrue(TEXT("Wrong-group refusal measures actual source support."),Error.Contains(TEXT("matching declared source support")));
	if (Path.IsEmpty())
	{
		auto Shared=Scene; Shared.SharedPartition=true; FPartitionOutput Control;
		if (!TestTrue(TEXT("Small shared storage preserves both owners and fresh body roles."),BuildSharedPartition(Shared,Control,Error))) { AddError(Error); return false; }
		TestEqual(TEXT("Small shared storage retains both required owner/cell outputs."),Control.Units.Num(),2);
		Shared.BodyQuality.Reset();
		TestFalse(TEXT("Missing body policy cannot reuse earlier body quality in shared storage."),BuildSharedPartition(Shared,Control,Error));
		TestTrue(TEXT("Shared policy refusal exposes no partial native outputs or expectations."),Control.Units.IsEmpty() && Control.Expected.IsEmpty());
		Shared.BodyQuality=Scene.BodyQuality; const auto Region=Shared.BodyQuality->GetArrayField(TEXT("regions"))[0]->AsObject();
		Region->SetNumberField(TEXT("maximum_gradient"),.03);
		TestFalse(TEXT("Shared storage cannot loosen the existing ordinary body limit."),BuildSharedPartition(Shared,Control,Error));
		Region->SetNumberField(TEXT("maximum_gradient"),.02);
		auto Sloped = [&](const FVector2D& Gradient)
		{
			auto Result=Scene; Result.SharedPartition=true;
			for (auto& Group : Result.Groups) for (auto& Face : Group.Field) for (auto& P : Face.Points)
				P.Z=1.25+Gradient.X*P.X+Gradient.Y*P.Y;
			return Result;
		};
		auto Steep=Sloped(FVector2D(.09,0));
		TestFalse(TEXT("Actual source-consistent fixed-deck geometry above 2 percent refuses in the shared role gate."),BuildSharedPartition(Steep,Control,Error));
		TestTrue(TEXT("The fixed-deck counterexample reaches actual role quality rather than an unrelated constructor failure."),Error.Contains(TEXT("Shared physical role quality refuses")) && Error.Contains(TEXT("role=fixed_pier")));
		TestTrue(TEXT("Actual fixed-deck refusal exposes no partial output."),Control.Units.IsEmpty() && Control.Expected.IsEmpty());
		for (const auto& Gradient : {FVector2D(.13,0),FVector2D(0,.03)})
		{
			Steep=Sloped(Gradient); Steep.BodyQuality=MakeShared<FJsonObject>();
			Steep.BodyQuality->SetStringField(TEXT("model"),TEXT("native_body_role_quality_v1"));
			auto Gangway=MakeShared<FJsonObject>(); Gangway->SetStringField(TEXT("role"),TEXT("gangway"));
			Gangway->SetNumberField(TEXT("maximum_grade"),.125); Gangway->SetNumberField(TEXT("maximum_cross_slope"),.02);
			Gangway->SetArrayField(TEXT("source_triangle_indices"),Numbers({0,1}));
			Steep.BodyQuality->SetArrayField(TEXT("regions"),{MakeShared<FJsonValueObject>(Gangway)});
			TestFalse(TEXT("Actual source-consistent gangway grade/cross violations refuse in shared storage."),BuildSharedPartition(Steep,Control,Error));
			TestTrue(TEXT("The gangway counterexample reaches its actual role-quality branch."),Error.Contains(TEXT("Shared physical role quality refuses")) && Error.Contains(TEXT("role=gangway")));
			TestTrue(TEXT("Actual gangway refusal exposes no partial output."),Control.Units.IsEmpty() && Control.Expected.IsEmpty());
		}
	}
	if (!TestTrue(TEXT("Only fully qualified grouped native output is published to fresh scratch."),Save(Scene,Units,MaximumNorm,PartitionReceipt,Error))) { AddError(Error); return false; }
	int32 Faces=0; for (const auto& Unit : Units) Faces+=Unit.Surface.Triangles.Num();
	AddInfo(FString::Printf(TEXT("GroupedNeighborhoodNative groups=%d units=%d faces=%d maximum_ordinary_norm=%.17g manifest=%s; not full-width Road/gameplay/streaming/production acceptance"),Scene.Groups.Num(),Units.Num(),Faces,MaximumNorm,*Scene.Hash));
	return !HasAnyErrors();
}
#endif
