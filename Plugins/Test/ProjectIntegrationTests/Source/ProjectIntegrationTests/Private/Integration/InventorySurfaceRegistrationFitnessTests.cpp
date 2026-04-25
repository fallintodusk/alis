// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// Slice 18 (+ Follow-up #2) - NoSurfaceRegistrationUsesWidgetClosures
//
// Invariant: widget code may not capture ANY state inside a lambda handed to
// the drag host's surface registration path. Slice 18 introduced
// IInventorySurfacePolicyProvider for OccupantAllowedChecker; Follow-up #2
// extended it to EnabledChecker and OccupantChecker. After Follow-up #2 the
// registration path is entirely closure-free in widgets - the subsystem
// installs subsystem-scoped closures that fan out to the provider.
//
// The previous OccupantAllowedChecker closure captured WeakVM and (pre-Slice
// 13) reached into global Slate state via
// UWidgetBlueprintLibrary::GetDragDroppingContent, which returned null in
// lifecycle edges and caused every empty cell to be rejected (equip-backpack
// regression). The EnabledChecker / OccupantChecker closures captured
// WeakVM plus a pocket index, duplicating the provider's job.
//
// Mechanism: walk Plugins/UI/**/Widgets/*.cpp at test time, find every line
// mentioning "RegisterSurface(", then scan a +/-30-line window around each
// call site for ANY C++ capture-list pattern (match [<anything>](...). The
// regex intentionally matches [X], [X, Y], [WeakVM], [=], [&], [this], etc.
// Array subscripts like "Array[0]" are NOT matched because the pattern
// requires an immediate "(" after the "]" (with optional whitespace).
// Anything matched is a violation; the offending absolute path + 1-based
// line number is reported on failure.
// ---------------------------------------------------------------------------

namespace
{
    constexpr int32 LambdaScanRadius = 30;

    void FindWidgetCppFiles(TArray<FString>& OutFiles)
    {
        OutFiles.Reset();

        const FString UIPluginsDir = FPaths::ProjectPluginsDir() / TEXT("UI");
        if (!IFileManager::Get().DirectoryExists(*UIPluginsDir))
        {
            return;
        }

        TArray<FString> AllCppFiles;
        IFileManager::Get().FindFilesRecursive(AllCppFiles, *UIPluginsDir, TEXT("*.cpp"), true, false);

        // Keep only files whose path contains a /Widgets/ segment; the
        // invariant is specifically about widget code.
        OutFiles.Reserve(AllCppFiles.Num());
        for (const FString& Path : AllCppFiles)
        {
            const FString Normalized = Path.Replace(TEXT("\\"), TEXT("/"));
            if (Normalized.Contains(TEXT("/Widgets/")))
            {
                OutFiles.Add(Path);
            }
        }
    }

    /**
     * Follow-up #2: ANY lambda capture counts. A lambda's capture list is
     * always followed by "(" (optionally after whitespace) for the
     * parameter list. Array subscripts like "Array[0]" are not followed by
     * "(" so the pattern does not false-match them.
     *
     * We scan character-by-character to avoid pulling in a regex dep; the
     * logic is: for every "[" in the line, collect until matching "]",
     * then check if the next non-whitespace char is "(".
     */
    bool LineHasClosureCapture(const FString& Line)
    {
        const int32 Length = Line.Len();
        for (int32 Index = 0; Index < Length; ++Index)
        {
            if (Line[Index] != TEXT('['))
            {
                continue;
            }
            // Find the matching "]". Lambda capture lists cannot contain
            // nested "[" so the first "]" closes the capture.
            int32 Close = INDEX_NONE;
            for (int32 Scan = Index + 1; Scan < Length; ++Scan)
            {
                if (Line[Scan] == TEXT(']'))
                {
                    Close = Scan;
                    break;
                }
                // Nested "[" is not a lambda capture; bail on this "[".
                if (Line[Scan] == TEXT('['))
                {
                    break;
                }
            }
            if (Close == INDEX_NONE)
            {
                continue;
            }
            // Skip whitespace after "]".
            int32 After = Close + 1;
            while (After < Length && FChar::IsWhitespace(Line[After]))
            {
                ++After;
            }
            if (After < Length && Line[After] == TEXT('('))
            {
                return true;
            }
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryNoSurfaceRegistrationUsesWidgetClosuresTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.NoSurfaceRegistrationUsesWidgetClosures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryNoSurfaceRegistrationUsesWidgetClosuresTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TArray<FString> WidgetFiles;
    FindWidgetCppFiles(WidgetFiles);

    if (!TestTrue(TEXT("Widget cpp discovery must find at least one file"), WidgetFiles.Num() > 0))
    {
        AddError(FString::Printf(
            TEXT("Widget discovery returned 0 files under %s. Fitness test cannot run."),
            *(FPaths::ProjectPluginsDir() / TEXT("UI"))));
        return false;
    }

    int32 ViolationCount = 0;
    int32 ScannedCallSites = 0;

    for (const FString& FilePath : WidgetFiles)
    {
        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FilePath))
        {
            AddWarning(FString::Printf(TEXT("Could not read %s; skipped."), *FilePath));
            continue;
        }

        TArray<FString> Lines;
        FileText.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FString& Line = Lines[Index];
            if (!Line.Contains(TEXT("RegisterSurface(")))
            {
                continue;
            }

            // Skip function definitions / declarations of RegisterSurface
            // itself (none currently in widget files, but be defensive).
            if (Line.Contains(TEXT("void RegisterSurface"))
                || Line.Contains(TEXT("::RegisterSurface(")))
            {
                continue;
            }

            ++ScannedCallSites;

            const int32 Start = FMath::Max(0, Index - LambdaScanRadius);
            const int32 End = FMath::Min(Lines.Num() - 1, Index + LambdaScanRadius);
            for (int32 ScanIdx = Start; ScanIdx <= End; ++ScanIdx)
            {
                if (LineHasClosureCapture(Lines[ScanIdx]))
                {
                    ++ViolationCount;
                    AddError(FString::Printf(
                        TEXT("Widget-closure capture found near RegisterSurface call. "
                             "File=%s Line=%d: %s"),
                        *FilePath,
                        ScanIdx + 1,
                        *Lines[ScanIdx].TrimStartAndEnd()));
                }
            }
        }
    }

    // Report a summary so the test is useful in clean runs too.
    AddInfo(FString::Printf(
        TEXT("Scanned %d RegisterSurface call sites across %d widget files; violations=%d."),
        ScannedCallSites, WidgetFiles.Num(), ViolationCount));

    return ViolationCount == 0;
}

#endif // WITH_DEV_AUTOMATION_TESTS
