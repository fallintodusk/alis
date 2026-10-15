#include "ProjectWorldLayerPipeline.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	class FSelectedProbe final : public IProjectWorldLayerProducer
	{
	public:
		FProjectWorldLayerProducerDeclaration Declaration;
		mutable int32 Calls = 0;
		TArray<FString>* Order = nullptr;
		explicit FSelectedProbe(const TCHAR* Id)
		{
			Declaration.GeneratorId = Id;
			Declaration.GeneratorVersion = 1;
			Declaration.CanonicalSelectors = {TEXT("terrain")};
			Declaration.SpatialOwnership = TEXT("cell_local");
			Declaration.RuntimeMapping = TEXT("world_partition_spatial");
			Declaration.OwnedActors.OwnedTags = {FName(Id)};
			IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
		}
		~FSelectedProbe()
		{
			IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
		}
		const FProjectWorldLayerProducerDeclaration& GetDeclaration() const override { return Declaration; }
		bool ValidateSettings(const FString&, FString&) const override { return true; }
		bool HashUnitInputs(const FProjectWorldLayerInputs&, FProjectWorldLayerUnitInputs&, FString&) const override { return true; }
		bool CaptureArtifacts(const FProjectWorldLayerApplyContext&, FProjectWorldLayerInventory&,
			FProjectWorldRealizationResult&, FString&) const override { return true; }
		bool Apply(const FProjectWorldLayerApplyContext&, FProjectWorldLayerInventory&,
			FProjectWorldRealizationResult&, FString&) const override
			{ ++Calls; if (Order != nullptr) Order->Add(Declaration.GeneratorId); return true; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldLayerExecutionTest,
	"Project.World.Realization.Layers.ExecutionSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLayerExecutionTest::RunTest(const FString&)
{
	FSelectedProbe Road(TEXT("project_selection_road"));
	FSelectedProbe Building(TEXT("project_selection_building"));
	FProjectWorldRealizationProfile Profile;
	for (const FString Id : {FString(TEXT("terrain")), FString(TEXT("roads")), FString(TEXT("buildings"))})
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = Id;
		Layer.GeneratorId = Id == TEXT("buildings") ? Building.Declaration.GeneratorId : Road.Declaration.GeneratorId;
		Layer.GeneratorVersion = 1;
		if (Id != TEXT("terrain")) Layer.DependsOn = {TEXT("terrain")};
		Profile.Layers.Add(Layer);
		Profile.TopologicalLayerIds.Add(Id);
	}
	FProjectWorldRealizationResult Result;
	for (const auto& Layer : Profile.Layers)
	{
		FProjectWorldLayerInventory Inventory;
		Inventory.LayerId = Layer.LayerId;
		if (Layer.LayerId != TEXT("terrain")) Inventory.FinalDirtyUnits = {TEXT("cell")};
		Result.LayerInventories.Add(Inventory);
	}
	Result.ExecutionLayerIds = {TEXT("buildings")};
	FString Error;
	TestTrue(TEXT("Dirty roads do not prevent an independent building leaf."),
		ProjectWorldLayerPipeline::ValidateExecutionSelection(Profile, Result.ExecutionLayerIds, Result.LayerInventories, Error));
	UWorld* World = GEditor->NewMap(false);
	if (!TestNotNull(TEXT("Disposable editor world."), World)) return false;
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldAuthoredOverlaySet Authored;
	const TMap<FName, UMaterialInterface*> Materials;
	TestTrue(TEXT("Selected producer executes."), ProjectWorldLayerPipeline::ApplyLayers(
		*World, Bundle, Profile, Authored, Materials, Result, Error));
	TestEqual(TEXT("Building executed once."), Building.Calls, 1);
	TestEqual(TEXT("Pending road producer did not execute."), Road.Calls, 0);
	TestEqual(TEXT("Pending road plan is retained."), Result.LayerInventories[1].FinalDirtyUnits.Num(), 1);
	TestFalse(TEXT("Unknown owner refuses."), ProjectWorldLayerPipeline::ValidateExecutionSelection(
		Profile, {TEXT("missing")}, Result.LayerInventories, Error));
	TestFalse(TEXT("Duplicate owner refuses."), ProjectWorldLayerPipeline::ValidateExecutionSelection(
		Profile, {TEXT("buildings"), TEXT("buildings")}, Result.LayerInventories, Error));
	TestFalse(TEXT("Upstream producer selection refuses without consumer closure."), ProjectWorldLayerPipeline::ValidateExecutionSelection(
		Profile, {TEXT("terrain")}, Result.LayerInventories, Error));
	Result.LayerInventories[0].FinalDirtyUnits = {TEXT("dirty_terrain")};
	TestFalse(TEXT("Dirty dependency refuses before writes."), ProjectWorldLayerPipeline::ApplyLayers(
		*World, Bundle, Profile, Authored, Materials, Result, Error));
	TestEqual(TEXT("Dependency refusal left calls unchanged."), Building.Calls, 1);
	TestEqual(TEXT("Dependency refusal did not execute roads."), Road.Calls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldLayerExecutionClosureTest,
	"Project.World.Realization.Layers.ExecutionClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLayerExecutionClosureTest::RunTest(const FString&)
{
	FSelectedProbe Road(TEXT("project_selection_road"));
	FSelectedProbe Water(TEXT("project_selection_water"));
	FSelectedProbe Vegetation(TEXT("project_selection_vegetation"));
	TArray<FString> Order;
	Road.Order = Water.Order = Vegetation.Order = &Order;
	FProjectWorldRealizationProfile Profile;
	FProjectWorldRealizationResult Result;
	for (const FString Id : {FString(TEXT("terrain")), FString(TEXT("roads")), FString(TEXT("water")),
		FString(TEXT("vegetation")), FString(TEXT("buildings"))})
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = Id;
		Layer.GeneratorId = Id == TEXT("roads") ? Road.Declaration.GeneratorId :
			Id == TEXT("water") ? Water.Declaration.GeneratorId : Vegetation.Declaration.GeneratorId;
		Layer.GeneratorVersion = 1;
		if (Id == TEXT("vegetation")) Layer.DependsOn = {TEXT("roads"), TEXT("water")};
		else if (Id != TEXT("terrain")) Layer.DependsOn = {TEXT("terrain")};
		Profile.Layers.Add(Layer);
		Profile.TopologicalLayerIds.Add(Id);
		FProjectWorldLayerInventory Inventory;
		Inventory.LayerId = Id;
		if (Id != TEXT("terrain")) Inventory.FinalDirtyUnits = {TEXT("cell")};
		Result.LayerInventories.Add(Inventory);
	}
	Result.ExecutionLayerIds = {TEXT("vegetation"), TEXT("water"), TEXT("roads")};
	FString Error;
	TestTrue(TEXT("Selected dirty dependencies and their complete consumer closure are accepted."),
		ProjectWorldLayerPipeline::ValidateExecutionSelection(Profile, Result.ExecutionLayerIds, Result.LayerInventories, Error));
	TestFalse(TEXT("An omitted direct consumer refuses."), ProjectWorldLayerPipeline::ValidateExecutionSelection(
		Profile, {TEXT("roads")}, Result.LayerInventories, Error));
	UWorld* World = GEditor->NewMap(false);
	if (!TestNotNull(TEXT("Disposable closure fixture."), World)) return false;
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldAuthoredOverlaySet Authored;
	const TMap<FName, UMaterialInterface*> Materials;
	TestTrue(TEXT("Only the selected closure executes."), ProjectWorldLayerPipeline::ApplyLayers(
		*World, Bundle, Profile, Authored, Materials, Result, Error));
	TestEqual(TEXT("Road executes once."), Road.Calls, 1);
	TestEqual(TEXT("Water executes once."), Water.Calls, 1);
	TestEqual(TEXT("Vegetation executes once, not once more for the carried Building."), Vegetation.Calls, 1);
	TestTrue(TEXT("Execution follows the profile topology rather than selection order."),
		Order == TArray<FString>({Road.Declaration.GeneratorId, Water.Declaration.GeneratorId, Vegetation.Declaration.GeneratorId}));
	TestEqual(TEXT("Carried Building dirty work remains pending."), Result.LayerInventories[4].FinalDirtyUnits.Num(), 1);
	Result.LayerInventories[0].FinalDirtyUnits = {TEXT("external_dirty")};
	TestFalse(TEXT("Dirty external terrain refuses before another owner call."), ProjectWorldLayerPipeline::ApplyLayers(
		*World, Bundle, Profile, Authored, Materials, Result, Error));
	TestEqual(TEXT("External-dependency refusal leaves owner calls unchanged."), Order.Num(), 3);
	return true;
}
#endif
