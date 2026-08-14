// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldLayerInventory.h"
#include "ProjectWorldRealizationService.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldRealizationProfileTests
{
	FString ProfilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json"));
	}

	const FProjectWorldLayerDirtyPlan* FindPlan(
		const TArray<FProjectWorldLayerDirtyPlan>& Plan,
		const FString& LayerId)
	{
		return Plan.FindByPredicate([&LayerId](const FProjectWorldLayerDirtyPlan& Entry)
		{
			return Entry.LayerId == LayerId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRealizationProfileContractTest,
	"Project.World.Realization.Layers.ProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRealizationProfileContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRealizationProfileTests;
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	TestTrue(
		TEXT("The shipped synthetic realization profile is accepted."),
		ProjectWorldRealizationProfile::Load(ProfilePath(), Profile, ErrorCode, Error));
	TestEqual(TEXT("Profile identity is loaded."), Profile.ProfileId, FString(TEXT("synthetic_landscape_water_twin")));
	TestEqual(TEXT("Profile owner is data-defined."), Profile.WorldDataPluginName, FString(TEXT("ProjectWorldTestData")));
	TestEqual(TEXT("Epic partitioning remains one component per proxy."), Profile.ComponentsPerProxy, 1);
	TestEqual(TEXT("The layer DAG has two nodes."), Profile.TopologicalLayerIds.Num(), 2);
	TestEqual(TEXT("Terrain executes before water."), Profile.TopologicalLayerIds[0], FString(TEXT("terrain")));
	TestEqual(TEXT("Water executes after terrain."), Profile.TopologicalLayerIds[1], FString(TEXT("water")));
	TestEqual(TEXT("Each layer has a normalized contract SHA-256."), Profile.Layers[0].ContractHash.Len(), 64);
	TestTrue(
		TEXT("The exact landscape generator pair is registered."),
		ProjectWorldRealizationProfile::IsGeneratorRegistered(
			TEXT("project_landscape"),
			1,
			EProjectWorldLayerKind::GeneratedGeography));
	TestFalse(
		TEXT("Unknown generator versions fail closed."),
		ProjectWorldRealizationProfile::IsGeneratorRegistered(
			TEXT("project_landscape"),
			2,
			EProjectWorldLayerKind::GeneratedGeography));

	FProjectWorldRealizationProfile Cyclic = Profile;
	Cyclic.Layers[0].DependsOn = {TEXT("water")};
	TestFalse(
		TEXT("A dependency cycle is rejected before mutation."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(Cyclic, Error));

	FProjectWorldRealizationProfile Overlap = Profile;
	Overlap.Layers[1].ArtifactRoot = Profile.Layers[0].ArtifactRoot + TEXT("Water/");
	TestFalse(
		TEXT("Overlapping artifact roots are rejected before mutation."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(Overlap, Error));

	FProjectWorldRealizationProfile UnknownGenerator = Profile;
	UnknownGenerator.Layers[1].GeneratorVersion = 2;
	TestFalse(
		TEXT("An unknown generator pair is rejected before mutation."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(UnknownGenerator, Error));
	TestFalse(
		TEXT("A future gameplay generator is not advertised before its typed unit domain exists."),
		ProjectWorldRealizationProfile::IsGeneratorRegistered(
			TEXT("project_gameplay_placement"),
			1,
			EProjectWorldLayerKind::GeneratedGameplayPlacement));

	FProjectWorldRealizationProfile RenamedLandscape = Profile;
	RenamedLandscape.LogicalLandscapeId = TEXT("synthetic_main_v2");
	TestTrue(
		TEXT("A changed logical Landscape identity remains a valid semantic profile."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(RenamedLandscape, Error));
	for (int32 Index = 0; Index < Profile.Layers.Num(); ++Index)
	{
		TestNotEqual(
			TEXT("Profile-level execution changes invalidate every generated layer contract."),
			RenamedLandscape.Layers[Index].ContractHash,
			Profile.Layers[Index].ContractHash);
	}

	FProjectWorldRealizationProfile InvalidGranularity = Profile;
	InvalidGranularity.Layers[0].DirtyGranularity = EProjectWorldDirtyGranularity::WholeLayer;
	TestFalse(
		TEXT("The landscape v1 generator cannot silently change dirty granularity."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(InvalidGranularity, Error));

	FProjectWorldRealizationProfile BroadRoot = Profile;
	BroadRoot.Layers[0].ArtifactRoot = TEXT("/ProjectWorldTestData/Generated/");
	TestFalse(
		TEXT("A layer cannot own its data plugin's complete Generated root."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(BroadRoot, Error));

	FProjectWorldRealizationProfile InvalidTuple = Profile;
	InvalidTuple.Layers[1].SpatialOwnership = TEXT("logical_landscape_with_cell_proxies");
	TestFalse(
		TEXT("Registered generator pairs pin their complete executable tuple."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(InvalidTuple, Error));

	FProjectWorldRealizationProfile InvalidSettings = Profile;
	InvalidSettings.Layers[1].NormalizedSettings = TEXT("{\"material_shading_model\":\"single_layer_water\",\"nanite\":true}");
	TestFalse(
		TEXT("Registered generators reject settings outside their typed contract."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(InvalidSettings, Error));

	FProjectWorldRealizationProfile ForeignProtectedRoot = Profile;
	ForeignProtectedRoot.ProtectedAuthoredRoots[0] = TEXT("/ProjectWorldData/Authored/");
	TestFalse(
		TEXT("Protected roots cannot escape the profile's data owner."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(ForeignProtectedRoot, Error));

	FProjectWorldRealizationProfile ForeignRuntimeRoot = Profile;
	ForeignRuntimeRoot.ExcludedRuntimeStateRoots[0] = TEXT("/ProjectWorldData/Runtime/");
	TestFalse(
		TEXT("Runtime exclusions cannot escape the profile's data owner."),
		ProjectWorldRealizationProfile::ValidateAndFinalize(ForeignRuntimeRoot, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRealizationDirtyClosureTest,
	"Project.World.Realization.Layers.DirtyClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRealizationDirtyClosureTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRealizationProfileTests;
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(ProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}

	Profile.Layers[1].DependencyHaloCells = 1;
	if (!ProjectWorldRealizationProfile::ValidateAndFinalize(Profile, Error))
	{
		AddError(Error);
		return false;
	}

	TArray<FProjectWorldLayerDirtyPlan> Plan;
	FProjectWorldDirtyInputs FirstApply;
	FirstApply.bFirstApply = true;
	TestTrue(
		TEXT("First Apply creates a full generated-layer plan."),
		ProjectWorldRealizationProfile::BuildDirtyPlan(Profile, FirstApply, Plan, Error));
	for (const FProjectWorldLayerDirtyPlan& Entry : Plan)
	{
		TestTrue(TEXT("Every first-Apply layer is fully dirty."), Entry.DirtyUnits.Contains(TEXT("*")));
	}

	FProjectWorldDirtyInputs Unchanged;
	TestTrue(
		TEXT("An unchanged Apply builds a valid no-op plan."),
		ProjectWorldRealizationProfile::BuildDirtyPlan(Profile, Unchanged, Plan, Error));
	for (const FProjectWorldLayerDirtyPlan& Entry : Plan)
	{
		TestEqual(TEXT("Unchanged layers remain clean."), Entry.DirtyUnits.Num(), 0);
	}

	const FString ChangedCell = TEXT("gridtwin:x0:y0");
	FProjectWorldDirtyInputs Incremental;
	Incremental.ValidUnits.FindOrAdd(TEXT("terrain")).Add(TEXT("gridtwin:x0:y0"));
	Incremental.ValidUnits.FindOrAdd(TEXT("terrain")).Add(TEXT("gridtwin:x1:y0"));
	Incremental.ValidUnits.FindOrAdd(TEXT("water")).Add(TEXT("gridtwin:x0:y0"));
	Incremental.ValidUnits.FindOrAdd(TEXT("water")).Add(TEXT("gridtwin:x1:y0"));
	Incremental.OperatorValidUnits = Incremental.ValidUnits;
	Incremental.ComputedUnits.FindOrAdd(TEXT("terrain")).Add(ChangedCell);
	Incremental.OperatorAdditions.FindOrAdd(TEXT("water")).Add(TEXT("gridtwin:x1:y0"));
	TestTrue(
		TEXT("Computed and operator-added units form one dependency closure."),
		ProjectWorldRealizationProfile::BuildDirtyPlan(Profile, Incremental, Plan, Error));
	const FProjectWorldLayerDirtyPlan* Terrain = FindPlan(Plan, TEXT("terrain"));
	const FProjectWorldLayerDirtyPlan* Water = FindPlan(Plan, TEXT("water"));
	TestNotNull(TEXT("Terrain plan exists."), Terrain);
	TestNotNull(TEXT("Water plan exists."), Water);
	if (Terrain != nullptr && Water != nullptr)
	{
		TestEqual(TEXT("The computed terrain unit is retained."), Terrain->DirtyUnits.Num(), 1);
		TestTrue(TEXT("Terrain changes propagate to water."), Water->DirtyUnits.Contains(ChangedCell));
		TestTrue(TEXT("Operator additions can expand water work."), Water->DirtyUnits.Contains(TEXT("gridtwin:x1:y0")));
		TestEqual(TEXT("The dependent halo is clipped to the real two-cell domain."), Water->DirtyUnits.Num(), 2);
	}

	FProjectWorldDirtyInputs Invalid;
	Invalid.ValidUnits = Incremental.ValidUnits;
	Invalid.OperatorValidUnits = Incremental.OperatorValidUnits;
	Invalid.OperatorAdditions.FindOrAdd(TEXT("water")).Add(TEXT("gridtwin:x2:y0"));
	TestFalse(
		TEXT("Operators cannot target a canonical cell outside the current unit domain."),
		ProjectWorldRealizationProfile::BuildDirtyPlan(Profile, Invalid, Plan, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRealizationIncrementalInventoryTest,
	"Project.World.Realization.Layers.IncrementalInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRealizationIncrementalInventoryTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRealizationProfileTests;
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(ProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}

	FProjectWorldCanonicalBundle Bundle;
	for (int32 CellIndex = 0; CellIndex < 2; ++CellIndex)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("gridtwin:x%d:y0"), CellIndex);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, CellIndex == 0 ? TEXT('a') : TEXT('b'));
		Cell.FeatureArtifactHash = FString::ChrN(64, CellIndex == 0 ? TEXT('c') : TEXT('d'));
		Bundle.Cells.Add(MoveTemp(Cell));
	}
	FProjectWorldCanonicalFeature Lake;
	Lake.FeatureId = TEXT("water/1");
	Lake.FeatureClass = TEXT("water");
	Lake.WidthMeters = 12.0;
	Lake.WaterSurface.bValid = true;
	Lake.WaterSurface.SurfaceGroupId = TEXT("lake/1");
	Lake.WaterSurface.SurfaceGroupMembers = {TEXT("water/1")};
	Lake.WaterSurface.Geometry = TEXT("polygon");
	Lake.WaterSurface.Behavior = TEXT("standing");
	Lake.WaterSurface.Role = TEXT("visible_surface");
	Lake.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Lake.WaterSurface.FunctionVersion = 1;
	Lake.WaterSurface.LevelMeters = 12.5;
	FProjectWorldCanonicalRepresentation LakeRepresentation;
	LakeRepresentation.CellId = Bundle.Cells[0].CellId;
	LakeRepresentation.Kind = TEXT("polygon");
	LakeRepresentation.Points = {FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(1.0, 1.0)};
	Lake.Representations.Add(MoveTemp(LakeRepresentation));
	Bundle.Cells[0].ReferencedFeatureIds.Add(Lake.FeatureId);
	Bundle.Features.Add(Lake.FeatureId, MoveTemp(Lake));

	FProjectWorldRealizationResult Baseline;
	TestTrue(
		TEXT("First Apply derives the current typed layer input domains."),
		ProjectWorldLayerInventory::Build(Bundle, Profile, true, nullptr, Baseline, Error));
	auto BaselineInventory = [&Baseline](const FString& LayerId)
	{
		return Baseline.LayerInventories.FindByPredicate([&LayerId](const auto& Inventory)
		{
			return Inventory.LayerId == LayerId;
		});
	};
	const FProjectWorldLayerInventory* BaselineTerrain = BaselineInventory(TEXT("terrain"));
	const FProjectWorldLayerInventory* BaselineWater = BaselineInventory(TEXT("water"));
	if (BaselineTerrain == nullptr || BaselineWater == nullptr)
	{
		AddError(TEXT("Baseline layer inventories are incomplete."));
		return false;
	}

	FProjectWorldLayerDirtyInput DirtyInput;
	DirtyInput.RealizationProfileId = Profile.ProfileId;
	for (const FProjectWorldLayerInventory& Inventory : Baseline.LayerInventories)
	{
		FProjectWorldLayerBaseIdentity& Base = DirtyInput.BaseLayers.Add(Inventory.LayerId);
		Base.NormalizedLayerContractHash = Inventory.NormalizedLayerContractHash;
		for (const FProjectWorldLayerInputInventory& Input : Inventory.CanonicalInputs)
		{
			Base.CanonicalInputs.Add(Input.UnitId, Input.Hash);
		}
	}
	DirtyInput.BaseLayers[TEXT("terrain")].CanonicalInputs[TEXT("gridtwin:x0:y0")] = FString::ChrN(64, TEXT('e'));
	DirtyInput.OperatorAdditions.FindOrAdd(TEXT("water")).Add(TEXT("gridtwin:x1:y0"));
	const FString DirtyInputPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/ProjectWorldLayers/dirty_input.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DirtyInputPath), true);
	const FString DirtyInputJson = FString::Printf(
		TEXT("{\n")
		TEXT("  \"$schema\": \"%s\",\n")
		TEXT("  \"schema_version\": 1,\n")
		TEXT("  \"realization_profile_id\": \"%s\",\n")
		TEXT("  \"base_layers\": [\n")
		TEXT("    {\"layer_id\": \"terrain\", \"normalized_layer_contract_sha256\": \"%s\", \"canonical_inputs\": [")
		TEXT("{\"unit_id\": \"gridtwin:x0:y0\", \"sha256\": \"%s\"},")
		TEXT("{\"unit_id\": \"gridtwin:x1:y0\", \"sha256\": \"%s\"}]},\n")
		TEXT("    {\"layer_id\": \"water\", \"normalized_layer_contract_sha256\": \"%s\", \"canonical_inputs\": [")
		TEXT("{\"unit_id\": \"gridtwin:x0:y0\", \"sha256\": \"%s\"},")
		TEXT("{\"unit_id\": \"gridtwin:x1:y0\", \"sha256\": \"%s\"}]}\n")
		TEXT("  ],\n")
		TEXT("  \"operator_additions\": [{\"layer_id\": \"water\", \"units\": [\"gridtwin:x1:y0\"]}]\n")
		TEXT("}\n"),
		*ProjectWorldSchemaTestUtilities::ReferenceFor(
			DirtyInputPath,
			TEXT("project_world_layer_dirty_input.schema.json")),
		*Profile.ProfileId,
		*Profile.Layers[0].ContractHash,
		*FString::ChrN(64, TEXT('e')),
		*BaselineTerrain->CanonicalInputs[1].Hash,
		*Profile.Layers[1].ContractHash,
		*BaselineWater->CanonicalInputs[0].Hash,
		*BaselineWater->CanonicalInputs[1].Hash);
	TestTrue(
		TEXT("The dirty operation fixture is writable."),
		FFileHelper::SaveStringToFile(DirtyInputJson, *DirtyInputPath));
	FProjectWorldLayerDirtyInput LoadedDirtyInput;
	TestTrue(
		TEXT("The commandlet-side dirty-input loader accepts the authenticated operation contract."),
		ProjectWorldLayerDirtyInput::Load(DirtyInputPath, LoadedDirtyInput, ErrorCode, Error));
	TestEqual(TEXT("Dirty operation identity is hashed."), LoadedDirtyInput.InputHash.Len(), 64);
	DirtyInput = MoveTemp(LoadedDirtyInput);

	FProjectWorldRealizationResult Result;
	TestTrue(
		TEXT("Current unit identities are compared with the accepted layer base."),
		ProjectWorldLayerInventory::Build(Bundle, Profile, false, &DirtyInput, Result, Error));
	const FProjectWorldLayerInventory* Terrain = Result.LayerInventories.FindByPredicate([](const auto& Inventory)
	{
		return Inventory.LayerId == TEXT("terrain");
	});
	const FProjectWorldLayerInventory* Water = Result.LayerInventories.FindByPredicate([](const auto& Inventory)
	{
		return Inventory.LayerId == TEXT("water");
	});
	TestNotNull(TEXT("Terrain inventory exists."), Terrain);
	TestNotNull(TEXT("Water inventory exists."), Water);
	if (Terrain != nullptr && Water != nullptr)
	{
		TestEqual(
			TEXT("Layer scope identity is namespaced by realization profile."),
			Terrain->ScopeId,
			FString(TEXT("layer_synthetic_landscape_water_twin_terrain")));
		TestEqual(TEXT("Terrain records exact cell-to-hash identities."), Terrain->CanonicalInputs.Num(), 2);
		TestEqual(TEXT("Only one computed terrain cell is dirty."), Terrain->FinalDirtyUnits.Num(), 1);
		TestTrue(TEXT("The changed terrain cell propagates to water."), Water->FinalDirtyUnits.Contains(TEXT("gridtwin:x0:y0")));
		TestTrue(TEXT("The operator can add but not remove water work."), Water->FinalDirtyUnits.Contains(TEXT("gridtwin:x1:y0")));
	}

	FProjectWorldLayerDirtyInput NoOp;
	NoOp.RealizationProfileId = Profile.ProfileId;
	for (const FProjectWorldLayerInventory& Inventory : Baseline.LayerInventories)
	{
		FProjectWorldLayerBaseIdentity& Base = NoOp.BaseLayers.Add(Inventory.LayerId);
		Base.NormalizedLayerContractHash = Inventory.NormalizedLayerContractHash;
		for (const FProjectWorldLayerInputInventory& Input : Inventory.CanonicalInputs)
		{
			Base.CanonicalInputs.Add(Input.UnitId, Input.Hash);
		}
	}
	Result.LayerInventories.Reset();
	TestTrue(
		TEXT("Matching accepted inputs build a no-op inventory."),
		ProjectWorldLayerInventory::Build(Bundle, Profile, false, &NoOp, Result, Error));
	for (const FProjectWorldLayerInventory& Inventory : Result.LayerInventories)
	{
		TestEqual(TEXT("No-op layers carry no dirty units."), Inventory.FinalDirtyUnits.Num(), 0);
	}

	FProjectWorldCanonicalBundle RoadOnlyChange = Bundle;
	RoadOnlyChange.Cells[0].FeatureArtifactHash = FString::ChrN(64, TEXT('9'));
	Result.LayerInventories.Reset();
	TestTrue(
		TEXT("An unrelated feature-artifact change remains a valid layer comparison."),
		ProjectWorldLayerInventory::Build(RoadOnlyChange, Profile, false, &NoOp, Result, Error));
	const FProjectWorldLayerInventory* UnchangedWater = Result.LayerInventories.FindByPredicate([](const auto& Inventory)
	{
		return Inventory.LayerId == TEXT("water");
	});
	TestNotNull(TEXT("Water inventory exists after an unrelated feature change."), UnchangedWater);
	if (UnchangedWater != nullptr)
	{
		TestEqual(
			TEXT("Road/building changes do not dirty unchanged water in the same cell."),
			UnchangedWater->FinalDirtyUnits.Num(),
			0);
	}

	FProjectWorldCanonicalBundle WaterChange = Bundle;
	WaterChange.Features.FindChecked(TEXT("water/1")).WidthMeters = 14.0;
	Result.LayerInventories.Reset();
	TestTrue(
		TEXT("A canonical water change remains a valid layer comparison."),
		ProjectWorldLayerInventory::Build(WaterChange, Profile, false, &NoOp, Result, Error));
	const FProjectWorldLayerInventory* ChangedWater = Result.LayerInventories.FindByPredicate([](const auto& Inventory)
	{
		return Inventory.LayerId == TEXT("water");
	});
	TestNotNull(TEXT("Water inventory exists after a water change."), ChangedWater);
	if (ChangedWater != nullptr)
	{
		TestEqual(TEXT("Only the water-owned cell is dirty."), ChangedWater->FinalDirtyUnits.Num(), 1);
		TestTrue(
			TEXT("The water-owned cell carries the changed water lineage."),
			ChangedWater->FinalDirtyUnits.Contains(TEXT("gridtwin:x0:y0")));
	}
	IFileManager::Get().Delete(*DirtyInputPath, false, true, true);
	return true;
}

#endif
