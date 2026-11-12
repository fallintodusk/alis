#include "ProjectWorldBuildingSelection.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldVerify.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldBuildingSelectionInputsTest,
    "Project.World.BuildingAssembly.SelectionInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
void FProjectWorldBuildingSelectionInputsTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_SELECTION_CASE")).IsEmpty()) {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}}
bool FProjectWorldBuildingSelectionInputsTest::RunTest(const FString&)
{
    const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_SELECTION_CASE"));
    if (!TestTrue(TEXT("Input contract fixture is scratch-local."),FPaths::IsUnderDirectory(Root,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world"))))) return false;
    FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation Validation;
    if (!TestTrue(TEXT("Normal reader authenticates the small compiled fixture."),FProjectWorldCanonicalLoader::Load(Root/TEXT("canonical/compile_result.json"),Bundle,Validation))) {AddError(Validation.Message);return false;}
    FProjectWorldRealizationProfile Profile;FString Error,Code;
    if (!TestTrue(TEXT("Normal profile loader admits the confined test profile."),ProjectWorldRealizationProfile::Load(Root/TEXT("realization.json"),Profile,Code,Error,Root))) {AddError(Error);return false;}
    const auto* Layer=Profile.Layers.FindByPredicate([](const auto& L){return L.LayerId==TEXT("buildings");});
    if (!TestNotNull(TEXT("The ordinary buildings layer is present."),Layer)) return false;
    FProjectWorldAuthoredOverlaySet Overlays;
    const FProjectWorldLayerInputs Inputs{Bundle,Profile,*Layer,Overlays};
    const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Layer->GeneratorId,Layer->GeneratorVersion,Error);
    ProjectWorldBuildingSelection::FSet Set;
    if (!TestTrue(TEXT("One source-shaped selection loads."),ProjectWorldBuildingSelection::Load(Inputs,Set,Error) && Set.Buildings.Num()==1)) {AddError(Error);return false;}
    FProjectWorldLayerUnitInputs Before,After;
    if (!TestTrue(TEXT("Every canonical cell has a consumed-input identity."),Producer->HashUnitInputs(Inputs,Before,Error) && Before.HashByUnit.Num()==2)) {AddError(Error);return false;}
    const auto Selection=Set.Buildings[0];auto& Feature=Bundle.Features.FindChecked(Selection.FeatureId);
    const auto Original=Feature;Feature.HeightMeters+=1;Feature.BuildingVolumes[0].HeightMeters+=1;
    if (!TestTrue(TEXT("A selected building-height change is consumed."),Producer->HashUnitInputs(Inputs,After,Error))) return false;
    for (const auto& Pair : Before.HashByUnit)
        TestEqual(TEXT("Building-height change dirties exactly the owning cell."),After.HashByUnit.FindChecked(Pair.Key)!=Pair.Value,Pair.Key==Selection.CellId);
    Feature=Original;After={};
    auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Selection.CellId;});
    const auto Heights=Cell->Terrain.HeightsMeters;
    for (auto& Z : Cell->Terrain.HeightsMeters) Z+=.1;
    if (!TestTrue(TEXT("Physical support changes affect identity beyond a retained artifact tag."),Producer->HashUnitInputs(Inputs,After,Error))) return false;
    for (const auto& Pair : Before.HashByUnit)
        TestEqual(TEXT("Changed physical support dirties exactly the selected owner."),After.HashByUnit.FindChecked(Pair.Key)!=Pair.Value,Pair.Key==Selection.CellId);
    Cell->Terrain.HeightsMeters=Heights;
    const FString ExtraIds[]={TEXT("alis:building_fixture:way:2002"),TEXT("alis:building_fixture:way:2003")};
    for (const auto& Id : ExtraIds) {auto S=Selection;S.FeatureId=Id;S.CellId=Bundle.Features.FindChecked(Id).OwnerCellId;Set.Buildings.Add(S);}
    for (const auto& C : Bundle.Cells)
    {
        FProjectWorldBuildingMeshBuildResult OriginalMassing,Remaining;
        if (!ProjectWorldBuildingRealization::BuildCellOutput(Bundle,C,*Layer,Overlays,OriginalMassing,Error) ||
            !ProjectWorldBuildingSelection::BuildCell(Bundle,C,Overlays,{},Set,Remaining,Error)) {AddError(Error);return false;}
        int Expected=0;
        for (const auto& S : Set.Buildings) if (S.CellId==C.CellId) ++Expected;
        TestEqual(TEXT("Batch excludes the whole admitted union once."),Remaining.AssemblyExcludedFeatures.Num(),Expected);
        TestEqual(TEXT("Unselected emission census remains complete."),Remaining.Emissions.Num()+Expected,OriginalMassing.Emissions.Num());
        for (const auto& E : Remaining.Emissions)
            TestFalse(TEXT("No selected blockout is accidentally restored by another preparation."),Set.Buildings.ContainsByPredicate([&](const auto& S){return S.FeatureId==E.FeatureId;}));
    }
    {
        FProjectWorldVerifyWorld Verify(TEXT("SelectionInputRace"));
        if (!TestTrue(TEXT("An empty native world isolates the selection race."),Verify.IsReady(Error))) return false;
        const auto Packages=Verify.PackageDigests();FProjectWorldLayerInventory Claimed;
        for (const auto& Pair : Before.HashByUnit) Claimed.CanonicalInputs.Add({Pair.Key,Pair.Value});
        const TArray<FString> Clean;TMap<FName,UMaterialInterface*> Materials;
        Materials.Add(TEXT("building"),UMaterial::GetDefaultMaterial(MD_Surface));
        const FProjectWorldLayerApplyContext Context{*Verify.GetWorld(),Inputs,Clean,Materials};
        auto Removed=Set;Removed.Buildings.Reset();FProjectWorldRealizationResult Result;Error.Reset();
        TestFalse(TEXT("Unqualified ISM material-only adoption refuses before mutation."),Producer->UpdateMaterialReferences(Context,{},Claimed,Result,true,Error));
        TestTrue(TEXT("The refusal identifies the unsupported material-only route."),Error.Contains(TEXT("Material-only adoption")));
        Error.Reset();
        TestFalse(TEXT("Selection removed after planning refuses before initial world mutation."),ProjectWorldBuildingSelection::Apply(Context,Removed,Claimed,Result,Error));
        TestTrue(TEXT("Refusal specifically reports the stale planned inputs."),Error.Contains(TEXT("inputs changed after dirty planning")));
        Error.Reset();
        TestFalse(TEXT("Empty configured capture cannot authenticate stale selected inputs."),ProjectWorldBuildingSelection::Capture(Context,Removed,Claimed,Result,Error));
        TestTrue(TEXT("Capture reports the same input identity mismatch."),Error.Contains(TEXT("inputs changed after dirty planning")));
        const auto AfterPackages=Verify.PackageDigests();TestEqual(TEXT("Refusal creates no package."),AfterPackages.Num(),Packages.Num());
        for (const auto& Pair : Packages) TestEqual(TEXT("Refusal leaves every native package byte unchanged."),AfterPackages.FindRef(Pair.Key),Pair.Value);
        TestEqual(TEXT("Refusal creates no actor."),Result.CreatedActorCount,0);
    }
    FProjectWorldAuthoredOverlay Protection;Protection.Anchor.Kind=EProjectWorldAnchorKind::Feature;
    Protection.Anchor.FeatureId=Selection.FeatureId;Overlays.Overlays.Add(Protection);After={};
    TestFalse(TEXT("Protected selection refuses before generated output mutation."),Producer->HashUnitInputs(Inputs,After,Error));
    return true;
}
#endif
