// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldBuildingMeshBuilder.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"

#include "ConstrainedDelaunay2.h"
#include "Curve/GeneralPolygon2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "SegmentTypes.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldBuildingMeshBuilder
{
	using namespace UE::Geometry;

	namespace
	{
		enum class EAdmissionState : uint8
		{
			Accepted,
			Duplicate,
			Contained,
			Conflict,
			Malformed,
			AuthoredMask
		};

		enum class EPairRelation : uint8
		{
			None,
			Duplicate,
			LeftContained,
			RightContained,
			Conflict
		};

		struct FPreparedBuilding
		{
			const FProjectWorldCanonicalFeature* Feature = nullptr;
			TArray<FGeneralPolygon2d> FullPolygons;
			TArray<FGeneralPolygon2d> CellPolygons;
			FAxisAlignedBox2d Bounds;
			double AreaSquareMeters = 0.0;
			EAdmissionState State = EAdmissionState::Accepted;
		};

		struct FBuildingPair
		{
			int32 Left = INDEX_NONE;
			int32 Right = INDEX_NONE;
			EPairRelation Relation = EPairRelation::None;
		};

		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("%d:"), Value.Len());
			Target += Value;
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		TArray<FVector2d> OpenRing(const TArray<FVector2D>& Source)
		{
			TArray<FVector2d> Result;
			Result.Reserve(Source.Num());
			for (const FVector2D& Point : Source)
			{
				Result.Add(FVector2d(Point.X, Point.Y));
			}
			if (Result.Num() > 1 && Result[0].Equals(Result.Last(), UE_DOUBLE_SMALL_NUMBER))
			{
				Result.Pop();
			}
			return Result;
		}

		bool IsSimpleRing(const TArray<FVector2d>& Ring)
		{
			if (Ring.Num() < 3)
			{
				return false;
			}
			for (int32 Index = 0; Index < Ring.Num(); ++Index)
			{
				const FVector2d& A = Ring[Index];
				const FVector2d& B = Ring[(Index + 1) % Ring.Num()];
				if (!FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y) || A.Equals(B, UE_DOUBLE_SMALL_NUMBER))
				{
					return false;
				}
				const FSegment2d Segment(A, B);
				for (int32 Other = Index + 1; Other < Ring.Num(); ++Other)
				{
					if (Other == Index || Other == Index + 1 || (Index == 0 && Other == Ring.Num() - 1))
					{
						continue;
					}
					const FSegment2d OtherSegment(Ring[Other], Ring[(Other + 1) % Ring.Num()]);
					if (Segment.Intersects(OtherSegment))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool BuildPolygon(const FProjectWorldCanonicalPolygon& Source, FGeneralPolygon2d& OutPolygon)
		{
			TArray<FVector2d> Outer = OpenRing(Source.Outer);
			if (!IsSimpleRing(Outer))
			{
				return false;
			}
			FPolygon2d OuterPolygon(MoveTemp(Outer));
			if (OuterPolygon.IsClockwise())
			{
				OuterPolygon.Reverse();
			}
			if (OuterPolygon.Area() <= UE_DOUBLE_SMALL_NUMBER)
			{
				return false;
			}
			OutPolygon = FGeneralPolygon2d(MoveTemp(OuterPolygon));
			for (const TArray<FVector2D>& SourceHole : Source.Holes)
			{
				TArray<FVector2d> Hole = OpenRing(SourceHole);
				if (!IsSimpleRing(Hole))
				{
					return false;
				}
				FPolygon2d HolePolygon(MoveTemp(Hole));
				if (!HolePolygon.IsClockwise())
				{
					HolePolygon.Reverse();
				}
				if (HolePolygon.Area() <= UE_DOUBLE_SMALL_NUMBER || !OutPolygon.AddHole(MoveTemp(HolePolygon)))
				{
					return false;
				}
			}
			return true;
		}

		FGeneralPolygon2d BoundsPolygon(const FVector4d& Bounds)
		{
			return FGeneralPolygon2d(FPolygon2d({
				FVector2d(Bounds.X, Bounds.Y),
				FVector2d(Bounds.Z, Bounds.Y),
				FVector2d(Bounds.Z, Bounds.W),
				FVector2d(Bounds.X, Bounds.W)}));
		}

		double PolygonArea(const TArray<FGeneralPolygon2d>& Polygons)
		{
			double Result = 0.0;
			for (const FGeneralPolygon2d& Polygon : Polygons)
			{
				Result += FMath::Abs(Polygon.SignedArea());
			}
			return Result;
		}

		bool IntersectArea(
			const TArray<FGeneralPolygon2d>& Left,
			const TArray<FGeneralPolygon2d>& Right,
			double& OutArea)
		{
			TArray<FGeneralPolygon2d> Intersection;
			if (!PolygonsIntersection(Left, Right, Intersection))
			{
				return false;
			}
			OutArea = PolygonArea(Intersection);
			return true;
		}

		void AddRejection(
			FProjectWorldBuildingMeshBuildResult& Result,
			const TCHAR* Reason,
			std::initializer_list<FString> FeatureIds)
		{
			FProjectWorldBuildingRejection& Rejection = Result.Rejections.AddDefaulted_GetRef();
			Rejection.Reason = Reason;
			for (const FString& FeatureId : FeatureIds)
			{
				Rejection.FeatureIds.Add(FeatureId);
			}
			Rejection.FeatureIds.Sort();
		}

		bool IntersectsBuildingMask(
			const TArray<FGeneralPolygon2d>& Polygons,
			const FProjectWorldAuthoredOverlaySet& OverlaySet,
			double AreaTolerance)
		{
			for (const FProjectWorldAuthoredOverlay& Overlay : OverlaySet.Overlays)
			{
				if (Overlay.Anchor.Kind != EProjectWorldAnchorKind::Mask ||
					!Overlay.Anchor.Excludes.Contains(TEXT("buildings")))
				{
					continue;
				}
				const TArray<FGeneralPolygon2d> Mask{BoundsPolygon(Overlay.Anchor.BoundsMeters)};
				double Area = 0.0;
				if (!IntersectArea(Polygons, Mask, Area) || Area > AreaTolerance)
				{
					return true;
				}
			}
			return false;
		}

		bool PrepareBuilding(
			const FProjectWorldCanonicalFeature& Feature,
			const FProjectWorldCanonicalCell& Cell,
			double MaximumHeightMeters,
			FPreparedBuilding& OutBuilding)
		{
			OutBuilding.Feature = &Feature;
			if ((Feature.GeometryType != TEXT("Polygon") && Feature.GeometryType != TEXT("MultiPolygon")) ||
				Feature.GeometryPolygons.IsEmpty() ||
				!FMath::IsFinite(Feature.HeightMeters) || Feature.HeightMeters <= 0.0 ||
				Feature.HeightMeters > MaximumHeightMeters)
			{
				OutBuilding.State = EAdmissionState::Malformed;
				return true;
			}
			for (const FProjectWorldCanonicalPolygon& Source : Feature.GeometryPolygons)
			{
				FGeneralPolygon2d Polygon;
				if (!BuildPolygon(Source, Polygon))
				{
					OutBuilding.State = EAdmissionState::Malformed;
					return true;
				}
				OutBuilding.FullPolygons.Add(MoveTemp(Polygon));
			}
			TArray<FGeneralPolygon2d> Unified;
			if (!PolygonsUnion(OutBuilding.FullPolygons, Unified, false) || Unified.IsEmpty())
			{
				OutBuilding.State = EAdmissionState::Malformed;
				return true;
			}
			OutBuilding.FullPolygons = MoveTemp(Unified);
			OutBuilding.AreaSquareMeters = PolygonArea(OutBuilding.FullPolygons);
			OutBuilding.Bounds = OutBuilding.FullPolygons[0].Bounds();
			for (int32 Index = 1; Index < OutBuilding.FullPolygons.Num(); ++Index)
			{
				OutBuilding.Bounds.Contain(OutBuilding.FullPolygons[Index].Bounds());
			}
			const TArray<FGeneralPolygon2d> CellBounds{BoundsPolygon(Cell.Bounds)};
			if (!PolygonsIntersection(OutBuilding.FullPolygons, CellBounds, OutBuilding.CellPolygons))
			{
				OutBuilding.State = EAdmissionState::Malformed;
				return true;
			}
			return true;
		}

		EPairRelation ClassifyPair(
			const FPreparedBuilding& Left,
			const FPreparedBuilding& Right,
			double AreaTolerance)
		{
			if (!Left.Bounds.Intersects(Right.Bounds))
			{
				return EPairRelation::None;
			}
			double IntersectionArea = 0.0;
			if (!IntersectArea(Left.FullPolygons, Right.FullPolygons, IntersectionArea))
			{
				return EPairRelation::Conflict;
			}
			if (IntersectionArea <= AreaTolerance)
			{
				return EPairRelation::None;
			}
			const bool bLeftCovered = IntersectionArea >= Left.AreaSquareMeters - AreaTolerance;
			const bool bRightCovered = IntersectionArea >= Right.AreaSquareMeters - AreaTolerance;
			if (bLeftCovered && bRightCovered)
			{
				return EPairRelation::Duplicate;
			}
			const bool bLeftContained = bLeftCovered;
			const bool bRightContained = bRightCovered;
			if (bLeftContained && !bRightContained)
			{
				return EPairRelation::LeftContained;
			}
			if (bRightContained && !bLeftContained)
			{
				return EPairRelation::RightContained;
			}
			return EPairRelation::Conflict;
		}

		const FProjectWorldCanonicalCell* OwnerCell(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalFeature& Feature)
		{
			return Bundle.Cells.FindByPredicate([&Feature](const FProjectWorldCanonicalCell& Cell)
			{
				return Cell.CellId == Feature.OwnerCellId;
			});
		}

		double BaseHeight(
			const FProjectWorldCanonicalBundle& Bundle,
			const FPreparedBuilding& Building)
		{
			const FVector2d Center = Building.Bounds.Center();
			const FProjectWorldCanonicalCell* Cell = OwnerCell(Bundle, *Building.Feature);
			return Cell != nullptr
				? ProjectWorldGeneratedGeometry::SampleTerrain(*Cell, Center.X, Center.Y)
				: Bundle.HeightOriginMeters;
		}

		void AddTriangle(
			FMeshDescription& Description,
			const FPolygonGroupID Group,
			const FVector Positions[3])
		{
			FStaticMeshAttributes Attributes(Description);
			TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
			const FVector Normal = FVector::CrossProduct(Positions[1] - Positions[0], Positions[2] - Positions[0]).GetSafeNormal();
			const FVector Tangent = (Positions[1] - Positions[0]).GetSafeNormal();
			TArray<FVertexInstanceID> Instances;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVertexID Vertex = Description.CreateVertex();
				VertexPositions[Vertex] = FVector3f(Positions[Corner]);
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				Normals[Instance] = FVector3f(Normal);
				Tangents[Instance] = FVector3f(Tangent);
				BinormalSigns[Instance] = 1.0f;
				Colors[Instance] = FVector4f(0.42f, 0.38f, 0.32f, 1.0f);
				UVs.Set(Instance, 0, FVector2f(Positions[Corner].X, Positions[Corner].Y) * 0.001f);
				Instances.Add(Instance);
			}
			Description.CreatePolygon(Group, Instances);
		}

		FVector LocalPoint(
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector& Origin,
			const FVector2d& Point,
			double Height)
		{
			return FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Point.X, Point.Y, Height)) - Origin;
		}

		void AddCap(
			FMeshDescription& Description,
			const FPolygonGroupID Group,
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector& Origin,
			const FGeneralPolygon2d& Polygon,
			double Height,
			bool bTop,
			int32& OutTriangleCount)
		{
			TArray<FVector2d> Vertices;
			for (const FIndex3i& Triangle : ConstrainedDelaunayTriangulateWithVertices(Polygon, Vertices))
			{
				FVector Local[3] = {
					LocalPoint(Bundle, Origin, Vertices[Triangle.A], Height),
					LocalPoint(Bundle, Origin, Vertices[Triangle.B], Height),
					LocalPoint(Bundle, Origin, Vertices[Triangle.C], Height)};
				const bool bFacesUp = FVector::CrossProduct(Local[1] - Local[0], Local[2] - Local[0]).Z > 0.0;
				if (bFacesUp != bTop)
				{
					Swap(Local[1], Local[2]);
				}
				AddTriangle(Description, Group, Local);
				++OutTriangleCount;
			}
		}

		void AddRingWalls(
			FMeshDescription& Description,
			const FPolygonGroupID Group,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FPreparedBuilding& Building,
			const FVector& Origin,
			const FPolygon2d& Ring,
			double Bottom,
			double Top,
			int32& OutTriangleCount)
		{
			for (int32 Index = 0; Index < Ring.VertexCount(); ++Index)
			{
				const FVector2d A = Ring[Index];
				const FVector2d B = Ring[(Index + 1) % Ring.VertexCount()];
				const double Tolerance = FMath::Max(Bundle.CoordinateQuantizationMeters * 2.0, 1.e-5);
				FVector2d OutsideOffset = FVector2d::Zero();
				if (FMath::IsNearlyEqual(A.X, Cell.Bounds.X, Tolerance) && FMath::IsNearlyEqual(B.X, Cell.Bounds.X, Tolerance))
				{
					OutsideOffset.X = -Tolerance;
				}
				else if (FMath::IsNearlyEqual(A.X, Cell.Bounds.Z, Tolerance) && FMath::IsNearlyEqual(B.X, Cell.Bounds.Z, Tolerance))
				{
					OutsideOffset.X = Tolerance;
				}
				else if (FMath::IsNearlyEqual(A.Y, Cell.Bounds.Y, Tolerance) && FMath::IsNearlyEqual(B.Y, Cell.Bounds.Y, Tolerance))
				{
					OutsideOffset.Y = -Tolerance;
				}
				else if (FMath::IsNearlyEqual(A.Y, Cell.Bounds.W, Tolerance) && FMath::IsNearlyEqual(B.Y, Cell.Bounds.W, Tolerance))
				{
					OutsideOffset.Y = Tolerance;
				}
				const FVector2d OutsidePoint = (A + B) * 0.5 + OutsideOffset;
				if (OutsideOffset.SquaredLength() > 0.0 && Building.FullPolygons.ContainsByPredicate(
					[&OutsidePoint](const FGeneralPolygon2d& Polygon) { return Polygon.Contains(OutsidePoint); }))
				{
					continue;
				}
				const FVector BottomA = LocalPoint(Bundle, Origin, A, Bottom);
				const FVector BottomB = LocalPoint(Bundle, Origin, B, Bottom);
				const FVector TopA = LocalPoint(Bundle, Origin, A, Top);
				const FVector TopB = LocalPoint(Bundle, Origin, B, Top);
				const FVector First[3] = {BottomA, TopB, BottomB};
				const FVector Second[3] = {BottomA, TopA, TopB};
				AddTriangle(Description, Group, First);
				AddTriangle(Description, Group, Second);
				OutTriangleCount += 2;
			}
		}

		void AddMassing(
			FProjectWorldBuildingMeshBuildResult& Result,
			const FPolygonGroupID Group,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FPreparedBuilding& Building)
		{
			const double Bottom = BaseHeight(Bundle, Building);
			const double Top = Bottom + Building.Feature->HeightMeters;
			for (const FGeneralPolygon2d& Polygon : Building.CellPolygons)
			{
				AddCap(Result.MeshDescription, Group, Bundle, Result.ActorOrigin, Polygon, Bottom, false, Result.TriangleCount);
				AddCap(Result.MeshDescription, Group, Bundle, Result.ActorOrigin, Polygon, Top, true, Result.TriangleCount);
				AddRingWalls(Result.MeshDescription, Group, Bundle, Cell, Building, Result.ActorOrigin, Polygon.GetOuter(), Bottom, Top, Result.TriangleCount);
				for (const FPolygon2d& Hole : Polygon.GetHoles())
				{
					AddRingWalls(Result.MeshDescription, Group, Bundle, Cell, Building, Result.ActorOrigin, Hole, Bottom, Top, Result.TriangleCount);
				}
			}
		}

		bool BuildSemanticDigest(FProjectWorldBuildingMeshBuildResult& Result)
		{
			FStaticMeshAttributes Attributes(Result.MeshDescription);
			TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
			FString Identity = FString::Printf(
				TEXT("project_building_cell_mesh_v1|origin=%.17g,%.17g,%.17g|triangles=%d|accepted=%d|"),
				Result.ActorOrigin.X,
				Result.ActorOrigin.Y,
				Result.ActorOrigin.Z,
				Result.TriangleCount,
				Result.Stats.AcceptedFragmentCount);
			for (const FVertexID Vertex : Result.MeshDescription.Vertices().GetElementIDs())
			{
				const FVector3f Position = Positions[Vertex];
				Identity += FString::Printf(TEXT("v=%.9g,%.9g,%.9g|"), Position.X, Position.Y, Position.Z);
			}
			for (const FProjectWorldBuildingRejection& Rejection : Result.Rejections)
			{
				AppendToken(Identity, Rejection.Reason);
				for (const FString& FeatureId : Rejection.FeatureIds)
				{
					AppendToken(Identity, FeatureId);
				}
			}
			return HashText(Identity, Result.SemanticDigest);
		}
	}

	TArray<FString> CellBuildingFeatureIds(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell)
	{
		TSet<FString> CandidateIds;
		CandidateIds.Append(Cell.OwnedFeatureIds);
		CandidateIds.Append(Cell.ReferencedFeatureIds);
		TArray<FString> Result;
		for (const FString& FeatureId : CandidateIds)
		{
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
			if (Feature != nullptr && Feature->FeatureClass == TEXT("building"))
			{
				Result.Add(FeatureId);
			}
		}
		Result.Sort();
		return Result;
	}

	bool BuildCell(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		const FProjectWorldBuildingSettings& Settings,
		FProjectWorldBuildingMeshBuildResult& OutResult,
		FString& OutError)
	{
		OutResult = FProjectWorldBuildingMeshBuildResult();
		FStaticMeshAttributes Attributes(OutResult.MeshDescription);
		Attributes.Register();
		Attributes.GetVertexInstanceUVs().SetNumChannels(1);
		const FPolygonGroupID Group = OutResult.MeshDescription.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Building");
		OutResult.ActorOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));

		const double AreaTolerance = FMath::Max(
			FMath::Square(Bundle.CoordinateQuantizationMeters), UE_DOUBLE_SMALL_NUMBER);
		TArray<FPreparedBuilding> Buildings;
		for (const FString& FeatureId : CellBuildingFeatureIds(Bundle, Cell))
		{
			FPreparedBuilding Building;
			if (!PrepareBuilding(Bundle.Features.FindChecked(FeatureId), Cell, Settings.MaximumHeightMeters, Building))
			{
				OutError = FString::Printf(TEXT("Cannot prepare building feature: %s"), *FeatureId);
				return false;
			}
			if (Building.State == EAdmissionState::Malformed)
			{
				++OutResult.Stats.CandidateFragmentCount;
				++OutResult.Stats.MalformedFragmentCount;
				AddRejection(OutResult, TEXT("malformed_geometry_or_height"), {FeatureId});
				continue;
			}
			if (Building.CellPolygons.IsEmpty() || PolygonArea(Building.CellPolygons) <= AreaTolerance)
			{
				continue;
			}
			++OutResult.Stats.CandidateFragmentCount;
			if (IntersectsBuildingMask(Building.CellPolygons, AuthoredOverlaySet, AreaTolerance))
			{
				Building.State = EAdmissionState::AuthoredMask;
				++OutResult.Stats.AuthoredMaskExcludedFragmentCount;
				AddRejection(OutResult, TEXT("authored_building_mask"), {FeatureId});
			}
			Buildings.Add(MoveTemp(Building));
		}

		TArray<FBuildingPair> Pairs;
		for (int32 Left = 0; Left < Buildings.Num(); ++Left)
		{
			for (int32 Right = Left + 1; Right < Buildings.Num(); ++Right)
			{
				if (Buildings[Left].State != EAdmissionState::Accepted ||
					Buildings[Right].State != EAdmissionState::Accepted)
				{
					continue;
				}
				const EPairRelation Relation = ClassifyPair(Buildings[Left], Buildings[Right], AreaTolerance);
				if (Relation != EPairRelation::None)
				{
					Pairs.Add({Left, Right, Relation});
				}
			}
		}
		Pairs.Sort([&Buildings](const FBuildingPair& Left, const FBuildingPair& Right)
		{
			const FString LeftKey = Buildings[Left.Left].Feature->FeatureId + TEXT("|") + Buildings[Left.Right].Feature->FeatureId;
			const FString RightKey = Buildings[Right.Left].Feature->FeatureId + TEXT("|") + Buildings[Right.Right].Feature->FeatureId;
			return LeftKey < RightKey;
		});

		for (const FBuildingPair& Pair : Pairs)
		{
			if (Pair.Relation != EPairRelation::Duplicate ||
				Buildings[Pair.Left].State != EAdmissionState::Accepted ||
				Buildings[Pair.Right].State != EAdmissionState::Accepted)
			{
				continue;
			}
			const int32 Duplicate = Buildings[Pair.Left].Feature->FeatureId < Buildings[Pair.Right].Feature->FeatureId
				? Pair.Right : Pair.Left;
			Buildings[Duplicate].State = EAdmissionState::Duplicate;
			AddRejection(OutResult, TEXT("exact_duplicate"), {
				Buildings[Pair.Left].Feature->FeatureId,
				Buildings[Pair.Right].Feature->FeatureId});
		}
		for (const FBuildingPair& Pair : Pairs)
		{
			if ((Pair.Relation != EPairRelation::LeftContained && Pair.Relation != EPairRelation::RightContained) ||
				Buildings[Pair.Left].State != EAdmissionState::Accepted ||
				Buildings[Pair.Right].State != EAdmissionState::Accepted)
			{
				continue;
			}
			const int32 Contained = Pair.Relation == EPairRelation::LeftContained ? Pair.Left : Pair.Right;
			Buildings[Contained].State = EAdmissionState::Contained;
			AddRejection(OutResult, TEXT("contained_footprint_associated"), {
				Buildings[Pair.Left].Feature->FeatureId,
				Buildings[Pair.Right].Feature->FeatureId});
		}
		for (const FBuildingPair& Pair : Pairs)
		{
			if (Pair.Relation != EPairRelation::Conflict ||
				Buildings[Pair.Left].State != EAdmissionState::Accepted ||
				Buildings[Pair.Right].State != EAdmissionState::Accepted)
			{
				continue;
			}
			Buildings[Pair.Left].State = EAdmissionState::Conflict;
			Buildings[Pair.Right].State = EAdmissionState::Conflict;
			AddRejection(OutResult, TEXT("unresolved_positive_overlap"), {
				Buildings[Pair.Left].Feature->FeatureId,
				Buildings[Pair.Right].Feature->FeatureId});
		}

		for (const FPreparedBuilding& Building : Buildings)
		{
			switch (Building.State)
			{
			case EAdmissionState::Accepted:
				++OutResult.Stats.AcceptedFragmentCount;
				AddMassing(OutResult, Group, Bundle, Cell, Building);
				break;
			case EAdmissionState::Duplicate:
				++OutResult.Stats.DuplicateFragmentCount;
				break;
			case EAdmissionState::Contained:
				++OutResult.Stats.ContainedFragmentCount;
				break;
			case EAdmissionState::Conflict:
				++OutResult.Stats.ConflictFragmentCount;
				break;
			default:
				break;
			}
		}
		OutResult.Rejections.Sort([](const auto& Left, const auto& Right)
		{
			const FString LeftKey = Left.Reason + TEXT("|") + FString::Join(Left.FeatureIds, TEXT(","));
			const FString RightKey = Right.Reason + TEXT("|") + FString::Join(Right.FeatureIds, TEXT(","));
			return LeftKey < RightKey;
		});
		if (!BuildSemanticDigest(OutResult))
		{
			OutError = TEXT("Cannot hash building cell mesh semantics.");
			return false;
		}
		return true;
	}
}
