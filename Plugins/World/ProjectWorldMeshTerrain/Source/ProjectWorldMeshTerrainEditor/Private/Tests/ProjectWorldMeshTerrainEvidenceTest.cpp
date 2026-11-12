#include "ProjectWorldMeshTerrainAuditCommandlet.h"
#include "ProjectWorldMeshTerrainReceiptCommandlet.h"
#include "ProjectWorldRealizeCommandlet.h"
#include "ProjectWorldRealizationEvidence.h"
#include "ProjectWorldRealizationService.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMeshTerrainEvidenceDestinationsTest,
    "Project.World.MeshTerrain.EvidenceDestinations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldMeshTerrainEvidenceDestinationsTest::RunTest(const FString&)
{
    const FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Root=Project/TEXT("tmp/world/evidence_destination_tests")/Token;
    const FString Canonical=Root/TEXT("canonical/compile_result.json"),Child=Root/TEXT("canonical/support.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Canonical),true);
    FFileHelper::SaveStringToFile(TEXT("protected canonical bytes"),*Canonical);
    FFileHelper::SaveStringToFile(TEXT("protected support bytes"),*Child);
    TestTrue(TEXT("Ordinary scratch receipts remain allowed."),UProjectWorldRealizeCommandlet::IsSafeResultPath(Root/TEXT("receipt.json")));
    TestTrue(TEXT("Existing Saved receipts remain allowed."),UProjectWorldRealizeCommandlet::IsSafeResultPath(Project/TEXT("Saved/Validation/WorldRealization/receipt.json")));
    TestFalse(TEXT("A sibling prefix cannot authorize a receipt."),UProjectWorldRealizeCommandlet::IsSafeResultPath(Project/TEXT("tmp/world_elsewhere/result.json")));
    const FString Input=Project/TEXT("tmp/world/inputs/evidence_destination_tests")/Token/TEXT("selection.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Input),true);
    FFileHelper::SaveStringToFile(TEXT("protected selection bytes"),*Input);
    FProjectWorldRealizationRequest Request;Request.CompileResultPath=Canonical;Request.ResultPath=Input;
    FProjectWorldRealizationResult Result;
    TestFalse(TEXT("Direct result writer cannot overwrite a consumed scratch selection."),FProjectWorldRealizationService::WriteResult(Request,Result));
    FString After;FFileHelper::LoadFileToString(After,*Input);TestEqual(TEXT("Direct refusal preserves input bytes."),After,FString(TEXT("protected selection bytes")));
    for (const auto& Suffix : {TEXT(""),TEXT(".tmp"),TEXT(".log")})
    {
        Request.ResultPath=Input+Suffix;
        TestFalse(TEXT("Scratch input sidecars cannot be evidence."),UProjectWorldRealizeCommandlet::IsSafeResultPath(Request.ResultPath));
        TestFalse(TEXT("Direct evidence isolation rejects sidecars."),ProjectWorldRealizationEvidence::IsInputDisjoint(Request));
    }
    TestFalse(TEXT("Normalized traversal cannot enter the reserved namespace."),UProjectWorldRealizeCommandlet::IsSafeResultPath(Root/TEXT("../../inputs/selection.json")));
    FString CaseVariant=Input;CaseVariant.ReplaceInline(TEXT("/inputs/"),TEXT("/INPUTS/"));
    TestFalse(TEXT("Case variants cannot enter the reserved namespace."),UProjectWorldRealizeCommandlet::IsSafeResultPath(CaseVariant));
    AddExpectedError(TEXT("Unsafe result path"),EAutomationExpectedErrorFlags::Contains,4);
    auto* Audit=NewObject<UProjectWorldMeshTerrainAuditCommandlet>();auto* Receipt=NewObject<UProjectWorldMeshTerrainReceiptCommandlet>();
    for (const auto& Destination : {Canonical,Child})
    {
        FString Before;FFileHelper::LoadFileToString(Before,*Destination);
        const FString Args=FString::Printf(TEXT("-CompileResult=\"%s\" -Result=\"%s\""),*Canonical,*Destination);
        TestEqual(TEXT("Audit refuses input overlap before deleting output."),Audit->Main(Args+TEXT(" -Map=/ProjectWorldTestData/Generated/EvidenceTest/L_Test -ExpectedSectionsPerVariant=0")),2);
        TestEqual(TEXT("Layout receipt refuses input overlap before deleting output."),Receipt->Main(Args),2);
        FFileHelper::LoadFileToString(After,*Destination);TestEqual(TEXT("Commandlet refusal preserves protected input bytes."),After,Before);
    }
    return true;
}
#endif
