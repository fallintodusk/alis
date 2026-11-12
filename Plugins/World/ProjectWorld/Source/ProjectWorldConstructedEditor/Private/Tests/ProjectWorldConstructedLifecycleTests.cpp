// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedVerifyFixture.h"
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedLifecycleTest,
	"Project.World.Realization.Constructed.DependencyLocalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedLifecycleTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRealization;
	FString Error; const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_constructed_surface"),1,Error);
	if (!Producer) {AddError(Error); return false;}
	FProjectWorldVerifyWorld Incremental(TEXT("ConstructedIncremental"));
	if (!Incremental.IsReady(Error)) {AddError(Error); return false;}
	auto Bundle=ProjectWorldConstructedVerifyFixture::Bundle(); auto Layer=ProjectWorldConstructedVerifyFixture::Layer();
	Layer.ArtifactRoot=Incremental.GetRoot(); Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
	FProjectWorldRealizationProfile Profile; Profile.ProfileId=TEXT("constructed_lifecycle"); Profile.Layers={Layer};
	FProjectWorldAuthoredOverlaySet Overlays; TMap<FName,UMaterialInterface*> Materials;
	Materials.Add(TEXT("road"),LoadObject<UMaterialInterface>(nullptr,ProjectWorldConstructedVerifyFixture::Material));
	FProjectWorldLayerInventory Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer);
	auto Apply=[&](FProjectWorldVerifyWorld& World,const TArray<FString>& Dirty,FProjectWorldRealizationResult& Result)
	{
		const FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
		const FProjectWorldLayerApplyContext Context{*World.GetWorld(),Inputs,Dirty,Materials};
		return Producer->Apply(Context,Inventory,Result,Error) && World.SaveOwned(Error) &&
			Producer->CaptureArtifacts(Context,Inventory,Result,Error);
	};
	FProjectWorldRealizationResult Initial;
	if (!TestTrue(TEXT("Cold registered realization produces the fixture."),Apply(Incremental,{TEXT("*")},Initial))) {AddError(Error); return false;}
	if (!TestTrue(TEXT("Saved initial outputs reload."),Incremental.Reload(Error))) {AddError(Error); return false;}
	const auto Before=Incremental.PackageDigests(); FPlan InitialPlan;
	if (!Plan(Bundle,Layer,InitialPlan,Error)) {AddError(Error); return false;}
	TArray<FString> UntouchedMeshes;
	for (const auto& Unit : InitialPlan.Units) if (Unit.FeatureId==TEXT("fixture:fixed"))
		UntouchedMeshes.Add(TEXT("ConstructedIncremental/Cells/SM_ProjectConstructed_")+Unit.Id+TEXT(".uasset"));
	FProjectWorldLayerUnitInputs OriginalHashes;
	{const FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
		if (!Producer->HashUnitInputs(Inputs,OriginalHashes,Error)) {AddError(Error); return false;}}
	auto& Changed=Bundle.Features.FindChecked(TEXT("fixture:gangways")).ConstructedSurface.GetValue();
	for (auto& Face : Changed.Triangles) for (auto& Point : Face.XYZ) Point.Z+=.05;
	Changed.ModelHash=FString::ChrN(64,'d');
	FProjectWorldLayerUnitInputs ChangedHashes;
	{const FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
		if (!Producer->HashUnitInputs(Inputs,ChangedHashes,Error)) {AddError(Error); return false;}}
	TestNotEqual(TEXT("Changed field invalidates its cell."),OriginalHashes.HashByUnit.FindChecked(TEXT("cell_0")),ChangedHashes.HashByUnit.FindChecked(TEXT("cell_0")));
	TestEqual(TEXT("Unconsumed neighboring cell identity remains exact."),OriginalHashes.HashByUnit.FindChecked(TEXT("cell_1")),ChangedHashes.HashByUnit.FindChecked(TEXT("cell_1")));
	FProjectWorldRealizationResult Local;
	if (!TestTrue(TEXT("Only the affected dependency cell is applied."),Apply(Incremental,{TEXT("cell_0")},Local))) {AddError(Error); return false;}
	TestEqual(TEXT("Exactly one changed mesh is written."),Inventory.Metrics.FindRef(TEXT("meshes_written")),int64(1));
	TestEqual(TEXT("The other feature inside that dirty cell is reused."),Inventory.Metrics.FindRef(TEXT("meshes_reused")),int64(1));
	TestEqual(TEXT("Only the changed unit actor is updated."),Local.UpdatedActorCount,1);
	const auto After=Incremental.PackageDigests();
	for (const auto& Path : UntouchedMeshes)
	{
		if (!TestTrue(*FString::Printf(TEXT("Unchanged mesh is present: %s"),*Path),Before.Contains(Path) && After.Contains(Path))) return false;
		TestEqual(TEXT("Unchanged mesh package bytes remain identical."),Before.FindChecked(Path),After.FindChecked(Path));
	}
	int32 ChangedPackages=0;
	for (const auto& Pair : Before) if (After.FindRef(Pair.Key)!=Pair.Value) ++ChangedPackages;
	TestEqual(TEXT("Only the changed mesh and its external actor package differ."),ChangedPackages,2);
	FProjectWorldRealizationResult NoOp;
	if (!TestTrue(TEXT("No-dirty apply authenticates and leaves all saved bytes unchanged."),Apply(Incremental,{},NoOp))) {AddError(Error); return false;}
	TestTrue(TEXT("No-op package census is exact."),After.OrderIndependentCompareEqual(Incremental.PackageDigests()));
	int32 Contract=0; FProjectWorldProjectFn Project;
	if (!ProjectWorldVerify::FindProjection(TEXT("project_constructed_surface"),1,Contract,Project)) return false;
	FProjectWorldProjection IncrementalOutput;
	if (!Incremental.Reload(Error) || !Project({Incremental.GetWorld(),Incremental.GetRoot()},IncrementalOutput,Error)) {AddError(Error); return false;}
	const FString Expected=IncrementalOutput.ToCanonicalJson();
	{
		FProjectWorldVerifyWorld Cold(TEXT("ConstructedCold"));
		if (!Cold.IsReady(Error)) {AddError(Error); return false;}
		Layer.ArtifactRoot=Cold.GetRoot(); Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)); Profile.Layers={Layer};
		Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer); FProjectWorldRealizationResult Fresh; FProjectWorldProjection ColdOutput;
		if (!Apply(Cold,{TEXT("*")},Fresh) || !Cold.Reload(Error) || !Project({Cold.GetWorld(),Cold.GetRoot()},ColdOutput,Error)) {AddError(Error); return false;}
		TestEqual(TEXT("Cold and incremental saved outputs have identical normalized geometry, ownership and physical state."),ColdOutput.ToCanonicalJson(),Expected);
	}
	if (!Incremental.Reload(Error)) {AddError(Error); return false;}
	Layer.ArtifactRoot=Incremental.GetRoot(); Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)); Profile.Layers={Layer};
	Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer);
	const auto BeforeRefusal=Incremental.PackageDigests();
	Bundle.Features.Remove(TEXT("fixture:gangways")); Bundle.Cells[0].OwnedFeatureIds.Remove(TEXT("fixture:gangways"));
	FProjectWorldRealizationResult Undeclared;
	TestFalse(TEXT("A removal without its dependency cell declared dirty refuses."),Apply(Incremental,{},Undeclared));
	TestTrue(TEXT("Undeclared-removal refusal preserves every saved package."),BeforeRefusal.OrderIndependentCompareEqual(Incremental.PackageDigests()));
	FProjectWorldRealizationResult Removed;
	if (!TestTrue(TEXT("A declared removal retires only the removed physical unit."),Apply(Incremental,{TEXT("cell_0")},Removed))) {AddError(Error); return false;}
	TestEqual(TEXT("Exactly one removed feature actor is retired."),Removed.RemovedActorCount,1);
	TestEqual(TEXT("Only the two fixed-feature meshes and actors remain in the artifact census."),Inventory.Artifacts.Num(),4);
	if (!Incremental.Reload(Error)) {AddError(Error); return false;}
	FProjectWorldRealizationResult AfterRemoval;
	if (!TestTrue(TEXT("Reloaded removal supports clean no-op."),Apply(Incremental,{},AfterRemoval))) {AddError(Error); return false;}
	const auto BeforeReceipt=Incremental.PackageDigests();
	Bundle.Features.FindChecked(TEXT("fixture:fixed")).ConstructedSurface->AdmissionHash=FString::ChrN(64,'e');
	const int64 BeforeValidation=NativeBuildCallsForTests(); FProjectWorldRealizationResult ReceiptOnly;
	if (!TestTrue(TEXT("Admission-only metadata retains a valid saved no-op."),Apply(Incremental,{},ReceiptOnly))) {AddError(Error); return false;}
	TestTrue(TEXT("Admission evidence changes do not rewrite saved output."),BeforeReceipt.OrderIndependentCompareEqual(Incremental.PackageDigests()));
	TestEqual(TEXT("Receipt-only changes perform no static mesh writes."),Inventory.Metrics.FindRef(TEXT("meshes_written")),int64(0));
	AddInfo(FString::Printf(TEXT("NoDirtyStructuralValidation native_expected_mesh_build_calls=%lld saved_mesh_writes=0; Apply and Capture retain independent output validation"),
		NativeBuildCallsForTests()-BeforeValidation));
	const auto BeforeModel=Incremental.PackageDigests();
	Bundle.Features.FindChecked(TEXT("fixture:fixed")).ConstructedSurface->ModelHash=FString::ChrN(64,'f');
	FProjectWorldRealizationResult ModelOnly;
	if (!TestTrue(TEXT("A new bound model identity updates its attribution without rebuilding identical geometry."),Apply(Incremental,{TEXT("*")},ModelOnly))) {AddError(Error); return false;}
	TestEqual(TEXT("Model identity alone writes no static meshes."),Inventory.Metrics.FindRef(TEXT("meshes_written")),int64(0));
	TestEqual(TEXT("Both identical source meshes are retained."),Inventory.Metrics.FindRef(TEXT("meshes_reused")),int64(2));
	TestEqual(TEXT("Only model-bound actor metadata is updated in its two cells."),ModelOnly.UpdatedActorCount,2);
	const auto AfterModel=Incremental.PackageDigests(); int32 RetainedMeshes=0;
	for (const auto& Pair : BeforeModel) if (Pair.Key.Contains(TEXT("/Cells/SM_ProjectConstructed_")))
	{ ++RetainedMeshes; TestEqual(TEXT("A model-only metadata update preserves each mesh package byte for byte."),AfterModel.FindRef(Pair.Key),Pair.Value); }
	TestEqual(TEXT("The retained mesh-byte check is nonvacuous."),RetainedMeshes,2);
	AddInfo(TEXT("Measured one mesh write, one same-cell mesh reuse, zero neighbor writes; cold/incremental normalized equivalence and selective retirement."));
	return !HasAnyErrors();
}
#endif
