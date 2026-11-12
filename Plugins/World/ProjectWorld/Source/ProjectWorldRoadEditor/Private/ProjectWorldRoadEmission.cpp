// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadEmission.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldRoadEmission
{
	bool Read(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer, const TSet<FString>& FeatureIds,
		FProjectWorldRoadEmission& Out, FString& Error)
	{
		Out = FProjectWorldRoadEmission();
		if (FeatureIds.IsEmpty() || FeatureIds.Num() > 4096)
		{ Error = TEXT("Road emission read requires a bounded explicit feature selection."); return false; }
		for (const auto& Id : FeatureIds)
		{
			const auto* Feature = Bundle.Features.Find(Id);
			if (!Feature || Feature->FeatureClass != TEXT("road"))
			{ Error = TEXT("Road emission selection contains a missing or non-Road feature."); return false; }
		}
		FMeshDescription Mesh; FVector Origin; FProjectWorldRoadEmission Candidate;
		if (!ProjectWorldRoadRealization::ReadSurfaceOffset(Layer,Candidate.PresentationLiftMeters,Error) ||
			!ProjectWorldRoadRealization::BuildDescription(Bundle,Cell,Layer,Mesh,Origin,Error,&FeatureIds,&Candidate.LocallyEligibleFeatureIds)) return false;
		const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
		Candidate.Surface.Origin=Origin;
		FString Encoded(TEXT("native_road_emission_frame_xyz_v2|"));
		Encoded += FString::Printf(TEXT("%.17g|%.17g,%.17g,%.17g|"),Candidate.PresentationLiftMeters,Origin.X,Origin.Y,Origin.Z);
		for (const auto Face : Mesh.Triangles().GetElementIDs())
		{
			FProjectWorldSurfaceTriangle Triangle;
			for (int32 I=0; I<3; ++I)
			{
				const auto Vertex = Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Face,I));
				Triangle.Points[I] = FVector(Positions[Vertex]);
				const auto& P = Triangle.Points[I];
				Encoded += FString::Printf(TEXT("%.17g,%.17g,%.17g;"),P.X,P.Y,P.Z);
			}
			Candidate.Surface.Triangles.Add(Triangle);
		}
		FTCHARToUTF8 Utf8(*Encoded); TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
		if (!FProjectSha256::HashBuffer(Bytes,Candidate.GeometryHash))
		{ Error = TEXT("Cannot identify the exact native Road emission."); return false; }
		Out = MoveTemp(Candidate); return true;
	}
}
