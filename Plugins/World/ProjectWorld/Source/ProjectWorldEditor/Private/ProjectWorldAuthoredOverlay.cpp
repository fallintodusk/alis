// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAnchorPlacement.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldSchemaReference.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldAuthoredOverlay
{
	namespace
	{
		const TCHAR* ExpectedSchemaFilename = TEXT("project_world_authored_overlay.schema.json");

		bool IsIdentifierToken(const FString& Value)
		{
			if (Value.IsEmpty())
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLowerAscii && !bDigitAscii && Character != TEXT('_'))
				{
					return false;
				}
			}
			return true;
		}

		bool IsGridIdentifier(const FString& Value)
		{
			if (!Value.StartsWith(TEXT("grid_")) || Value.Len() != 21)
			{
				return false;
			}
			for (int32 Index = 5; Index < Value.Len(); ++Index)
			{
				const TCHAR Character = Value[Index];
				const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
				const bool bHex = Character >= TEXT('a') && Character <= TEXT('f');
				if (!bDigit && !bHex)
				{
					return false;
				}
			}
			return true;
		}

		bool ReadRequiredString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			FString& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Missing or empty authored-overlay string: %s"), Name);
				return false;
			}
			return true;
		}

		bool ReadFiniteNumber(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			double& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetNumberField(Name, OutValue) || !FMath::IsFinite(OutValue))
			{
				OutError = FString::Printf(TEXT("Missing or non-finite authored-overlay number: %s"), Name);
				return false;
			}
			return true;
		}

		bool IsMaskExclusion(const FString& Value)
		{
			return Value == TEXT("vegetation") || Value == TEXT("roads") ||
				Value == TEXT("buildings");
		}

		bool HasOnlyFields(
			const TSharedPtr<FJsonObject>& Object,
			std::initializer_list<const TCHAR*> Allowed,
			FString& OutError)
		{
			TSet<FString> AllowedFields;
			for (const TCHAR* Field : Allowed)
			{
				AllowedFields.Add(Field);
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
			{
				if (!AllowedFields.Contains(Field.Key))
				{
					OutError = FString::Printf(TEXT("Unknown authored-overlay field: %s"), *Field.Key);
					return false;
				}
			}
			return true;
		}

		bool ReadPlacementContract(
			const TSharedPtr<FJsonObject>& Anchor,
			FProjectWorldAnchorRecord& Record,
			FString& OutError)
		{
			FString PlacementClass;
			FString VerticalMode;
			if (!ReadRequiredString(Anchor, TEXT("placement_class"), PlacementClass, OutError) ||
				!ReadRequiredString(Anchor, TEXT("provenance_ref"), Record.ProvenanceRef, OutError) ||
				!IsIdentifierToken(Record.ProvenanceRef) ||
				!ReadFiniteNumber(
					Anchor,
					TEXT("horizontal_tolerance_m"),
					Record.HorizontalToleranceMeters,
					OutError) ||
				!ReadRequiredString(Anchor, TEXT("vertical_mode"), VerticalMode, OutError) ||
				!ReadFiniteNumber(
					Anchor,
					TEXT("vertical_tolerance_m"),
					Record.VerticalToleranceMeters,
					OutError) ||
				Record.VerticalToleranceMeters <= 0.0)
			{
				OutError = TEXT("Placement contract is incomplete.");
				return false;
			}

			double ClassMaximumMeters = 0.0;
			if (PlacementClass == TEXT("precision"))
			{
				Record.PlacementClass = EProjectWorldPlacementClass::Precision;
				ClassMaximumMeters = 2.0;
			}
			else if (PlacementClass == TEXT("standard"))
			{
				Record.PlacementClass = EProjectWorldPlacementClass::Standard;
				ClassMaximumMeters = 5.0;
			}
			else
			{
				OutError = TEXT("Placement class must be precision or standard.");
				return false;
			}
			if (Record.HorizontalToleranceMeters <= 0.0 ||
				Record.HorizontalToleranceMeters > ClassMaximumMeters)
			{
				OutError = TEXT("Horizontal tolerance exceeds its placement-class budget.");
				return false;
			}

			if (VerticalMode == TEXT("surface_snap"))
			{
				Record.VerticalMode = EProjectWorldVerticalMode::SurfaceSnap;
				if (Anchor->HasField(TEXT("vertical_datum")) ||
					Anchor->HasField(TEXT("vertical_provenance_ref")) ||
					Anchor->HasField(TEXT("height_m")))
				{
					OutError = TEXT("Surface-snap placement must not declare absolute-height fields.");
					return false;
				}
				return true;
			}
			if (VerticalMode != TEXT("absolute"))
			{
				OutError = TEXT("Vertical mode must be surface_snap or absolute.");
				return false;
			}

			Record.VerticalMode = EProjectWorldVerticalMode::Absolute;
			if (!ReadRequiredString(
					Anchor,
					TEXT("vertical_provenance_ref"),
					Record.VerticalProvenanceRef,
					OutError) ||
				!IsIdentifierToken(Record.VerticalProvenanceRef) ||
				!ReadRequiredString(Anchor, TEXT("vertical_datum"), Record.VerticalDatum, OutError) ||
				!ReadFiniteNumber(Anchor, TEXT("height_m"), Record.HeightMeters, OutError) ||
				Record.VerticalDatum.IsEmpty())
			{
				OutError = TEXT("Absolute placement requires datum, height and positive vertical tolerance.");
				return false;
			}
			return true;
		}

		bool ReadOptionalPlacement(
			const TSharedPtr<FJsonObject>& Anchor,
			FProjectWorldAnchorRecord& Record,
			FString& OutError)
		{
			const TSharedPtr<FJsonObject>* Orientation = nullptr;
			if (Anchor->TryGetObjectField(TEXT("orientation"), Orientation))
			{
				if (!HasOnlyFields(*Orientation, {TEXT("yaw_degrees")}, OutError) ||
					!ReadFiniteNumber(*Orientation, TEXT("yaw_degrees"), Record.YawDegrees, OutError) ||
					Record.YawDegrees < -360.0 || Record.YawDegrees > 360.0)
				{
					OutError = TEXT("Anchor orientation must declare yaw_degrees in [-360, 360].");
					return false;
				}
			}
			const TSharedPtr<FJsonObject>* Offset = nullptr;
			if (Anchor->TryGetObjectField(TEXT("offset"), Offset))
			{
				if (!HasOnlyFields(*Offset, {TEXT("east_m"), TEXT("north_m"), TEXT("up_m")}, OutError) ||
					!ReadFiniteNumber(*Offset, TEXT("east_m"), Record.OffsetEastMeters, OutError) ||
					!ReadFiniteNumber(*Offset, TEXT("north_m"), Record.OffsetNorthMeters, OutError) ||
					!ReadFiniteNumber(*Offset, TEXT("up_m"), Record.OffsetUpMeters, OutError))
				{
					OutError = TEXT("Anchor offset must declare east_m, north_m and up_m.");
					return false;
				}
			}
			return true;
		}

		bool ReadAnchor(
			const TSharedPtr<FJsonObject>& Anchor,
			FProjectWorldAnchorRecord& Record,
			FString& OutError)
		{
			FString Kind;
			if (!Anchor->TryGetStringField(TEXT("kind"), Kind))
			{
				OutError = TEXT("Anchor declares no kind.");
				return false;
			}

			if (Kind == TEXT("coordinate"))
			{
				Record.Kind = EProjectWorldAnchorKind::Coordinate;
				if (!HasOnlyFields(Anchor, {TEXT("kind"), TEXT("placement_class"), TEXT("provenance_ref"),
						TEXT("horizontal_tolerance_m"), TEXT("canonical_crs"), TEXT("easting_m"),
						TEXT("northing_m"), TEXT("vertical_mode"), TEXT("vertical_provenance_ref"),
						TEXT("vertical_datum"), TEXT("height_m"),
						TEXT("vertical_tolerance_m"), TEXT("offset"), TEXT("orientation")}, OutError))
				{
					return false;
				}
				if (!ReadRequiredString(Anchor, TEXT("canonical_crs"), Record.CanonicalCrs, OutError) ||
					!ReadFiniteNumber(Anchor, TEXT("easting_m"), Record.EastingMeters, OutError) ||
					!ReadFiniteNumber(Anchor, TEXT("northing_m"), Record.NorthingMeters, OutError) ||
					!ReadPlacementContract(Anchor, Record, OutError))
				{
					OutError = TEXT("Coordinate anchor is missing a required field.");
					return false;
				}
				return ReadOptionalPlacement(Anchor, Record, OutError);
			}

			if (Kind == TEXT("feature"))
			{
				Record.Kind = EProjectWorldAnchorKind::Feature;
				if (!HasOnlyFields(Anchor, {TEXT("kind"), TEXT("placement_class"), TEXT("provenance_ref"),
						TEXT("horizontal_tolerance_m"), TEXT("vertical_mode"), TEXT("vertical_datum"),
						TEXT("vertical_provenance_ref"), TEXT("height_m"),
						TEXT("vertical_tolerance_m"), TEXT("feature_id"),
						TEXT("offset"), TEXT("orientation"), TEXT("expected_easting_m"),
						TEXT("expected_northing_m"), TEXT("expected_feature_class"),
						TEXT("expected_geometry_type")}, OutError))
				{
					return false;
				}
				if (!ReadRequiredString(Anchor, TEXT("feature_id"), Record.FeatureId, OutError) ||
					!ReadRequiredString(Anchor, TEXT("expected_feature_class"), Record.ExpectedFeatureClass, OutError) ||
					!ReadRequiredString(Anchor, TEXT("expected_geometry_type"), Record.ExpectedGeometryType, OutError) ||
					!ReadFiniteNumber(Anchor, TEXT("expected_easting_m"), Record.ExpectedEastingMeters, OutError) ||
					!ReadFiniteNumber(Anchor, TEXT("expected_northing_m"), Record.ExpectedNorthingMeters, OutError) ||
					!ReadPlacementContract(Anchor, Record, OutError))
				{
					OutError = TEXT("Feature anchor requires identity, expected geometry and a valid placement contract.");
					return false;
				}
				return ReadOptionalPlacement(Anchor, Record, OutError);
			}

			if (Kind == TEXT("mask"))
			{
				Record.Kind = EProjectWorldAnchorKind::Mask;
				if (!HasOnlyFields(Anchor, {TEXT("kind"), TEXT("canonical_crs"), TEXT("bounds_m"), TEXT("excludes")}, OutError))
				{
					return false;
				}
				const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
				if (!ReadRequiredString(Anchor, TEXT("canonical_crs"), Record.CanonicalCrs, OutError) ||
					!Anchor->TryGetArrayField(TEXT("bounds_m"), Bounds) || Bounds->Num() != 4)
				{
					OutError = TEXT("Mask anchor requires canonical_crs and four bounds values.");
					return false;
				}
				double ParsedBounds[4] = {};
				for (int32 Index = 0; Index < 4; ++Index)
				{
					if (!(*Bounds)[Index]->TryGetNumber(ParsedBounds[Index]) ||
						!FMath::IsFinite(ParsedBounds[Index]))
					{
						OutError = TEXT("Mask anchor bounds must contain four finite numbers.");
						return false;
					}
				}
				Record.BoundsMeters = FVector4d(
					ParsedBounds[0], ParsedBounds[1], ParsedBounds[2], ParsedBounds[3]);
				if (Record.BoundsMeters.X >= Record.BoundsMeters.Z ||
					Record.BoundsMeters.Y >= Record.BoundsMeters.W)
				{
					OutError = TEXT("Mask anchor bounds must be ordered west,south,east,north.");
					return false;
				}
				const TArray<TSharedPtr<FJsonValue>>* Excludes = nullptr;
				if (Anchor->TryGetArrayField(TEXT("excludes"), Excludes))
				{
					for (const TSharedPtr<FJsonValue>& Value : *Excludes)
					{
						FString Exclusion;
						if (!Value->TryGetString(Exclusion) || !IsMaskExclusion(Exclusion))
						{
							OutError = TEXT("Mask anchor excludes an unsupported generated layer.");
							return false;
						}
						Record.Excludes.Add(MoveTemp(Exclusion));
					}
				}
				return true;
			}

			OutError = FString::Printf(TEXT("Unsupported anchor kind: %s"), *Kind);
			return false;
		}
	}

	bool Load(
		const FString& Path,
		FProjectWorldAuthoredOverlaySet& OutSet,
		FString& OutErrorCode,
		FString& OutError)
	{
		OutError.Reset();
		OutErrorCode = TEXT("authored-overlay-contract");
		FProjectWorldAuthoredOverlaySet Candidate;
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Path))
		{
			OutError = FString::Printf(TEXT("Authored overlay set is unreadable: %s"), *Path);
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Source);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("Authored overlay set is not valid JSON: %s"), *Path);
			return false;
		}
		if (!HasOnlyFields(Root, {TEXT("$schema"), TEXT("schema_version"), TEXT("overlay_set_id"),
				TEXT("world_data_plugin"), TEXT("grid_id"), TEXT("resolver_version"), TEXT("provenance"),
				TEXT("overlays")}, OutError))
		{
			return false;
		}
		FString Schema;
		double SchemaVersion = 0.0;
		if (!Root->TryGetStringField(TEXT("$schema"), Schema) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(
				Path,
				Schema,
				ExpectedSchemaFilename,
				OutError) ||
			!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1)
		{
			OutError = TEXT("Authored overlay set does not declare the frozen schema.");
			return false;
		}
		if (!ReadRequiredString(Root, TEXT("overlay_set_id"), Candidate.OverlaySetId, OutError) ||
			!IsIdentifierToken(Candidate.OverlaySetId) ||
			!ReadRequiredString(Root, TEXT("world_data_plugin"), Candidate.WorldDataPluginName, OutError) ||
			!ReadRequiredString(Root, TEXT("grid_id"), Candidate.GridId, OutError) ||
			!IsGridIdentifier(Candidate.GridId))
		{
			OutError = TEXT("Authored overlay set identity does not match the frozen schema.");
			return false;
		}
		FProjectWorldDataRoots WorldDataRoots;
		if (!FProjectWorldDataRoots::Resolve(
			Candidate.WorldDataPluginName,
			WorldDataRoots,
			OutError))
		{
			return false;
		}
		double ResolverVersion = 0.0;
		if (!Root->TryGetNumberField(TEXT("resolver_version"), ResolverVersion) ||
			ResolverVersion != SupportedResolverVersion)
		{
			// A changed resolver must not silently re-place authored content.
			OutErrorCode = TEXT("authored-overlay-resolver");
			OutError = FString::Printf(
				TEXT("Authored overlay set targets resolver version %.17g; this build implements %d."),
				ResolverVersion,
				SupportedResolverVersion);
			return false;
		}
		Candidate.ResolverVersion = static_cast<int32>(ResolverVersion);

		const TArray<TSharedPtr<FJsonValue>>* ProvenanceEntries = nullptr;
		if (!Root->TryGetArrayField(TEXT("provenance"), ProvenanceEntries) ||
			ProvenanceEntries->IsEmpty())
		{
			OutError = TEXT("Authored overlay set declares no provenance records.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *ProvenanceEntries)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			FProjectWorldAnchorProvenance Provenance;
			if (!Value->TryGetObject(Object) || Object == nullptr ||
				!HasOnlyFields(
					*Object,
					{TEXT("provenance_id"), TEXT("horizontal_accuracy_m"),
						TEXT("vertical_datum"), TEXT("vertical_accuracy_m")},
					OutError) ||
				!ReadRequiredString(*Object, TEXT("provenance_id"), Provenance.ProvenanceId, OutError) ||
				!IsIdentifierToken(Provenance.ProvenanceId) ||
				Candidate.Provenance.Contains(Provenance.ProvenanceId))
			{
				OutError = TEXT("Authored-overlay provenance identity is invalid or duplicated.");
				return false;
			}

			const bool bDeclaresHorizontalAccuracy = (*Object)->HasField(TEXT("horizontal_accuracy_m"));
			const bool bDeclaresVerticalDatum = (*Object)->HasField(TEXT("vertical_datum"));
			const bool bDeclaresVerticalAccuracy = (*Object)->HasField(TEXT("vertical_accuracy_m"));
			const bool bReadsHorizontalAccuracy = !bDeclaresHorizontalAccuracy ||
				((*Object)->HasTypedField<EJson::Number>(TEXT("horizontal_accuracy_m")) &&
					(*Object)->TryGetNumberField(
						TEXT("horizontal_accuracy_m"),
						Provenance.HorizontalAccuracyMeters));
			const bool bReadsVerticalDatum = !bDeclaresVerticalDatum ||
				((*Object)->HasTypedField<EJson::String>(TEXT("vertical_datum")) &&
					(*Object)->TryGetStringField(TEXT("vertical_datum"), Provenance.VerticalDatum));
			const bool bReadsVerticalAccuracy = !bDeclaresVerticalAccuracy ||
				((*Object)->HasTypedField<EJson::Number>(TEXT("vertical_accuracy_m")) &&
					(*Object)->TryGetNumberField(
						TEXT("vertical_accuracy_m"),
						Provenance.VerticalAccuracyMeters));
			if (!bReadsHorizontalAccuracy || !bReadsVerticalDatum || !bReadsVerticalAccuracy ||
				(!bDeclaresHorizontalAccuracy && !bDeclaresVerticalAccuracy) ||
				(bDeclaresHorizontalAccuracy &&
					(!FMath::IsFinite(Provenance.HorizontalAccuracyMeters) ||
						Provenance.HorizontalAccuracyMeters < 0.0)) ||
				bDeclaresVerticalDatum != bDeclaresVerticalAccuracy ||
				(bDeclaresVerticalDatum &&
					(Provenance.VerticalDatum.IsEmpty() ||
						!FMath::IsFinite(Provenance.VerticalAccuracyMeters) ||
						Provenance.VerticalAccuracyMeters < 0.0)))
			{
				OutError = TEXT("Provenance axes must be declared with valid types and finite accuracy values.");
				return false;
			}
			Provenance.bHasHorizontalAccuracy = bDeclaresHorizontalAccuracy;
			Provenance.bHasVerticalAccuracy = bDeclaresVerticalAccuracy;
			Candidate.Provenance.Add(Provenance.ProvenanceId, MoveTemp(Provenance));
		}

		const TArray<TSharedPtr<FJsonValue>>* Overlays = nullptr;
		if (!Root->TryGetArrayField(TEXT("overlays"), Overlays))
		{
			OutError = TEXT("Authored overlay set declares no overlays array.");
			return false;
		}
		TSet<FString> SeenIds;
		for (const TSharedPtr<FJsonValue>& Value : *Overlays)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object))
			{
				OutError = TEXT("Authored overlay entry is not an object.");
				return false;
			}
			if (!HasOnlyFields(*Object, {TEXT("overlay_id"), TEXT("authored_package"), TEXT("anchor")}, OutError))
			{
				return false;
			}
			FProjectWorldAuthoredOverlay Overlay;
			const TSharedPtr<FJsonObject>* Anchor = nullptr;
			if (!(*Object)->TryGetStringField(TEXT("overlay_id"), Overlay.OverlayId) ||
				!(*Object)->TryGetStringField(TEXT("authored_package"), Overlay.AuthoredPackage) ||
				!(*Object)->TryGetObjectField(TEXT("anchor"), Anchor))
			{
				OutError = TEXT("Authored overlay entry is missing a required field.");
				return false;
			}
			if (!IsIdentifierToken(Overlay.OverlayId))
			{
				OutError = FString::Printf(
					TEXT("Authored overlay id does not match the frozen schema: %s"),
					*Overlay.OverlayId);
				return false;
			}
			if (SeenIds.Contains(Overlay.OverlayId))
			{
				OutError = FString::Printf(TEXT("Authored overlay id is duplicated: %s"), *Overlay.OverlayId);
				return false;
			}
			// Authored content never lives under a generated root, and the
			// reference is soft by construction: regeneration must not be able
			// to reach it through ownership or outer relationships.
			if (!WorldDataRoots.IsAuthoredPackage(Overlay.AuthoredPackage))
			{
				OutError = FString::Printf(
					TEXT("Authored package escapes the authored root: %s"), *Overlay.AuthoredPackage);
				return false;
			}
			if (!ReadAnchor(*Anchor, Overlay.Anchor, OutError))
			{
				return false;
			}
			if (Overlay.Anchor.Kind != EProjectWorldAnchorKind::Mask)
			{
				const FProjectWorldAnchorProvenance* Provenance =
					Candidate.Provenance.Find(Overlay.Anchor.ProvenanceRef);
				const FProjectWorldAnchorProvenance* VerticalProvenance =
					Overlay.Anchor.VerticalMode == EProjectWorldVerticalMode::Absolute
					? Candidate.Provenance.Find(Overlay.Anchor.VerticalProvenanceRef)
					: nullptr;
				if (Provenance == nullptr || !Provenance->bHasHorizontalAccuracy ||
					(Overlay.Anchor.VerticalMode == EProjectWorldVerticalMode::Absolute &&
						(VerticalProvenance == nullptr || !VerticalProvenance->bHasVerticalAccuracy ||
							VerticalProvenance->VerticalDatum != Overlay.Anchor.VerticalDatum)))
				{
					OutError = TEXT("Placement anchor has missing or incompatible provenance.");
					return false;
				}
			}
			SeenIds.Add(Overlay.OverlayId);
			Candidate.Overlays.Add(MoveTemp(Overlay));
		}

		if (!FProjectSha256::HashFile(Path, Candidate.SetHash) || Candidate.SetHash.Len() != 64)
		{
			OutError = FString::Printf(TEXT("Authored overlay set could not be hashed: %s"), *Path);
			return false;
		}
		OutSet = MoveTemp(Candidate);
		OutErrorCode.Reset();
		return true;
	}

	bool FeatureAnchorPoint(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAnchorRecord& Anchor,
		FVector2D& OutPoint,
		FString& OutError)
	{
		const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(Anchor.FeatureId);
		if (Feature == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Anchored canonical feature no longer exists: %s"),
				*Anchor.FeatureId);
			return false;
		}
		if (Feature->FeatureClass != Anchor.ExpectedFeatureClass)
		{
			OutError = FString::Printf(
				TEXT("Anchored canonical feature %s changed class from %s to %s."),
				*Anchor.FeatureId,
				*Anchor.ExpectedFeatureClass,
				*Feature->FeatureClass);
			return false;
		}
		if (Feature->GeometryType != Anchor.ExpectedGeometryType)
		{
			OutError = FString::Printf(
				TEXT("Anchored canonical feature %s changed geometry type from %s to %s."),
				*Anchor.FeatureId,
				*Anchor.ExpectedGeometryType,
				*Feature->GeometryType);
			return false;
		}

		const FVector2D Expected(
			Anchor.ExpectedEastingMeters,
			Anchor.ExpectedNorthingMeters);
		double BestDistanceSquared = TNumericLimits<double>::Max();
		bool bFoundPoint = false;
		auto ConsiderPoint = [&Expected, &BestDistanceSquared, &bFoundPoint, &OutPoint](
			const FVector2D& Point)
		{
			const double DistanceSquared = FVector2D::DistSquared(Expected, Point);
			if (!bFoundPoint || DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				OutPoint = Point;
				bFoundPoint = true;
			}
		};
		auto ConsiderPart = [&Expected, &ConsiderPoint](const TArray<FVector2D>& Part)
		{
			if (Part.Num() == 1)
			{
				ConsiderPoint(Part[0]);
				return;
			}
			for (int32 Index = 0; Index < Part.Num() - 1; ++Index)
			{
				const FVector2D Start = Part[Index];
				const FVector2D Delta = Part[Index + 1] - Start;
				const double LengthSquared = Delta.SizeSquared();
				const double Alpha = LengthSquared <= UE_DOUBLE_SMALL_NUMBER
					? 0.0
					: FMath::Clamp(FVector2D::DotProduct(Expected - Start, Delta) / LengthSquared, 0.0, 1.0);
				ConsiderPoint(Start + Delta * Alpha);
			}
		};
		for (const TArray<FVector2D>& Part : Feature->GeometryParts)
		{
			ConsiderPart(Part);
		}
		if (!bFoundPoint)
		{
			ConsiderPart(Feature->GeometryPoints);
		}
		if (!bFoundPoint)
		{
			OutError = FString::Printf(
				TEXT("Anchored canonical feature has no geometry: %s"),
				*Anchor.FeatureId);
			return false;
		}
		return true;
	}

	bool Resolve(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		const FProjectWorldAuthoredOverlay& Overlay,
		FProjectWorldAnchorResolution& OutResolution,
		FString& OutError)
	{
		OutResolution = FProjectWorldAnchorResolution();
		OutResolution.OverlayId = Overlay.OverlayId;

		if (Set.GridId != Bundle.GridId)
		{
			OutError = FString::Printf(
				TEXT("Authored overlay set targets grid %s but the canonical bundle is %s."),
				*Set.GridId,
				*Bundle.GridId);
			return false;
		}
		if (Set.WorldDataPluginName != Bundle.WorldDataPluginName)
		{
			OutError = FString::Printf(
				TEXT("Authored overlay set belongs to %s but the canonical bundle belongs to %s."),
				*Set.WorldDataPluginName,
				*Bundle.WorldDataPluginName);
			return false;
		}

		const FProjectWorldAnchorRecord& Anchor = Overlay.Anchor;
		if (Anchor.Kind == EProjectWorldAnchorKind::Mask)
		{
			// Masks are protected regeneration INPUTS, not placements.
			OutResolution.bPlaces = false;
			if (Anchor.CanonicalCrs != Bundle.CanonicalCrs)
			{
				OutError = FString::Printf(
					TEXT("Authored mask %s declares CRS %s but the canonical bundle is %s."),
					*Overlay.OverlayId, *Anchor.CanonicalCrs, *Bundle.CanonicalCrs);
				return false;
			}
			return true;
		}

		FVector2D CanonicalPoint = FVector2D::ZeroVector;
		double HeightMeters = 0.0;

		if (Anchor.Kind == EProjectWorldAnchorKind::Coordinate)
		{
			if (Anchor.CanonicalCrs != Bundle.CanonicalCrs)
			{
				OutError = FString::Printf(
					TEXT("Authored overlay %s declares CRS %s but the canonical bundle is %s."),
					*Overlay.OverlayId, *Anchor.CanonicalCrs, *Bundle.CanonicalCrs);
				return false;
			}
			CanonicalPoint = FVector2D(Anchor.EastingMeters, Anchor.NorthingMeters);
		}
		else
		{
			FString FeatureError;
			if (!FeatureAnchorPoint(Bundle, Anchor, CanonicalPoint, FeatureError))
			{
				// Fail closed: a vanished or geometry-less feature is reported,
				// never re-anchored to something nearby.
				OutError = FString::Printf(TEXT("Authored overlay %s: %s"), *Overlay.OverlayId, *FeatureError);
				return false;
			}
			const FVector2D Expected(Anchor.ExpectedEastingMeters, Anchor.ExpectedNorthingMeters);
			OutResolution.DriftMeters = FVector2D::Distance(Expected, CanonicalPoint);
		}

		CanonicalPoint += FVector2D(Anchor.OffsetEastMeters, Anchor.OffsetNorthMeters);
		if (!ProjectWorldAnchorPlacement::ResolveHeight(
			Bundle,
			Set,
			Anchor,
			CanonicalPoint,
			OutResolution.DriftMeters,
			OutResolution,
			HeightMeters,
			OutError))
		{
			OutError = FString::Printf(TEXT("Authored overlay %s: %s"), *Overlay.OverlayId, *OutError);
			return false;
		}
		HeightMeters += Anchor.OffsetUpMeters;
		OutResolution.bSurfaceSnapped =
			Anchor.VerticalMode == EProjectWorldVerticalMode::SurfaceSnap;

		OutResolution.WorldLocation = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle, FVector(CanonicalPoint, HeightMeters));
		// Canonical northing is mirrored into Unreal Y, so a canonical yaw
		// becomes its negation in world space.
		OutResolution.WorldRotation = FRotator(0.0, -Anchor.YawDegrees, 0.0);
		OutResolution.bPlaces = true;
		return true;
	}
}
