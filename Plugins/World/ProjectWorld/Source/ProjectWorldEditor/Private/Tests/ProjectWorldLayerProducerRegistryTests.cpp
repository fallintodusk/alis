#include "ProjectWorldLayerProducerRegistry.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldLayerInventory.h"
#include "ProjectWorldLayerPipeline.h"
#include "ProjectWorldStaticPartitionAudit.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Features/IModularFeatures.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FProbeLayerProducer final : public IProjectWorldLayerProducer
	{
	public:
		FProjectWorldLayerProducerDeclaration Declaration;

		FProbeLayerProducer()
		{
			Declaration.GeneratorId = TEXT("project_test_probe");
			Declaration.GeneratorVersion = 1;
			Declaration.CanonicalSelectors = {TEXT("terrain")};
			Declaration.SpatialOwnership = TEXT("cell_local");
			Declaration.RuntimeMapping = TEXT("world_partition_spatial");
			Declaration.DependencyHaloCells = 0;
			Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.TestProbe.v1"))};
		}

		virtual const FProjectWorldLayerProducerDeclaration& GetDeclaration() const override
		{
			return Declaration;
		}

		virtual bool ValidateSettings(const FString& Settings, FString& OutError) const override
		{
			if (Settings == TEXT("{}"))
			{
				return true;
			}
			OutError = TEXT("Probe settings are invalid.");
			return false;
		}

		virtual bool HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
			FProjectWorldLayerUnitInputs& OutUnits, FString&) const override
		{
			for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
			{
				OutUnits.HashByUnit.Add(Cell.CellId, Cell.Terrain.ArtifactHash);
			}
			return true;
		}

		virtual bool Apply(const FProjectWorldLayerApplyContext& Context,
			FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult&, FString&) const override
		{
			int64 Written = 0;
			for (const FProjectWorldCanonicalCell& Cell : Context.Inputs.Bundle.Cells)
			{
				AActor* Actor = Context.World.SpawnActor<AActor>();
				if (Actor == nullptr)
				{
					return false;
				}
				Actor->Tags.Add(Declaration.OwnedActors.OwnedTags[0]);
				Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
				Actor->Tags.Add(FName(*(TEXT("ProjectWorld.Grid=") + Context.Inputs.Bundle.GridId)));
				Actor->Tags.Add(FName(*(Declaration.OwnedActors.CellTagPrefix + Cell.CellId)));
				++Written;
			}
			Inventory.Metrics.Add(TEXT("actors_written"), Written);
			return true;
		}

		virtual bool CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
			FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult&, FString&) const override
		{
			int64 Count = 0;
			for (TActorIterator<AActor> It(&Context.World); It; ++It)
			{
				Count += It->Tags.Contains(Declaration.OwnedActors.OwnedTags[0]) ? 1 : 0;
			}
			Inventory.Metrics.Add(TEXT("cell_actors"), Count);
			return true;
		}
	};

	class FRegisteredProbe final
	{
	public:
		explicit FRegisteredProbe(FProbeLayerProducer& InProducer) : Producer(InProducer)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IProjectWorldLayerProducer::GetModularFeatureName(), &Producer);
		}

		~FRegisteredProbe()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IProjectWorldLayerProducer::GetModularFeatureName(), &Producer);
		}

	private:
		FProbeLayerProducer& Producer;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldLayerProducerRegistryTest,
	"Project.World.Realization.Layers.ProducerRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLayerProducerRegistryTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestNull(TEXT("An unregistered producer fails closed."),
		ProjectWorldLayerProducerRegistry::Find(TEXT("project_test_probe"), 1, Error));
	TestTrue(TEXT("The unknown key is named."), Error.Contains(TEXT("project_test_probe:v1")));

	FProbeLayerProducer Producer;
	FRegisteredProbe Registration(Producer);
	TestEqual(TEXT("The probe can be found by its exact pair."),
		ProjectWorldLayerProducerRegistry::Find(TEXT("project_test_probe"), 1, Error),
		static_cast<const IProjectWorldLayerProducer*>(&Producer));
	FProjectWorldRealizationLayer Layer;
	Layer.GeneratorId = TEXT("project_test_probe");
	Layer.GeneratorVersion = 1;
	Layer.CanonicalSelectors = {TEXT("terrain")};
	Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
	Layer.SpatialOwnership = TEXT("cell_local");
	Layer.RuntimeMapping = TEXT("world_partition_spatial");
	Layer.NormalizedSettings = TEXT("{}");
	TestTrue(TEXT("The declared tuple and settings are accepted."),
		ProjectWorldLayerProducerRegistry::ValidateLayer(Layer, Error));
	FProjectWorldRealizationProfile Profile;
	Profile.ProfileId = TEXT("probe_profile");
	Profile.WorldDataPluginName = TEXT("ProjectWorldTestData");
	Profile.CanonicalProfileId = TEXT("probe_canonical");
	Profile.MapPackagePath = TEXT("/ProjectWorldTestData/Generated/L_Probe");
	Profile.RuntimeProfileId = TEXT("none");
	Profile.ProtectedAuthoredRoots = {TEXT("/ProjectWorldTestData/Authored/")};
	Profile.ExcludedRuntimeStateRoots = {TEXT("/ProjectWorldTestData/Runtime/")};
	Layer.LayerId = TEXT("probe");
	Layer.ArtifactRoot = TEXT("/ProjectWorldTestData/Generated/Probe/");
	Profile.Layers.Add(Layer);
	TestTrue(TEXT("Profile admission uses a registered producer without a host allowlist edit."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(Profile, Error));
	Layer.CanonicalSelectors = {TEXT("water")};
	TestFalse(TEXT("The tuple cannot change its source selector."),
		ProjectWorldLayerProducerRegistry::ValidateLayer(Layer, Error));
	Layer.CanonicalSelectors = {TEXT("terrain")};
	Layer.NormalizedSettings = TEXT("{\"unexpected\":true}");
	TestFalse(TEXT("Settings are checked by the producer."),
		ProjectWorldLayerProducerRegistry::ValidateLayer(Layer, Error));

	FProbeLayerProducer Duplicate;
	{
		FRegisteredProbe DuplicateRegistration(Duplicate);
		TestNull(TEXT("Duplicate registrations fail closed."),
			ProjectWorldLayerProducerRegistry::Find(TEXT("project_test_probe"), 1, Error));
		TestTrue(TEXT("The duplicate key is named."), Error.Contains(TEXT("project_test_probe:v1")));
	}
	Producer.Declaration.OwnedActors.OwnedTags.Reset();
	TestNull(TEXT("Malformed declarations fail closed."),
		ProjectWorldLayerProducerRegistry::Find(TEXT("project_test_probe"), 1, Error));
	TestTrue(TEXT("The malformed key is named."), Error.Contains(TEXT("project_test_probe:v1")));
	Producer.Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.TestProbe.v1"))};
	FProjectWorldPostApplyBuilder UnsafeBuilder;
	UnsafeBuilder.InventoryCommandlet = TEXT("ProbeInventory");
	UnsafeBuilder.InventorySchema = TEXT("probe-inventory:v1");
	UnsafeBuilder.BuilderCommandlet = TEXT("ProbeBuilder");
	UnsafeBuilder.BuilderArguments = {TEXT(";Remove-Item")};
	Producer.Declaration.PostApplyBuilder = UnsafeBuilder;
	TestNull(TEXT("An unsafe builder declaration fails before realization."),
		ProjectWorldLayerProducerRegistry::Find(TEXT("project_test_probe"), 1, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldLayerHostBoundaryTest,
	"Project.World.Realization.Layers.HostBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLayerHostBoundaryTest::RunTest(const FString& Parameters)
{
	FProbeLayerProducer Producer;
	Producer.Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.TestProbeCell=");
	FProjectWorldPostApplyBuilder Builder;
	Builder.InventoryCommandlet = TEXT("ProbeInventory");
	Builder.InventorySchema = TEXT("probe-inventory:v1");
	Builder.BuilderCommandlet = TEXT("ProbeBuilder");
	Producer.Declaration.PostApplyBuilder = Builder;
	FRegisteredProbe Registration(Producer);
	FProjectWorldRealizationProfile Profile;
	Profile.ProfileId = TEXT("probe_profile");
	Profile.WorldDataPluginName = TEXT("ProjectWorldTestData");
	Profile.CanonicalProfileId = TEXT("probe_canonical");
	Profile.MapPackagePath = TEXT("/ProjectWorldTestData/Generated/L_Probe");
	Profile.RuntimeProfileId = TEXT("none");
	Profile.ProtectedAuthoredRoots = {TEXT("/ProjectWorldTestData/Authored/")};
	Profile.ExcludedRuntimeStateRoots = {TEXT("/ProjectWorldTestData/Runtime/")};
	FProjectWorldRealizationLayer Layer;
	Layer.LayerId = TEXT("probe");
	Layer.GeneratorId = TEXT("project_test_probe");
	Layer.GeneratorVersion = 1;
	Layer.CanonicalSelectors = {TEXT("terrain")};
	Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
	Layer.SpatialOwnership = TEXT("cell_local");
	Layer.RuntimeMapping = TEXT("world_partition_spatial");
	Layer.ArtifactRoot = TEXT("/ProjectWorldTestData/Generated/Probe/");
	Layer.NormalizedSettings = TEXT("{}");
	Profile.Layers.Add(Layer);
	FString Error;
	if (!TestTrue(TEXT("The probe profile validates through the registered declaration."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(Profile, Error)))
	{
		AddError(Error);
		return false;
	}
	FProjectWorldCanonicalBundle Bundle;
	Bundle.ProfileId = TEXT("probe_canonical");
	Bundle.WorldDataPluginName = TEXT("ProjectWorldTestData");
	Bundle.GridId = TEXT("probe_grid");
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("probe_grid:x%d:y0"), Index);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, Index == 0 ? TEXT('a') : TEXT('b'));
		Bundle.Cells.Add(MoveTemp(Cell));
	}
	const FProjectWorldAuthoredOverlaySet Overlays;
	FProjectWorldRealizationResult Result;
	if (!TestTrue(TEXT("The generic inventory hashes both producer units."),
		ProjectWorldLayerInventory::Build(Bundle, Profile, Overlays, true, nullptr, Result, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("One layer inventory is created."), Result.LayerInventories.Num(), 1);
	TestEqual(TEXT("Both canonical units are inventoried."),
		Result.LayerInventories[0].CanonicalInputs.Num(), 2);
	UWorld* World = GEditor->NewMap(false);
	if (!TestNotNull(TEXT("The transient map exists."), World))
	{
		return false;
	}
	const TMap<FName, UMaterialInterface*> Materials;
	if (!TestTrue(TEXT("The generic pipeline applies a new producer."),
		ProjectWorldLayerPipeline::ApplyLayers(*World, Bundle, Profile, Overlays, Materials, Result, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Apply reports two actor writes."),
		Result.LayerInventories[0].Metrics.FindRef(TEXT("actors_written")), int64(2));
	if (!TestTrue(TEXT("Artifact capture dispatches to the same producer."),
		ProjectWorldLayerInventory::CaptureArtifacts(World, Bundle, Profile, Overlays, Result, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Capture reports two owned actors."),
		Result.LayerInventories[0].Metrics.FindRef(TEXT("cell_actors")), int64(2));
	FProjectWorldRealizationRequest Request;
	Request.CompileResultPath = TEXT("probe-compile.json");
	Request.ResultPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/locality/s1/probe-result.json"));
	if (!TestTrue(TEXT("The generic host writes a result."),
		FProjectWorldRealizationService::WriteResult(Request, Result)))
	{
		return false;
	}
	FString ResultText;
	TSharedPtr<FJsonObject> ResultJson;
	const bool bReadResult = FFileHelper::LoadFileToString(ResultText, *Request.ResultPath) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResultText), ResultJson) &&
		ResultJson.IsValid();
	if (!TestTrue(TEXT("The written result is valid JSON."), bReadResult))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> LayerJson =
		ResultJson->GetArrayField(TEXT("layer_inventories"))[0]->AsObject();
	TestEqual(TEXT("The writer preserves producer metrics."),
		LayerJson->GetObjectField(TEXT("metrics"))->GetIntegerField(TEXT("cell_actors")), 2);
	TestEqual(TEXT("The writer preserves the producer builder declaration."),
		LayerJson->GetObjectField(TEXT("post_apply_builder"))->GetStringField(TEXT("builder_commandlet")),
		FString(TEXT("ProbeBuilder")));
	AActor* Ownerless = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("The ownerless control actor exists."), Ownerless))
	{
		return false;
	}
	Ownerless->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
	Ownerless->Tags.Add(FName(TEXT("ProjectWorld.Grid=probe_grid")));
	Ownerless->Tags.Add(FName(TEXT("ProjectWorld.Cell=probe_grid:x0:y0")));
	const IProjectWorldLayerProducer* OwnerlessProducer = nullptr;
	TestTrue(TEXT("The ownerless control has no ambiguous declaration."),
		ProjectWorldLayerProducerRegistry::FindActorOwner(*Ownerless, OwnerlessProducer, Error));
	TestNull(TEXT("A generic cell tag does not select a producer."), OwnerlessProducer);
	TestFalse(TEXT("The static audit rejects an ownerless generated actor."),
		ProjectWorldStaticPartitionAudit::ValidateGeneratedActorOwnership(*Ownerless, OwnerlessProducer, Error));
	TestTrue(TEXT("The audit names the missing owner."), Error.Contains(TEXT("no registered producer")));
	FProjectWorldRealizationResult OwnerlessResult;
	TestTrue(TEXT("The lifecycle removes an ownerless generated actor."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), OwnerlessResult, &Error));
	TestEqual(TEXT("Only the ownerless generated actor is removed."), OwnerlessResult.RemovedActorCount, 1);
	TestTrue(TEXT("Current producer-owned cells survive the generic lifecycle."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), Result, &Error));
	TestEqual(TEXT("Current cells are preserved."), Result.RemovedActorCount, 0);
	Bundle.Cells.RemoveAt(1);
	TestTrue(TEXT("A retired cell is removed through declared ownership."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), Result, &Error));
	TestEqual(TEXT("Only the retired cell is removed."), Result.RemovedActorCount, 1);
	TestTrue(TEXT("Default Delete sweeps producer-owned actors."),
		ProjectWorldLayerPipeline::DeleteLayers(*World, Bundle, Profile, Overlays, Result, Error));
	TestEqual(TEXT("Delete removes both producer-owned actors."), Result.RemovedActorCount, 2);
	return true;
}

#endif
