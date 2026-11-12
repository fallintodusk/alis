#include "ProjectWorldConstructedVerifyFixture.h"
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "HAL/PlatformTime.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedDependencyPreparationTest,
    "Project.World.Realization.Constructed.DependencyPreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedDependencyPreparationTest::RunTest(const FString&)
{
    using namespace ProjectWorldConstructedRealization;
    AddInfo(TEXT("[Fast][Integration][World]"));
    const auto Bundle=ProjectWorldConstructedVerifyFixture::Bundle();
    const auto Layer=ProjectWorldConstructedVerifyFixture::Layer();
    FProjectWorldRealizationProfile Profile; Profile.Layers={Layer}; FProjectWorldAuthoredOverlaySet Overlays;
    const FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
    FString Error; const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_constructed_surface"),1,Error);
    if (!TestNotNull(TEXT("The actual registered structural owner supplies dependency hashes."),Producer)) return false;
    const int64 Before=NativeBuildCallsForTests(); const double Start=FPlatformTime::Seconds();
    FProjectWorldLayerUnitInputs Units;
    if (!TestTrue(TEXT("The closed two-cell input contract hashes successfully."),Producer->HashUnitInputs(Inputs,Units,Error)))
    {AddError(Error);return false;}
    const double Seconds=FPlatformTime::Seconds()-Start;
    const int64 Calls=NativeBuildCallsForTests()-Before;
    AddInfo(FString::Printf(TEXT("StructuralDependencyPreparation native_mesh_build_calls=%lld cells=%d elapsed_seconds=%.9f"),Calls,Units.HashByUnit.Num(),Seconds));
    TestEqual(TEXT("Dependency preparation does not construct native mesh descriptions."),Calls,int64(0));
    TestEqual(TEXT("Both declared canonical cells retain dependency identity."),Units.HashByUnit.Num(),2);
    FDependencyInputs Prepared;
    if (!TestTrue(TEXT("The shared source-only plan supplies the same public dependency census."),
        DependencyInputs(Bundle,Layer,Prepared,Error))) {AddError(Error);return false;}
    TestTrue(TEXT("Public hashing and the semantic plan cannot disagree."),Units.HashByUnit.OrderIndependentCompareEqual(Prepared.CellHashes));
    AddInfo(FString::Printf(TEXT("StructuralDependencySource validated_faces=%lld semantic_identity_bytes=%lld; includes source-field validation, not zero geometry work"),
        Prepared.ValidatedSourceFaces,Prepared.SemanticIdentityBytes));
    FPlan Native;
    if (!TestTrue(TEXT("Full realization keeps independent native output construction and validation."),Plan(Bundle,Layer,Native,Error))) {AddError(Error);return false;}
    TestTrue(TEXT("Full realization uses the same semantic dependency identities."),Units.HashByUnit.OrderIndependentCompareEqual(Native.CellHashes));
    TestEqual(TEXT("The full native plan still constructs all three expected physical units."),Native.Units.Num(),3);
    const int64 AfterNativePlan=NativeBuildCallsForTests();
    auto Hash=[&](const FProjectWorldCanonicalBundle& Value,const FProjectWorldRealizationLayer& Definition,FProjectWorldLayerUnitInputs& Out)
    {
        const FProjectWorldLayerInputs Context{Value,Profile,Definition,Overlays};
        return Producer->HashUnitInputs(Context,Out,Error);
    };
    FProjectWorldLayerUnitInputs Other;
    TestTrue(TEXT("Repeated semantic preparation is deterministic."),Hash(Bundle,Layer,Other) && Other.HashByUnit.OrderIndependentCompareEqual(Units.HashByUnit));
    for (const FString Kind : {TEXT("role"),TEXT("policy"),TEXT("join")})
    {
        auto Value=Bundle; auto Definition=Layer;
        auto& Source=Value.Features.FindChecked(TEXT("fixture:fixed")).ConstructedSurface.GetValue();
        if (Kind==TEXT("role")) Source.FaceRegions[0].Role=TEXT("inferred_apron");
        if (Kind==TEXT("policy")) Source.OperationalPolicy->MaximumOrdinaryGradient=.03;
        if (Kind==TEXT("join")) Definition.NormalizedSettings.ReplaceInline(TEXT("0.001"),TEXT("0.0005"));
        TestTrue(FString::Printf(TEXT("A valid consumed %s change invalidates both actual dependency cells."),*Kind),Hash(Value,Definition,Other) &&
            Other.HashByUnit.FindChecked(TEXT("cell_0"))!=Units.HashByUnit.FindChecked(TEXT("cell_0")) &&
            Other.HashByUnit.FindChecked(TEXT("cell_1"))!=Units.HashByUnit.FindChecked(TEXT("cell_1")));
    }
    auto Changed=Bundle;
    for (auto& Face : Changed.Features.FindChecked(TEXT("fixture:gangways")).ConstructedSurface->Triangles)
        for (auto& Point : Face.XYZ) Point.Z+=.05;
    if (!TestTrue(TEXT("A changed source field remains a valid dependency input."),Hash(Changed,Layer,Other))) return false;
    TestNotEqual(TEXT("Changed geometry invalidates its actual dependency cell."),Other.HashByUnit.FindChecked(TEXT("cell_0")),Units.HashByUnit.FindChecked(TEXT("cell_0")));
    TestEqual(TEXT("A cell that does not consume that feature remains unchanged."),Other.HashByUnit.FindChecked(TEXT("cell_1")),Units.HashByUnit.FindChecked(TEXT("cell_1")));
    Changed=Bundle; Changed.EngineGeoreferenceOriginMeters.X+=1;
    TestTrue(TEXT("The consumed engine frame moves dependency identity."),Hash(Changed,Layer,Other) && !Other.HashByUnit.OrderIndependentCompareEqual(Units.HashByUnit));
    Changed=Bundle; Changed.Cells[1].Bounds.Z+=1;
    TestTrue(TEXT("The actual clipping domain participates in dependency identity."),Hash(Changed,Layer,Other) &&
        Other.HashByUnit.FindChecked(TEXT("cell_0"))==Units.HashByUnit.FindChecked(TEXT("cell_0")) &&
        Other.HashByUnit.FindChecked(TEXT("cell_1"))!=Units.HashByUnit.FindChecked(TEXT("cell_1")));
    Changed=Bundle; Changed.Features.FindChecked(TEXT("fixture:fixed")).ConstructedSurface->AdmissionHash=FString::ChrN(64,'e');
    TestTrue(TEXT("Receipt-only admission metadata does not regenerate geometry."),Hash(Changed,Layer,Other) && Other.HashByUnit.OrderIndependentCompareEqual(Units.HashByUnit));
    FProjectWorldVerifyIdentity BeforeEvidence,AfterEvidence;
    BeforeEvidence.AddFixtureBundle(TEXT("bundle"),Bundle); AfterEvidence.AddFixtureBundle(TEXT("bundle"),Changed);
    TestNotEqual(TEXT("That metadata still invalidates the acceptance evidence."),BeforeEvidence.FixtureInputs[0].Value,AfterEvidence.FixtureInputs[0].Value);
    Changed=Bundle; Changed.Cells[0].Terrain.HeightsMeters={9,9,9,9};
    TestTrue(TEXT("Unconsumed Terrain cannot dirty structural output."),Hash(Changed,Layer,Other) && Other.HashByUnit.OrderIndependentCompareEqual(Units.HashByUnit));
    Changed=Bundle; Changed.Cells[1].ReferencedFeatureIds.Reset();
    TestFalse(TEXT("Missing membership refuses without constructing a replacement mesh."),Hash(Changed,Layer,Other));
    TestTrue(TEXT("Membership refusal exposes no partial hash census."),Other.HashByUnit.IsEmpty());
    Changed=Bundle; Changed.Features.FindChecked(TEXT("fixture:fixed")).ConstructedSurface->FaceRegions[0].Role=TEXT("unknown");
    TestFalse(TEXT("Source-only policy validation survives the hash split."),Hash(Changed,Layer,Other));
    TestTrue(TEXT("Invalid policy exposes no partial hash census."),Other.HashByUnit.IsEmpty());
    Changed=Bundle; Changed.EngineGeoreferenceOriginMeters.X=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Unknown coordinate frames cannot be hashed as successful input."),Hash(Changed,Layer,Other));
    TestTrue(TEXT("Unknown frame refusal clears prior output."),Other.HashByUnit.IsEmpty());
    TestEqual(TEXT("All changed, provenance-only and refused hash paths avoid native mesh construction."),NativeBuildCallsForTests(),AfterNativePlan);
    return !HasAnyErrors();
}
#endif
