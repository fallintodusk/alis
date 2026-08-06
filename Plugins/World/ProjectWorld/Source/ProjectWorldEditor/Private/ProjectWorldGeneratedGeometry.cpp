// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGeneratedGeometry.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "GeoReferencingSystem.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"
#include "NavigationSystem.h"
#include "ProceduralMeshComponent.h"

namespace ProjectWorldGeneratedGeometry
{
	const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));

	FGuid StableGuid(const FString& Value)
	{
		FGuid Guid;
		const FString Digest = FMD5::HashAnsiString(*Value);
		FGuid::ParseExact(Digest, EGuidFormats::Digits, Guid);
		return Guid;
	}

	FString StableObjectName(const FString& Prefix, const FString& Identity)
	{
		FString Name = Prefix + TEXT("_") + Identity;
		for (TCHAR& Character : Name)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		return Name.Left(96);
	}

	double SampleTerrain(
		const FProjectWorldCanonicalCell& Cell,
		double X,
		double Y)
	{
		const FProjectWorldCanonicalTerrain& Terrain = Cell.Terrain;
		const double SampleX = FMath::Clamp(
			(X - Terrain.Bounds.X) / Terrain.SampleSpacing.X,
			0.0,
			static_cast<double>(Terrain.SamplesX - 1));
		const double SampleY = FMath::Clamp(
			FProjectWorldCanonicalLoader::TerrainSampleRow(Terrain, Y),
			0.0,
			static_cast<double>(Terrain.SamplesY - 1));
		const int32 X0 = FMath::FloorToInt(SampleX);
		const int32 Y0 = FMath::FloorToInt(SampleY);
		const int32 X1 = FMath::Min(X0 + 1, Terrain.SamplesX - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Terrain.SamplesY - 1);
		const double FracX = SampleX - X0;
		const double FracY = SampleY - Y0;
		auto HeightAt = [&Terrain](int32 Column, int32 Row)
		{
			return Terrain.HeightsMeters[Row * Terrain.SamplesX + Column];
		};
		return FMath::Lerp(
			FMath::Lerp(HeightAt(X0, Y0), HeightAt(X1, Y0), FracX),
			FMath::Lerp(HeightAt(X0, Y1), HeightAt(X1, Y1), FracX),
			FracY);
	}

	void CommitSection(
		UProceduralMeshComponent* Mesh,
		int32 SectionIndex,
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		const FLinearColor& Color,
		bool bCreateCollision,
		UMaterialInterface* Material)
	{
		TArray<FVector2D> UVs;
		UVs.SetNumZeroed(Vertices.Num());
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
			Vertices,
			Triangles,
			UVs,
			Normals,
			Tangents);
		TArray<FLinearColor> Colors;
		Colors.Init(Color, Vertices.Num());
		Mesh->CreateMeshSection_LinearColor(
			SectionIndex,
			Vertices,
			Triangles,
			Normals,
			UVs,
			Colors,
			Tangents,
			bCreateCollision);
		if (Material != nullptr)
		{
			Mesh->SetMaterial(SectionIndex, Material);
		}
	}

	void BuildTerrainSection(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FVector& ActorOrigin,
		UProceduralMeshComponent* Mesh,
		int32& InOutSection,
		UMaterialInterface* Material,
		const FLinearColor& Color)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
		{
			for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
			{
				const int32 SampleIndex = Row * Cell.Terrain.SamplesX + Column;
				const FVector Canonical(
					Cell.Terrain.Bounds.X + Column * Cell.Terrain.SampleSpacing.X,
					FProjectWorldCanonicalLoader::TerrainRowNorthing(Cell.Terrain, Row),
					Cell.Terrain.HeightsMeters[SampleIndex]);
				Vertices.Add(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Canonical) - ActorOrigin);
			}
		}

		for (int32 Row = 0; Row < Cell.Terrain.SamplesY - 1; ++Row)
		{
			for (int32 Column = 0; Column < Cell.Terrain.SamplesX - 1; ++Column)
			{
				const int32 A = Row * Cell.Terrain.SamplesX + Column;
				const int32 B = A + 1;
				const int32 C = A + Cell.Terrain.SamplesX;
				const int32 D = C + 1;
				Triangles.Append({A, B, C, B, D, C});
			}
		}
		CommitSection(Mesh, InOutSection++, Vertices, Triangles, Color, true, Material);
	}

	bool BuildRoadSection(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		const FVector& ActorOrigin,
		UProceduralMeshComponent* Mesh,
		int32& InOutSection,
		UMaterialInterface* Material,
		const FLinearColor& Color,
		bool bCreateCollision)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		const double HalfWidth = FMath::Max(Feature.WidthMeters * 0.5, 1.0);
		const double DrapeStepMeters = FMath::Max(
			FMath::Min(Cell.Terrain.SampleSpacing.X, Cell.Terrain.SampleSpacing.Y) * 0.25,
			0.25);
		constexpr double SurfaceOffsetMeters = 0.65;
		for (const FProjectWorldCanonicalRepresentation& Representation : Feature.Representations)
		{
			if (Representation.CellId != Cell.CellId ||
				Representation.Kind != TEXT("road_fragment"))
			{
				continue;
			}

			for (const TArray<FVector2D>& Part : Representation.Parts)
			{
				for (int32 PointIndex = 0; PointIndex < Part.Num() - 1; ++PointIndex)
				{
					const FVector2D Start = Part[PointIndex];
					const FVector2D End = Part[PointIndex + 1];
					const FVector2D Direction = (End - Start).GetSafeNormal();
					if (Direction.IsNearlyZero())
					{
						continue;
					}
					const FVector2D Side(-Direction.Y * HalfWidth, Direction.X * HalfWidth);
					const int32 StepCount = FMath::Max(
						1,
						FMath::CeilToInt((End - Start).Length() / DrapeStepMeters));
					const int32 Base = Vertices.Num();
					for (int32 StepIndex = 0; StepIndex <= StepCount; ++StepIndex)
					{
						const double Alpha = static_cast<double>(StepIndex) / StepCount;
						const FVector2D Center = FMath::Lerp(Start, End, Alpha);
						const double Height = SampleTerrain(Cell, Center.X, Center.Y) + SurfaceOffsetMeters;
						Vertices.Add(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(Center + Side, Height)) - ActorOrigin);
						Vertices.Add(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(Center - Side, Height)) - ActorOrigin);
						if (StepIndex > 0)
						{
							const int32 Current = Base + StepIndex * 2;
							Triangles.Append({Current - 2, Current - 1, Current, Current - 1, Current + 1, Current});
						}
					}
				}
			}
		}

		if (Vertices.IsEmpty())
		{
			return false;
		}
		CommitSection(Mesh, InOutSection++, Vertices, Triangles, Color, bCreateCollision, Material);
		return true;
	}

	bool BuildBuildingSection(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		const FVector& ActorOrigin,
		UProceduralMeshComponent* Mesh,
		int32& InOutSection,
		UMaterialInterface* Material,
		const FLinearColor& Color)
	{
		if (Feature.OwnerCellId != Cell.CellId || Feature.GeometryParts.IsEmpty())
		{
			return false;
		}

		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		for (const TArray<FVector2D>& Part : Feature.GeometryParts)
		{
			FBox2D Bounds(ForceInit);
			for (const FVector2D& Point : Part)
			{
				Bounds += Point;
			}
			if (Part.Num() < 4 || !Bounds.bIsValid || Bounds.GetSize().GetMin() <= 0.0)
			{
				continue;
			}

			const FVector2D Center = Bounds.GetCenter();
			const double BaseHeight = SampleTerrain(Cell, Center.X, Center.Y);
			const double TopHeight = BaseHeight + FMath::Max(Feature.HeightMeters, 3.0);
			const FVector2D Min = Bounds.Min;
			const FVector2D Max = Bounds.Max;
			const FVector CanonicalVertices[] = {
				FVector(Min.X, Min.Y, BaseHeight), FVector(Max.X, Min.Y, BaseHeight),
				FVector(Max.X, Max.Y, BaseHeight), FVector(Min.X, Max.Y, BaseHeight),
				FVector(Min.X, Min.Y, TopHeight), FVector(Max.X, Min.Y, TopHeight),
				FVector(Max.X, Max.Y, TopHeight), FVector(Min.X, Max.Y, TopHeight)
			};
			const int32 Base = Vertices.Num();
			for (const FVector& Vertex : CanonicalVertices)
			{
				Vertices.Add(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Vertex) - ActorOrigin);
			}
			const int32 BoxTriangles[] = {
				0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
				0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
				2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7
			};
			for (const int32 Index : BoxTriangles)
			{
				Triangles.Add(Base + Index);
			}
		}
		if (Vertices.IsEmpty())
		{
			return false;
		}
		CommitSection(Mesh, InOutSection++, Vertices, Triangles, Color, true, Material);
		return true;
	}

	TSet<FString> ValidRoadCells(const FProjectWorldCanonicalFeature& Feature)
	{
		TSet<FString> Cells;
		for (const FProjectWorldCanonicalRepresentation& Representation : Feature.Representations)
		{
			if (Representation.Kind == TEXT("road_fragment") &&
				Representation.Parts.ContainsByPredicate([](const TArray<FVector2D>& Part)
				{
					return Part.Num() >= 2;
				}))
			{
				Cells.Add(Representation.CellId);
			}
		}
		return Cells;
	}

	int32 CountSharedRoadBoundaryPoints(
		const FProjectWorldCanonicalFeature& Feature,
		double ToleranceMeters)
	{
		int32 Matches = 0;
		for (int32 LeftIndex = 0; LeftIndex < Feature.Representations.Num(); ++LeftIndex)
		{
			const FProjectWorldCanonicalRepresentation& Left = Feature.Representations[LeftIndex];
			if (Left.Kind != TEXT("road_fragment"))
			{
				continue;
			}
			for (int32 RightIndex = LeftIndex + 1; RightIndex < Feature.Representations.Num(); ++RightIndex)
			{
				const FProjectWorldCanonicalRepresentation& Right = Feature.Representations[RightIndex];
				if (Right.Kind != TEXT("road_fragment") || Left.CellId == Right.CellId)
				{
					continue;
				}

				for (const TArray<FVector2D>& LeftPart : Left.Parts)
				{
					if (LeftPart.Num() < 2)
					{
						continue;
					}
					const FVector2D LeftEndpoints[] = {LeftPart[0], LeftPart.Last()};
					for (const TArray<FVector2D>& RightPart : Right.Parts)
					{
						if (RightPart.Num() < 2)
						{
							continue;
						}
						const FVector2D RightEndpoints[] = {RightPart[0], RightPart.Last()};
						for (const FVector2D& LeftPoint : LeftEndpoints)
						{
							for (const FVector2D& RightPoint : RightEndpoints)
							{
								if (FVector2D::Distance(LeftPoint, RightPoint) <= ToleranceMeters)
								{
									++Matches;
								}
							}
						}
					}
				}
			}
		}
		return Matches;
	}

	bool CreateOwnedActors(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bIncludeTerrain,
		int32 MaxRoadFeatures,
		int32 MaxBuildingFeatures,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError,
		const FProjectWorldGeometryPresentation* Presentation,
		const FString& CollisionRoadFeatureId)
	{
		UMaterialInterface* DefaultTerrainMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
		UMaterialInterface* DefaultShapeMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		const FProjectWorldGeometryPresentation DefaultPresentation{
			DefaultTerrainMaterial,
			DefaultShapeMaterial,
			DefaultShapeMaterial};
		const FProjectWorldGeometryPresentation& Style =
			Presentation == nullptr ? DefaultPresentation : *Presentation;

		TArray<FString> FeatureIds;
		Bundle.Features.GetKeys(FeatureIds);
		FeatureIds.Sort();
		TSet<FString> SelectedFeatureIds;
		TArray<FString> CrossCellRoadIds;
		TArray<FString> OtherRoadIds;
		TArray<FString> BuildingIds;
		for (const FString& FeatureId : FeatureIds)
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			if (Feature.FeatureClass == TEXT("road"))
			{
				(ValidRoadCells(Feature).Num() >= 2 ? CrossCellRoadIds : OtherRoadIds).Add(FeatureId);
			}
			else if (Feature.FeatureClass == TEXT("building"))
			{
				BuildingIds.Add(FeatureId);
			}
		}

		int32 SelectedRoads = 0;
		if (MaxRoadFeatures > 0 && Bundle.Cells.Num() > 1)
		{
			if (CrossCellRoadIds.IsEmpty())
			{
				OutError = TEXT("Canonical input has no road with valid fragments in two target cells.");
				return false;
			}
			OutResult.CrossCellRoadFeatureId = CrossCellRoadIds[0];
			const FProjectWorldCanonicalFeature& CrossCellRoad =
				Bundle.Features.FindChecked(OutResult.CrossCellRoadFeatureId);
			OutResult.CrossCellRoadExpectedFragmentCount = ValidRoadCells(CrossCellRoad).Num();
			OutResult.CrossCellRoadSharedBoundaryPointCount = CountSharedRoadBoundaryPoints(
				CrossCellRoad,
				Bundle.CoordinateQuantizationMeters);
			if (OutResult.CrossCellRoadSharedBoundaryPointCount == 0)
			{
				OutError = TEXT("Selected cross-cell road fragments do not share a canonical boundary coordinate.");
				return false;
			}
			SelectedFeatureIds.Add(OutResult.CrossCellRoadFeatureId);
			SelectedRoads = 1;
		}
		for (const FString& FeatureId : CrossCellRoadIds)
		{
			if (SelectedRoads >= MaxRoadFeatures)
			{
				break;
			}
			if (SelectedFeatureIds.Contains(FeatureId))
			{
				continue;
			}
			SelectedFeatureIds.Add(FeatureId);
			++SelectedRoads;
		}
		for (const FString& FeatureId : OtherRoadIds)
		{
			if (SelectedRoads >= MaxRoadFeatures)
			{
				break;
			}
			SelectedFeatureIds.Add(FeatureId);
			++SelectedRoads;
		}
		for (int32 Index = 0; Index < FMath::Min(MaxBuildingFeatures, BuildingIds.Num()); ++Index)
		{
			SelectedFeatureIds.Add(BuildingIds[Index]);
		}
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString ObjectName = StableObjectName(TEXT("ProjectWorld_Cell"), Cell.CellId);
			const FVector ActorOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
			const FName CellTag(*FString::Printf(TEXT("ProjectWorld.Cell=%s"), *Cell.CellId));
			AActor* Actor = nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->Tags.Contains(GeneratedTag) && It->Tags.Contains(CellTag))
				{
					if (Actor != nullptr)
					{
						OutError = FString::Printf(TEXT("Generated cell identity is duplicated: %s."), *Cell.CellId);
						return false;
					}
					Actor = *It;
				}
			}
			const bool bUpdatingActor = Actor != nullptr;
			if (!bUpdatingActor)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*ObjectName);
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = StableGuid(Bundle.GridId + TEXT("|") + Cell.CellId);
				Actor = World->SpawnActor<AActor>(
					AActor::StaticClass(),
					ActorOrigin,
					FRotator::ZeroRotator,
					SpawnParameters);
			}
			if (Actor == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create generated cell actor %s."), *Cell.CellId);
				return false;
			}

			Actor->Modify();
			Actor->Tags.Reset();
			Actor->Tags.Add(GeneratedTag);
			Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Grid=%s"), *Bundle.GridId)));
			Actor->Tags.Add(CellTag);
			Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Input=%s"), *Bundle.InputsHash)));
			Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Terrain=%s"), *Cell.Terrain.ArtifactHash)));
			Actor->Tags.Add(FName(TEXT("ProjectWorld.Geometry=preview_v2")));
			Actor->SetActorLabel(ObjectName);
			Actor->SetFolderPath(FName(*FString::Printf(TEXT("ProjectWorld/Generated/%s"), *Bundle.GridId)));
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = CollisionRoadFeatureId.IsEmpty();

			UProceduralMeshComponent* Mesh = bUpdatingActor
				? Actor->FindComponentByClass<UProceduralMeshComponent>()
				: NewObject<UProceduralMeshComponent>(Actor, TEXT("CanonicalGeometry"), RF_Transactional);
			if (Mesh == nullptr)
			{
				OutError = FString::Printf(TEXT("Generated cell actor has no procedural mesh: %s."), *Cell.CellId);
				return false;
			}
			if (bUpdatingActor)
			{
				Mesh->Modify();
				Mesh->ClearAllMeshSections();
				Mesh->ClearCollisionConvexMeshes();
			}
			else
			{
				Actor->SetRootComponent(Mesh);
				Actor->AddInstanceComponent(Mesh);
				Mesh->RegisterComponent();
			}
			Actor->SetActorLocation(ActorOrigin, false, nullptr, ETeleportType::TeleportPhysics);
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->bUseAsyncCooking = false;
			Mesh->bUseComplexAsSimpleCollision = !CollisionRoadFeatureId.IsEmpty();
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Mesh->SetCollisionObjectType(ECC_WorldStatic);
			Mesh->SetCanEverAffectNavigation(true);
			int32 SectionIndex = 0;
			if (bIncludeTerrain)
			{
				BuildTerrainSection(
					Bundle,
					Cell,
					ActorOrigin,
					Mesh,
					SectionIndex,
					Style.TerrainMaterial,
					Style.TerrainColor);
				++OutResult.TerrainSectionCount;
			}

			for (const FString& FeatureId : FeatureIds)
			{
				if (!SelectedFeatureIds.Contains(FeatureId))
				{
					continue;
				}
				const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
				if (Feature.FeatureClass == TEXT("road") &&
					BuildRoadSection(
						Bundle,
						Cell,
						Feature,
						ActorOrigin,
						Mesh,
						SectionIndex,
						Style.RoadMaterial,
						Style.RoadColor,
						Feature.FeatureId == CollisionRoadFeatureId))
				{
					Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Feature=%s"), *Feature.FeatureId)));
					++OutResult.RoadSectionCount;
					if (Feature.FeatureId == OutResult.CrossCellRoadFeatureId)
					{
						++OutResult.CrossCellRoadRealizedFragmentCount;
					}
				}
				else if (Feature.FeatureClass == TEXT("building") &&
					BuildBuildingSection(
						Bundle,
						Cell,
						Feature,
						ActorOrigin,
						Mesh,
						SectionIndex,
						Style.BuildingMaterial,
						Style.BuildingColor))
				{
					Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Feature=%s"), *Feature.FeatureId)));
					++OutResult.BuildingSectionCount;
				}
			}
			if (!CollisionRoadFeatureId.IsEmpty())
			{
				UNavigationSystemV1::UpdateComponentInNavOctree(*Mesh);
			}

			Actor->MarkPackageDirty();
			if (bUpdatingActor)
			{
				++OutResult.UpdatedActorCount;
			}
			else
			{
				++OutResult.CreatedActorCount;
			}
		}
		if (!OutResult.CrossCellRoadFeatureId.IsEmpty() &&
			OutResult.CrossCellRoadRealizedFragmentCount != OutResult.CrossCellRoadExpectedFragmentCount)
		{
			OutError = TEXT("Selected cross-cell road was not realized in every declared fragment cell.");
			return false;
		}
		return true;
	}

	double MeasureCoordinateRoundTrip(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bPersistActor,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TArray<FVector> ProjectedPoints;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			ProjectedPoints.AddUnique(FVector(Cell.Bounds.X, Cell.Bounds.Y, Bundle.HeightOriginMeters));
			ProjectedPoints.AddUnique(FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
			ProjectedPoints.AddUnique(FVector(Cell.Bounds.Z, Cell.Bounds.Y, Bundle.HeightOriginMeters));
			ProjectedPoints.AddUnique(FVector(Cell.Bounds.Z, Cell.Bounds.W, Bundle.HeightOriginMeters));
			ProjectedPoints.AddUnique(FVector(
				(Cell.Bounds.X + Cell.Bounds.Z) * 0.5,
				(Cell.Bounds.Y + Cell.Bounds.W) * 0.5,
				Bundle.HeightOriginMeters));
		}
		TArray<FString> FeatureIds;
		Bundle.Features.GetKeys(FeatureIds);
		FeatureIds.Sort();
		for (const FString& FeatureId : FeatureIds)
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			if (!Feature.GeometryPoints.IsEmpty())
			{
				ProjectedPoints.Add(FVector(Feature.GeometryPoints[0], Bundle.HeightOriginMeters));
				break;
			}
		}
		OutResult.GeoReferencingProbePointCount = ProjectedPoints.Num();

		if (Bundle.CanonicalCrs.StartsWith(TEXT("EPSG:")))
		{
			const FName GridTag(*FString::Printf(TEXT("ProjectWorld.Grid=%s"), *Bundle.GridId));
			AGeoReferencingSystem* Geo = nullptr;
			if (bPersistActor)
			{
				for (TActorIterator<AGeoReferencingSystem> It(World); It; ++It)
				{
					if (It->Tags.Contains(GeneratedTag) &&
						(It->Tags.Contains(GridTag) || It->GetName().EndsWith(Bundle.GridId)))
					{
						if (Geo != nullptr)
						{
							OutError = TEXT("Generated GeoReferencing identity is duplicated.");
							return TNumericLimits<double>::Max();
						}
						Geo = *It;
					}
				}
			}
			const bool bUpdatingActor = Geo != nullptr;
			if (!bUpdatingActor)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*StableObjectName(TEXT("ProjectWorld_Geo"), Bundle.GridId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = StableGuid(Bundle.GridId + TEXT("|georeferencing"));
				Geo = World->SpawnActor<AGeoReferencingSystem>(
					AGeoReferencingSystem::StaticClass(),
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					SpawnParameters);
			}
			if (Geo == nullptr)
			{
				OutError = TEXT("Cannot create the GeoReferencing system.");
				return TNumericLimits<double>::Max();
			}

			Geo->Modify();
			Geo->Tags.Reset();
			Geo->Tags.Add(GeneratedTag);
			Geo->Tags.Add(GridTag);
			Geo->SetActorLabel(TEXT("ProjectWorld GeoReferencing"));
			Geo->SetFolderPath(FName(TEXT("ProjectWorld/Generated")));
			Geo->SetIsSpatiallyLoaded(false);
			Geo->PlanetShape = EPlanetShape::FlatPlanet;
			Geo->ProjectedCRS = Bundle.CanonicalCrs;
			Geo->GeographicCRS = TEXT("EPSG:4326");
			Geo->bOriginLocationInProjectedCRS = true;
			Geo->OriginProjectedCoordinatesEasting = Bundle.OriginMeters.X;
			Geo->OriginProjectedCoordinatesNorthing = Bundle.OriginMeters.Y;
			Geo->OriginProjectedCoordinatesUp = Bundle.HeightOriginMeters;
			Geo->ApplySettings();

			double MaximumRoundTripError = 0.0;
			double MaximumPlacementError = 0.0;
			for (const FVector& Projected : ProjectedPoints)
			{
				FVector GeoEngine;
				FVector RoundTripped;
				Geo->ProjectedToEngine(Projected, GeoEngine);
				Geo->EngineToProjected(GeoEngine, RoundTripped);
				MaximumRoundTripError = FMath::Max(
					MaximumRoundTripError,
					FVector::Distance(Projected, RoundTripped));
				MaximumPlacementError = FMath::Max(
					MaximumPlacementError,
					FVector::Distance(
						GeoEngine,
						FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Projected)) * 0.01);
			}
			OutResult.GeoReferencingPlacementErrorMeters = MaximumPlacementError;
			OutResult.bGeoReferencingProbed = true;
			if (bPersistActor)
			{
				if (bUpdatingActor)
				{
					++OutResult.UpdatedActorCount;
				}
				else
				{
					++OutResult.CreatedActorCount;
				}
				Geo->MarkPackageDirty();
			}
			else
			{
				World->EditorDestroyActor(Geo, false);
			}
			if (MaximumPlacementError > Bundle.CoordinateQuantizationMeters)
			{
				OutError = TEXT("GeoReferencing and canonical actor placement transforms differ.");
			}
			return FMath::Max(MaximumRoundTripError, MaximumPlacementError);
		}

		double MaximumRoundTripError = 0.0;
		for (const FVector& Projected : ProjectedPoints)
		{
			const FVector Engine = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Projected);
			const FVector RoundTripped = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Engine);
			MaximumRoundTripError = FMath::Max(
				MaximumRoundTripError,
				FVector::Distance(Projected, RoundTripped));
		}
		return MaximumRoundTripError;
	}
}
