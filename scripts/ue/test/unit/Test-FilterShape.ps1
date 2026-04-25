# Shared filter-shape validator + rejection-message printer for the dev-loop
# guardrail scripts (run_single.ps1, iterate.ps1, run_cpp_tests_safe.ps1 -Mode Dev).
#
# Why a shared file:
#   The Dev Loop Contract (docs/agents/canonical.md "Dev Loop Contract")
#   must be enforced identically across every guardrail entry point. One
#   helper = one source of truth for what "broad" means and one source of
#   truth for the rejection-message contract.
#
# Usage:
#   . "$PSScriptRoot\Test-FilterShape.ps1"
#   $result = Test-ExactFilter -Filter $TestFilter
#   if (-not $result.IsExact) {
#       Write-BroadFilterRejection -Filter $TestFilter -Reason $result.Reason `
#           -ScriptName "iterate.ps1"
#       exit 2
#   }
#
# Rejection-message contract (enforced here, not in each caller):
#   1. rejected filter (verbatim)
#   2. reason it is considered broad
#   3. one exact accepted example drawn from the current test base
#   4. how to override intentionally (-Mode Gate, -AllowBroadFilter)
#
# Keep this file free of UE-process concerns; it is pure string validation.

# A concrete, working test ID from the ProjectIntegrationTests base. Used in
# rejection messages so an agent reading the refusal immediately sees a
# copy-pasteable accepted shape. Update when the canonical example shifts.
$script:ExactFilterExample = "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell"

# Cache for the source-scan. Re-reading 100+ test files per invocation is
# wasted; scripts that validate multiple filters in one run (future -Tags +
# -TestFilter combo) benefit, and there is no downside to caching within a
# single process lifetime.
$script:TestSourceBlobCache = $null

function Get-TestRepoRoot {
    # scripts/ue/test/unit/ -> .. -> .. -> .. -> .. = repo root
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
}

function Get-TestSourceBlob {
    # Concatenated content of every .cpp/.h under Plugins/Test, read once
    # per process. Used as a haystack for the exact-quoted-literal check.
    if ($null -ne $script:TestSourceBlobCache) {
        return $script:TestSourceBlobCache
    }
    $repoRoot = Get-TestRepoRoot
    $testRoot = Join-Path $repoRoot "Plugins\Test"
    if (-not (Test-Path $testRoot)) {
        $script:TestSourceBlobCache = ""
        return $script:TestSourceBlobCache
    }
    $sb = [System.Text.StringBuilder]::new()
    Get-ChildItem $testRoot -Recurse -File -Include *.cpp, *.h -ErrorAction SilentlyContinue |
        ForEach-Object {
            try {
                [void]$sb.Append([System.IO.File]::ReadAllText($_.FullName))
                [void]$sb.Append("`n")
            } catch { }
        }
    $script:TestSourceBlobCache = $sb.ToString()
    return $script:TestSourceBlobCache
}

function Test-ExactQuotedLiteralInTestSource {
    # Returns $true if $Filter appears as a complete quoted literal
    # ("<filter>" or '<filter>') anywhere in test source. Used to reject
    # broad-prefix filters that accidentally have enough dot segments to
    # pass the segment-count check (e.g. "ProjectIntegrationTests.UI.Framework.Inventory"
    # has 4 segments but is NOT a real test ID, only a prefix).
    param(
        [Parameter(Mandatory = $true)][string]$Filter
    )
    $blob = Get-TestSourceBlob
    if ([string]::IsNullOrEmpty($blob)) { return $false }
    $escaped = [regex]::Escape($Filter)
    # Match either "<filter>" or '<filter>' — UE IMPLEMENT_*_AUTOMATION_TEST
    # macros use double-quoted literals, but tolerate either style.
    $pattern = '(?:"|' + "'" + ')' + $escaped + '(?:"|' + "'" + ')'
    return [regex]::IsMatch($blob, $pattern)
}

function Test-ExactFilter {
    param(
        [Parameter(Mandatory = $true)][string]$Filter
    )

    $out = [PSCustomObject]@{
        IsExact = $false
        Reason  = ""
        Example = $script:ExactFilterExample
    }

    if ([string]::IsNullOrWhiteSpace($Filter)) {
        $out.Reason = "empty filter"
        return $out
    }

    # Wildcards: UE treats `*` as a match-many prefix. Always broad.
    if ($Filter -match '\*') {
        $out.Reason = "contains wildcard '*' (matches many tests)"
        return $out
    }

    # Union separators: `;` is UE's automation list separator; `,` is a
    # common agent mistake. Either shape means "more than one test".
    if ($Filter -match '[;,]') {
        $out.Reason = "contains union separator (',' or ';') - more than one test"
        return $out
    }

    # Group:/Filter: prefixes: UE's built-in broad selectors.
    if ($Filter -match '^(Group|Filter):') {
        $out.Reason = "uses '$($Matches[1]):' prefix (broad selector syntax)"
        return $out
    }

    # Tag expressions: reserved for Phase 2 -Tags flag.
    if ($Filter -match '(\[|\]|&&|\|\|)') {
        $out.Reason = "looks like a tag expression (reserved for Phase 2 -Tags flag)"
        return $out
    }

    # Short prefix paths: UE automation treats any filter as a prefix match,
    # so `ProjectIntegrationTests.UI` matches hundreds of tests. Require at
    # least 4 dot-separated segments for an exact full test name.
    # Shape: Root.Category.Group.Name (minimum 4). The canonical example
    # has 6 segments; most real exact IDs have 5-7.
    $segments = $Filter.Split('.')
    if ($segments.Count -lt 4) {
        $out.Reason = "only $($segments.Count) dot-separated segment(s); an exact full test name needs >= 4 (Root.Category.Group.Name...)"
        return $out
    }

    # Trailing dot / empty segment guard.
    foreach ($s in $segments) {
        if ([string]::IsNullOrWhiteSpace($s)) {
            $out.Reason = "empty dot-separated segment (malformed filter)"
            return $out
        }
    }

    # Stronger than segment counting: require the filter to appear as a
    # complete quoted literal in test source. A broad prefix like
    # `ProjectIntegrationTests.UI.Framework.Inventory` has 4 segments but
    # is never registered as a test ID - it only appears as a substring
    # inside longer quoted literals, so this check rejects it.
    if (-not (Test-ExactQuotedLiteralInTestSource -Filter $Filter)) {
        $out.Reason = "not found as an exact quoted test id in Plugins/Test source (likely a broad prefix or typo)"
        return $out
    }

    $out.IsExact = $true
    return $out
}

function Write-BroadFilterRejection {
    param(
        [Parameter(Mandatory = $true)][string]$Filter,
        [Parameter(Mandatory = $true)][string]$Reason,
        [string]$Example = $script:ExactFilterExample,
        [string]$ScriptName = "iterate.ps1",

        # Caller-supplied override hints. Each string should be a one-line
        # command the user can copy-paste. Defaults to the two flags accepted
        # by iterate.ps1 / run_cpp_tests_safe.ps1. Scripts that are
        # intentionally strict-only (e.g. run_single.ps1) pass their own list
        # pointing users to the right wrapper.
        [string[]]$OverrideCommands = @(
            "iterate.ps1 -Mode Gate           -TestFilter `"{0}`"",
            "iterate.ps1 -AllowBroadFilter    -TestFilter `"{0}`""
        )
    )

    Write-Host ""
    Write-Host "================ DEV-MODE REFUSAL ================" -ForegroundColor Red
    Write-Host " Rejected filter: $Filter"                           -ForegroundColor Red
    Write-Host " Reason:          $Reason"                           -ForegroundColor Yellow
    Write-Host " Refused by:      $ScriptName"                       -ForegroundColor Gray
    Write-Host ""
    Write-Host " Exact accepted example:"                            -ForegroundColor Cyan
    Write-Host "   $Example"                                         -ForegroundColor Cyan
    Write-Host ""
    Write-Host " To override intentionally (gate / end-of-slice / CI):" -ForegroundColor Gray
    foreach ($cmd in $OverrideCommands) {
        Write-Host "   $([string]::Format($cmd, $Filter))"            -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host " Contract:  docs/agents/canonical.md (Dev Loop Contract)" -ForegroundColor Gray
    Write-Host "=================================================" -ForegroundColor Red
    Write-Host ""
}
