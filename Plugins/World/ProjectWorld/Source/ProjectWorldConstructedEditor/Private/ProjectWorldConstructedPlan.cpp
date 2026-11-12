// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldVerify.h"
#include "Dom/JsonObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldConstructedRealization
{
#if WITH_DEV_AUTOMATION_TESTS
	namespace { TAtomic<int64> ConstructedNativeBuildCalls{0}; }
	int64 NativeBuildCallsForTests() { return ConstructedNativeBuildCalls.Load(); }
#endif
	bool Describe(FMeshDescription& Description,FString& Hash)
	{
		TArray<uint8> Bytes; FMemoryWriter Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory,false);
		Description.Serialize(Archive);
		return !Archive.IsError() && FProjectSha256::HashBuffer(Bytes,Hash);
	}
	bool Qualify(const FProjectWorldCanonicalConstructedSurface& Surface,const FMeshDescription& Description,FString& Error)
	{
		TArray<int32> RegionBySource;
		if (!ValidateSourcePolicy(Surface,RegionBySource,Error)) return false;
		if (!Description.TriangleAttributes().HasAttributeOfType<int32>(SourceFaceAttribute) || Description.Triangles().Num()==0)
		{ Error=TEXT("Physical structural output requires complete native source-face correspondence."); return false; }
		const auto& Policy=Surface.OperationalPolicy.GetValue();
		const auto Sources=Description.TriangleAttributes().GetAttributesRef<int32>(SourceFaceAttribute);
		const auto Positions=FStaticMeshConstAttributes(Description).GetVertexPositions();
		for (const auto Triangle : Description.Triangles().GetElementIDs())
		{
			const int32 Source=Sources[Triangle];
			if (!RegionBySource.IsValidIndex(Source)) { Error=TEXT("Stored structural face has no source correspondence."); return false; }
			const auto& Region=Surface.FaceRegions[RegionBySource[Source]]; FProjectWorldSurfaceTriangle Face;
			for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Description.GetVertexInstanceVertex(Description.GetTriangleVertexInstance(Triangle,I))]);
			double Height;
			if (!ProjectWorldTerrainSurface::Height(Face,FVector2D(Face.Points[0]),Height,Error)) return false;
			const FVector A=Face.Points[1]-Face.Points[0],B=Face.Points[2]-Face.Points[0]; const double D=A.X*B.Y-A.Y*B.X;
			const FVector2D Gradient((A.Z*B.Y-A.Y*B.Z)/D,(A.X*B.Z-A.Z*B.X)/D);
			if (Region.Role==TEXT("gangway"))
			{
				const auto Axis=Region.AxisEnd-Region.AxisStart;
				const auto Along=FVector2D(Axis.X,-Axis.Y).GetSafeNormal(0.0); const FVector2D Across(-Along.Y,Along.X);
				const double Grade=FMath::Abs(FVector2D::DotProduct(Gradient,Along)),Cross=FMath::Abs(FVector2D::DotProduct(Gradient,Across));
				if (!FMath::IsFinite(Grade) || !FMath::IsFinite(Cross) || Grade>Policy.MaximumGangwayGrade || Cross>Policy.MaximumCrossSlope)
				{ Error=TEXT("Actual stored gangway exceeds its declared grade or crossfall."); return false; }
			}
			else if (!FMath::IsFinite(Gradient.Length()) || Gradient.Length()>Policy.MaximumOrdinaryGradient)
			{ Error=TEXT("Actual stored ordinary structural face exceeds its declared gradient norm."); return false; }
		}
		return true;
	}
	bool Plan(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldRealizationLayer& Layer,FPlan& Out,FString& Error)
	{
		Out={}; Error.Reset(); FPlan Candidate; FDependencyInputs SourceInputs;
		if (!DependencyInputs(Bundle,Layer,SourceInputs,Error)) return false;
		const auto& Options=SourceInputs.Options;
		Candidate.CellHashes=SourceInputs.CellHashes;
		const auto& Fields=SourceInputs.Fields;
		for (const auto& Cell : Bundle.Cells)
		{
			TArray<FString> Ids=Cell.OwnedFeatureIds; for (const auto& Id : Cell.ReferencedFeatureIds) Ids.AddUnique(Id); Ids.Sort();
			for (const auto& Id : Ids)
			{
				const auto* Field=Fields.Find(Id); if (!Field) continue;
				const auto& Feature=Bundle.Features.FindChecked(Id); const auto& Surface=Feature.ConstructedSurface.GetValue();
				FUnit Unit; Unit.FeatureId=Id; Unit.CellId=Cell.CellId; Unit.ModelHash=Surface.ModelHash; Unit.Surface=&Surface;
				Unit.Id=UnitIdentity(Id,Cell.CellId);
				TArray<int32> SourceOrdinals;
#if WITH_DEV_AUTOMATION_TESTS
				++ConstructedNativeBuildCalls;
#endif
				if (!ProjectWorldConstructedMesh::Build(Bundle,Cell,*Field,Options.MaximumJoinStepMeters,Unit.Description,Unit.Origin,Error,&SourceOrdinals)) return false;
				if (SourceOrdinals.Num()!=Unit.Description.Triangles().Num())
				{ Error=TEXT("Structural conversion returned incomplete source-face provenance."); return false; }
				if (Unit.Description.Triangles().Num()==0)
				{
					++Candidate.BoundaryContacts; continue;
				}
				Unit.Description.TriangleAttributes().RegisterAttribute<int32>(SourceFaceAttribute,1,INDEX_NONE,EMeshAttributeFlags::None);
				auto Attribute=Unit.Description.TriangleAttributes().GetAttributesRef<int32>(SourceFaceAttribute); int32 Ordinal=0;
				for (const auto Triangle : Unit.Description.Triangles().GetElementIDs()) Attribute[Triangle]=SourceOrdinals[Ordinal++];
				if (!Qualify(Surface,Unit.Description,Error) || !Describe(Unit.Description,Unit.GeometryHash)) return false;
				const auto& Policy=Surface.OperationalPolicy.GetValue();
				FString Consumed=Unit.Id+TEXT("|")+Unit.ModelHash+TEXT("|")+Unit.GeometryHash+TEXT("|")+
					FString::Printf(TEXT("%.17g|%.17g|%.17g|%.17g|%d|%.17g|%.17g|%.17g|%.17g"),Unit.Origin.X,Unit.Origin.Y,Unit.Origin.Z,
						Options.MaximumJoinStepMeters,Policy.MaximumTriangles,Policy.MaximumOrdinaryGradient,Policy.MaximumGangwayGrade,
						Policy.MaximumCrossSlope,Policy.MaximumNumericErrorMeters);
				for (const auto& Region : Surface.FaceRegions)
				{
					Consumed+=TEXT("|")+Region.Role+TEXT("|")+FString::Join(Region.SourceIds,TEXT(";"))+TEXT("|")+Region.AxisSourceId;
					Consumed+=FString::Printf(TEXT("|%.17g|%.17g|%.17g|%.17g"),Region.AxisStart.X,Region.AxisStart.Y,Region.AxisEnd.X,Region.AxisEnd.Y);
					for (int32 Source : Region.SourceTriangles) Consumed+=FString::Printf(TEXT("|%d"),Source);
				}
				Unit.InputHash=ProjectWorldProjection::HashText(Consumed);
				Candidate.Units.Add(MoveTemp(Unit));
			}
		}
		Out=MoveTemp(Candidate); return true;
	}
}
