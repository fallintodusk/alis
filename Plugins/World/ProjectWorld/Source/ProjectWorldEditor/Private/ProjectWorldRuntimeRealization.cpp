// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeProfile.h"

#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Components/BrushComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/HLOD/HLODActor.h"

namespace ProjectWorldRuntimeRealization
{
	namespace
	{
		const FString RuntimeRolePrefix(TEXT("ProjectWorld.RuntimeRole="));
		const FString RuntimeProfilePrefix(TEXT("ProjectWorld.Runtime="));
		const FString RuntimeProfileHashPrefix(TEXT("ProjectWorld.RuntimeHash="));
		const FString GridPrefix(TEXT("ProjectWorld.Grid="));
		const FString RoutePrefix(TEXT("ProjectWorld.Route="));

		void SetTagValue(AActor& Actor, const FString& Prefix, const FString& Value)
		{
			Actor.Tags.RemoveAll([&Prefix](const FName& Tag)
			{
				return Tag.ToString().StartsWith(Prefix);
			});
			Actor.Tags.Add(FName(*(Prefix + Value)));
		}

		FString RuntimeRole(const AActor& Actor)
		{
			for (const FName& Tag : Actor.Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(RuntimeRolePrefix))
				{
					return Value.RightChop(RuntimeRolePrefix.Len());
				}
			}
			return FString();
		}

		const FProjectWorldCanonicalFeature* RouteFeature(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			FString& OutError)
		{
			if (Bundle.GridId != Profile.GridId)
			{
				OutError = FString::Printf(
					TEXT("Runtime profile grid %s does not match canonical grid %s."),
					*Profile.GridId,
					*Bundle.GridId);
				return nullptr;
			}
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(Profile.RouteFeatureId);
			if (Feature == nullptr || Feature->FeatureClass != TEXT("road") ||
				Feature->GeometryType != TEXT("LineString") || Feature->GeometryPoints.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Runtime route must pin an accepted canonical LineString road: %s."),
					*Profile.RouteFeatureId);
				return nullptr;
			}
			return Feature;
		}

		FVector2D InsetEndpoint(
			const TArray<FVector2D>& Points,
			bool bFromStart,
			double InsetMeters)
		{
			const FVector2D Endpoint = bFromStart ? Points[0] : Points.Last();
			const FVector2D Neighbor = bFromStart ? Points[1] : Points[Points.Num() - 2];
			const FVector2D Direction = (Neighbor - Endpoint).GetSafeNormal();
			return Endpoint + Direction * InsetMeters;
		}

		const FProjectWorldCanonicalCell* CellAt(
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector2D& Point)
		{
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				if (Point.X >= Cell.Bounds.X && Point.X <= Cell.Bounds.Z &&
					Point.Y >= Cell.Bounds.Y && Point.Y <= Cell.Bounds.W)
				{
					return &Cell;
				}
			}
			return nullptr;
		}

		const FProjectWorldCanonicalCell* CellById(
			const FProjectWorldCanonicalBundle& Bundle,
			const FString& CellId)
		{
			return Bundle.Cells.FindByPredicate([&CellId](const FProjectWorldCanonicalCell& Cell)
			{
				return Cell.CellId == CellId;
			});
		}

		bool PolylineInteriorPoint(const TArray<FVector2D>& Points, FVector2D& OutPoint)
		{
			double TotalLength = 0.0;
			for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
			{
				TotalLength += FVector2D::Distance(Points[Index], Points[Index + 1]);
			}
			if (TotalLength <= UE_DOUBLE_SMALL_NUMBER)
			{
				return false;
			}

			double Remaining = TotalLength * 0.5;
			for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
			{
				const double SegmentLength = FVector2D::Distance(Points[Index], Points[Index + 1]);
				if (Remaining <= SegmentLength && SegmentLength > UE_DOUBLE_SMALL_NUMBER)
				{
					OutPoint = FMath::Lerp(Points[Index], Points[Index + 1], Remaining / SegmentLength);
					return true;
				}
				Remaining -= SegmentLength;
			}
			OutPoint = Points.Last();
			return true;
		}

		bool RepresentationInteriorPoint(
			const FProjectWorldCanonicalRepresentation& Representation,
			FVector2D& OutPoint)
		{
			const TArray<FVector2D>* LongestPart = nullptr;
			double LongestLength = 0.0;
			for (const TArray<FVector2D>& Part : Representation.Parts)
			{
				double Length = 0.0;
				for (int32 Index = 0; Index < Part.Num() - 1; ++Index)
				{
					Length += FVector2D::Distance(Part[Index], Part[Index + 1]);
				}
				if (Length > LongestLength)
				{
					LongestLength = Length;
					LongestPart = &Part;
				}
			}
			return LongestPart != nullptr && PolylineInteriorPoint(*LongestPart, OutPoint);
		}

		bool RepresentationInteriorFrame(
			const FProjectWorldCanonicalRepresentation& Representation,
			FVector2D& OutPoint,
			FVector2D& OutTangent,
			double& OutPartLength)
		{
			const TArray<FVector2D>* LongestPart = nullptr;
			double LongestLength = 0.0;
			for (const TArray<FVector2D>& Part : Representation.Parts)
			{
				double Length = 0.0;
				for (int32 Index = 0; Index < Part.Num() - 1; ++Index)
				{
					Length += FVector2D::Distance(Part[Index], Part[Index + 1]);
				}
				if (Length > LongestLength)
				{
					LongestLength = Length;
					LongestPart = &Part;
				}
			}
			if (LongestPart == nullptr || LongestLength <= UE_DOUBLE_SMALL_NUMBER)
			{
				return false;
			}

			const TArray<FVector2D>& Points = *LongestPart;
			double Remaining = LongestLength * 0.5;
			for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
			{
				const double SegmentLength = FVector2D::Distance(Points[Index], Points[Index + 1]);
				if (Remaining <= SegmentLength && SegmentLength > UE_DOUBLE_SMALL_NUMBER)
				{
					OutPoint = FMath::Lerp(Points[Index], Points[Index + 1], Remaining / SegmentLength);
					OutTangent = (Points[Index + 1] - Points[Index]).GetSafeNormal();
					OutPartLength = LongestLength;
					return !OutTangent.IsNearlyZero();
				}
				Remaining -= SegmentLength;
			}
			return false;
		}

		bool RouteLocations(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			const FProjectWorldCanonicalFeature& Feature,
			FVector& OutStart,
			FVector& OutEnd,
			FString& OutError)
		{
			const FVector2D Start = InsetEndpoint(Feature.GeometryPoints, true, Profile.EndpointInsetMeters);
			const FVector2D End = InsetEndpoint(Feature.GeometryPoints, false, Profile.EndpointInsetMeters);
			const FProjectWorldCanonicalCell* StartCell = CellAt(Bundle, Start);
			const FProjectWorldCanonicalCell* EndCell = CellAt(Bundle, End);
			if (StartCell == nullptr || EndCell == nullptr)
			{
				OutError = TEXT("Runtime route endpoints must remain inside accepted canonical cells.");
				return false;
			}
			constexpr double RoadSurfaceOffsetMeters = 0.65;
			OutStart = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Start, ProjectWorldGeneratedGeometry::SampleTerrain(*StartCell, Start.X, Start.Y) + RoadSurfaceOffsetMeters));
			OutEnd = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(End, ProjectWorldGeneratedGeometry::SampleTerrain(*EndCell, End.X, End.Y) + RoadSurfaceOffsetMeters));
			return true;
		}

		enum class ERouteSurfaceExpectation : uint8
		{
			OnRoad,
			OffRoad
		};

		// The cell actor carries terrain, roads, and buildings in one component, so actor tags
		// cannot separate a road hit from a terrain hit 0.65 m below it. The road surface height
		// band is the discriminator; the tolerance stays well under the 0.65 m road lift.
		bool TraceRouteSurface(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FVector2D& CanonicalPoint,
			ERouteSurfaceExpectation Expectation,
			const FName& FeatureTag,
			const FName& CellTag,
			FString& OutError)
		{
			constexpr double RoadSurfaceOffsetMeters = 0.65;
			constexpr double RoadSurfaceBandCentimeters = 35.0;
			constexpr double MinimumUpwardNormalZ = 0.94;
			if (CanonicalPoint.X < Cell.Bounds.X || CanonicalPoint.X > Cell.Bounds.Z ||
				CanonicalPoint.Y < Cell.Bounds.Y || CanonicalPoint.Y > Cell.Bounds.W)
			{
				OutError = TEXT("Route orientation probe point left the accepted cell bounds.");
				return false;
			}

			const FVector Surface = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(CanonicalPoint, ProjectWorldGeneratedGeometry::SampleTerrain(
					Cell, CanonicalPoint.X, CanonicalPoint.Y) + RoadSurfaceOffsetMeters));
			FHitResult Hit;
			FCollisionQueryParams Query(TEXT("ProjectWorldRuntimeRouteFragment"), true);
			const bool bHit = World->LineTraceSingleByChannel(
				Hit,
				Surface + FVector(0.0, 0.0, 5000.0),
				Surface - FVector(0.0, 0.0, 5000.0),
				ECC_Visibility,
				Query);
			const bool bInRoadBand = bHit &&
				FMath::Abs(Hit.ImpactPoint.Z - Surface.Z) <= RoadSurfaceBandCentimeters;

			if (Expectation == ERouteSurfaceExpectation::OffRoad)
			{
				if (bInRoadBand)
				{
					OutError = TEXT("Road collision extends beyond the declared half-width.");
					return false;
				}
				return true;
			}

			if (!bInRoadBand || Hit.GetActor() == nullptr ||
				!Hit.GetActor()->Tags.Contains(FeatureTag) || !Hit.GetActor()->Tags.Contains(CellTag))
			{
				OutError = TEXT("Road-band surface was not hit where the route must provide collision.");
				return false;
			}
			if (Hit.ImpactNormal.Z < MinimumUpwardNormalZ)
			{
				OutError = TEXT("Road collision surface does not face upward at the probe point.");
				return false;
			}
			return true;
		}

		bool ProbeRouteCollision(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			const FProjectWorldCanonicalFeature& Feature,
			FProjectWorldRealizationResult& OutResult,
			FString& OutError)
		{
			const FName FeatureTag(*FString::Printf(TEXT("ProjectWorld.Feature=%s"), *Profile.RouteFeatureId));
			const double HalfWidth = FMath::Max(Feature.WidthMeters * 0.5, 1.0);
			TSet<FString> ProbedCells;
			TSet<FString> OrientationProvenCells;
			for (const FProjectWorldCanonicalRepresentation& Representation : Feature.Representations)
			{
				FVector2D CanonicalPoint;
				FVector2D Tangent;
				double PartLength = 0.0;
				if (Representation.Kind != TEXT("road_fragment") ||
					!RepresentationInteriorFrame(Representation, CanonicalPoint, Tangent, PartLength))
				{
					continue;
				}
				const FProjectWorldCanonicalCell* Cell = CellById(Bundle, Representation.CellId);
				if (Cell == nullptr || ProbedCells.Contains(Representation.CellId))
				{
					OutError = TEXT("Runtime route representations must map one-to-one to accepted cells.");
					return false;
				}
				const FName CellTag(*FString::Printf(TEXT("ProjectWorld.Cell=%s"), *Representation.CellId));

				const double AlongOffset = FMath::Min(3.0 * HalfWidth, 0.25 * PartLength);
				if (AlongOffset < 1.0)
				{
					OutError = FString::Printf(
						TEXT("Route fragment in cell %s is too short for the orientation probe."),
						*Representation.CellId);
					return false;
				}
				const FVector2D Perpendicular(-Tangent.Y, Tangent.X);
				const double LateralInside = 0.5 * HalfWidth;
				const double LateralOutside = HalfWidth + FMath::Max(2.0, HalfWidth);

				struct FRouteProbe
				{
					FVector2D Point;
					ERouteSurfaceExpectation Expectation;
				};
				const FRouteProbe Probes[] =
				{
					{CanonicalPoint, ERouteSurfaceExpectation::OnRoad},
					{CanonicalPoint + Tangent * AlongOffset, ERouteSurfaceExpectation::OnRoad},
					{CanonicalPoint - Tangent * AlongOffset, ERouteSurfaceExpectation::OnRoad},
					{CanonicalPoint + Perpendicular * LateralInside, ERouteSurfaceExpectation::OnRoad},
					{CanonicalPoint - Perpendicular * LateralInside, ERouteSurfaceExpectation::OnRoad},
					{CanonicalPoint + Perpendicular * LateralOutside, ERouteSurfaceExpectation::OffRoad},
					{CanonicalPoint - Perpendicular * LateralOutside, ERouteSurfaceExpectation::OffRoad}
				};
				for (const FRouteProbe& Probe : Probes)
				{
					FString ProbeError;
					if (!TraceRouteSurface(
						World, Bundle, *Cell, Probe.Point, Probe.Expectation, FeatureTag, CellTag, ProbeError))
					{
						OutError = FString::Printf(
							TEXT("Route %s failed the orientation probe in cell %s: %s"),
							*Profile.RouteFeatureId,
							*Representation.CellId,
							*ProbeError);
						return false;
					}
				}
				ProbedCells.Add(Representation.CellId);
				OrientationProvenCells.Add(Representation.CellId);
			}
			OutResult.RuntimeCollisionProbeCount = ProbedCells.Num();
			OutResult.bRuntimeRouteCollisionProbed =
				OutResult.RuntimeCollisionProbeCount == OutResult.CrossCellRoadExpectedFragmentCount;
			OutResult.RuntimeCollisionOrientationProbeCount = OrientationProvenCells.Num();
			OutResult.bRuntimeRouteCollisionOrientationProbed =
				OutResult.RuntimeCollisionOrientationProbeCount == OutResult.CrossCellRoadExpectedFragmentCount;
			if (!OutResult.bRuntimeRouteCollisionProbed || !OutResult.bRuntimeRouteCollisionOrientationProbed)
			{
				OutError = TEXT("Collision and orientation were not proven in every declared route fragment cell.");
				return false;
			}
			return true;
		}

		AActor* ReuseOrSpawn(
			UWorld* World,
			UClass* ActorClass,
			const FString& Role,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			FProjectWorldRealizationResult& OutResult)
		{
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|") + Profile.ProfileId + TEXT("|") + Role);
			AActor* Existing = nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (RuntimeRole(**It) == Role)
				{
					if (Existing != nullptr)
					{
						return nullptr;
					}
					Existing = *It;
				}
			}
			if (Existing != nullptr && Existing->GetClass() == ActorClass &&
				Existing->GetActorGuid() == ExpectedGuid)
			{
				++OutResult.UpdatedActorCount;
				return Existing;
			}
			if (Existing != nullptr)
			{
				if (!World->EditorDestroyActor(Existing, true))
				{
					return nullptr;
				}
				++OutResult.RemovedActorCount;
			}

			FActorSpawnParameters Parameters;
			Parameters.Name = FName(*FString::Printf(TEXT("ProjectWorld_%s"), *Role));
			Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			Parameters.OverrideActorGuid = ExpectedGuid;
			AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, Parameters);
			if (Actor != nullptr)
			{
				++OutResult.CreatedActorCount;
			}
			return Actor;
		}

		void SetIdentity(
			AActor& Actor,
			const FString& Role,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			bool bSpatiallyLoaded)
		{
			Actor.Tags.AddUnique(ProjectWorldGeneratedGeometry::GeneratedTag);
			SetTagValue(Actor, RuntimeRolePrefix, Role);
			SetTagValue(Actor, RuntimeProfilePrefix, Profile.ProfileId);
			SetTagValue(Actor, RuntimeProfileHashPrefix, Profile.ProfileHash);
			SetTagValue(Actor, GridPrefix, Bundle.GridId);
			SetTagValue(Actor, RoutePrefix, Profile.RouteId);
			Actor.SetActorLabel(FString::Printf(TEXT("ProjectWorld_%s"), *Role), false);
			Actor.SetFolderPath(TEXT("ProjectWorld/Generated/Runtime"));
			Actor.bEnableAutoLODGeneration = false;
			if (Actor.CanChangeIsSpatiallyLoadedFlag())
			{
				Actor.SetIsSpatiallyLoaded(bSpatiallyLoaded);
			}
			Actor.MarkPackageDirty();
		}
	}

	bool Validate(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FString& OutError)
	{
		const FProjectWorldCanonicalFeature* Feature = RouteFeature(Bundle, Profile, OutError);
		FVector Start;
		FVector End;
		return Feature != nullptr && RouteLocations(Bundle, Profile, *Feature, Start, End, OutError);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldCanonicalFeature* Feature = RouteFeature(Bundle, Profile, OutError);
		FVector Start;
		FVector End;
		if (Feature == nullptr || !RouteLocations(Bundle, Profile, *Feature, Start, End, OutError))
		{
			return false;
		}

		ATargetPoint* StartActor = Cast<ATargetPoint>(ReuseOrSpawn(
			World, ATargetPoint::StaticClass(), TEXT("RouteStart"), Bundle, Profile, OutResult));
		ATargetPoint* EndActor = Cast<ATargetPoint>(ReuseOrSpawn(
			World, ATargetPoint::StaticClass(), TEXT("RouteEnd"), Bundle, Profile, OutResult));
		ANavMeshBoundsVolume* NavBounds = Cast<ANavMeshBoundsVolume>(ReuseOrSpawn(
			World, ANavMeshBoundsVolume::StaticClass(), TEXT("RouteNavigation"), Bundle, Profile, OutResult));
		if (StartActor == nullptr || EndActor == nullptr || NavBounds == nullptr)
		{
			OutError = TEXT("Cannot create unique runtime-route actors.");
			return false;
		}

		StartActor->SetActorLocation(Start + FVector(0.0, 0.0, 100.0));
		EndActor->SetActorLocation(End + FVector(0.0, 0.0, 100.0));
		SetIdentity(*StartActor, TEXT("RouteStart"), Bundle, Profile, true);
		SetIdentity(*EndActor, TEXT("RouteEnd"), Bundle, Profile, true);
		UNavigationInvokerComponent* Invoker = StartActor->FindComponentByClass<UNavigationInvokerComponent>();
		if (Invoker == nullptr)
		{
			Invoker = NewObject<UNavigationInvokerComponent>(StartActor, TEXT("RouteNavigationInvoker"), RF_Transactional);
			StartActor->AddInstanceComponent(Invoker);
			Invoker->RegisterComponent();
		}
		const float GenerationRadius = FVector::Distance(Start, End) + Profile.NavigationPaddingMeters * 100.0;
		Invoker->SetGenerationRadii(GenerationRadius, GenerationRadius + Profile.NavigationPaddingMeters * 100.0);

		// The volume brush is built in actor space along X, so yawing the actor onto the
		// route direction keeps the box route-local instead of an axis-aligned diagonal hull.
		const FVector RouteCenter = (Start + End) * 0.5;
		const FVector2D RouteDelta(End.X - Start.X, End.Y - Start.Y);
		const double RouteYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(RouteDelta.Y, RouteDelta.X));
		const FVector RouteExtent(
			RouteDelta.Size() * 0.5 + Profile.NavigationPaddingMeters * 100.0,
			Profile.NavigationPaddingMeters * 100.0,
			Profile.NavigationHeightMeters * 50.0);
		NavBounds->SetActorLocation(RouteCenter);
		NavBounds->SetActorRotation(FRotator(0.0, RouteYawDegrees, 0.0));
		OutResult.RuntimeRouteVolumeYawDegrees = RouteYawDegrees;
		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		Builder->X = RouteExtent.X * 2.0;
		Builder->Y = RouteExtent.Y * 2.0;
		Builder->Z = RouteExtent.Z * 2.0;
		Builder->Hollow = false;
		Builder->Tessellated = false;
		UActorFactory::CreateBrushForVolumeActor(NavBounds, Builder);
		SetIdentity(*NavBounds, TEXT("RouteNavigation"), Bundle, Profile, false);
		if (UBrushComponent* Brush = NavBounds->GetBrushComponent())
		{
			Brush->UpdateBounds();
		}
		NavBounds->ReregisterAllComponents();
		const FBox NavigationBox = NavBounds->GetComponentsBoundingBox(true);
		if (!NavigationBox.IsValid)
		{
			OutError = TEXT("Generated route navigation volume has invalid bounds.");
			return false;
		}

		UNavigationSystemV1* Navigation = Cast<UNavigationSystemV1>(World->GetNavigationSystem());
		if (Navigation == nullptr)
		{
			OutError = TEXT("World has no NavigationSystemV1 for the accepted route.");
			return false;
		}
		if (Navigation->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock))
		{
			Navigation->RemoveNavigationBuildLock(
				ENavigationBuildLock::AsyncLoadLock,
				UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		}
		Invoker->RegisterWithNavigationSystem(*Navigation);
		Navigation->OnNavigationBoundsUpdated(NavBounds);
		Navigation->InitializeLevelCollisions();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It != NavBounds && It->GetComponentsBoundingBox(true).Intersect(NavigationBox))
			{
				UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(**It);
			}
		}
		Navigation->Tick(0.0f);
		FBoolProperty* InvokerOnlyProperty = FindFProperty<FBoolProperty>(
			UNavigationSystemV1::StaticClass(),
			TEXT("bGenerateNavigationOnlyAroundNavigationInvokers"));
		if (InvokerOnlyProperty == nullptr)
		{
			OutError = TEXT("Cannot access the UE invoker-only navigation setting.");
			return false;
		}
		const bool bInvokerOnly = Navigation->IsActiveTilesGenerationEnabled();
		if (bInvokerOnly)
		{
			// Empty Recast data cannot derive active tiles, so bootstrap only the bounded route volume.
			InvokerOnlyProperty->SetPropertyValue_InContainer(Navigation, false);
		}
		const auto RestoreInvokerOnly = [&]()
		{
			if (bInvokerOnly)
			{
				InvokerOnlyProperty->SetPropertyValue_InContainer(Navigation, true);
			}
		};
		ANavigationData* NavigationData = Navigation->GetDefaultNavDataInstance(FNavigationSystem::Create);
		if (NavigationData == nullptr)
		{
			RestoreInvokerOnly();
			OutError = TEXT("Navigation data was not created for the accepted route.");
			return false;
		}
		ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavigationData);
		if (Recast == nullptr)
		{
			RestoreInvokerOnly();
			OutError = TEXT("Accepted route requires the stock Recast navigation data.");
			return false;
		}
		TArray<FBox> RegisteredNavigationBounds;
		Navigation->GetNavigationBoundsForNavData(*Recast, RegisteredNavigationBounds);
		if (RegisteredNavigationBounds.IsEmpty())
		{
			RestoreInvokerOnly();
			OutError = FString::Printf(
				TEXT("Navigation did not register the generated route volume (volume_bounds=%s)."),
				*NavigationBox.ToString());
			return false;
		}
		if (Navigation->GetInvokerLocations().IsEmpty())
		{
			RestoreInvokerOnly();
			OutError = TEXT("Navigation did not retain the accepted route invoker.");
			return false;
		}
		Navigation->Build();
		NavigationData->EnsureBuildCompletion();
		RestoreInvokerOnly();
		if (bInvokerOnly)
		{
			Recast->UpdateActiveTiles(Navigation->GetInvokerLocations());
		}

		if (!ProbeRouteCollision(World, Bundle, Profile, *Feature, OutResult, OutError))
		{
			return false;
		}

		FNavLocation ProjectedStart;
		FNavLocation ProjectedEnd;
		const FVector QueryExtent(500.0, 500.0, 500.0);
		const bool bProjectedStart = Navigation->ProjectPointToNavigation(
			Start + FVector(0.0, 0.0, 100.0), ProjectedStart, QueryExtent, NavigationData);
		const bool bProjectedEnd = Navigation->ProjectPointToNavigation(
			End + FVector(0.0, 0.0, 100.0), ProjectedEnd, QueryExtent, NavigationData);
		if (!bProjectedStart || !bProjectedEnd)
		{
			OutError = FString::Printf(
				TEXT("Navigation did not project both accepted route endpoints (start=%d end=%d active_set=%d built_tiles=%d nav_bounds=%s)."),
				bProjectedStart ? 1 : 0,
				bProjectedEnd ? 1 : 0,
				Recast->GetActiveTileSet().Num(),
				Recast->GetNumActiveTiles(),
				*Recast->GetNavMeshBounds().ToString());
			return false;
		}
		double PathLengthCentimeters = 0.0;
		const ENavigationQueryResult::Type PathResult = UNavigationSystemV1::GetPathLength(
			World,
			ProjectedStart.Location,
			ProjectedEnd.Location,
			PathLengthCentimeters,
			NavigationData);
		if (PathResult != ENavigationQueryResult::Success || PathLengthCentimeters <= 0.0)
		{
			OutError = TEXT("Navigation cannot traverse the accepted cross-cell gameplay route.");
			return false;
		}
		OutResult.bRuntimeNavigationProbed = true;
		OutResult.RuntimeNavigationPathMeters = PathLengthCentimeters * 0.01;
		return true;
	}

	bool CaptureAndCheckStructuralBudgets(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FName FeatureTag(*FString::Printf(TEXT("ProjectWorld.Feature=%s"), *Profile.RouteFeatureId));
		int32 RouteFeatureActorCount = 0;
		bool bRouteUsesOnlyProceduralGeometry = true;
		bool bRouteUsesNoInstancing = true;
		bool bRouteActorsSpatiallyLoaded = true;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<AWorldPartitionHLOD>())
			{
				++OutResult.HlodProxyActorCount;
			}
			if (!It->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
			{
				continue;
			}
			++OutResult.GeneratedActorCount;
			if (It->GetIsSpatiallyLoaded())
			{
				++OutResult.SpatiallyLoadedActorCount;
			}
			const FString Role = RuntimeRole(**It);
			if (Role == TEXT("RouteStart") || Role == TEXT("RouteEnd"))
			{
				OutResult.RuntimeRouteSpatialActorCount += It->GetIsSpatiallyLoaded() ? 1 : 0;
			}
			else if (Role == TEXT("RouteNavigation"))
			{
				OutResult.RuntimeAlwaysLoadedActorCount += It->GetIsSpatiallyLoaded() ? 0 : 1;
			}
			const bool bRouteFeatureActor = It->Tags.Contains(FeatureTag);
			if (bRouteFeatureActor)
			{
				++RouteFeatureActorCount;
				bRouteActorsSpatiallyLoaded &= It->GetIsSpatiallyLoaded();
			}
			bool bHasProceduralMesh = false;
			TInlineComponentArray<UActorComponent*> Components;
			It->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				if (UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(Component))
				{
					bHasProceduralMesh = true;
					OutResult.ProceduralMeshSectionDrawCallUpperBound += Mesh->GetNumSections();
					for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
					{
						if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex))
						{
							OutResult.ProceduralMeshBufferBytes += Section->ProcVertexBuffer.GetAllocatedSize();
							OutResult.ProceduralMeshBufferBytes += Section->ProcIndexBuffer.GetAllocatedSize();
						}
					}
				}
				if (bRouteFeatureActor && Cast<UStaticMeshComponent>(Component) != nullptr)
				{
					bRouteUsesOnlyProceduralGeometry = false;
				}
				if (bRouteFeatureActor && Cast<UInstancedStaticMeshComponent>(Component) != nullptr)
				{
					bRouteUsesNoInstancing = false;
				}
			}
			if (bRouteFeatureActor)
			{
				bRouteUsesOnlyProceduralGeometry &= bHasProceduralMesh;
			}
		}

		OutResult.bRuntimeStreamingPolicyProbed =
			OutResult.RuntimeRouteSpatialActorCount == 2 &&
			OutResult.RuntimeAlwaysLoadedActorCount == 1 &&
			RouteFeatureActorCount == OutResult.CrossCellRoadExpectedFragmentCount &&
			bRouteActorsSpatiallyLoaded;
		OutResult.bRuntimeNanitePolicyProbed =
			RouteFeatureActorCount > 0 && bRouteUsesOnlyProceduralGeometry;
		OutResult.bRuntimeInstancingPolicyProbed =
			RouteFeatureActorCount > 0 && bRouteUsesNoInstancing;
		OutResult.bRuntimeHlodPolicyProbed = OutResult.HlodProxyActorCount == 0;
		if (!OutResult.bRuntimeStreamingPolicyProbed || !OutResult.bRuntimeNanitePolicyProbed ||
			!OutResult.bRuntimeInstancingPolicyProbed || !OutResult.bRuntimeHlodPolicyProbed)
		{
			OutError = FString::Printf(
				TEXT("Runtime policy mismatch: streamed_route=%d always_loaded=%d route_cells=%d nanite=%d instancing=%d hlod=%d."),
				OutResult.RuntimeRouteSpatialActorCount,
				OutResult.RuntimeAlwaysLoadedActorCount,
				RouteFeatureActorCount,
				OutResult.bRuntimeNanitePolicyProbed ? 1 : 0,
				OutResult.bRuntimeInstancingPolicyProbed ? 1 : 0,
				OutResult.bRuntimeHlodPolicyProbed ? 1 : 0);
			return false;
		}

		if (OutResult.GeneratedSourceBytes > Profile.Budgets.GeneratedSourceBytes ||
			OutResult.ProceduralMeshBufferBytes > Profile.Budgets.ProceduralMeshBufferBytes ||
			OutResult.GeneratedActorCount > Profile.Budgets.GeneratedActorCount ||
			OutResult.ProceduralMeshSectionDrawCallUpperBound > Profile.Budgets.MeshSectionDrawCallUpperBound)
		{
			OutError = FString::Printf(
				TEXT("Runtime structural budget exceeded: source=%lld mesh=%lld actors=%d sections=%d."),
				OutResult.GeneratedSourceBytes,
				OutResult.ProceduralMeshBufferBytes,
				OutResult.GeneratedActorCount,
				OutResult.ProceduralMeshSectionDrawCallUpperBound);
			return false;
		}
		OutResult.bRuntimeStructuralBudgetsPassed = true;
		return true;
	}
}
