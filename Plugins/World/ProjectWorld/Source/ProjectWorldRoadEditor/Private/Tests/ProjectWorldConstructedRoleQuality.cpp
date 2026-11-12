// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRoleQuality.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "Dom/JsonObject.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

namespace ProjectWorldConstructedRoleQuality
{
	bool Parse(const FJsonObject& Document,const FString& AssemblyHash,const TArray<FString>& SourceRoles,FPolicy& Out,FString& Error)
	{
		Out=FPolicy(); Error.Reset(); FString Model,Expected;
		const TArray<TSharedPtr<FJsonValue>>* Regions=nullptr;
		if (!Document.TryGetStringField(TEXT("model"),Model) || Model!=TEXT("native_body_role_quality_v1") ||
			!Document.TryGetStringField(TEXT("assembly_sha256"),Expected) || Expected!=AssemblyHash || AssemblyHash.Len()!=64 ||
			SourceRoles.IsEmpty() || SourceRoles.Num()>4096 || !Document.TryGetArrayField(TEXT("regions"),Regions) ||
			!Regions || Regions->IsEmpty() || Regions->Num()>3)
		{ Error=TEXT("Role quality needs the exact bounded source assembly and a supported policy."); return false; }
		FPolicy Candidate; Candidate.RegionBySource.Init(INDEX_NONE,SourceRoles.Num()); TSet<FString> Seen;
		for (const auto& Value : *Regions)
		{
			if (!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Role quality region is not an object."); return false; }
			const auto Object=Value->AsObject(); FRegion Region; const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;
			if (!Object->TryGetStringField(TEXT("role"),Region.Role) || Seen.Contains(Region.Role) ||
				!Object->TryGetArrayField(TEXT("source_triangle_indices"),Sources) || !Sources || Sources->IsEmpty())
			{ Error=TEXT("Role quality region has an unknown, repeated or empty source binding."); return false; }
			const bool Gangway=Region.Role==TEXT("gangway");
			if (Gangway)
			{
				if (!Object->TryGetNumberField(TEXT("maximum_grade"),Region.MaximumGrade) || !FMath::IsFinite(Region.MaximumGrade) ||
					Region.MaximumGrade<0 || Region.MaximumGrade>.125 || !Object->TryGetNumberField(TEXT("maximum_cross_slope"),Region.MaximumCrossSlope) ||
					!FMath::IsFinite(Region.MaximumCrossSlope) || Region.MaximumCrossSlope<0 || Region.MaximumCrossSlope>.02)
				{ Error=TEXT("Gangway role quality cannot weaken the frozen grade/cross limits."); return false; }
			}
			else if ((Region.Role!=TEXT("fixed_pier") && Region.Role!=TEXT("floating_deck")) ||
				!Object->TryGetNumberField(TEXT("maximum_gradient"),Region.MaximumNorm) || !FMath::IsFinite(Region.MaximumNorm) ||
				Region.MaximumNorm<0 || Region.MaximumNorm>.02)
			{ Error=TEXT("Ordinary role quality requires a frozen conservative gradient bound."); return false; }
			for (const auto& Source : *Sources)
			{
				if (!Source.IsValid() || Source->Type!=EJson::Number) { Error=TEXT("Role source ordinal is not numeric."); return false; }
				const double Number=Source->AsNumber();
				if (!FMath::IsFinite(Number) || Number<0 || Number>=SourceRoles.Num() || Number!=FMath::FloorToDouble(Number))
				{ Error=TEXT("Role source ordinal is outside the exact assembly."); return false; }
				const int32 Index=static_cast<int32>(Number);
				const FString ExpectedRole=Region.Role==TEXT("fixed_pier") ? TEXT("fixed_pier") : TEXT("floating_assembly");
				if (Candidate.RegionBySource[Index]!=INDEX_NONE || SourceRoles[Index]!=ExpectedRole)
				{ Error=TEXT("Role quality duplicates or contradicts a declared structural source."); return false; }
				Candidate.RegionBySource[Index]=Candidate.Regions.Num();
			}
			Seen.Add(Region.Role); Candidate.Regions.Add(MoveTemp(Region));
		}
		if (Candidate.RegionBySource.Contains(INDEX_NONE)) { Error=TEXT("Role quality omits an original source face."); return false; }
		Out=MoveTemp(Candidate); return true;
	}
	bool Load(const FJsonObject& Fixture,const FString& AssemblyHash,const TArray<FString>& SourceRoles,FPolicy& Out,FString& Error)
	{
		Out=FPolicy(); Error.Reset();
		if (!Fixture.HasField(TEXT("body_role_quality_path")) && !Fixture.HasField(TEXT("body_role_quality_sha256"))) return true;
		FString Path,Expected,Actual,Text; TSharedPtr<FJsonObject> Document;
		if (!Fixture.TryGetStringField(TEXT("body_role_quality_path"),Path) || !Fixture.TryGetStringField(TEXT("body_role_quality_sha256"),Expected) ||
			Expected.Len()!=64 || !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Expected ||
			!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Document))
		{ Error=TEXT("The exact source-role quality policy cannot be authenticated."); return false; }
		if (!Parse(*Document,AssemblyHash,SourceRoles,Out,Error)) return false;
		Out.Hash=Actual; return true;
	}
	bool Validate(const FPolicy& Policy,const FMeshDescription& Mesh,const TArray<int32>& SourceOrdinals,
		const FVector2D& AlongAxis,TArray<TSharedPtr<FJsonValue>>& Receipts,FString& Error)
	{
		Receipts.Reset(); Error.Reset();
		if (Policy.Regions.IsEmpty() || Policy.RegionBySource.IsEmpty() || Mesh.Triangles().Num()==0 || SourceOrdinals.Num()!=Mesh.Triangles().Num() ||
			AlongAxis.ContainsNaN() || !FMath::IsFinite(AlongAxis.Length()) || AlongAxis.Length()==0)
		{ Error=TEXT("Actual role quality requires complete source provenance and a finite declared axis."); return false; }
		struct FMetrics { int32 Faces=0; double Norm=0,Grade=0,Cross=0; };
		TArray<FMetrics> Metrics; Metrics.SetNum(Policy.Regions.Num()); FStaticMeshConstAttributes Attributes(Mesh);
		const auto Along=AlongAxis.GetSafeNormal(0.0); const FVector2D Across(-Along.Y,Along.X);
		int32 Ordinal=0,FirstFailure=INDEX_NONE,FailingSource=INDEX_NONE; FString FailingRole;
		for (const auto Triangle : Mesh.Triangles().GetElementIDs())
		{
			const int32 Source=SourceOrdinals[Ordinal];
			if (!Policy.RegionBySource.IsValidIndex(Source) || !Metrics.IsValidIndex(Policy.RegionBySource[Source]))
			{ Error=TEXT("Actual stored face has no unique source-role policy."); return false; }
			const int32 RegionIndex=Policy.RegionBySource[Source]; const auto& Region=Policy.Regions[RegionIndex]; auto& M=Metrics[RegionIndex];
			FProjectWorldSurfaceTriangle Face;
			for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Attributes.GetVertexPositions()[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Triangle,I))]);
			double Height;
			if (!ProjectWorldTerrainSurface::Height(Face,FVector2D(Face.Points[0]),Height,Error)) return false;
			const FVector U=Face.Points[1]-Face.Points[0],V=Face.Points[2]-Face.Points[0]; const double D=U.X*V.Y-U.Y*V.X;
			const FVector2D Gradient((U.Z*V.Y-U.Y*V.Z)/D,(U.X*V.Z-U.Z*V.X)/D);
			const double Norm=Gradient.Length(),Grade=FMath::Abs(FVector2D::DotProduct(Gradient,Along)),Cross=FMath::Abs(FVector2D::DotProduct(Gradient,Across));
			if (!FMath::IsFinite(Norm) || !FMath::IsFinite(Grade) || !FMath::IsFinite(Cross)) { Error=TEXT("Actual role gradient is nonfinite."); return false; }
			++M.Faces; M.Norm=FMath::Max(M.Norm,Norm); M.Grade=FMath::Max(M.Grade,Grade); M.Cross=FMath::Max(M.Cross,Cross);
			const bool Accepted=Region.Role==TEXT("gangway") ? Grade<=Region.MaximumGrade && Cross<=Region.MaximumCrossSlope : Norm<=Region.MaximumNorm;
			if (!Accepted && FirstFailure==INDEX_NONE) { FirstFailure=Ordinal; FailingSource=Source; FailingRole=Region.Role; }
			++Ordinal;
		}
		for (int32 I=0; I<Metrics.Num(); ++I)
		{
			const auto& M=Metrics[I]; auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"),Policy.Regions[I].Role); Row->SetNumberField(TEXT("face_count"),M.Faces);
			if (M.Faces>0) { Row->SetNumberField(TEXT("maximum_norm"),M.Norm); Row->SetNumberField(TEXT("maximum_grade"),M.Grade); Row->SetNumberField(TEXT("maximum_cross_slope"),M.Cross); }
			Receipts.Add(MakeShared<FJsonValueObject>(Row));
		}
		if (FirstFailure!=INDEX_NONE)
		{
			const auto& M=Metrics[Policy.RegionBySource[FailingSource]];
			Error=FString::Printf(TEXT("Actual stored role quality exceeds frozen limits: role=%s triangle_ordinal=%d source_face=%d max_norm=%.17g max_grade=%.17g max_cross=%.17g."),
				*FailingRole,FirstFailure,FailingSource,M.Norm,M.Grade,M.Cross); return false;
		}
		return true;
	}
}
