// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizationService.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldAuthoredOverlayRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldLayerInventory.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldPresentationMaterialRealization.h"
#include "ProjectWorldPresentationRealization.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRuntimeRealization.h"
#include "ProjectWorldSemanticEvidence.h"
#include "ProjectWorldWaterRealization.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionMiniMap.h"

namespace ProjectWorldRealization
{
	void Reject(
		FProjectWorldRealizationResult& Result,
		const TCHAR* Code,
		const FString& Message,
		const FString& Detail = FString())
	{
		Result.Status = TEXT("rejected");
		Result.ErrorCode = Code;
		Result.Message = Message;
		Result.Detail = Detail;
	}

	bool ValidateMapPackagePath(
		const FString& PackagePath,
		const FProjectWorldDataRoots& Roots)
	{
		return Roots.IsGeneratedPackage(PackagePath);
	}

	UWorld* LoadOrCreateWorld(
		const FString& MapPackagePath,
		bool& bOutExisting,
		FString& OutError)
	{
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			MapPackagePath,
			FPackageName::GetMapPackageExtension());
		bOutExisting = IFileManager::Get().FileExists(*MapFilename);
		if (bOutExisting)
		{
			if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
			{
				OutError = TEXT("Cannot load the generated target map.");
				return nullptr;
			}
			return GEditor->GetEditorWorldContext().World();
		}

		UWorld* World = GEditor->NewMap(true);
		if (World == nullptr || !World->IsPartitionedWorld())
		{
			OutError = TEXT("Cannot create a World Partition target map.");
			return nullptr;
		}
		return World;
	}

	bool SaveGeneratedWorld(UWorld* World, const FString& MapPackagePath)
	{
		// The editor creates this preview actor with a time-based UAID. It has no
		// cooked/runtime role and would make clean reconstruction change packages.
		TArray<AWorldPartitionMiniMap*> MiniMaps;
		for (TActorIterator<AWorldPartitionMiniMap> It(World); It; ++It)
		{
			MiniMaps.Add(*It);
		}
		for (AWorldPartitionMiniMap* MiniMap : MiniMaps)
		{
			if (!World->EditorDestroyActor(MiniMap, true))
			{
				return false;
			}
		}
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			MapPackagePath,
			FPackageName::GetMapPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(MapFilename), true);
		FString SavedFilename;
		if (!FEditorFileUtils::SaveLevel(World->PersistentLevel, MapFilename, &SavedFilename))
		{
			return false;
		}
		return FEditorFileUtils::SaveDirtyPackages(false, true, true, true, false, false);
	}
}

int32 FProjectWorldRealizationResult::ExitCode() const
{
	if (Status == TEXT("accepted"))
	{
		return 0;
	}
	if (ErrorCode.StartsWith(TEXT("save")))
	{
		return 6;
	}
	if (ErrorCode.StartsWith(TEXT("editor")) || ErrorCode.StartsWith(TEXT("geometry")) ||
		ErrorCode.StartsWith(TEXT("runtime")) || ErrorCode == TEXT("presentation-create") ||
		ErrorCode == TEXT("presentation-material"))
	{
		return 5;
	}
	return 4;
}

int32 FProjectWorldRealizationService::Run(
	const FProjectWorldRealizationRequest& Request,
	FProjectWorldRealizationResult& OutResult)
{
	using namespace ProjectWorldRealization;
	const double StartSeconds = FPlatformTime::Seconds();
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldCanonicalValidation Validation;
	if (!FProjectWorldCanonicalLoader::Load(Request.CompileResultPath, Bundle, Validation))
	{
		Reject(OutResult, *Validation.ErrorCode, Validation.Message, Validation.Detail);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	FProjectWorldDataRoots WorldDataRoots;
	FString WorldDataRootsError;
	if (!FProjectWorldDataRoots::Resolve(
		Bundle.WorldDataPluginName,
		WorldDataRoots,
		WorldDataRootsError))
	{
		Reject(
			OutResult,
			TEXT("world-data-owner"),
			TEXT("Canonical inputs declare an invalid world-data owner."),
			WorldDataRootsError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	FProjectWorldPresentationProfile PresentationProfile;
	FProjectWorldRuntimeProfile RuntimeProfile;
	FProjectWorldAuthoredOverlaySet AuthoredOverlaySet;
	FProjectWorldRealizationProfile RealizationProfile;
	FProjectWorldLayerDirtyInput LayerDirtyInput;
	FProjectWorldPresentationResources PresentationResources;
	FString PresentationErrorCode;
	FString PresentationError;
	const bool bNeedsPresentation = Request.Mode != EProjectWorldRealizationMode::Delete;
	const bool bNeedsRuntime = bNeedsPresentation && !Request.RuntimeProfilePath.IsEmpty();
	const bool bNeedsLayerPlan = bNeedsPresentation && !Request.RealizationProfilePath.IsEmpty();
	const bool bNeedsDirtyInput = bNeedsLayerPlan && !Request.LayerDirtyInputPath.IsEmpty();
	if (bNeedsLayerPlan &&
		(!ProjectWorldRealizationProfile::Load(
			Request.RealizationProfilePath,
			RealizationProfile,
			PresentationErrorCode,
			PresentationError) ||
		RealizationProfile.WorldDataPluginName != Bundle.WorldDataPluginName ||
		RealizationProfile.CanonicalProfileId != Bundle.ProfileId ||
		RealizationProfile.MapPackagePath != Request.MapPackagePath))
	{
		Reject(
			OutResult,
			PresentationErrorCode.IsEmpty() ? TEXT("realization-profile-identity") : *PresentationErrorCode,
			TEXT("Realization profile is not accepted for these canonical inputs and map."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsLayerPlan && Request.Mode == EProjectWorldRealizationMode::Apply && !bNeedsDirtyInput)
	{
		Reject(
			OutResult,
			TEXT("layer-dirty-input-required"),
			TEXT("Layer Apply requires an authenticated base/operator dirty input."));
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsDirtyInput &&
		(!ProjectWorldLayerDirtyInput::Load(
			Request.LayerDirtyInputPath,
			LayerDirtyInput,
			PresentationErrorCode,
			PresentationError) ||
		LayerDirtyInput.RealizationProfileId != RealizationProfile.ProfileId))
	{
		Reject(
			OutResult,
			PresentationErrorCode.IsEmpty() ? TEXT("layer-dirty-input-profile") : *PresentationErrorCode,
			TEXT("Layer dirty input is not accepted."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsPresentation && (!ProjectWorldPresentationProfile::Load(
		Request.PresentationProfilePath,
		PresentationProfile,
		PresentationErrorCode,
		PresentationError) ||
		!ProjectWorldPresentationProfile::ResolveResources(
			PresentationProfile,
			PresentationResources,
			PresentationError)))
	{
		Reject(
			OutResult,
			PresentationErrorCode.IsEmpty()
				? TEXT("presentation-profile-resource")
				: *PresentationErrorCode,
			TEXT("Presentation profile is not accepted."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsRuntime && (!ProjectWorldRuntimeProfile::Load(
		Request.RuntimeProfilePath,
		RuntimeProfile,
		PresentationErrorCode,
		PresentationError) ||
		!ProjectWorldRuntimeRealization::Validate(Bundle, RuntimeProfile, PresentationError)))
	{
		Reject(
			OutResult,
			PresentationErrorCode.IsEmpty() ? TEXT("runtime-profile-route") : *PresentationErrorCode,
			TEXT("Runtime profile is not accepted."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsPresentation && !ProjectWorldAuthoredOverlayRealization::Resolve(
		Bundle,
		Request.AuthoredOverlayProfilePath,
		AuthoredOverlaySet,
		OutResult,
		PresentationErrorCode,
		PresentationError))
	{
		Reject(
			OutResult,
			PresentationErrorCode.IsEmpty()
				? TEXT("authored-overlay-resolution")
				: *PresentationErrorCode,
			TEXT("Authored overlay set is not accepted."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	const FString PresentationIdentity = bNeedsPresentation
		? PresentationProfile.ProfileHash.Left(12)
		: TEXT("delete");
	OutResult.OperationId = FString::Printf(
		TEXT("realize:%s:%s:%s:%s:%s:%s:%s"),
		*Bundle.ProfileId,
		*Bundle.InputsHash.Left(12),
		*PresentationIdentity,
		bNeedsRuntime ? *RuntimeProfile.ProfileHash.Left(12) : TEXT("no-runtime"),
		bNeedsPresentation ? *AuthoredOverlaySet.SetHash.Left(12) : TEXT("delete"),
		bNeedsLayerPlan ? *RealizationProfile.ProfileHash.Left(12) : TEXT("no-layers"),
		bNeedsDirtyInput ? *LayerDirtyInput.InputHash.Left(12) : TEXT("no-dirty-input"));
	OutResult.InputHash = Bundle.InputsHash;
	OutResult.CompileResultHash = Bundle.CompileResultHash;
	OutResult.PresentationProfileId = PresentationProfile.ProfileId;
	OutResult.PresentationProfileHash = PresentationProfile.ProfileHash;
	OutResult.RuntimeProfileId = RuntimeProfile.ProfileId;
	OutResult.RuntimeProfileHash = RuntimeProfile.ProfileHash;
	OutResult.RuntimeRouteId = RuntimeProfile.RouteId;
	OutResult.RuntimeRouteFeatureId = RuntimeProfile.RouteFeatureId;
	OutResult.RealizationProfileId = RealizationProfile.ProfileId;
	OutResult.RealizationProfileHash = RealizationProfile.ProfileHash;
	OutResult.LayerDirtyInputHash = LayerDirtyInput.InputHash;
	OutResult.NanitePolicy = RuntimeProfile.NanitePolicy;
	OutResult.InstancingPolicy = RuntimeProfile.InstancingPolicy;
	OutResult.HlodPolicy = RuntimeProfile.HlodPolicy;
	OutResult.RuntimeP95FrameTimeBudgetMilliseconds = RuntimeProfile.Budgets.P95FrameTimeMilliseconds;
	OutResult.WorldDataPluginName = Bundle.WorldDataPluginName;
	OutResult.GridId = Bundle.GridId;
	OutResult.CanonicalCrs = Bundle.CanonicalCrs;
	OutResult.CoordinateTransform = Bundle.CoordinateTransform;
	OutResult.VerticalOriginMeters = Bundle.HeightOriginMeters;
	OutResult.LatticeOriginMeters = Bundle.LatticeOriginMeters;
	OutResult.EngineGeoreferenceOriginMeters = Bundle.EngineGeoreferenceOriginMeters;
	OutResult.SampleSpacingMeters = Bundle.SampleSpacingMeters;
	OutResult.MapPackagePath = Request.MapPackagePath;
	OutResult.VerifiedOutputCount = Bundle.VerifiedOutputCount;
	OutResult.CanonicalCellCount = Bundle.Cells.Num();
	if (bNeedsLayerPlan && !ProjectWorldLayerInventory::Build(
		Bundle,
		RealizationProfile,
		Request.Mode == EProjectWorldRealizationMode::Apply && Request.bFirstLayerApply,
		bNeedsDirtyInput ? &LayerDirtyInput : nullptr,
		OutResult,
		PresentationError))
	{
		Reject(
			OutResult,
			TEXT("realization-dirty-plan"),
			TEXT("Layer dirty closure is invalid."),
			PresentationError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	OutResult.bLandscapeCompatible = Layout.bCompatible;
	OutResult.LandscapeReason = Layout.Reason;
	if (Request.bRequireLandscapeCompatible && !Layout.bCompatible)
	{
		Reject(OutResult, TEXT("landscape-layout"), TEXT("Canonical grid is not Landscape-compatible."), Layout.Reason);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	const FVector Canonical(
		Bundle.EngineGeoreferenceOriginMeters.X,
		Bundle.EngineGeoreferenceOriginMeters.Y,
		Bundle.HeightOriginMeters);
	OutResult.CoordinateRoundTripErrorMeters = FVector::Distance(
		Canonical,
		FProjectWorldCanonicalLoader::UnrealToCanonical(
			Bundle,
			FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Canonical)));
	if (OutResult.CoordinateRoundTripErrorMeters > Bundle.CoordinateQuantizationMeters)
	{
		Reject(OutResult, TEXT("coordinate-roundtrip"), TEXT("Canonical coordinate mapping exceeds its quantization tolerance."));
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	if (Request.Mode == EProjectWorldRealizationMode::Validate)
	{
		if (Bundle.CanonicalCrs.StartsWith(TEXT("EPSG:")))
		{
			UWorld* ProbeWorld = GEditor->NewMap(false);
			FString ProbeError;
			if (ProbeWorld == nullptr)
			{
				Reject(OutResult, TEXT("coordinate-georeferencing"), TEXT("Cannot create a transient GeoReferencing probe world."));
				OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
				return OutResult.ExitCode();
			}
			OutResult.CoordinateRoundTripErrorMeters =
				ProjectWorldGeneratedGeometry::MeasureCoordinateRoundTrip(
					ProbeWorld,
					Bundle,
					false,
					OutResult,
					ProbeError);
			if (OutResult.CoordinateRoundTripErrorMeters > Bundle.CoordinateQuantizationMeters)
			{
				Reject(OutResult, TEXT("coordinate-georeferencing"), TEXT("Projected coordinate round trip exceeds tolerance."), ProbeError);
				OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
				return OutResult.ExitCode();
			}
		}
		OutResult.Status = TEXT("accepted");
		OutResult.Message = TEXT("Canonical inputs are accepted for Unreal realization.");
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return 0;
	}

	if (!ValidateMapPackagePath(Request.MapPackagePath, WorldDataRoots))
	{
		Reject(
			OutResult,
			TEXT("editor-map-path"),
			FString::Printf(
				TEXT("Generated map must stay under %s."),
				*WorldDataRoots.GeneratedPackageRoot),
			Request.MapPackagePath);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	bool bExistingMap = false;
	FString EditorError;
	UWorld* World = LoadOrCreateWorld(Request.MapPackagePath, bExistingMap, EditorError);
	if (World == nullptr || !World->IsPartitionedWorld())
	{
		Reject(OutResult, TEXT("editor-world"), EditorError.IsEmpty() ? TEXT("Target is not a World Partition map.") : EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	OutResult.bWorldPartition = true;
	const bool bPartitionHlodPolicyChanged =
		ProjectWorldPartitionPolicy::CountHLODLayerReferences(World) > 0;
	if (!ProjectWorldPartitionPolicy::DisableHLOD(World, EditorError))
	{
		Reject(OutResult, TEXT("editor-hlod-policy"), TEXT("Cannot apply the production no-HLOD policy."), EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (Request.Mode == EProjectWorldRealizationMode::Apply &&
		!ProjectWorldPresentationMaterialRealization::Prepare(
			PresentationProfile,
			PresentationResources,
			WorldDataRoots.GeneratedPackageRoot,
			EditorError))
	{
		Reject(OutResult, TEXT("presentation-material"), TEXT("Cannot prepare generated presentation materials."), EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	TArray<FWorldPartitionReference> LoadedActorReferences;
	if (UWorldPartition* WorldPartition = World->GetWorldPartition();
		WorldPartition != nullptr && WorldPartition->IsInitialized())
	{
		WorldPartition->LoadAllActors(LoadedActorReferences);
	}
	const bool bPreserveLandscape = Layout.bCompatible;
	const bool bOwnedActorsPrepared = Request.Mode == EProjectWorldRealizationMode::Apply
		? ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World,
			Bundle,
			bNeedsRuntime ? RuntimeProfile.ProfileId : FString(),
			bPreserveLandscape,
			OutResult)
		: ProjectWorldGeneratedGeometry::RemoveOwnedActors(
			World,
			bPreserveLandscape,
			OutResult);
	if (!bOwnedActorsPrepared)
	{
		Reject(OutResult, TEXT("editor-delete"), TEXT("Cannot remove a previously generated actor."));
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (OutResult.RemovedActorCount > 0 &&
		Request.Mode != EProjectWorldRealizationMode::Apply)
	{
		LoadedActorReferences.Reset();
		CollectGarbage(RF_NoFlags);
	}

	if (Request.Mode == EProjectWorldRealizationMode::Apply)
	{
		const FProjectWorldGeometryPresentation GeometryPresentation{
			PresentationResources.TerrainMaterial,
			PresentationResources.RoadMaterial,
			PresentationResources.BuildingMaterial,
			PresentationProfile.TerrainPrimaryColor};
		OutResult.CoordinateRoundTripErrorMeters =
			ProjectWorldGeneratedGeometry::MeasureCoordinateRoundTrip(
				World,
				Bundle,
				true,
				OutResult,
				EditorError);
		if (OutResult.CoordinateRoundTripErrorMeters > Bundle.CoordinateQuantizationMeters)
		{
			Reject(OutResult, TEXT("coordinate-georeferencing"), TEXT("Projected coordinate round trip exceeds tolerance."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (Layout.bCompatible &&
			!ProjectWorldLandscapeRealization::CreateOrUpdate(
				World,
				Bundle,
				Layout,
				bNeedsLayerPlan ? RealizationProfile.LogicalLandscapeId : FString(),
				bNeedsLayerPlan ? RealizationProfile.ComponentsPerProxy : 1,
				OutResult,
				EditorError,
				PresentationResources.TerrainMaterial))
		{
			Reject(OutResult, TEXT("geometry-landscape"), TEXT("Cannot realize canonical terrain as a Landscape."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (!bNeedsLayerPlan && !ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World,
			Bundle,
			!Layout.bCompatible,
			Request.MaxRoadFeatures,
			Request.MaxBuildingFeatures,
			OutResult,
			EditorError,
			&GeometryPresentation,
			bNeedsRuntime ? RuntimeProfile.RouteFeatureId : FString()))
		{
			Reject(OutResult, TEXT("geometry-create"), TEXT("Cannot realize canonical geometry."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (bNeedsLayerPlan && !ProjectWorldWaterRealization::Apply(
			World,
			Bundle,
			RealizationProfile,
			OutResult,
			EditorError))
		{
			Reject(OutResult, TEXT("geometry-water"), TEXT("Cannot realize persistent cell-local water."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (bNeedsRuntime && !ProjectWorldRuntimeRealization::Apply(
			World,
			Bundle,
			RuntimeProfile,
			OutResult,
			EditorError))
		{
			Reject(OutResult, TEXT("runtime-route"), TEXT("Cannot realize the gameplay route."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (!ProjectWorldPresentationRealization::Apply(
			World,
			Bundle,
			PresentationProfile,
			PresentationResources,
			OutResult,
			EditorError))
		{
			Reject(OutResult, TEXT("presentation-create"), TEXT("Cannot realize the presentation profile."), EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
		if (!ProjectWorldAuthoredOverlayRealization::Apply(
			World,
			Bundle,
			AuthoredOverlaySet,
			OutResult,
			EditorError))
		{
			Reject(
				OutResult,
				TEXT("authored-overlay-create"),
				TEXT("Cannot realize authored-overlay anchor actors."),
				EditorError);
			OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
			return OutResult.ExitCode();
		}
	}
	else if (Layout.bCompatible &&
		!ProjectWorldLandscapeRealization::ClearGeneratedLayers(
			World,
			Bundle,
			OutResult,
			EditorError))
	{
		Reject(OutResult, TEXT("geometry-delete"), TEXT("Cannot clear generated Landscape layers."), EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}

	OutResult.UpdatedActorCount += ProjectWorldPartitionPolicy::DisableGeneratedActorHLOD(World);
	const bool bGeneratedWorldChanged = bPartitionHlodPolicyChanged ||
		OutResult.CreatedActorCount > 0 || OutResult.UpdatedActorCount > 0 ||
		OutResult.RemovedActorCount > 0 || OutResult.UpdatedLandscapeComponentCount > 0;
	if (bGeneratedWorldChanged && !SaveGeneratedWorld(World, Request.MapPackagePath))
	{
		Reject(OutResult, TEXT("save-map"), TEXT("Cannot save the generated World Partition map."), Request.MapPackagePath);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsLayerPlan && !ProjectWorldLayerInventory::CaptureArtifacts(
		World,
		Bundle,
		RealizationProfile,
		OutResult,
		EditorError))
	{
		Reject(OutResult, TEXT("save-layer-inventory"), TEXT("Cannot authenticate generated layer artifacts."), EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (!ProjectWorldSemanticEvidence::Capture(World, OutResult, EditorError))
	{
		Reject(OutResult, TEXT("save-evidence"), TEXT("Cannot capture generated-world evidence."), EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	if (bNeedsRuntime && !ProjectWorldRuntimeRealization::CaptureAndCheckStructuralBudgets(
		World,
		RuntimeProfile,
		OutResult,
		EditorError))
	{
		Reject(
			OutResult,
			TEXT("runtime-acceptance"),
			TEXT("Generated runtime profile fails an executable policy or frozen structural budget."),
			EditorError);
		OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		return OutResult.ExitCode();
	}
	OutResult.DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
	if (bNeedsRuntime && OutResult.DurationSeconds > RuntimeProfile.Budgets.RegenerationSeconds)
	{
		Reject(
			OutResult,
			TEXT("runtime-regeneration-budget"),
			TEXT("Runtime-profile regeneration exceeds its frozen duration budget."),
			FString::Printf(TEXT("measured=%.3f budget=%.3f"), OutResult.DurationSeconds, RuntimeProfile.Budgets.RegenerationSeconds));
		return OutResult.ExitCode();
	}

	OutResult.Status = TEXT("accepted");
	OutResult.Message = Request.Mode == EProjectWorldRealizationMode::Delete
		? TEXT("Generated actors and Landscape layers were cleared; authored content was preserved.")
		: TEXT("Canonical inputs were realized into owned World Partition actors.");
	return 0;
}

bool FProjectWorldRealizationService::WriteResult(
	const FProjectWorldRealizationRequest& Request,
	const FProjectWorldRealizationResult& Result)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("https://alis.world/schemas/world-realization/realization-result-v1.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("operation_id"), Result.OperationId);
	Root->SetStringField(TEXT("operation"), TEXT("unreal_world_realization"));
	Root->SetStringField(TEXT("status"), Result.Status);
	Root->SetStringField(TEXT("input_compile_result"), FPaths::ConvertRelativePathToFull(Request.CompileResultPath));
	Root->SetStringField(TEXT("input_sha256"), Result.CompileResultHash);
	Root->SetStringField(TEXT("presentation_profile"), Result.PresentationProfileId);
	Root->SetStringField(TEXT("presentation_profile_sha256"), Result.PresentationProfileHash);
	Root->SetStringField(TEXT("runtime_profile"), Result.RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), Result.RuntimeProfileHash);
	Root->SetStringField(TEXT("runtime_route"), Result.RuntimeRouteId);
	Root->SetStringField(TEXT("runtime_route_feature_id"), Result.RuntimeRouteFeatureId);
	Root->SetStringField(TEXT("authored_overlay_set"), Result.AuthoredOverlaySetId);
	Root->SetStringField(TEXT("authored_overlay_set_sha256"), Result.AuthoredOverlaySetHash);
	Root->SetStringField(TEXT("realization_profile"), Result.RealizationProfileId);
	Root->SetStringField(TEXT("realization_profile_sha256"), Result.RealizationProfileHash);
	Root->SetStringField(TEXT("layer_dirty_input_sha256"), Result.LayerDirtyInputHash);
	TArray<TSharedPtr<FJsonValue>> LayerInventories;
	for (const FProjectWorldLayerInventory& Inventory : Result.LayerInventories)
	{
		TSharedRef<FJsonObject> Layer = MakeShared<FJsonObject>();
		Layer->SetStringField(TEXT("layer_id"), Inventory.LayerId);
		Layer->SetStringField(TEXT("scope_id"), Inventory.ScopeId);
		Layer->SetStringField(
			TEXT("normalized_layer_contract_sha256"),
			Inventory.NormalizedLayerContractHash);
		Layer->SetStringField(TEXT("generator_id"), Inventory.GeneratorId);
		Layer->SetNumberField(TEXT("generator_version"), Inventory.GeneratorVersion);
		Layer->SetStringField(TEXT("artifact_root"), Inventory.ArtifactRoot);
		auto StringArray = [](const TArray<FString>& Values)
		{
			TArray<TSharedPtr<FJsonValue>> ResultValues;
			for (const FString& Value : Values)
			{
				ResultValues.Add(MakeShared<FJsonValueString>(Value));
			}
			return ResultValues;
		};
		auto InputArray = [](const TArray<FProjectWorldLayerInputInventory>& Inputs)
		{
			TArray<TSharedPtr<FJsonValue>> ResultInputs;
			for (const FProjectWorldLayerInputInventory& Input : Inputs)
			{
				TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
				Record->SetStringField(TEXT("unit_id"), Input.UnitId);
				Record->SetStringField(TEXT("sha256"), Input.Hash);
				ResultInputs.Add(MakeShared<FJsonValueObject>(Record));
			}
			return ResultInputs;
		};
		Layer->SetArrayField(TEXT("canonical_inputs"), InputArray(Inventory.CanonicalInputs));
		Layer->SetArrayField(TEXT("dependency_inputs"), InputArray(Inventory.DependencyInputs));
		Layer->SetArrayField(TEXT("final_dirty_units"), StringArray(Inventory.FinalDirtyUnits));
		TArray<TSharedPtr<FJsonValue>> Artifacts;
		for (const FProjectWorldLayerArtifactInventory& InventoryArtifact : Inventory.Artifacts)
		{
			TSharedRef<FJsonObject> Artifact = MakeShared<FJsonObject>();
			Artifact->SetStringField(TEXT("path"), InventoryArtifact.Path);
			Artifact->SetStringField(TEXT("kind"), InventoryArtifact.Kind);
			Artifact->SetStringField(TEXT("digest_kind"), TEXT("sha256"));
			Artifact->SetStringField(TEXT("digest"), InventoryArtifact.Digest);
			Artifact->SetStringField(TEXT("semantic_sha256"), InventoryArtifact.SemanticHash);
			Artifacts.Add(MakeShared<FJsonValueObject>(Artifact));
		}
		Layer->SetArrayField(TEXT("artifacts"), Artifacts);
		LayerInventories.Add(MakeShared<FJsonValueObject>(Layer));
	}
	Root->SetArrayField(TEXT("layer_inventories"), LayerInventories);
	Root->SetNumberField(TEXT("authored_anchor_resolved_count"), Result.AuthoredAnchorResolvedCount);
	Root->SetNumberField(TEXT("authored_anchor_refused_count"), Result.AuthoredAnchorRefusedCount);
	Root->SetNumberField(TEXT("authored_anchor_placed_count"), Result.AuthoredAnchorPlacedCount);
	Root->SetNumberField(TEXT("authored_mask_count"), Result.AuthoredMaskCount);
	Root->SetNumberField(TEXT("authored_anchor_maximum_drift_m"), Result.AuthoredAnchorMaximumDriftMeters);
	TArray<TSharedPtr<FJsonValue>> AuthoredAnchors;
	for (const FProjectWorldAuthoredAnchorEvidence& Evidence : Result.AuthoredAnchors)
	{
		TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
		Anchor->SetStringField(TEXT("overlay_id"), Evidence.OverlayId);
		Anchor->SetStringField(TEXT("authored_package"), Evidence.AuthoredPackage);
		Anchor->SetArrayField(TEXT("world_location"), {
			MakeShared<FJsonValueNumber>(Evidence.WorldLocation.X),
			MakeShared<FJsonValueNumber>(Evidence.WorldLocation.Y),
			MakeShared<FJsonValueNumber>(Evidence.WorldLocation.Z)});
		Anchor->SetArrayField(TEXT("world_rotation"), {
			MakeShared<FJsonValueNumber>(Evidence.WorldRotation.Pitch),
			MakeShared<FJsonValueNumber>(Evidence.WorldRotation.Yaw),
			MakeShared<FJsonValueNumber>(Evidence.WorldRotation.Roll)});
		Anchor->SetNumberField(TEXT("drift_m"), Evidence.DriftMeters);
		Anchor->SetNumberField(TEXT("horizontal_total_error_m"), Evidence.HorizontalTotalErrorMeters);
		Anchor->SetNumberField(TEXT("vertical_total_error_m"), Evidence.VerticalTotalErrorMeters);
		Anchor->SetBoolField(TEXT("surface_snapped"), Evidence.bSurfaceSnapped);
		Anchor->SetBoolField(TEXT("places"), Evidence.bPlaces);
		AuthoredAnchors.Add(MakeShared<FJsonValueObject>(Anchor));
	}
	Root->SetArrayField(TEXT("authored_anchor_resolutions"), AuthoredAnchors);
	Root->SetStringField(TEXT("inputs_hash"), Result.InputHash);
	Root->SetStringField(TEXT("world_data_plugin"), Result.WorldDataPluginName);
	Root->SetStringField(TEXT("grid_id"), Result.GridId);
	Root->SetStringField(TEXT("canonical_crs"), Result.CanonicalCrs);
	Root->SetStringField(TEXT("coordinate_transform"), Result.CoordinateTransform);
	Root->SetStringField(TEXT("map_package"), Result.MapPackagePath);
	Root->SetNumberField(TEXT("verified_output_count"), Result.VerifiedOutputCount);
	Root->SetNumberField(TEXT("canonical_cell_count"), Result.CanonicalCellCount);
	Root->SetNumberField(TEXT("coordinate_roundtrip_error_m"), Result.CoordinateRoundTripErrorMeters);
	Root->SetNumberField(TEXT("vertical_origin_m"), Result.VerticalOriginMeters);
	Root->SetArrayField(TEXT("lattice_origin_m"), {
		MakeShared<FJsonValueNumber>(Result.LatticeOriginMeters.X),
		MakeShared<FJsonValueNumber>(Result.LatticeOriginMeters.Y)});
	Root->SetArrayField(TEXT("engine_georeference_origin_m"), {
		MakeShared<FJsonValueNumber>(Result.EngineGeoreferenceOriginMeters.X),
		MakeShared<FJsonValueNumber>(Result.EngineGeoreferenceOriginMeters.Y)});
	Root->SetArrayField(TEXT("sample_spacing_m"), {
		MakeShared<FJsonValueNumber>(Result.SampleSpacingMeters.X),
		MakeShared<FJsonValueNumber>(Result.SampleSpacingMeters.Y)});
	Root->SetNumberField(
		TEXT("georeferencing_placement_error_m"),
		Result.GeoReferencingPlacementErrorMeters);
	Root->SetNumberField(TEXT("georeferencing_probe_points"), Result.GeoReferencingProbePointCount);
	Root->SetNumberField(TEXT("duration_seconds"), Result.DurationSeconds);
	Root->SetBoolField(TEXT("world_partition"), Result.bWorldPartition);
	Root->SetBoolField(TEXT("landscape_compatible"), Result.bLandscapeCompatible);
	Root->SetBoolField(TEXT("georeferencing_probed"), Result.bGeoReferencingProbed);
	Root->SetStringField(TEXT("landscape_reason"), Result.LandscapeReason);
	Root->SetStringField(TEXT("semantic_fingerprint"), Result.SemanticFingerprint);
	Root->SetNumberField(TEXT("generated_source_bytes"), Result.GeneratedSourceBytes);
	Root->SetNumberField(TEXT("procedural_mesh_buffer_bytes"), Result.ProceduralMeshBufferBytes);
	Root->SetNumberField(TEXT("generated_actor_count"), Result.GeneratedActorCount);
	Root->SetNumberField(TEXT("spatially_loaded_actor_count"), Result.SpatiallyLoadedActorCount);
	Root->SetNumberField(TEXT("runtime_route_spatial_actor_count"), Result.RuntimeRouteSpatialActorCount);
	Root->SetNumberField(TEXT("runtime_always_loaded_actor_count"), Result.RuntimeAlwaysLoadedActorCount);
	Root->SetNumberField(
		TEXT("procedural_mesh_section_draw_call_upper_bound"),
		Result.ProceduralMeshSectionDrawCallUpperBound);
	Root->SetNumberField(TEXT("hlod_proxy_actor_count"), Result.HlodProxyActorCount);
	Root->SetNumberField(TEXT("hlod_layer_reference_count"), Result.HlodLayerReferenceCount);
	Root->SetNumberField(TEXT("hlod_eligible_generated_actor_count"), Result.HlodEligibleGeneratedActorCount);
	Root->SetStringField(TEXT("nanite_policy"), Result.NanitePolicy);
	Root->SetStringField(TEXT("instancing_policy"), Result.InstancingPolicy);
	Root->SetStringField(TEXT("hlod_policy"), Result.HlodPolicy);
	Root->SetBoolField(TEXT("runtime_route_collision_probed"), Result.bRuntimeRouteCollisionProbed);
	Root->SetNumberField(TEXT("runtime_collision_probe_count"), Result.RuntimeCollisionProbeCount);
	Root->SetBoolField(TEXT("runtime_route_collision_orientation_probed"), Result.bRuntimeRouteCollisionOrientationProbed);
	Root->SetNumberField(TEXT("runtime_collision_orientation_probe_count"), Result.RuntimeCollisionOrientationProbeCount);
	Root->SetNumberField(TEXT("runtime_route_volume_yaw_degrees"), Result.RuntimeRouteVolumeYawDegrees);
	Root->SetBoolField(TEXT("runtime_navigation_probed"), Result.bRuntimeNavigationProbed);
	Root->SetNumberField(TEXT("runtime_navigation_path_m"), Result.RuntimeNavigationPathMeters);
	Root->SetBoolField(TEXT("runtime_streaming_policy_probed"), Result.bRuntimeStreamingPolicyProbed);
	Root->SetBoolField(TEXT("runtime_nanite_policy_probed"), Result.bRuntimeNanitePolicyProbed);
	Root->SetBoolField(TEXT("runtime_instancing_policy_probed"), Result.bRuntimeInstancingPolicyProbed);
	Root->SetBoolField(TEXT("runtime_hlod_policy_probed"), Result.bRuntimeHlodPolicyProbed);
	Root->SetBoolField(TEXT("runtime_structural_budgets_passed"), Result.bRuntimeStructuralBudgetsPassed);
	Root->SetNumberField(
		TEXT("runtime_p95_frame_time_budget_ms"),
		Result.RuntimeP95FrameTimeBudgetMilliseconds);
	Root->SetStringField(TEXT("authored_correction_layer_guid"), Result.AuthoredCorrectionLayerGuid);
	Root->SetStringField(TEXT("authored_correction_layer_sha256"), Result.AuthoredCorrectionLayerHash);
	Root->SetBoolField(
		TEXT("authored_correction_layer_preserved"),
		Result.bAuthoredCorrectionLayerPreserved);

	TSharedRef<FJsonObject> Changes = MakeShared<FJsonObject>();
	Changes->SetNumberField(TEXT("created_actors"), Result.CreatedActorCount);
	Changes->SetNumberField(TEXT("updated_actors"), Result.UpdatedActorCount);
	Changes->SetNumberField(TEXT("removed_actors"), Result.RemovedActorCount);
	Changes->SetNumberField(TEXT("preserved_actors"), Result.PreservedActorCount);
	Changes->SetNumberField(TEXT("terrain_sections"), Result.TerrainSectionCount);
	Changes->SetNumberField(TEXT("landscape_components"), Result.LandscapeComponentCount);
	Changes->SetNumberField(
		TEXT("updated_landscape_components"),
		Result.UpdatedLandscapeComponentCount);
	Changes->SetNumberField(TEXT("landscape_proxies"), Result.LandscapeProxyCount);
	Changes->SetNumberField(TEXT("water_cell_actors"), Result.WaterCellActorCount);
	Changes->SetNumberField(TEXT("water_mesh_assets"), Result.WaterMeshAssetCount);
	Changes->SetNumberField(TEXT("water_triangles"), Result.WaterTriangleCount);
	Changes->SetNumberField(TEXT("road_sections"), Result.RoadSectionCount);
	Changes->SetNumberField(TEXT("building_sections"), Result.BuildingSectionCount);
	Changes->SetNumberField(TEXT("presentation_actors"), Result.PresentationActorCount);
	Changes->SetNumberField(TEXT("capture_viewpoints"), Result.CaptureViewpointCount);
	Changes->SetStringField(TEXT("cross_cell_road_feature_id"), Result.CrossCellRoadFeatureId);
	Changes->SetNumberField(
		TEXT("cross_cell_road_expected_fragments"),
		Result.CrossCellRoadExpectedFragmentCount);
	Changes->SetNumberField(
		TEXT("cross_cell_road_realized_fragments"),
		Result.CrossCellRoadRealizedFragmentCount);
	Changes->SetNumberField(
		TEXT("cross_cell_road_shared_boundary_points"),
		Result.CrossCellRoadSharedBoundaryPointCount);
	Root->SetObjectField(TEXT("changes"), Changes);

	TArray<TSharedPtr<FJsonValue>> Errors;
	if (!Result.ErrorCode.IsEmpty())
	{
		TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), Result.ErrorCode);
		Error->SetStringField(TEXT("message"), Result.Message);
		Error->SetStringField(TEXT("detail"), Result.Detail);
		Errors.Add(MakeShared<FJsonValueObject>(Error));
	}
	Root->SetArrayField(TEXT("errors"), Errors);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	Text += LINE_TERMINATOR;

	const FString FullResultPath = FPaths::ConvertRelativePathToFull(Request.ResultPath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullResultPath), true);
	const FString TemporaryPath = FullResultPath + TEXT(".tmp");
	return FFileHelper::SaveStringToFile(Text, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
		IFileManager::Get().Move(*FullResultPath, *TemporaryPath, true, true, false, true);
}
