# update_engine.ps1 - THE supported engine-update entry point (one
# command, resumable). Operator contract: docs/ue_engine/version_update.md.
#
#   update_engine.ps1 -LauncherRoot "C:\UnrealEngine\UE_X.Y"        # dry-run
#   update_engine.ps1 -LauncherRoot ... -Apply
#   update_engine.ps1 -Resume   [-RunId <id>]
#   update_engine.ps1 -Status   [-RunId <id>]
#   update_engine.ps1 -Rollback [-RunId <id>]
#   update_engine.ps1 -CompleteSource -RunId <dev-run-id> -SourceRoot ...
#
# Versions derive from each root's Engine/Build/Build.version; the
# uproject association derives from that. -ExpectedVersion is an
# assertion only. Deterministic drift is owned by this script + its
# writers; agents only fix semantic build errors, then -Resume.

param(
    [string]$LauncherRoot,
    [string]$SourceRoot,
    [switch]$Apply,
    [switch]$Resume,
    [switch]$Status,
    [switch]$Rollback,
    [switch]$CompleteSource,
    [string]$RunId,
    [string]$ExpectedVersion,
    # Test hooks (synthetic repos / phases); production uses defaults.
    [string]$RepoRoot,
    [ValidateSet("User", "Process")][string]$EnvScope = "User",
    [switch]$SkipProcessCheck,
    [string]$PhasesScript,
    # Explicit, logged override: admit -Apply although managed files carry
    # uncommitted edits (ONLY when those edits are intentionally part of
    # this update session, e.g. the SOT migration itself is not yet
    # committed). Backups still capture the pre-apply state.
    [switch]$AllowDirtyManaged
)

$ErrorActionPreference = "Stop"
if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}
Import-Module (Join-Path $PSScriptRoot "UpdateEngineCore.psm1") -Force
Import-Module (Join-Path $PSScriptRoot "UpdateEnginePhases.psm1") -Force

function Get-PhaseList($Plan) {
    if ($PhasesScript) { return (& $PhasesScript -RepoRoot $RepoRoot -Plan $Plan) }
    return (Get-UEUpdatePhases -RepoRoot $RepoRoot -Plan $Plan)
}

function Get-SourcePhaseList($Plan) {
    $phases = if ($PhasesScript) {
        @(& $PhasesScript -RepoRoot $RepoRoot -Plan $Plan)
    } else {
        @(Get-UESourceCompletionPhases -RepoRoot $RepoRoot -Plan $Plan)
    }
    return @($phases | Where-Object {
        $_.Name -notin @("restore-persistent-editor", "restart-externals")
    })
}

function Get-ColdPhaseList($Plan) {
    return @(Get-PhaseList $Plan | Where-Object {
        $_.Name -notin @("restore-persistent-editor", "restart-externals")
    })
}

function Get-FinalizationPhaseList($Plan) {
    return @(Get-PhaseList $Plan | Where-Object {
        $_.Name -in @("restore-persistent-editor", "restart-externals")
    })
}

function Add-PhaseNamespace(
    [Parameter()][AllowEmptyCollection()][array]$Phases,
    [Parameter(Mandatory)][string]$Prefix
) {
    return @($Phases | ForEach-Object {
        $copy = @{}
        foreach ($key in $_.Keys) { $copy[$key] = $_[$key] }
        $copy.Name = "$Prefix$($_.Name)"
        $copy
    })
}

function Reset-FinalizationState($Run, [string]$Prefix = "") {
    $names = @("${Prefix}restore-persistent-editor", "${Prefix}restart-externals")
    $Run.State.phases = @($Run.State.phases | Where-Object {
        $_.name -notin $names
    })
    Save-UERunState $Run
}

function Require-SourceDevelopmentClosure($Run, [string]$Reason) {
    $previousReason = $Run.State.developmentClosureReason
    $Run.State.developmentClosure = "rerun-required"
    $Run.State | Add-Member -NotePropertyName developmentClosureReason `
        -NotePropertyValue $Reason -Force
    if ($previousReason -ne $Reason) {
        $Run.State.notes += "$(Get-Date -Format 'o') development closure: $Reason"
    }
    Save-UERunState $Run
}

function Stop-RestoredPersistentEditor($Run, [string]$Prefix = "") {
    $plan = Read-UEJson (Join-Path $Run.Dir "plan.json")
    if (-not $plan.persistentEditorWasRunning) { return }
    $restoreName = "${Prefix}restore-persistent-editor"
    $wasRestored = @($Run.State.phases | Where-Object {
        $_.name -eq $restoreName -and $_.status -eq "ok"
    }).Count -gt 0
    if (-not $wasRestored) { return }

    & (Join-Path $RepoRoot "scripts\ue\test\unit\persistent_editor_stop.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "persistent editor stop failed before cold-closure rerun"
    }
    $global:LASTEXITCODE = 0
}

function Stop-SourceChildPersistentEditor($Run) {
    if (-not $Run.State.persistentEditorWasRunning) { return }
    $pidFile = Join-Path $RepoRoot `
        "scripts\ue\artifacts\persistent\editor.pid"
    if (-not (Test-Path $pidFile)) { return }
    $editorPid = (Get-Content $pidFile | Select-Object -First 1).ToString().Trim()
    if (-not (Get-Process -Id $editorPid -ErrorAction SilentlyContinue)) { return }

    & (Join-Path $RepoRoot "scripts\ue\test\unit\persistent_editor_stop.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "persistent editor stop failed before source cold gates"
    }
    $global:LASTEXITCODE = 0
}

function Get-FingerprintChangeSummary($Before, $After) {
    $reasons = @()
    if (-not (Test-UEFingerprintSchema $Before)) {
        return @("checkpoint missing or stale")
    }
    if ($Before.Head -ne $After.Head) { $reasons += "HEAD" }
    if ($Before.SubmoduleStatusHash -ne $After.SubmoduleStatusHash) {
        $reasons += "submodules"
    }
    $reasons += @(Get-UEChangedPathsSince -Baseline $Before -Current $After)
    return @($reasons | Sort-Object -Unique)
}

function Get-SourceActivationPhase {
    param(
        [Parameter(Mandatory)]$Run,
        [Parameter(Mandatory)]$Plan,
        [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$ActivationRepoRoot,
        [Parameter(Mandatory)][ValidateSet("User", "Process")]
        [string]$ActivationEnvScope
    )
    $activate = {
        try {
            Write-Host "activating verified source root..." -ForegroundColor Cyan
            $env:UE_PATH = $Plan.launcherRoot
            $env:UE_SOURCE_PATH = $Plan.sourceRoot
            Invoke-UEDeterministicMutations -Run $Run `
                -RepoRoot $ActivationRepoRoot `
                -LauncherRoot $Plan.launcherRoot -SourceRoot $Plan.sourceRoot `
                -PreviousLauncherRoot $Plan.previousLauncherRoot `
                -PreviousSourceRoot $Plan.previousSourceRoot `
                -EnvScope $ActivationEnvScope
        } catch {
            $activationError = $_.Exception.Message
            try {
                Invoke-UERollback -Run $Run -RepoRoot $ActivationRepoRoot `
                    -EnvScope $ActivationEnvScope `
                    -AllowIncompleteManagedState | Out-Null
            } catch {
                throw ("source activation failed ($activationError); rollback " +
                    "also failed: $($_.Exception.Message)")
            }
            throw ("source activation failed and prior SOT state was restored: " +
                $activationError)
        }
    }.GetNewClosure()
    return @(@{ Name = "source-activation"; Kind = "mutation"
        ResumeHint = "verified source-root activation failed."
        Run = $activate })
}

function Invoke-RunPipeline($Run) {
    $plan = Read-UEJson (Join-Path $Run.Dir "plan.json")
    $resumeCmd = "scripts/ue/update/update_engine.ps1 -Resume -RunId $($Run.Id)"
    $before = Get-WorkspaceFingerprint $RepoRoot
    $closureInvalidated = $Run.State.lastVerificationFingerprint -and
        (-not (Compare-UEFingerprint $Run.State.lastVerificationFingerprint $before))
    if ($closureInvalidated) {
        Stop-RestoredPersistentEditor $Run
        Reset-FinalizationState $Run
    }
    $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
        -Phases (Get-ColdPhaseList $plan) -ResumeCommand $resumeCmd `
        -ForceRerun:$closureInvalidated
    if (-not $result.Stopped) {
        $finalizationPhases = @(Get-FinalizationPhaseList $plan)
        if ($finalizationPhases.Count -gt 0) {
            $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
                -Phases $finalizationPhases -ResumeCommand $resumeCmd
        }
    }
    if (-not $result.Stopped) {
        $final = Get-WorkspaceFingerprint $RepoRoot
        if (-not (Compare-UEFingerprint `
                $Run.State.lastVerificationFingerprint $final)) {
            $changes = Get-FingerprintChangeSummary `
                $Run.State.lastVerificationFingerprint $final
            Stop-RestoredPersistentEditor $Run
            Reset-FinalizationState $Run
            Set-UERunState $Run "BLOCKED" ("finalization changed the tracked " +
                "workspace: " + ($changes -join ", ") + ". Resume to rerun " +
                "the cold closure.")
            Write-Host "Finalization changed tracked state; cold closure must rerun." `
                -ForegroundColor Yellow
            exit 3
        }
        $Run.State.completionFingerprint = $final
        Save-UERunState $Run
        Set-UERunState $Run "DEV_COMPLETE" ("all dev gates passed on " +
            "$($plan.launcherRoot). Source release migration: " +
            "FROZEN - UE_SOURCE_PATH is owned by the -CompleteSource child")
        Write-Host ""
        Write-Host "Development engine update: COMPLETE ($($Run.Id))" -ForegroundColor Green
        Write-Host "Source release migration: FROZEN until -CompleteSource passes." -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "Run $($Run.Id) stopped in state $($result.State) at phase '$($result.Phase)'." -ForegroundColor Yellow
        Write-Host "Status: scripts/ue/update/update_engine.ps1 -Status -RunId $($Run.Id)"
        exit 3
    }
}

function Invoke-SourceRunPipeline($Run) {
    $plan = Read-UEJson (Join-Path $Run.Dir "plan.json")
    $resumeCmd = "scripts/ue/update/update_engine.ps1 -Resume -RunId $($Run.Id)"
    $current = Get-WorkspaceFingerprint $RepoRoot
    $closureInvalidated = $Run.State.lastVerificationFingerprint -and
        (-not (Compare-UEFingerprint $Run.State.lastVerificationFingerprint $current))
    $developmentClosurePending =
        $Run.State.developmentClosure -eq "rerun-required"
    if ($closureInvalidated) {
        $reason = if ($developmentClosurePending -and
            $Run.State.developmentClosureReason -eq "finalization-drift") {
            "finalization-drift"
        } else { "source-child-repair" }
        Require-SourceDevelopmentClosure $Run $reason
        $developmentClosurePending = $true
    }
    $pipelineInvalidated = $closureInvalidated -or $developmentClosurePending
    $mustRunDevelopmentClosure =
        [bool]$plan.developmentClosureRequired -or $pipelineInvalidated
    $devPrefix = "dev-revalidation--"
    $activationComplete = @($Run.State.phases | Where-Object {
        $_.name -eq "source-activation" -and $_.status -eq "ok"
    }).Count -gt 0
    if (-not $activationComplete -or $pipelineInvalidated) {
        Stop-SourceChildPersistentEditor $Run
    }
    if ($pipelineInvalidated) {
        Reset-FinalizationState $Run
    }

    if ($mustRunDevelopmentClosure) {
        $devPlan = $plan.PSObject.Copy()
        $devPlan.completionKind = "development-revalidation"
        $devCold = Add-PhaseNamespace `
            -Phases @(Get-ColdPhaseList $devPlan) -Prefix $devPrefix
        $devResult = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
            -Phases $devCold -ResumeCommand $resumeCmd `
            -ForceRerun:$pipelineInvalidated
        if ($devResult.Stopped) {
            Write-Host "Source child stopped during development revalidation." `
                -ForegroundColor Yellow
            exit 3
        }
        $Run.State.developmentClosure = "rerun-complete"
        Save-UERunState $Run
    }

    $sourcePhases = @(Get-SourcePhaseList $plan)
    $postActivationNames = @("restore-launcher-editor-binaries")
    $preActivation = @($sourcePhases | Where-Object {
        $_.Name -notin $postActivationNames
    })
    $postActivation = @($sourcePhases | Where-Object {
        $_.Name -in $postActivationNames
    })
    $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
        -Phases $preActivation -ResumeCommand $resumeCmd `
        -ForceRerun:$pipelineInvalidated
    if (-not $result.Stopped) {
        $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
            -Phases (Get-SourceActivationPhase -Run $Run -Plan $plan `
                -ActivationRepoRoot $RepoRoot `
                -ActivationEnvScope $EnvScope) `
            -ResumeCommand $resumeCmd -ForceRerun:$pipelineInvalidated
    }
    if (-not $result.Stopped -and $postActivation.Count -gt 0) {
        $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
            -Phases $postActivation -ResumeCommand $resumeCmd `
            -ForceRerun:$pipelineInvalidated
    }
    if (-not $result.Stopped) {
        $Run.State.lastVerificationFingerprint = $Run.State.managedStateFingerprint
        Save-UERunState $Run
        $finalization = @(Get-FinalizationPhaseList $plan)
        if ($finalization.Count -gt 0) {
            $result = Invoke-UEPhaseList -Run $Run -RepoRoot $RepoRoot `
                -Phases $finalization -ResumeCommand $resumeCmd
        }
    }
    if (-not $result.Stopped) {
        $final = Get-WorkspaceFingerprint $RepoRoot
        if (-not (Compare-UEFingerprint `
                $Run.State.lastVerificationFingerprint $final)) {
            $changes = Get-FingerprintChangeSummary `
                $Run.State.lastVerificationFingerprint $final
            Stop-SourceChildPersistentEditor $Run
            Require-SourceDevelopmentClosure $Run "finalization-drift"
            Reset-FinalizationState $Run
            Set-UERunState $Run "BLOCKED" ("source finalization changed the " +
                "tracked workspace: " + ($changes -join ", "))
            Write-Host "Source finalization changed tracked state; rerun required." `
                -ForegroundColor Yellow
            exit 3
        }
        $Run.State.completionFingerprint = Get-WorkspaceFingerprint $RepoRoot
        Save-UERunState $Run
        Set-UERunState $Run "SOURCE_COMPLETE" `
            ("source package gates and launcher editor-binary restoration " +
                "passed")
        Write-Host ""
        Write-Host "Source engine update: COMPLETE ($($Run.Id))" -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "Run $($Run.Id) stopped in state $($result.State) at phase '$($result.Phase)'." -ForegroundColor Yellow
        Write-Host "Status: scripts/ue/update/update_engine.ps1 -Status -RunId $($Run.Id)"
        exit 3
    }
}

$lock = Enter-UEUpdateLock $RepoRoot
try {
    if ($Status) {
        $runs = Get-UERuns $RepoRoot
        if ($RunId) { $runs = @($runs | Where-Object { $_.Id -eq $RunId }) }
        if (-not $runs) { Write-Host "no runs recorded."; exit 0 }
        foreach ($r in $runs) {
            Write-Host "$($r.Id): $($r.State.state)"
            foreach ($p in $r.State.phases) {
                Write-Host ("  [{0}] {1}" -f $p.status, $p.name)
            }
            foreach ($n in ($r.State.notes | Select-Object -Last 3)) {
                Write-Host "  note: $n"
            }
        }
        exit 0
    }

    if ($Rollback) {
        $run = Select-UERun -RepoRoot $RepoRoot -RunId $RunId
        $liveChildren = @(Get-UERuns $RepoRoot | Where-Object {
            $_.State.parentRunId -eq $run.Id -and
            $_.State.state -ne "ROLLED_BACK"
        })
        if ($liveChildren.Count -gt 0) {
            throw ("run $($run.Id) has an active source child: " +
                (($liveChildren | ForEach-Object { $_.Id + '=' + $_.State.state }) -join ', ') +
                ". Roll back the child first.")
        }
        $semantic = Invoke-UERollback -Run $run -RepoRoot $RepoRoot -EnvScope $EnvScope
        Write-Host "Deterministic engine state restored ($($run.Id))." -ForegroundColor Green
        if ($semantic.Count -gt 0) {
            Write-Host "Semantic migration changes remain in:" -ForegroundColor Yellow
            $semantic | ForEach-Object { Write-Host "  $_" }
            Write-Host "Previous-engine rebuild NOT attempted. Revert or adapt those"
            Write-Host "changes, then run: scripts/ue/standalone/build.ps1"
        } else {
            Write-Host "No semantic changes since -Apply. Project files regenerated."
            Write-Host "Rebuild with: scripts/ue/standalone/build.ps1"
        }
        exit 0
    }

    if ($Resume) {
        $run = Select-UERun -RepoRoot $RepoRoot -RunId $RunId
        if (@("APPLYING", "BLOCKED", "ACTION_REQUIRED") -notcontains $run.State.state) {
            throw "run $($run.Id) is $($run.State.state) - nothing to resume"
        }
        $plan = Read-UEJson (Join-Path $run.Dir "plan.json")
        # A resumed process may inherit the pre-update cache from its parent.
        # Bind this process to the immutable run plan before any resolver runs.
        $env:UE_PATH = $plan.launcherRoot
        if ($plan.sourceRoot) { $env:UE_SOURCE_PATH = $plan.sourceRoot }
        Set-UERunState $run "APPLYING" "resumed"
        # If the deterministic mutations never completed (crash mid-apply:
        # managed-state checkpoint missing), rerun them first - they are
        # idempotent and the roots come from the recorded plan.
        if (-not $run.State.managedStateFingerprint -and
            $plan.completionKind -ne "source") {
            Write-Host "re-applying deterministic mutations (no managed-state checkpoint)..." -ForegroundColor Cyan
            Invoke-UEDeterministicMutations -Run $run -RepoRoot $RepoRoot `
                -LauncherRoot $plan.launcherRoot -SourceRoot $plan.sourceRoot `
                -PreviousLauncherRoot $plan.previousLauncherRoot `
                -PreviousSourceRoot $plan.previousSourceRoot `
                -EnvScope $EnvScope
        }
        if ($plan.completionKind -eq "source") {
            Invoke-SourceRunPipeline $run
        } else {
            Invoke-RunPipeline $run
        }
        exit 0
    }

    if ($CompleteSource) {
        if (-not $RunId) { throw "-CompleteSource requires -RunId <dev-run-id>" }
        if (-not $SourceRoot) { throw "-CompleteSource requires -SourceRoot" }
        $parent = Select-UERun -RepoRoot $RepoRoot -RunId $RunId
        if ($parent.State.state -ne "DEV_COMPLETE") {
            throw "source completion requires a DEV_COMPLETE parent; $RunId is $($parent.State.state)"
        }
        $activeRuns = @(Get-UENonTerminalRuns $RepoRoot)
        if ($activeRuns.Count -gt 0) {
            throw ("a nonterminal update run already exists: " +
                (($activeRuns | ForEach-Object { $_.Id + '=' + $_.State.state }) -join ', ') +
                ". Resume or roll it back before creating a source child.")
        }
        $existing = @(Get-UERuns $RepoRoot | Where-Object {
            $_.State.parentRunId -eq $parent.Id -and
            $_.State.state -ne "ROLLED_BACK"
        })
        if ($existing.Count -gt 0) {
            throw ("source child already exists for $($parent.Id): " +
                (($existing | ForEach-Object { $_.Id + '=' + $_.State.state }) -join ', ') +
                ". Resume or inspect that run instead of creating another.")
        }

        $parentPlan = Read-UEJson (Join-Path $parent.Dir "plan.json")
        $lineageProblems = @()
        try {
            . (Join-Path $RepoRoot "scripts\config\Resolve-UEConfig.ps1")
            $currentConfig = Resolve-UEConfig `
                -ConfigDir (Join-Path $RepoRoot "scripts\config")
            $expectedRoot = [IO.Path]::GetFullPath(
                [string]$parentPlan.launcherRoot).TrimEnd('\', '/')
            $resolvedRoot = [IO.Path]::GetFullPath(
                [string]$currentConfig.UE_PATH).TrimEnd('\', '/')
            if (-not $resolvedRoot.Equals($expectedRoot,
                    [StringComparison]::OrdinalIgnoreCase)) {
                $lineageProblems += ("current resolved UE_PATH '$resolvedRoot' " +
                    "does not match parent launcher '$expectedRoot'")
            }
            $resolvedInfo = Get-UEEngineVersionInfo $resolvedRoot
            if ($resolvedInfo.Line -ne $parentPlan.launcherVersion.Line) {
                $lineageProblems += ("current UE_PATH line $($resolvedInfo.Line) " +
                    "does not match parent line $($parentPlan.launcherVersion.Line)")
            }
        } catch {
            $lineageProblems += "cannot resolve current UE_PATH lineage: $($_.Exception.Message)"
        }
        $association = Get-UEProjectAssociation (Join-Path $RepoRoot "Alis.uproject")
        if ($association -ne $parentPlan.launcherVersion.Line) {
            $lineageProblems += ("Alis.uproject association $association does not " +
                "match parent line $($parentPlan.launcherVersion.Line)")
        }
        try {
            $sourceLine = (Get-UEEngineVersionInfo $SourceRoot).Line
            if ($sourceLine -ne $parentPlan.launcherVersion.Line) {
                $lineageProblems += ("source line $sourceLine does not match parent " +
                    "line $($parentPlan.launcherVersion.Line)")
            }
        } catch {
            $lineageProblems += $_.Exception.Message
        }
        if ($lineageProblems.Count -gt 0) {
            Write-Host "LINEAGE BLOCKERS:" -ForegroundColor Red
            $lineageProblems | ForEach-Object {
                Write-Host "  $_" -ForegroundColor Red
            }
            exit 2
        }

        $currentFingerprint = Get-WorkspaceFingerprint $RepoRoot
        $parentCompletion = $parent.State.completionFingerprint
        $developmentClosureRequired = (-not (Test-UEFingerprintSchema `
            $parentCompletion)) -or (-not (Compare-UEFingerprint `
            $parentCompletion $currentFingerprint))

        $parentVersion = ("{0}.{1}.{2}" -f $parentPlan.launcherVersion.Major,
            $parentPlan.launcherVersion.Minor, $parentPlan.launcherVersion.Patch)
        $pf = Invoke-UEPreflight -RepoRoot $RepoRoot `
            -LauncherRoot $parentPlan.launcherRoot -SourceRoot $SourceRoot `
            -ExpectedVersion $parentVersion -SkipProcessCheck:$SkipProcessCheck `
            -AllowDirtyManaged:$AllowDirtyManaged
        $pf.Blockers += @(Get-UESourceCandidateProblems `
            -SourceRoot $SourceRoot -ExpectedLine $parentPlan.launcherVersion.Line)

        Write-Host "=== source completion plan ===" -ForegroundColor Cyan
        Write-Host "parent: $($parent.Id) ($($parent.State.state))"
        Write-Host "source: $SourceRoot"
        Write-Host ("development closure: " + $(if ($developmentClosureRequired) {
            "rerun in immutable-parent source child"
        } else { "reuse parent completion checkpoint" }))
        foreach ($n in $pf.Notes) { Write-Host "  $n" -ForegroundColor Yellow }
        if ($pf.Blockers.Count -gt 0) {
            Write-Host "BLOCKERS:" -ForegroundColor Red
            $pf.Blockers | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
            exit 2
        }

        $pf.Plan.completionKind = "source"
        $pf.Plan.parentRunId = $parent.Id
        $pf.Plan.parentCompletionFingerprint = $parentCompletion
        $pf.Plan.sourceRequestFingerprint = $currentFingerprint
        $pf.Plan.developmentClosureRequired = $developmentClosureRequired
        $pf.Plan.sourcePackageOutput = Join-Path $RepoRoot `
            "Saved\PackageRelease\EngineUpdateSource\$($parent.Id)"
        $baseline = Get-WorkspaceFingerprint $RepoRoot
        $run = New-UERun -RepoRoot $RepoRoot -Plan $pf.Plan `
            -BaselineFingerprint $baseline -ParentRunId $parent.Id
        $run.State.parentCompletionFingerprint = $parentCompletion
        $run.State.sourceRequestFingerprint = $currentFingerprint
        $run.State.developmentClosure = if ($developmentClosureRequired) {
            "rerun-required"
        } else { "parent-reused" }
        $run.State.developmentClosureReason = if ($developmentClosureRequired) {
            "parent-drift"
        } else { $null }
        Save-UERunState $run

        # A source child owns its cold process lifecycle even when it reuses
        # the completed development checkpoint.
        if ($pf.Plan.persistentEditorPid) {
            $run.State.persistentEditorWasRunning = $true
            Save-UERunState $run
            Write-Host ("stopping persistent editor (pid " +
                "$($pf.Plan.persistentEditorPid)) before source gates...") `
                -ForegroundColor Cyan
            & (Join-Path $RepoRoot `
                "scripts\ue\test\unit\persistent_editor_stop.ps1")
            if ($LASTEXITCODE -ne 0) {
                Set-UERunState $run "BLOCKED" `
                    "failed to stop persistent editor before source gates"
                throw "failed to stop the persistent editor; run is BLOCKED."
            }
        }

        Invoke-SourceRunPipeline $run
        exit 0
    }

    # ---- dry-run / -Apply ----
    if (-not $LauncherRoot) {
        throw "provide -LauncherRoot, or use -CompleteSource/-Status/-Resume/-Rollback"
    }
    if ($SourceRoot) {
        throw "-SourceRoot is valid only with -CompleteSource after DEV_COMPLETE"
    }

    $nonTerminal = Get-UENonTerminalRuns $RepoRoot
    if ($nonTerminal.Count -gt 0) {
        $list = ($nonTerminal | ForEach-Object { "$($_.Id)=$($_.State.state)" }) -join ", "
        throw ("a nonterminal update run exists ($list). Finish it with " +
               "-Resume/-Rollback before starting a new -Apply.")
    }

    $pf = Invoke-UEPreflight -RepoRoot $RepoRoot -LauncherRoot $LauncherRoot `
        -SourceRoot $SourceRoot -ExpectedVersion $ExpectedVersion `
        -SkipProcessCheck:$SkipProcessCheck -AllowDirtyManaged:$AllowDirtyManaged

    Write-Host "=== engine update plan ===" -ForegroundColor Cyan
    Write-Host ("launcher: {0} (Build.version {1}.{2}.{3})" -f $LauncherRoot,
        $pf.Plan.launcherVersion.Major, $pf.Plan.launcherVersion.Minor,
        $pf.Plan.launcherVersion.Patch)
    if ($SourceRoot) {
        Write-Host ("source:   {0} (Build.version {1}.{2}.{3})" -f $SourceRoot,
            $pf.Plan.sourceVersion.Major, $pf.Plan.sourceVersion.Minor,
            $pf.Plan.sourceVersion.Patch)
    }
    foreach ($c in $pf.Plan.changes) {
        Write-Host ("  {0} : {1} '{2}' -> '{3}'" -f $c.file, $c.key, $c.from, $c.to)
    }
    Write-Host "  + $($pf.Plan.machineLocalSync)"
    foreach ($n in $pf.Notes) { Write-Host "  $n" -ForegroundColor Yellow }
    if ($pf.Blockers.Count -gt 0) {
        Write-Host "BLOCKERS:" -ForegroundColor Red
        $pf.Blockers | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        exit 2
    }

    if (-not $Apply) {
        Write-Host ""
        Write-Host "dry-run only - no run created. Add -Apply to execute." -ForegroundColor Yellow
        exit 0
    }

    $baseline = Get-WorkspaceFingerprint $RepoRoot
    $run = New-UERun -RepoRoot $RepoRoot -Plan $pf.Plan -BaselineFingerprint $baseline

    # Persistent editor stops before mutation and is restored by the
    # pipeline after cold verification/package gates have finished.
    if ($pf.Plan.persistentEditorPid) {
        $run.State.persistentEditorWasRunning = $true
        Save-UERunState $run
        Write-Host "stopping persistent editor (pid $($pf.Plan.persistentEditorPid))..." -ForegroundColor Cyan
        & (Join-Path $RepoRoot "scripts\ue\test\unit\persistent_editor_stop.ps1")
        if ($LASTEXITCODE -ne 0) {
            Set-UERunState $run "BLOCKED" "failed to stop persistent editor - stop it manually, then -Resume"
            throw "failed to stop the persistent editor; run is BLOCKED."
        }
    }

    Write-Host "applying deterministic mutations..." -ForegroundColor Cyan
    Invoke-UEDeterministicMutations -Run $run -RepoRoot $RepoRoot `
        -LauncherRoot $LauncherRoot -SourceRoot $SourceRoot `
        -PreviousLauncherRoot $pf.Plan.previousLauncherRoot `
        -PreviousSourceRoot $pf.Plan.previousSourceRoot -EnvScope $EnvScope

    Invoke-RunPipeline $run
    exit 0
}
finally {
    Exit-UEUpdateLock $lock
}
