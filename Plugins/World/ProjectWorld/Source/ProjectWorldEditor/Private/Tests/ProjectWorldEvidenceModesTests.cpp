// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldEvidenceCapture.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldEvidenceModesTest,
    "Project.World.VisualEvidence.RenderModeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldEvidenceModesTest::RunTest(const FString&)
{
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("tmp/world/visual_verification/tests"), FGuid::NewGuid().ToString()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Directory, false, true); };
    const FString Path = FPaths::Combine(Directory, TEXT("plan.json"));
    auto Read = [&](const FString& Mode, TArray<FProjectWorldCaptureVantage>& Views)
    {
        const FString Json = FString::Printf(TEXT("{\"capture_width\":32,\"capture_height\":20,"
            "\"fov_degrees\":75,\"vantages\":[{\"name\":\"rim\",\"x\":1,\"y\":2,\"z\":3,"
            "\"pitch\":-65,\"yaw\":90%s}]}"), *Mode);
        if (!FFileHelper::SaveStringToFile(Json, *Path)) return false;
        int32 Width = 0, Height = 0;
        FString Hash, Error;
        return ProjectWorldEvidenceCapture::LoadVantagePlan(Path, Width, Height, Views, Hash, Error);
    };
    TArray<FProjectWorldCaptureVantage> Default;
    TestTrue(TEXT("Existing normal plans remain normal."), Read(TEXT(""), Default));
    if (!Default.IsEmpty()) TestEqual(TEXT("Normal mode is explicit internally."), Default[0].RenderMode, FString(TEXT("lit")));
    for (const TCHAR* Mode : {TEXT("lit"), TEXT("base_color"), TEXT("lighting_only"), TEXT("world_normal")})
    {
        TArray<FProjectWorldCaptureVantage> Views;
        TestTrue(TEXT("Native diagnostic mode parses."), Read(FString::Printf(TEXT(",\"render_mode\":\"%s\""), Mode), Views));
        if (!Views.IsEmpty())
        {
            TestEqual(TEXT("The requested mode is retained."), Views[0].RenderMode, FString(Mode));
            TestEqual(TEXT("Mode never adjusts the pose."), Views[0].Location, FVector(1, 2, 3));
        }
    }
    for (const TCHAR* Mode : {TEXT(",\"render_mode\":\"brightened\""), TEXT(",\"render_mode\":42"), TEXT(",\"render_mode\":null")})
    {
        TArray<FProjectWorldCaptureVantage> Views;
        TestFalse(TEXT("Unknown or malformed modes cannot silently capture a normal view."), Read(Mode, Views));
    }
    return true;
}
#endif
