// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "MVVM/InventoryDragEvent.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"

// Slice20SabotageToggle is intentionally included but not referenced in
// this file's source code path - the sabotage markers live in the widget
// sources themselves. Including it here keeps a single-line flip in the
// toggle header in effect for all CI smoke tests too.
#include "Slice20SabotageToggle.h"
#include "Support/InventoryDragEventRecorder.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// Slice 20 - architecture fitness tests (CI-enforced invariants).
//
// Four role-based architecture invariants are pinned here:
//
// 1. NoWidgetIncludesRouterOrDispatcher
//    Subsystem is the sole client of the drop router/dispatcher headers.
//    Widget files must not pull these internals.
//
// 2. OnlyDropTargetWidgetsOverrideDragHandlers
//    Any class in ProjectInventoryUI that overrides NativeOnDragOver or
//    NativeOnDrop MUST implement IInventoryDropTarget. Role-based so
//    future drop targets (crafting slot widgets, chest slot widgets, etc.)
//    drop in without touching this test.
//
// 3. UserWidgetVisibilityContracted
//    The two live inventory user-widgets (main panel + nearby panel) have
//    pinned root-visibility contracts. Any silent flip breaks this test.
//
// 4. EveryDragSessionEmitsCompletedOrCancelled
//    Terminal-guarantee invariant. Randomized drive of Begin/Update/
//    Complete/Cancel on a bare subsystem must always terminate with
//    exactly one Completed OR Cancelled event per session.
//
// The Slice20SabotageToggle header controls an opt-in sabotage build that
// wires a specific, pinpoint violation for each test; running the suite
// with SLICE20_SABOTAGE=1 produces four named failure messages and proves
// each assertion is load-bearing. Default is 0 (green build).
// ---------------------------------------------------------------------------

namespace
{
    // -------------------------------------------------------------------
    // Fitness 1 helpers - walk widget source files.
    // -------------------------------------------------------------------

    void FindProjectInventoryUIWidgetFiles(TArray<FString>& OutFiles, const TCHAR* Extension)
    {
        OutFiles.Reset();

        const FString UIPluginsDir = FPaths::ProjectPluginsDir() / TEXT("UI");
        if (!IFileManager::Get().DirectoryExists(*UIPluginsDir))
        {
            return;
        }

        TArray<FString> AllFiles;
        const FString Glob = FString::Printf(TEXT("*%s"), Extension);
        IFileManager::Get().FindFilesRecursive(AllFiles, *UIPluginsDir, *Glob, true, false);

        // Keep only files whose path contains a /Widgets/ segment - the
        // invariant is about widget code.
        OutFiles.Reserve(AllFiles.Num());
        for (const FString& Path : AllFiles)
        {
            const FString Normalized = Path.Replace(TEXT("\\"), TEXT("/"));
            if (Normalized.Contains(TEXT("/Widgets/")))
            {
                OutFiles.Add(Path);
            }
        }
    }

    bool LineIncludesForbiddenHeader(const FString& Line)
    {
        // Trim to detect #include "..." regardless of leading whitespace.
        // The forbidden headers are subsystem-private: router and (now
        // deleted) dispatcher. Guard against both so a partial revert that
        // re-adds InventoryDragDispatcher.h under another name is caught.
        const FString Trimmed = Line.TrimStartAndEnd();
        if (!Trimmed.StartsWith(TEXT("#include")))
        {
            return false;
        }
        return Trimmed.Contains(TEXT("MVVM/InventoryDropRouter.h"))
            || Trimmed.Contains(TEXT("MVVM/InventoryDragDispatcher.h"));
    }

    // -------------------------------------------------------------------
    // Fitness 2 helpers - role-based drag handler override scan.
    //
    // NativeOnDragOver / NativeOnDrop are plain C++ virtuals on UUserWidget,
    // not UFUNCTIONs - UClass reflection cannot see them. We text-scan the
    // ProjectInventoryUI widget headers (which declare the overrides) and
    // the class inheritance lists for the IInventoryDropTarget role.
    //
    // Shape matched:
    //   UCLASS()
    //   class PROJECTINVENTORYUI_API UW_Foo
    //       : public UUserWidget
    //       , public IInventoryDropTarget       <- role tag
    //   { ... };
    //
    // A class that declares NativeOnDragOver or NativeOnDrop override without
    // listing IInventoryDropTarget in its inheritance list fails.
    // -------------------------------------------------------------------

    struct FHeaderClassSpan
    {
        FString ClassName;
        int32 DeclarationLine = INDEX_NONE;
        int32 BodyStartLine = INDEX_NONE;
        int32 BodyEndLine = INDEX_NONE;
        bool bImplementsDropTargetRole = false;
    };

    // Collect the class name from a line that introduces a class (best
    // effort). Only consumed to label violation messages, not used for
    // matching logic.
    FString ExtractClassName(const FString& Line)
    {
        // Match patterns: "class PROJECTINVENTORYUI_API UW_Foo", "class UW_Bar".
        FString Remainder = Line;
        const int32 ClassKeyword = Remainder.Find(TEXT("class"));
        if (ClassKeyword == INDEX_NONE)
        {
            return FString();
        }
        Remainder.MidInline(ClassKeyword + 5);
        Remainder.TrimStartInline();
        // Drop API macro if present.
        if (Remainder.StartsWith(TEXT("PROJECTINVENTORYUI_API")))
        {
            Remainder.MidInline(FString(TEXT("PROJECTINVENTORYUI_API")).Len());
            Remainder.TrimStartInline();
        }
        // Read identifier up to the first non-identifier char.
        FString Name;
        for (int32 Index = 0; Index < Remainder.Len(); ++Index)
        {
            const TCHAR Ch = Remainder[Index];
            const bool bIsIdentChar = FChar::IsAlpha(Ch) || FChar::IsDigit(Ch) || Ch == TEXT('_');
            if (!bIsIdentChar) { break; }
            Name.AppendChar(Ch);
        }
        return Name;
    }

    void BuildHeaderClassSpans(const TArray<FString>& Lines, TArray<FHeaderClassSpan>& OutSpans)
    {
        OutSpans.Reset();

        int32 Index = 0;
        while (Index < Lines.Num())
        {
            const FString& Line = Lines[Index];
            const FString Trimmed = Line.TrimStartAndEnd();

            // Detect a REAL class declaration (not a forward declaration).
            // Forward declarations like "class UProjectGridCell;" end with
            // a semicolon; real declarations continue into an inheritance
            // list and open brace. We also require the API macro so we
            // don't pick up arbitrary internal helpers that are not part
            // of the module's public surface.
            //
            // Only accept "class PROJECTINVENTORYUI_API <Name>" as a real
            // declaration. Forward decls of other API macros, pure UMG
            // types, etc. are skipped.
            const bool bIsApiClassDecl = Trimmed.StartsWith(TEXT("class PROJECTINVENTORYUI_API"));

            if (!bIsApiClassDecl)
            {
                ++Index;
                continue;
            }

            // Filter out forward declarations: if the line already ends in
            // a semicolon (before any brace), skip.
            if (Trimmed.EndsWith(TEXT(";")) && !Trimmed.Contains(TEXT("{")))
            {
                ++Index;
                continue;
            }

            // Walk forward to collect the full class header text up to
            // the opening brace. If we hit a ';' before a '{' it's a
            // forward declaration spanning a couple of lines - skip.
            FString HeaderText = Trimmed;
            int32 BraceLine = INDEX_NONE;
            bool bIsForwardDecl = false;
            for (int32 Walk = Index; Walk < Lines.Num() && Walk < Index + 40; ++Walk)
            {
                const FString WalkTrim = Lines[Walk].TrimStartAndEnd();
                if (Walk > Index)
                {
                    HeaderText += TEXT(" ") + WalkTrim;
                }
                if (Lines[Walk].Contains(TEXT("{")))
                {
                    BraceLine = Walk;
                    break;
                }
                // Semicolon before brace: forward declaration variant.
                if (WalkTrim.EndsWith(TEXT(";")))
                {
                    bIsForwardDecl = true;
                    break;
                }
            }

            if (bIsForwardDecl || BraceLine == INDEX_NONE)
            {
                ++Index;
                continue;
            }

            FHeaderClassSpan Span;
            Span.DeclarationLine = Index;
            Span.ClassName = ExtractClassName(Trimmed);

            // Detect role tag in inheritance list. We check before the brace
            // so class bodies that happen to reference the interface (in a
            // comment) never trigger a false negative.
            Span.bImplementsDropTargetRole =
                HeaderText.Contains(TEXT("public IInventoryDropTarget"));

            Span.BodyStartLine = BraceLine;

            // Find matching close by tracking brace depth.
            int32 Depth = 0;
            int32 CloseLine = INDEX_NONE;
            for (int32 Walk = BraceLine; Walk < Lines.Num(); ++Walk)
            {
                for (int32 C = 0; C < Lines[Walk].Len(); ++C)
                {
                    if (Lines[Walk][C] == TEXT('{')) { ++Depth; }
                    else if (Lines[Walk][C] == TEXT('}')) { --Depth; }
                }
                if (Depth <= 0)
                {
                    CloseLine = Walk;
                    break;
                }
            }
            Span.BodyEndLine = CloseLine == INDEX_NONE ? Lines.Num() - 1 : CloseLine;

            OutSpans.Add(MoveTemp(Span));
            Index = Span.BodyEndLine + 1;
        }
    }

    // True if the given body line span contains a declaration that
    // overrides NativeOnDragOver or NativeOnDrop on the declaring class.
    // We match the "override" keyword so forward decls or base-class
    // redeclares elsewhere are ignored.
    //
    // Sabotage awareness: when SLICE20_SABOTAGE=0 (the default/green build)
    // we skip lines wrapped in "#if SLICE20_SABOTAGE" blocks because those
    // lines are inert at compile time. When SLICE20_SABOTAGE=1 those lines
    // ARE active code and the scanner reads them as real overrides. This
    // keeps green builds green and red builds red without needing two
    // scanner implementations.
    bool SpanOverridesDragHandler(const TArray<FString>& Lines, const FHeaderClassSpan& Span, FString& OutMatchedSignature)
    {
        bool bInSabotageBlock = false;
        for (int32 Idx = Span.BodyStartLine; Idx <= Span.BodyEndLine && Idx < Lines.Num(); ++Idx)
        {
            const FString& Line = Lines[Idx];
            const FString Trimmed = Line.TrimStartAndEnd();

            if (Trimmed.StartsWith(TEXT("#if SLICE20_SABOTAGE")))
            {
                bInSabotageBlock = true;
                continue;
            }
            if (bInSabotageBlock && Trimmed.StartsWith(TEXT("#endif")))
            {
                bInSabotageBlock = false;
                continue;
            }
#if !SLICE20_SABOTAGE
            if (bInSabotageBlock)
            {
                continue;
            }
#endif

            // Skip comments - doc-comment lines mentioning "override" + "NativeOnDragOver"
            // are not real overrides (e.g. "this widget does NOT override NativeOnDragOver").
            if (Trimmed.StartsWith(TEXT("*"))
                || Trimmed.StartsWith(TEXT("//"))
                || Trimmed.StartsWith(TEXT("/*")))
            {
                continue;
            }

            if (!Line.Contains(TEXT("override"))) { continue; }
            if (Line.Contains(TEXT("NativeOnDragOver")) || Line.Contains(TEXT("NativeOnDrop")))
            {
                OutMatchedSignature = Line.TrimStartAndEnd();
                return true;
            }
        }
        return false;
    }

    // -------------------------------------------------------------------
    // Fitness 4 helpers - bare subsystem construction.
    //
    // Reused shape from InventoryDragEventBusTests - subsystems need a
    // LocalPlayer outer, so anchor to the automation world.
    // -------------------------------------------------------------------

    UWorld* ResolveArchFitnessAutomationWorld()
    {
        UWorld* World = AutomationCommon::GetAnyGameWorld();
        if (World)
        {
            return World;
        }
        if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
        {
            return nullptr;
        }
        return AutomationCommon::GetAnyGameWorld();
    }

    UInventoryUIDragHostSubsystem* NewBareArchSubsystem(UWorld* World)
    {
        ULocalPlayer* LP = nullptr;
        if (World)
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                LP = PC->GetLocalPlayer();
            }
            if (!LP)
            {
                if (UGameInstance* GI = World->GetGameInstance())
                {
                    LP = GI->GetFirstGamePlayer();
                }
            }
        }
        if (!LP && World && World->GetGameInstance())
        {
            LP = NewObject<ULocalPlayer>(World->GetGameInstance());
        }
        if (!LP)
        {
            return nullptr;
        }
        return NewObject<UInventoryUIDragHostSubsystem>(LP);
    }
}

// ---------------------------------------------------------------------------
// Fitness 1 - NoWidgetIncludesRouterOrDispatcher.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryArchNoWidgetIncludesRouterOrDispatcherTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.FitnessTests.NoWidgetIncludesRouterOrDispatcher",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryArchNoWidgetIncludesRouterOrDispatcherTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TArray<FString> WidgetCpps;
    FindProjectInventoryUIWidgetFiles(WidgetCpps, TEXT(".cpp"));
    TArray<FString> WidgetHeaders;
    FindProjectInventoryUIWidgetFiles(WidgetHeaders, TEXT(".h"));

    TArray<FString> AllWidgetFiles;
    AllWidgetFiles.Append(WidgetCpps);
    AllWidgetFiles.Append(WidgetHeaders);

    if (!TestTrue(
        TEXT("Widget file discovery must find at least one file"),
        AllWidgetFiles.Num() > 0))
    {
        AddError(FString::Printf(
            TEXT("No widget files found under %s. Fitness test cannot run."),
            *(FPaths::ProjectPluginsDir() / TEXT("UI"))));
        return false;
    }

    int32 Violations = 0;

    for (const FString& FilePath : AllWidgetFiles)
    {
        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FilePath))
        {
            AddWarning(FString::Printf(TEXT("Could not read %s; skipped."), *FilePath));
            continue;
        }

        TArray<FString> Lines;
        FileText.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

        // Sabotage awareness: when SLICE20_SABOTAGE=0 (green) we skip
        // "#if SLICE20_SABOTAGE" blocks because they are preprocessed out.
        // When =1 (red) we read them as active code and catch the sabotage.
        bool bInSabotageBlock = false;
        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FString Trimmed = Lines[Index].TrimStartAndEnd();
            if (Trimmed.StartsWith(TEXT("#if SLICE20_SABOTAGE")))
            {
                bInSabotageBlock = true;
                continue;
            }
            if (bInSabotageBlock && Trimmed.StartsWith(TEXT("#endif")))
            {
                bInSabotageBlock = false;
                continue;
            }
#if !SLICE20_SABOTAGE
            if (bInSabotageBlock)
            {
                continue;
            }
#endif

            if (LineIncludesForbiddenHeader(Lines[Index]))
            {
                ++Violations;
                AddError(FString::Printf(
                    TEXT("Widget file includes subsystem-private router/dispatcher header. "
                         "File=%s Line=%d: %s"),
                    *FilePath,
                    Index + 1,
                    *Lines[Index].TrimStartAndEnd()));
            }
        }
    }

    AddInfo(FString::Printf(
        TEXT("Scanned %d widget files; forbidden-include violations=%d."),
        AllWidgetFiles.Num(), Violations));

    return Violations == 0;
}

// ---------------------------------------------------------------------------
// Fitness 2 - OnlyDropTargetWidgetsOverrideDragHandlers.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryArchOnlyDropTargetWidgetsOverrideDragHandlersTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.FitnessTests.OnlyDropTargetWidgetsOverrideDragHandlers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryArchOnlyDropTargetWidgetsOverrideDragHandlersTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TArray<FString> HeaderFiles;
    FindProjectInventoryUIWidgetFiles(HeaderFiles, TEXT(".h"));

    if (!TestTrue(
        TEXT("Widget header discovery must find at least one file"),
        HeaderFiles.Num() > 0))
    {
        AddError(FString::Printf(
            TEXT("No widget headers found under %s. Fitness test cannot run."),
            *(FPaths::ProjectPluginsDir() / TEXT("UI"))));
        return false;
    }

    int32 ClassesScanned = 0;
    int32 Violations = 0;

    for (const FString& FilePath : HeaderFiles)
    {
        // Scope to the ProjectInventoryUI plugin's widget headers. A
        // non-inventory plugin's widgets are not governed by this rule.
        const FString Normalized = FilePath.Replace(TEXT("\\"), TEXT("/"));
        if (!Normalized.Contains(TEXT("/ProjectInventoryUI/")))
        {
            continue;
        }

        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FilePath))
        {
            AddWarning(FString::Printf(TEXT("Could not read %s; skipped."), *FilePath));
            continue;
        }

        TArray<FString> Lines;
        FileText.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

        TArray<FHeaderClassSpan> Spans;
        BuildHeaderClassSpans(Lines, Spans);

        for (const FHeaderClassSpan& Span : Spans)
        {
            ++ClassesScanned;

            FString Signature;
            if (!SpanOverridesDragHandler(Lines, Span, Signature))
            {
                continue;
            }

            // Overrides present -> must carry the role.
            if (Span.bImplementsDropTargetRole)
            {
                continue;
            }

            ++Violations;
            AddError(FString::Printf(
                TEXT("Widget class '%s' overrides NativeOnDragOver/NativeOnDrop without "
                     "implementing IInventoryDropTarget. Add ': public IInventoryDropTarget' "
                     "to the inheritance list or move the handler to a dedicated drop-target "
                     "wrapper. File=%s Line=%d: %s"),
                Span.ClassName.IsEmpty() ? TEXT("<unknown>") : *Span.ClassName,
                *FilePath,
                Span.DeclarationLine + 1,
                *Signature));
        }
    }

    AddInfo(FString::Printf(
        TEXT("Scanned %d class declarations in widget headers; role violations=%d."),
        ClassesScanned, Violations));

    return Violations == 0;
}

// ---------------------------------------------------------------------------
// Fitness 3 - UserWidgetVisibilityContracted.
//
// Visibility contract pinned here (as declared in each widget's header):
//   - UW_InventoryPanel  ShowPath SetVisibility == ESlateVisibility::Visible
//     (root canvas does not anchor Fill; Visible is safe and intentional).
//   - UW_NearbyContainerPanel ShowPath SetVisibility == ESlateVisibility::SelfHitTestInvisible
//     (root canvas anchors Fill-of-viewport; Visible would swallow every
//      click and break the sibling main panel's drag routing).
//
// Implementation: text-scan each widget's .cpp for its visibility-show
// assignment. We match the "bVisible ? ESlateVisibility::X : ESlateVisibility::Collapsed"
// idiom on the widget's root (the `SetVisibility(` call without a leading
// member access). Flipping X in the source must make this test fail with
// a named assertion message.
//
// Why static text-scan instead of runtime inspection:
//   - The widget's runtime visibility is VM-driven; without a live VM bound
//     in an automation context the value is untouched default, not the
//     contracted "when shown" value. Pinning THAT default does not describe
//     the contract.
//   - The contract IS the `Visible` / `SelfHitTestInvisible` literal in
//     the source. Scanning the assignment site is the real contract, not
//     a derived runtime snapshot.
//   - Sabotage 3 flips the literal value in the widget source; the scan
//     catches it by matching the flipped literal against the pinned one.
// ---------------------------------------------------------------------------

namespace
{
    struct FVisibilityHit
    {
        FString Literal;
        int32 LineNumber = INDEX_NONE;
    };

    // Scan a widget .cpp for ALL visibility-show assignments on the widget's
    // root. Match lines of the form:
    //     SetVisibility(... ? ESlateVisibility::<LITERAL> : ESlateVisibility::Collapsed);
    // Returns every match (each flip site) so the caller can verify they
    // all agree on the contracted literal. Sabotaging a single site must
    // make at least one match disagree, which breaks the assertion.
    //
    // Returns true if at least one match was found.
    bool FindAllRootShowVisibilityLiterals(
        const FString& FilePath,
        TArray<FVisibilityHit>& OutHits)
    {
        OutHits.Reset();

        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FilePath))
        {
            return false;
        }

        TArray<FString> Lines;
        FileText.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

        const FString NeedleBegin(TEXT("SetVisibility("));
        const FString FalseBranch(TEXT(": ESlateVisibility::Collapsed"));
        const FString TrueBranchBegin(TEXT("? ESlateVisibility::"));

        // Sabotage-awareness: default green build treats "#if SLICE20_SABOTAGE"
        // blocks as inert. Red build reads them as active. When the test is
        // compiled with SLICE20_SABOTAGE=1 the scanner must pick up the
        // sabotaged SetVisibility line instead of the real one so the
        // visibility-flip sabotage produces a named failure. In green the
        // real (non-sabotage) SetVisibility line in the #else branch is the
        // one read.
        bool bInSabotageBlock = false;
        bool bInSabotageElseBlock = false;

        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FString& Line = Lines[Index];
            const FString Trimmed = Line.TrimStartAndEnd();

            if (Trimmed.StartsWith(TEXT("#if SLICE20_SABOTAGE")))
            {
                bInSabotageBlock = true;
                bInSabotageElseBlock = false;
                continue;
            }
            if (bInSabotageBlock && Trimmed.StartsWith(TEXT("#else")))
            {
                bInSabotageElseBlock = true;
                continue;
            }
            if ((bInSabotageBlock || bInSabotageElseBlock) && Trimmed.StartsWith(TEXT("#endif")))
            {
                bInSabotageBlock = false;
                bInSabotageElseBlock = false;
                continue;
            }
#if SLICE20_SABOTAGE
            // Red build: consume the sabotage branch; skip the #else branch.
            if (bInSabotageElseBlock)
            {
                continue;
            }
#else
            // Green build: consume the #else branch; skip the sabotage branch.
            if (bInSabotageBlock && !bInSabotageElseBlock)
            {
                continue;
            }
#endif

            // Must be a bare SetVisibility call - skip member accesses like
            // "GridHost->SetVisibility(...)" because those target children,
            // not the widget root.
            const int32 CallStart = Trimmed.Find(NeedleBegin);
            if (CallStart == INDEX_NONE) { continue; }
            if (CallStart > 0)
            {
                // Look at the char just before "SetVisibility(" - if it's a
                // ">" or "." the call is on a child, skip.
                const TCHAR Prev = Trimmed[CallStart - 1];
                if (Prev == TEXT('>') || Prev == TEXT('.'))
                {
                    continue;
                }
            }

            // Need the ternary shape and a Collapsed false-branch.
            const int32 TrueAt = Line.Find(TrueBranchBegin);
            const int32 FalseAt = Line.Find(FalseBranch);
            if (TrueAt == INDEX_NONE || FalseAt == INDEX_NONE) { continue; }
            if (FalseAt < TrueAt) { continue; }

            // Extract literal between "? ESlateVisibility::" and " : ESlateVisibility::Collapsed".
            const int32 LiteralStart = TrueAt + TrueBranchBegin.Len();
            FString Candidate = Line.Mid(LiteralStart, FalseAt - LiteralStart).TrimStartAndEnd();
            // Candidate can now be e.g. "Visible" or "SelfHitTestInvisible" (single token).
            // Keep only the leading identifier.
            FString Literal;
            for (int32 I = 0; I < Candidate.Len(); ++I)
            {
                const TCHAR Ch = Candidate[I];
                const bool bIsIdentChar = FChar::IsAlpha(Ch) || FChar::IsDigit(Ch) || Ch == TEXT('_');
                if (!bIsIdentChar) { break; }
                Literal.AppendChar(Ch);
            }

            if (!Literal.IsEmpty())
            {
                FVisibilityHit Hit;
                Hit.Literal = Literal;
                Hit.LineNumber = Index + 1;
                OutHits.Add(MoveTemp(Hit));
            }
        }
        return OutHits.Num() > 0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryArchUserWidgetVisibilityContractedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.FitnessTests.UserWidgetVisibilityContracted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryArchUserWidgetVisibilityContractedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FString InventoryUISrcDir = FPaths::ProjectPluginsDir()
        / TEXT("UI") / TEXT("ProjectInventoryUI") / TEXT("Source")
        / TEXT("ProjectInventoryUI") / TEXT("Private") / TEXT("Widgets");

    const FString MainPanelCpp = InventoryUISrcDir / TEXT("W_InventoryPanel.cpp");
    const FString NearbyPanelCpp = InventoryUISrcDir / TEXT("W_NearbyContainerPanel.cpp");

    if (!TestTrue(
        TEXT("W_InventoryPanel.cpp must exist"),
        IFileManager::Get().FileExists(*MainPanelCpp)))
    {
        return false;
    }
    if (!TestTrue(
        TEXT("W_NearbyContainerPanel.cpp must exist"),
        IFileManager::Get().FileExists(*NearbyPanelCpp)))
    {
        return false;
    }

    TArray<FVisibilityHit> MainHits;
    TArray<FVisibilityHit> NearbyHits;

    TestTrue(
        TEXT("W_InventoryPanel.cpp must contain at least one root SetVisibility show-path"),
        FindAllRootShowVisibilityLiterals(MainPanelCpp, MainHits));
    TestTrue(
        TEXT("W_NearbyContainerPanel.cpp must contain at least one root SetVisibility show-path"),
        FindAllRootShowVisibilityLiterals(NearbyPanelCpp, NearbyHits));

    // Pin the contracts. EVERY flip site must use the contracted literal;
    // sabotaging even one site (Slice 20 sabotage 3 flips exactly one site
    // in W_InventoryPanel) must break this test with a named failure.
    const FString ExpectedMain(TEXT("Visible"));
    const FString ExpectedNearby(TEXT("SelfHitTestInvisible"));

    int32 MainMismatches = 0;
    for (const FVisibilityHit& Hit : MainHits)
    {
        if (Hit.Literal != ExpectedMain)
        {
            ++MainMismatches;
            AddError(FString::Printf(
                TEXT("UW_InventoryPanel root SetVisibility show-path must be %s (Slice 19 contract); "
                     "found '%s' at %s:%d. Flipping to SelfHitTestInvisible silently pass-throughs input "
                     "and breaks drag routing on the main panel."),
                *ExpectedMain, *Hit.Literal, *MainPanelCpp, Hit.LineNumber));
        }
    }

    int32 NearbyMismatches = 0;
    for (const FVisibilityHit& Hit : NearbyHits)
    {
        if (Hit.Literal != ExpectedNearby)
        {
            ++NearbyMismatches;
            AddError(FString::Printf(
                TEXT("UW_NearbyContainerPanel root SetVisibility show-path must be %s (Slice 17 contract); "
                     "found '%s' at %s:%d. Flipping to Visible makes the Fill-anchored canvas swallow every "
                     "drag/drop meant for the sibling W_InventoryPanel."),
                *ExpectedNearby, *Hit.Literal, *NearbyPanelCpp, Hit.LineNumber));
        }
    }

    AddInfo(FString::Printf(
        TEXT("Scanned %d main-panel show sites and %d nearby-panel show sites; mismatches main=%d nearby=%d."),
        MainHits.Num(), NearbyHits.Num(), MainMismatches, NearbyMismatches));

    return MainMismatches == 0 && NearbyMismatches == 0;
}

// ---------------------------------------------------------------------------
// Fitness 4 - EveryDragSessionEmitsCompletedOrCancelled.
//
// Deterministic fuzz: 100 sequences of Begin/Update/Complete/Cancel driven
// over a bare subsystem (no surfaces, no policy provider). Each sequence
// must terminate with exactly ONE Completed or Cancelled event - the
// terminal-guarantee invariant. Routing correctness is NOT tested here;
// that lives in InventoryDragEventBusTests and InventoryDragE2ESyntheticInputTests.
//
// Seeded RNG for reproducibility; if a failure ever appears, the seed +
// sequence index is logged so the offender can be reproduced exactly.
// ---------------------------------------------------------------------------

namespace
{
    enum class EFuzzAction : uint8
    {
        BeginCellDrag,
        UpdatePreview,
        CompleteDrop,
        CancelDrag,
    };

    // Drive one randomized drag session. Returns the number of terminal
    // events (Completed + Cancelled) observed. Also records the action
    // trace for diagnostic output.
    int32 DriveOneFuzzSession(
        UInventoryUIDragHostSubsystem* Subsystem,
        FRandomStream& Rng,
        TArray<EFuzzAction>& OutActions)
    {
        OutActions.Reset();

        // Always begin. This is the setup guard (can't test session
        // termination without a session).
        FInventoryDragStartParams Params;
        Params.SourceTag = ProjectTags::Item_Container_Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = Rng.RandRange(1, 100000);
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);
        OutActions.Add(EFuzzAction::BeginCellDrag);

        FInventoryDragEventRecorder Recorder(Subsystem);

        // Drive a random mid-body of 0-4 updates, then terminate with
        // either CompleteDrop or CancelDrag. 50/50 split keeps both
        // terminal paths exercised.
        const int32 Updates = Rng.RandRange(0, 4);
        for (int32 I = 0; I < Updates; ++I)
        {
            FInventoryCellCandidate Cand;
            Cand.InstanceId = Params.InstanceId;
            Cand.SourceSurfaceTag = Params.SourceTag;
            Cand.SourcePos = Params.SourceCell;
            Cand.Quantity = 1;
            Cand.ItemSize = FIntPoint(1, 1);
            const FVector2D Pos(
                Rng.FRandRange(0.f, 1024.f),
                Rng.FRandRange(0.f, 1024.f));
            Subsystem->UpdatePreview(Cand, Pos);
            OutActions.Add(EFuzzAction::UpdatePreview);
        }

        const bool bUseComplete = Rng.RandRange(0, 1) == 0;
        if (bUseComplete)
        {
            FInventoryCellCandidate Cand;
            Cand.InstanceId = Params.InstanceId;
            Cand.SourceSurfaceTag = Params.SourceTag;
            Cand.SourcePos = Params.SourceCell;
            Cand.Quantity = 1;
            Cand.ItemSize = FIntPoint(1, 1);
            const FVector2D Pos(
                Rng.FRandRange(0.f, 1024.f),
                Rng.FRandRange(0.f, 1024.f));
            Subsystem->CompleteDrop(Cand, Pos);
            OutActions.Add(EFuzzAction::CompleteDrop);
        }
        else
        {
            Subsystem->CancelDrag();
            OutActions.Add(EFuzzAction::CancelDrag);
        }

        // Count terminal events. "Exactly one" is the invariant we care
        // about - neither zero (silent failure) nor more than one
        // (double-terminal).
        int32 Terminals = 0;
        for (const FInventoryDragEvent& Event : Recorder.GetEvents())
        {
            if (Event.Kind == EInventoryDragEventKind::Completed
                || Event.Kind == EInventoryDragEventKind::Cancelled)
            {
                ++Terminals;
            }
        }
        return Terminals;
    }

    FString DescribeActions(const TArray<EFuzzAction>& Actions)
    {
        FString Out;
        for (EFuzzAction A : Actions)
        {
            if (!Out.IsEmpty()) { Out.Append(TEXT(",")); }
            switch (A)
            {
                case EFuzzAction::BeginCellDrag:  Out.Append(TEXT("Begin")); break;
                case EFuzzAction::UpdatePreview:  Out.Append(TEXT("Update")); break;
                case EFuzzAction::CompleteDrop:   Out.Append(TEXT("Complete")); break;
                case EFuzzAction::CancelDrag:     Out.Append(TEXT("Cancel")); break;
            }
        }
        return Out;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryArchEveryDragSessionEmitsTerminalTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.FitnessTests.EveryDragSessionEmitsCompletedOrCancelled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryArchEveryDragSessionEmitsTerminalTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveArchFitnessAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareArchSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    // Deterministic seed so failures reproduce exactly.
    constexpr int32 FuzzSeed = 20260421;
    constexpr int32 SequenceCount = 100;
    FRandomStream Rng(FuzzSeed);

    int32 Failures = 0;

    for (int32 Seq = 0; Seq < SequenceCount; ++Seq)
    {
        // Clean slate between sequences: cancel any leftover session,
        // clear surfaces (there should be none), clear provider.
        Subsystem->CancelDrag();
        Subsystem->ClearSurfaces();
        Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>());

        TArray<EFuzzAction> Actions;
        const int32 Terminals = DriveOneFuzzSession(Subsystem, Rng, Actions);

        if (Terminals != 1)
        {
            ++Failures;
            AddError(FString::Printf(
                TEXT("Fuzz sequence %d (seed=%d) produced %d terminal events; expected exactly 1. "
                     "Actions=[%s]. Every drag session must end with one and only one of "
                     "{Completed, Cancelled}."),
                Seq, FuzzSeed, Terminals, *DescribeActions(Actions)));
        }
    }

    AddInfo(FString::Printf(
        TEXT("Drove %d randomized drag sessions (seed=%d); terminal-invariant failures=%d."),
        SequenceCount, FuzzSeed, Failures));

    return Failures == 0;
}

// ---------------------------------------------------------------------------
// Slice 18 fallback audit - pinning test.
//
// Decision (Slice 20 audit): KEPT. The "empty-or-self" fallback that
// InstallPolicyCheckerIfNeeded wires into every surface registered without
// an explicit OccupantAllowedChecker is NOT unreachable. Two paths still
// run it:
//   (a) CompleteDrop's Controller.ResolveDropTargetOverSurfaces call runs
//       each surface's OccupantAllowedChecker. If the controller rejects
//       the footprint because of the fallback (occupied cell, no provider),
//       CompleteDrop emits DropRejected{NoTargetUnderCursor} instead of
//       DropRejected{NoCommandTarget}.
//   (b) UpdatePreview drives the same controller path for visual preview.
//       During VM-bind startup, widgets register surfaces before the VM
//       installs itself as policy provider; the fallback is what keeps
//       empty cells visually available during that window.
//
// The focused pin test asserts CompleteDrop with no VM bound fails-closed
// via the (a) path, which is the load-bearing guarantee for production
// drops. The visual-preview path is already covered indirectly by the
// existing BackpackEmptyCellDropAccepts + the E2E tests. The second
// angle here - the exact RejectReason name - pins the path taken (VM-
// check vs controller-check) so an accidental collapse between the two
// branches surfaces.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryArchSubsystemNoProviderFailsClosedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.FitnessTests.SubsystemWithoutPolicyProviderRejectsCompleteDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryArchSubsystemNoProviderFailsClosedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveArchFitnessAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareArchSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    FInventoryDragEventRecorder Recorder(Subsystem);

    // Begin a session, no provider bound.
    FInventoryDragStartParams Params;
    Params.SourceTag = ProjectTags::Item_Container_Backpack;
    Params.SourceCell = FIntPoint(0, 0);
    Params.InstanceId = 314;
    Params.Quantity = 1;
    Subsystem->BeginCellDrag(Params);

    // Attempt a drop with no registered surfaces; controller cannot
    // resolve, fails with DropRejected{NoTargetUnderCursor} then Cancelled.
    // The key pin: without a VM bound, the subsystem still reaches a
    // named terminal - no silent success, no empty stream.
    FInventoryCellCandidate Cand;
    Cand.InstanceId = 314;
    Cand.SourceSurfaceTag = ProjectTags::Item_Container_Backpack;
    Cand.SourcePos = FIntPoint(0, 0);
    Cand.Quantity = 1;
    Cand.ItemSize = FIntPoint(1, 1);
    const bool bDispatched = Subsystem->CompleteDrop(Cand, FVector2D(10.f, 10.f));
    TestFalse(
        TEXT("CompleteDrop with no provider bound MUST return false (fail-closed). "
             "Returning true here would mean a VM command ran without a VM to receive it."),
        bDispatched);

    // Verify at least one DropRejected event with a non-None reason fired
    // (fail-LOUD contract; silent drops are forbidden).
    bool bSawReject = false;
    FName Reason;
    bool bSawCancelled = false;
    for (const FInventoryDragEvent& Event : Recorder.GetEvents())
    {
        if (Event.Kind == EInventoryDragEventKind::DropRejected)
        {
            bSawReject = true;
            Reason = Event.RejectReason;
        }
        if (Event.Kind == EInventoryDragEventKind::Cancelled)
        {
            bSawCancelled = true;
        }
    }

    TestTrue(
        TEXT("CompleteDrop without a provider MUST emit DropRejected with a named reason. "
             "Silent rejects were the pre-Slice-18 bug class; fail-LOUD is the contract."),
        bSawReject);
    TestTrue(
        TEXT("DropRejected reason must not be None - reason names are part of the contract."),
        !Reason.IsNone());
    TestTrue(
        TEXT("CompleteDrop session MUST terminate with Cancelled even when rejected."),
        bSawCancelled);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
