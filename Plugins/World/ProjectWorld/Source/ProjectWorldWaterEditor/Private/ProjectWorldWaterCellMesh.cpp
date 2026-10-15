// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterCellMesh.h"
#include "ProjectWorldWaterHorizontalComposition.h"
#include "ProjectWorldCanonicalBundle.h"
#include "StaticMeshOperations.h"
#include "Utilities/ProjectSha256.h"
#include "Curve/PolygonIntersectionUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldWaterCellMesh, Log, All);

namespace ProjectWorldWaterCellMesh
{
	namespace
	{
		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}
		bool ConstantHeight(const FProjectWorldCanonicalFeature& Feature, double Step, double& Height)
		{
			const auto& Surface = Feature.WaterSurface;
			if (Surface.Behavior == TEXT("standing"))
			{
				Height = Surface.LevelMeters;
				return FMath::IsFinite(Height);
			}
			if (Surface.Behavior != TEXT("flowing") || Surface.Knots.Num() < 2 || !FMath::IsFinite(Step) || Step <= 0)
				return false;
			Height = FMath::RoundToDouble(Surface.Knots[0].Z / Step) * Step;
			for (const FVector& Knot : Surface.Knots)
				if (!FMath::IsFinite(Knot.Z) || FMath::RoundToDouble(Knot.Z / Step) * Step != Height) return false;
			return FMath::IsFinite(Height);
		}

		struct FSurfaceGroup
		{
			TArray<FString> Ids;
			double Height = 0;
			bool bConstant = false;
		};
	}

	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const TMap<FString, FProjectWorldPreparedWaterSurface>& PreparedSurfaces,
		const TArray<FString>& FeatureIds,
		double SurfaceOffsetMeters,
		FMeshDescription& OutDescription,
		FVector& OutOrigin,
		FString& OutSemantic,
		int32& OutTriangles,
		FString& OutError)
	{
		OutDescription = {};
		OutTriangles = 0;
		OutSemantic.Reset();
		OutError.Reset();
		TArray<FString> SortedIds = FeatureIds;
		SortedIds.Sort();
		TArray<FSurfaceGroup> Groups;
		int32 Nonconstant = 0;
		for (const FString& Id : SortedIds)
		{
			const auto* Feature = Bundle.Features.Find(Id);
			if (Feature == nullptr || Feature->FeatureClass != TEXT("water") || !Feature->WaterSurface.bValid || !PreparedSurfaces.Contains(Id))
			{
				OutError = FString::Printf(TEXT("Water feature was not prepared: %s"), *Id);
				return false;
			}
			double Height = 0;
			const bool bConstant = ConstantHeight(*Feature, Bundle.HeightQuantizationMeters, Height);
			FSurfaceGroup* Group = bConstant ? Groups.FindByPredicate([Height](const auto& G)
				{ return G.bConstant && G.Height == Height; }) : nullptr;
			if (Group == nullptr)
			{
				Group = &Groups.AddDefaulted_GetRef();
				Group->Height = Height;
				Group->bConstant = bConstant;
			}
			Group->Ids.Add(Id);
			Nonconstant += bConstant ? 0 : 1;
		}
		TArray<FString> SemanticParts;
		bool bHasMesh = false;
		int32 Composed = 0;
		for (const FSurfaceGroup& Group : Groups)
		{
			const FString& FeatureId = Group.Ids[0];
			const FProjectWorldPreparedWaterSurface* PreparedSurface = &PreparedSurfaces.FindChecked(FeatureId);
			FProjectWorldPreparedWaterSurface Combined;
			if (Group.Ids.Num() > 1)
			{
				using namespace UE::Geometry;
				TArray<FGeneralPolygon2d> Inputs;
				for (const FString& Id : Group.Ids)
				{
					const auto& Source = PreparedSurfaces.FindChecked(Id);
					if (!Source.TriangleVertices.IsEmpty() && Source.Footprints.IsEmpty())
					{
						OutError = TEXT("Constant-plane composition requires original water footprints.");
						return false;
					}
					Inputs.Append(Source.Footprints);
				}
				const FVector4d& B = Cell.Bounds;
				const TArray<FGeneralPolygon2d> CellPolygon{FGeneralPolygon2d(FPolygon2d({
					FVector2d(B.X, B.Y), FVector2d(B.Z, B.Y), FVector2d(B.Z, B.W), FVector2d(B.X, B.W)}))};
				TArray<FGeneralPolygon2d> Clipped, United;
				if (!PolygonsIntersection(Inputs, CellPolygon, Clipped) || !PolygonsUnion(Clipped, United, false) ||
					!ProjectWorldWaterMeshBuilder::PreparePolygons(United, Combined, OutError))
				{
					if (OutError.IsEmpty()) OutError = TEXT("GeometryAlgorithms could not compose constant-plane water footprints.");
					return false;
				}
				PreparedSurface = &Combined;
				++Composed;
			}
			FProjectWorldWaterMeshBuildResult Surface;
			if (!ProjectWorldWaterMeshBuilder::BuildCellSurface(
				Bundle,
				Cell,
				Bundle.Features.FindChecked(FeatureId),
				*PreparedSurface,
				SurfaceOffsetMeters,
				Surface,
				OutError))
			{
				return false;
			}
			// Every admitted source remains provenance, including a fully covered
			// duplicate. The visible union never invents a source connectivity law.
			for (const FString& Id : Group.Ids)
				SemanticParts.Add(FString::Printf(TEXT("source=%d:%s"), Id.Len(), *Id));
			SemanticParts.Add(TEXT("geometry:") + Surface.SemanticDigest);
			if (Surface.TriangleCount == 0)
			{
				continue;
			}
			if (!bHasMesh)
			{
				OutDescription = MoveTemp(Surface.MeshDescription);
				OutOrigin = Surface.ActorOrigin;
				bHasMesh = true;
			}
			else
			{
				FStaticMeshOperations::AppendMeshDescription(
					Surface.MeshDescription,
					OutDescription,
					FStaticMeshOperations::FAppendSettings());
			}
			OutTriangles += Surface.TriangleCount;
		}
		bool bComposed=false;
		FString GeometryHash;
		if (bHasMesh && !ProjectWorldWaterHorizontalComposition::Compose(
			OutDescription,bComposed,GeometryHash,OutError)) return false;
		if (bComposed)
		{
			OutTriangles=OutDescription.Triangles().Num();
			SemanticParts.Add(TEXT("emitted_horizontal_union_v1:")+GeometryHash);
		}
		UE_LOG(LogProjectWorldWaterCellMesh, Display,
			TEXT("[Build] Cell=%s composed_constant_groups=%d retained_nonconstant_features=%d surface_groups=%d"),
			*Cell.CellId, Composed, Nonconstant, Groups.Num());
		return !bHasMesh || HashText(
			TEXT("project_water_cell_mesh_v2|") + Cell.CellId + TEXT("|") +
			FString::Join(SemanticParts, TEXT("|")),
			OutSemantic);
	}

}
