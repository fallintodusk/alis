// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldPresentationMaterialRealization.h"
#include "ProjectWorldPresentationRealization.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldSemanticEvidence.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldPresentationTests
{
	FString ShippedProfilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorld/Data/Presentation/kazan_representative_v1.json"));
	}

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("grid_presentation_test");
		Bundle.InputsHash = TEXT("presentation_input");
		Bundle.LatticeOriginMeters = FVector2D(1000.0, 2000.0);
		Bundle.EngineGeoreferenceOriginMeters = FVector2D(1000.0, 2000.0);
		Bundle.HeightOriginMeters = 75.0;
		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			FProjectWorldCanonicalCell Cell;
			Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
			Cell.CellX = CellX;
			Cell.CellY = 0;
			Cell.Bounds = FVector4d(1000.0 + CellX * 900.0, 2000.0, 1900.0 + CellX * 900.0, 2900.0);
			Bundle.Cells.Add(MoveTemp(Cell));
		}
		return Bundle;
	}

	FString PresentationRole(const AActor& Actor)
	{
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(TEXT("ProjectWorld.PresentationRole=")))
			{
				return Value.RightChop(30);
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPresentationProfileContractTest,
	"Project.World.Realization.Presentation.ProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPresentationProfileContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldPresentationTests;
	using namespace ProjectWorldSchemaTestUtilities;
	FProjectWorldPresentationProfile Profile;
	FString ErrorCode;
	FString Error;
	TestTrue(
		TEXT("The shipped presentation profile passes its runtime contract."),
		ProjectWorldPresentationProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error));
	TestEqual(TEXT("Presentation profile identity."), Profile.ProfileId, FString(TEXT("kazan_representative_v1")));
	TestEqual(TEXT("Presentation profile has three fixed capture viewpoints."), Profile.CaptureViewpoints.Num(), 3);
	TestEqual(TEXT("Presentation profile SHA-256 is complete."), Profile.ProfileHash.Len(), 64);

	FProjectWorldPresentationResources Resources;
	TestTrue(
		TEXT("Every approved engine-provided material resolves."),
		ProjectWorldPresentationProfile::ResolveResources(Profile, Resources, Error));

	FString ShippedSource;
	TestTrue(TEXT("Profile fixture is readable."), FFileHelper::LoadFileToString(ShippedSource, *ShippedProfilePath()));
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectWorldPresentation"));
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString InvalidMaterialPath = FPaths::Combine(Root, TEXT("invalid_material.json"));
	const FString Source = Rewrite(
		ShippedSource,
		InvalidMaterialPath,
		TEXT("project_world_presentation_profile.schema.json"));
	const FString ProductionRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldData/Data/Profiles"));
	const FString ProductionPath = FPaths::Combine(ProductionRoot, TEXT("presentation_loader_test.json"));
	const FString ProductionSchema = ReferenceFor(
		ProductionPath,
		TEXT("project_world_presentation_profile.schema.json"));
	TestEqual(
		TEXT("Production presentation schema climbs to the canonical logic plugin."),
		ProductionSchema,
		FString(TEXT("../../../ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json")));
	IFileManager::Get().MakeDirectory(*ProductionRoot, true);
	TestTrue(
		TEXT("Production-root presentation fixture is writable."),
		FFileHelper::SaveStringToFile(
			Rewrite(
				ShippedSource,
				ProductionPath,
				TEXT("project_world_presentation_profile.schema.json")),
			*ProductionPath));
	TestTrue(
		TEXT("The presentation loader accepts an owner-relative production schema."),
		ProjectWorldPresentationProfile::Load(ProductionPath, Profile, ErrorCode, Error));
	const FString ScratchSchema = ReferenceFor(
		InvalidMaterialPath,
		TEXT("project_world_presentation_profile.schema.json"));
	auto ExpectSchemaRejected = [this, &InvalidMaterialPath, &Profile, &ErrorCode, &Error](
		const TCHAR* Label,
		const FString& Candidate)
	{
		TestTrue(
			TEXT("Schema-sabotage fixture is writable."),
			FFileHelper::SaveStringToFile(Candidate, *InvalidMaterialPath));
		TestFalse(
			Label,
			ProjectWorldPresentationProfile::Load(
				InvalidMaterialPath,
				Profile,
				ErrorCode,
				Error));
		TestEqual(
			TEXT("Schema rejection is structured."),
			ErrorCode,
			FString(TEXT("presentation-profile-contract")));
	};
	ExpectSchemaRejected(
		TEXT("A schema URL cannot replace the repository-relative authority."),
		Source.Replace(*ScratchSchema, TEXT("https://example.invalid/presentation.schema.json")));
	FString AbsoluteSchema = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json"));
	FPaths::NormalizeFilename(AbsoluteSchema);
	ExpectSchemaRejected(
		TEXT("An absolute schema path is not portable authority."),
		Source.Replace(*ScratchSchema, *AbsoluteSchema));
	ExpectSchemaRejected(
		TEXT("A relative path to a different canonical schema is rejected."),
		Source.Replace(
			*ScratchSchema,
			*ReferenceFor(InvalidMaterialPath, TEXT("project_world_runtime_profile.schema.json"))));
	ExpectSchemaRejected(
		TEXT("A relative path that escapes to another target is rejected."),
		Source.Replace(*ScratchSchema, TEXT("../../../../outside/presentation.schema.json")));
	ExpectSchemaRejected(
		TEXT("Backslash schema paths are rejected."),
		Source.Replace(*ScratchSchema, *ScratchSchema.Replace(TEXT("/"), TEXT("\\\\"))));
	const FString InvalidMaterial = Source.Replace(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"),
		TEXT("/ProjectObject/Unclassified.Material"),
		ESearchCase::CaseSensitive);
	TestTrue(TEXT("Invalid material fixture is writable."), FFileHelper::SaveStringToFile(InvalidMaterial, *InvalidMaterialPath));
	TestFalse(
		TEXT("A non-engine dependency cannot enter the public presentation profile."),
		ProjectWorldPresentationProfile::Load(InvalidMaterialPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Material rejection is structured."), ErrorCode, FString(TEXT("presentation-profile-material")));

	const FString DuplicateViewpointPath = FPaths::Combine(Root, TEXT("duplicate_viewpoint.json"));
	const FString DuplicateViewpoint = Source.Replace(TEXT("terrain_oblique"), TEXT("overview"));
	TestTrue(TEXT("Duplicate viewpoint fixture is writable."), FFileHelper::SaveStringToFile(DuplicateViewpoint, *DuplicateViewpointPath));
	TestFalse(
		TEXT("Capture viewpoint names must remain unique."),
		ProjectWorldPresentationProfile::Load(DuplicateViewpointPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Viewpoint rejection is structured."), ErrorCode, FString(TEXT("presentation-profile-viewpoints")));

	const FString InvalidProfileIdPath = FPaths::Combine(Root, TEXT("invalid_profile_id.json"));
	const FString InvalidProfileId = Source.Replace(TEXT("kazan_representative_v1"), TEXT("Kazan Invalid"));
	TestTrue(TEXT("Invalid profile-id fixture is writable."), FFileHelper::SaveStringToFile(InvalidProfileId, *InvalidProfileIdPath));
	TestFalse(
		TEXT("Profile identity uses the schema token contract at runtime."),
		ProjectWorldPresentationProfile::Load(InvalidProfileIdPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Profile token rejection is structured."), ErrorCode, FString(TEXT("presentation-profile-contract")));

	const FString InvalidViewpointTokenPath = FPaths::Combine(Root, TEXT("invalid_viewpoint_token.json"));
	const FString InvalidViewpointToken = Source.Replace(TEXT("terrain_oblique"), TEXT("terrain-oblique"));
	TestTrue(TEXT("Invalid viewpoint-token fixture is writable."), FFileHelper::SaveStringToFile(InvalidViewpointToken, *InvalidViewpointTokenPath));
	TestFalse(
		TEXT("Viewpoint identity uses the schema token contract at runtime."),
		ProjectWorldPresentationProfile::Load(InvalidViewpointTokenPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Viewpoint token rejection is structured."), ErrorCode, FString(TEXT("presentation-profile-viewpoints")));

	const FString UnsupportedTerrainPath = FPaths::Combine(Root, TEXT("unsupported_terrain.json"));
	const FString UnsupportedTerrain = Source.Replace(
		TEXT("/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid.MI_ProcGrid"),
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	TestTrue(TEXT("Unsupported terrain fixture is writable."), FFileHelper::SaveStringToFile(UnsupportedTerrain, *UnsupportedTerrainPath));
	TestFalse(
		TEXT("Presentation v1 accepts only the terrain parent whose parameter contract it owns."),
		ProjectWorldPresentationProfile::Load(UnsupportedTerrainPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Terrain parent rejection is structured."), ErrorCode, FString(TEXT("presentation-profile-material")));

	TestTrue(
		TEXT("The shipped profile reloads after negative contract fixtures."),
		ProjectWorldPresentationProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error));
	FProjectWorldPresentationResources FirstMaterialResources;
	TestTrue(
		TEXT("Terrain material resources resolve for first preparation."),
		ProjectWorldPresentationProfile::ResolveResources(Profile, FirstMaterialResources, Error));
	TestTrue(
		TEXT("Generated terrain material is prepared."),
		ProjectWorldPresentationMaterialRealization::Prepare(
			Profile,
			FirstMaterialResources,
			TEXT("/ProjectWorld/Generated/"),
			Error));
	const FString MaterialFilename = FPackageName::LongPackageNameToFilename(
		TEXT("/ProjectWorld/Generated/Presentation/MI_ProjectWorldTerrain_kazan_representative_v1"),
		FPackageName::GetAssetPackageExtension());
	const FDateTime FirstTimestamp = IFileManager::Get().GetTimeStamp(*MaterialFilename);
	FPlatformProcess::Sleep(1.1f);
	FProjectWorldPresentationResources SecondMaterialResources;
	TestTrue(
		TEXT("Terrain material resources resolve for unchanged preparation."),
		ProjectWorldPresentationProfile::ResolveResources(Profile, SecondMaterialResources, Error));
	TestTrue(
		TEXT("Unchanged terrain material is accepted without a rewrite."),
		ProjectWorldPresentationMaterialRealization::Prepare(
			Profile,
			SecondMaterialResources,
			TEXT("/ProjectWorld/Generated/"),
			Error));
	TestEqual(
		TEXT("Unchanged terrain material keeps its package timestamp."),
		IFileManager::Get().GetTimeStamp(*MaterialFilename),
		FirstTimestamp);

	IFileManager::Get().Delete(*InvalidMaterialPath, false, true, true);
	IFileManager::Get().Delete(*DuplicateViewpointPath, false, true, true);
	IFileManager::Get().Delete(*InvalidProfileIdPath, false, true, true);
	IFileManager::Get().Delete(*InvalidViewpointTokenPath, false, true, true);
	IFileManager::Get().Delete(*ProductionPath, false, true, true);
	IFileManager::Get().Delete(*UnsupportedTerrainPath, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPresentationActorLifecycleTest,
	"Project.World.Realization.Presentation.ActorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPresentationActorLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldPresentationTests;
	FProjectWorldPresentationProfile Profile;
	FProjectWorldPresentationResources Resources;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldPresentationProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error) ||
		!ProjectWorldPresentationProfile::ResolveResources(Profile, Resources, Error))
	{
		AddError(FString::Printf(TEXT("Cannot load presentation fixture: %s %s"), *ErrorCode, *Error));
		return false;
	}

	UWorld* World = GEditor->NewMap(false);
	AActor* AuthoredActor = World->SpawnActor<AActor>();
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	FProjectWorldRealizationResult FirstResult;
	TestTrue(
		TEXT("The complete outdoor environment and viewpoints are created."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Profile, Resources, FirstResult, Error));
	TestEqual(TEXT("Six environment actors and three viewpoints are generated."), FirstResult.PresentationActorCount, 9);
	TestEqual(TEXT("First realization creates every presentation actor."), FirstResult.CreatedActorCount, 9);

	TMap<FString, FGuid> ActorGuids;
	ADirectionalLight* Sun = nullptr;
	APostProcessVolume* PostProcess = nullptr;
	ACameraActor* CaptureCamera = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Role = PresentationRole(**It);
		if (!Role.IsEmpty())
		{
			ActorGuids.Add(Role, It->GetActorGuid());
		}
		if (It->IsA<ADirectionalLight>())
		{
			Sun = Cast<ADirectionalLight>(*It);
		}
		if (It->IsA<APostProcessVolume>())
		{
			PostProcess = Cast<APostProcessVolume>(*It);
		}
		if (Role == TEXT("Capture_overview"))
		{
			CaptureCamera = Cast<ACameraActor>(*It);
		}
	}
	TestEqual(TEXT("Every generated role is unique."), ActorGuids.Num(), 9);
	TestNotNull(TEXT("Generated sun exists."), Sun);
	if (Sun != nullptr)
	{
		TestEqual(
			TEXT("Sun intensity comes from the profile."),
			Sun->GetComponent()->Intensity,
			Profile.SunIntensityLux);
	}
	TestNotNull(TEXT("Generated post-process volume exists."), PostProcess);
	if (PostProcess != nullptr)
	{
		TestTrue(
			TEXT("Generated post-process volume has valid editor bounds."),
			PostProcess->GetRootComponent() != nullptr &&
				PostProcess->GetRootComponent()->Bounds.SphereRadius > UE_SMALL_NUMBER);
		TestEqual(
			TEXT("Exposure is deterministic manual metering."),
			PostProcess->Settings.AutoExposureMethod,
			EAutoExposureMethod::AEM_Manual);
		TestTrue(
			TEXT("Manual exposure uses physical camera values."),
			PostProcess->Settings.AutoExposureApplyPhysicalCameraExposure);
		TestTrue(
			TEXT("The profile EV100 derives the physical shutter speed."),
			FMath::IsNearlyEqual(
				PostProcess->Settings.CameraShutterSpeed,
				FMath::Pow(2.0f, Profile.FixedExposureEv100) / FMath::Square(4.0f)));
	}
	TestNotNull(TEXT("Generated overview camera exists."), CaptureCamera);
	if (CaptureCamera != nullptr)
	{
		const UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent();
		TestEqual(TEXT("Capture camera owns its fixed exposure."), CameraComponent->PostProcessBlendWeight, 1.0f);
		TestEqual(
			TEXT("Capture camera uses deterministic manual metering."),
			CameraComponent->PostProcessSettings.AutoExposureMethod,
			EAutoExposureMethod::AEM_Manual);
		TestTrue(
			TEXT("Capture camera derives the profile EV100."),
			FMath::IsNearlyEqual(
				CameraComponent->PostProcessSettings.CameraShutterSpeed,
				FMath::Pow(2.0f, Profile.FixedExposureEv100) / FMath::Square(4.0f)));
	}

	FProjectWorldRealizationResult GenericCleanupResult;
	TestTrue(
		TEXT("The generic Apply cleanup delegates current-grid presentation actors to their owner."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World,
			Bundle,
			FString(),
			false,
			GenericCleanupResult));
	TestEqual(TEXT("Generic cleanup removes no current presentation actor."), GenericCleanupResult.RemovedActorCount, 0);

	FProjectWorldRealizationResult SecondResult;
	TestTrue(
		TEXT("An unchanged presentation reuses its actors."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Profile, Resources, SecondResult, Error));
	TestEqual(TEXT("Unchanged realization creates no actor."), SecondResult.CreatedActorCount, 0);
	TestEqual(TEXT("Unchanged realization removes no actor."), SecondResult.RemovedActorCount, 0);
	TestEqual(TEXT("Unchanged realization updates the nine stable actors."), SecondResult.UpdatedActorCount, 9);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Role = PresentationRole(**It);
		if (!Role.IsEmpty())
		{
			TestEqual(TEXT("Presentation actor GUID stays stable."), It->GetActorGuid(), ActorGuids.FindChecked(Role));
		}
	}
	TestTrue(TEXT("Untagged authored content survives presentation regeneration."), IsValid(AuthoredActor));

	AActor* Retired = World->SpawnActor<AActor>();
	Retired->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
	Retired->Tags.Add(FName(TEXT("ProjectWorld.PresentationRole=Capture_retired")));
	FProjectWorldRealizationResult CleanupResult;
	TestTrue(
		TEXT("A removed presentation role is cleaned without touching authored content."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Profile, Resources, CleanupResult, Error));
	TestEqual(TEXT("Exactly one retired role is removed."), CleanupResult.RemovedActorCount, 1);
	TestTrue(TEXT("Authored content remains after stale-role cleanup."), IsValid(AuthoredActor));

	FProjectWorldRealizationResult ExistingEvidence;
	TestTrue(
		TEXT("Existing presentation produces semantic evidence."),
		ProjectWorldSemanticEvidence::Capture(World, ExistingEvidence, Error));
	UWorld* CleanWorld = GEditor->NewMap(false);
	FProjectWorldRealizationResult CleanResult;
	TestTrue(
		TEXT("A clean map recreates the presentation."),
		ProjectWorldPresentationRealization::Apply(CleanWorld, Bundle, Profile, Resources, CleanResult, Error));
	TMap<FString, FGuid> CleanActorGuids;
	for (TActorIterator<AActor> It(CleanWorld); It; ++It)
	{
		const FString Role = PresentationRole(**It);
		if (!Role.IsEmpty())
		{
			CleanActorGuids.Add(Role, It->GetActorGuid());
		}
	}
	TestEqual(TEXT("Clean rebuild recreates every role."), CleanActorGuids.Num(), ActorGuids.Num());
	for (const TPair<FString, FGuid>& Pair : ActorGuids)
	{
		TestEqual(TEXT("Clean rebuild keeps deterministic role GUIDs."), CleanActorGuids.FindChecked(Pair.Key), Pair.Value);
	}
	FProjectWorldRealizationResult CleanEvidence;
	TestTrue(
		TEXT("Clean presentation produces semantic evidence."),
		ProjectWorldSemanticEvidence::Capture(CleanWorld, CleanEvidence, Error));
	TestEqual(
		TEXT("Clean-map presentation has the same complete D3 fingerprint."),
		CleanEvidence.SemanticFingerprint,
		ExistingEvidence.SemanticFingerprint);

	FProjectWorldPresentationProfile TransitionProfile = Profile;
	TransitionProfile.ProfileId = TEXT("kazan_representative_v2");
	FProjectWorldRealizationResult TransitionResult;
	TestTrue(
		TEXT("Applying another profile replaces actors whose deterministic identity changed."),
		ProjectWorldPresentationRealization::Apply(
			CleanWorld,
			Bundle,
			TransitionProfile,
			Resources,
			TransitionResult,
			Error));
	TestEqual(TEXT("Profile transition replaces every old presentation actor."), TransitionResult.RemovedActorCount, 9);
	TestEqual(TEXT("Profile transition creates every new presentation actor."), TransitionResult.CreatedActorCount, 9);
	for (TActorIterator<AActor> It(CleanWorld); It; ++It)
	{
		const FString Role = PresentationRole(**It);
		if (!Role.IsEmpty())
		{
			TestEqual(
				TEXT("Transitioned actor owns the target profile GUID."),
				It->GetActorGuid(),
				ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|") + TransitionProfile.ProfileId + TEXT("|") + Role));
		}
	}
	FProjectWorldRealizationResult TransitionEvidence;
	TestTrue(
		TEXT("Transitioned presentation produces semantic evidence."),
		ProjectWorldSemanticEvidence::Capture(CleanWorld, TransitionEvidence, Error));

	UWorld* CleanTransitionWorld = GEditor->NewMap(false);
	FProjectWorldRealizationResult CleanTransitionResult;
	TestTrue(
		TEXT("A clean map creates the target presentation profile."),
		ProjectWorldPresentationRealization::Apply(
			CleanTransitionWorld,
			Bundle,
			TransitionProfile,
			Resources,
			CleanTransitionResult,
			Error));
	FProjectWorldRealizationResult CleanTransitionEvidence;
	TestTrue(
		TEXT("Clean target presentation produces semantic evidence."),
		ProjectWorldSemanticEvidence::Capture(
			CleanTransitionWorld,
			CleanTransitionEvidence,
			Error));
	TestEqual(
		TEXT("Incremental profile transition matches a clean target-profile build."),
		TransitionEvidence.SemanticFingerprint,
		CleanTransitionEvidence.SemanticFingerprint);
	return true;
}

#endif
