// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldTerrainSurface.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "GeoReferencingSystem.h"
#include "Misc/SecureHash.h"

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

	bool SampleTerrain(
		const FProjectWorldCanonicalCell& Cell,
		double X,
		double Y, double& OutHeight, FString& OutError)
	{
		const FProjectWorldCanonicalTerrain& Terrain = Cell.Terrain;
		if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || Terrain.SamplesX < 2 || Terrain.SamplesY < 2 ||
			Terrain.SampleSpacing.X <= 0 || Terrain.SampleSpacing.Y <= 0 || Terrain.HeightsMeters.Num() != Terrain.SamplesX * Terrain.SamplesY)
		{ OutError = TEXT("Terrain placement query has an invalid grid or coordinate."); return false; }
		for (const auto& Quad : Terrain.RefinedQuads)
		{
			const double Column = (X - Terrain.Bounds.X) / Terrain.SampleSpacing.X;
			const double Row = FProjectWorldCanonicalLoader::TerrainSampleRow(Terrain, Y);
			if (Column < Quad.Coordinate.X || Column > Quad.Coordinate.X + 1 || Row < Quad.Coordinate.Y || Row > Quad.Coordinate.Y + 1) continue;
			for (const FIntVector& Face : Quad.Triangles)
			{
				if (!Quad.Vertices.IsValidIndex(Face.X) || !Quad.Vertices.IsValidIndex(Face.Y) || !Quad.Vertices.IsValidIndex(Face.Z))
				{ OutError = TEXT("Refined placement query has an invalid triangle."); return false; }
				FProjectWorldSurfaceTriangle T{{Quad.Vertices[Face.X], Quad.Vertices[Face.Y], Quad.Vertices[Face.Z]}};
				bool bInside = true;
				for (int32 I=0; I<3; ++I)
				{
					const FVector D = T.Points[(I+1)%3] - T.Points[I];
					bInside &= D.X * (Y - T.Points[I].Y) - D.Y * (X - T.Points[I].X) >= -1e-8;
				}
				if (bInside) return ProjectWorldTerrainSurface::Height(T, FVector2D(X, Y), OutHeight, OutError);
			}
			OutError = TEXT("Refined placement query has missing final-surface coverage.");
			return false;
		}
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
		OutHeight = FMath::Lerp(
			FMath::Lerp(HeightAt(X0, Y0), HeightAt(X1, Y0), FracX),
			FMath::Lerp(HeightAt(X0, Y1), HeightAt(X1, Y1), FracX),
			FracY);
		if (!FMath::IsFinite(OutHeight)) { OutError = TEXT("Terrain placement query produced a nonfinite height."); return false; }
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
			const FGuid ExpectedGuid = StableGuid(Bundle.GridId + TEXT("|georeferencing"));
			if (!bUpdatingActor)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*StableObjectName(TEXT("ProjectWorld_Geo"), Bundle.GridId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ExpectedGuid;
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

			const bool bCurrentActor = bUpdatingActor && Geo->GetActorGuid() == ExpectedGuid &&
				Geo->Tags.Contains(GridTag) && !Geo->GetIsSpatiallyLoaded() &&
				Geo->PlanetShape == EPlanetShape::FlatPlanet &&
				Geo->ProjectedCRS == Bundle.CanonicalCrs && Geo->GeographicCRS == TEXT("EPSG:4326") &&
				Geo->bOriginLocationInProjectedCRS &&
				FMath::IsNearlyEqual(
					Geo->OriginProjectedCoordinatesEasting, Bundle.EngineGeoreferenceOriginMeters.X) &&
				FMath::IsNearlyEqual(
					Geo->OriginProjectedCoordinatesNorthing, Bundle.EngineGeoreferenceOriginMeters.Y) &&
				FMath::IsNearlyEqual(Geo->OriginProjectedCoordinatesUp, Bundle.HeightOriginMeters);
			if (!bCurrentActor)
			{
				Geo->Modify();
				Geo->Tags.Reset();
				Geo->Tags.Add(GeneratedTag);
				Geo->Tags.Add(GridTag);
				Geo->SetActorLabel(TEXT("ProjectWorld GeoReferencing"));
				Geo->SetIsSpatiallyLoaded(false);
				Geo->PlanetShape = EPlanetShape::FlatPlanet;
				Geo->ProjectedCRS = Bundle.CanonicalCrs;
				Geo->GeographicCRS = TEXT("EPSG:4326");
				Geo->bOriginLocationInProjectedCRS = true;
				Geo->OriginProjectedCoordinatesEasting = Bundle.EngineGeoreferenceOriginMeters.X;
				Geo->OriginProjectedCoordinatesNorthing = Bundle.EngineGeoreferenceOriginMeters.Y;
				Geo->OriginProjectedCoordinatesUp = Bundle.HeightOriginMeters;
				Geo->ApplySettings();
			}

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
				if (bCurrentActor)
				{
					++OutResult.PreservedActorCount;
				}
				else if (bUpdatingActor)
				{
					++OutResult.UpdatedActorCount;
				}
				else
				{
					++OutResult.CreatedActorCount;
				}
				if (!bCurrentActor)
				{
					Geo->MarkPackageDirty();
				}
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
