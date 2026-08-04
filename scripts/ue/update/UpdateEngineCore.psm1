# UpdateEngineCore.psm1 - state machine for the engine-update orchestrator.
#
# Owns: OS-handle locking + admission, run manifests under
# Saved/EngineUpdate/<run-id>/, two-checkpoint workspace fingerprints,
# deterministic SOT mutations (conf / uproject / local.conf), honest
# rollback, and the generic phase engine. Phase LISTS live in
# UpdateEnginePhases.psm1; this module never hardcodes pipeline content.
#
# Operator contract: docs/ue_engine/version_update.md.

$script:NonTerminalStates = @("APPLYING", "BLOCKED", "ACTION_REQUIRED")

# ---------------------------------------------------------------- utils --

function Get-UESha256 {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))
        return ([BitConverter]::ToString($bytes) -replace '-', '').ToLower()
    } finally { $sha.Dispose() }
}

. (Join-Path $PSScriptRoot "UpdateEngineFingerprint.ps1")

function Save-UEJson {
    # Atomic, BOM-less manifest writes.
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)]$Object)
    $tmp = "$Path.tmp-$([IO.Path]::GetRandomFileName())"
    [IO.File]::WriteAllText($tmp, ($Object | ConvertTo-Json -Depth 16),
        (New-Object System.Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $tmp -Destination $Path -Force
}

function Read-UEJson {
    param([Parameter(Mandatory)][string]$Path)
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-UEUpdateRoot {
    param([Parameter(Mandatory)][string]$RepoRoot)
    Join-Path $RepoRoot "Saved\EngineUpdate"
}

# ----------------------------------------------------------------- lock --

function Enter-UEUpdateLock {
    # OS-exclusive handle held for the invocation. PID/time/command in
    # the file are DIAGNOSTICS only - never a deletion trigger; stale
    # metadata is reused only after this handle is acquired.
    param([Parameter(Mandatory)][string]$RepoRoot)
    $root = Get-UEUpdateRoot $RepoRoot
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $lockPath = Join-Path $root "update.lock"
    try {
        $stream = [IO.File]::Open($lockPath,
            [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite,
            [IO.FileShare]::None)
    } catch [System.IO.IOException] {
        throw ("another engine-update invocation holds the lock " +
               "($lockPath). Wait for it or check its process; the lock " +
               "releases when that process exits.")
    }
    $proc = Get-Process -Id $PID
    $diag = "pid=$PID start=$($proc.StartTime.ToString('o')) cmd=$($MyInvocation.Line)"
    $bytes = [Text.Encoding]::UTF8.GetBytes($diag)
    $stream.SetLength(0)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    return @{ Stream = $stream; Path = $lockPath }
}

function Exit-UEUpdateLock {
    param([Parameter(Mandatory)]$Lock)
    if ($Lock -and $Lock.Stream) { $Lock.Stream.Dispose() }
}

# --------------------------------------------------------- engine info --

function Get-UEEngineVersionInfo {
    param([Parameter(Mandatory)][string]$Root)
    $bv = Join-Path $Root "Engine\Build\Build.version"
    if (-not (Test-Path -LiteralPath $bv)) {
        throw "not an engine root (no Engine/Build/Build.version): $Root"
    }
    $v = Read-UEJson $bv
    $branch = [string]$v.BranchName
    $changelist = [Int64]$v.Changelist
    $buildId = if ($branch) {
        "$branch-CL-$changelist"
    } else {
        "UE$($v.MajorVersion).$($v.MinorVersion)-CL-$changelist"
    }
    return @{
        Major = [int]$v.MajorVersion
        Minor = [int]$v.MinorVersion
        Patch = [int]$v.PatchVersion
        Line = "$($v.MajorVersion).$($v.MinorVersion)"
        Changelist = $changelist
        Branch = $branch
        BuildId = $buildId
    }
}

function Get-UEProjectAssociation {
    param([Parameter(Mandatory)][string]$UProjectPath)
    $raw = Get-Content -LiteralPath $UProjectPath -Raw
    if ($raw -match '"EngineAssociation"\s*:\s*"([^"]*)"') { return $Matches[1] }
    return $null
}

# ----------------------------------------------------------------- runs --

function Get-UERuns {
    param([Parameter(Mandatory)][string]$RepoRoot)
    $root = Get-UEUpdateRoot $RepoRoot
    if (-not (Test-Path $root)) { return @() }
    $runs = @()
    foreach ($dir in (Get-ChildItem $root -Directory -Filter "run-*" |
            Sort-Object Name)) {
        $statePath = Join-Path $dir.FullName "state.json"
        if (Test-Path $statePath) {
            $state = Read-UEJson $statePath
            $runs += @{ Id = $dir.Name; Dir = $dir.FullName; State = $state }
        }
    }
    return $runs
}

function Get-UENonTerminalRuns {
    param([Parameter(Mandatory)][string]$RepoRoot)
    return @(Get-UERuns $RepoRoot |
        Where-Object { $script:NonTerminalStates -contains $_.State.state })
}

function Select-UERun {
    # -RunId or exactly one nonterminal candidate; otherwise fail + list.
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter()][string]$RunId,
        [Parameter()][string[]]$AcceptStates
    )
    $runs = Get-UERuns $RepoRoot
    if ($RunId) {
        $run = $runs | Where-Object { $_.Id -eq $RunId }
        if (-not $run) { throw "run not found: $RunId" }
        return $run
    }
    $candidates = @($runs | Where-Object {
        if ($AcceptStates) { $AcceptStates -contains $_.State.state }
        else { $script:NonTerminalStates -contains $_.State.state } })
    if ($candidates.Count -eq 1) { return $candidates[0] }
    if ($candidates.Count -eq 0) { throw "no candidate run; pass -RunId. Known runs: $(($runs | ForEach-Object { $_.Id + '=' + $_.State.state }) -join ', ')" }
    throw ("ambiguous - pass -RunId. Candidates: " +
        (($candidates | ForEach-Object { $_.Id + "=" + $_.State.state }) -join ", "))
}

function New-UERun {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)]$Plan,
        [Parameter(Mandatory)]$BaselineFingerprint,
        [Parameter()][string]$ParentRunId
    )
    $id = "run-" + (Get-Date -Format "yyyyMMdd-HHmmss")
    $dir = Join-Path (Get-UEUpdateRoot $RepoRoot) $id
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $dir "backups") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $dir "logs") -Force | Out-Null
    Save-UEJson (Join-Path $dir "plan.json") $Plan
    $state = @{
        state = "APPLYING"
        created = (Get-Date -Format "o")
        baselineFingerprint = $BaselineFingerprint
        managedStateFingerprint = $null
        lastVerificationFingerprint = $null
        completionFingerprint = $null
        phases = @()
        notes = @()
        persistentEditorWasRunning = $false
        parentRunId = $ParentRunId
    }
    Save-UEJson (Join-Path $dir "state.json") $state
    return @{ Id = $id; Dir = $dir; State = $state }
}

function Save-UERunState {
    param([Parameter(Mandatory)]$Run)
    Save-UEJson (Join-Path $Run.Dir "state.json") $Run.State
}

function Set-UERunState {
    param(
        [Parameter(Mandatory)]$Run,
        [Parameter(Mandatory)][string]$State,
        [Parameter()][string]$Note
    )
    $Run.State.state = $State
    if ($Note) { $Run.State.notes += "$(Get-Date -Format 'o') $Note" }
    Save-UERunState $Run
}

# ------------------------------------------------------------ preflight --

function Invoke-UEPreflight {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$LauncherRoot,
        [Parameter()][string]$SourceRoot,
        [Parameter()][string]$ExpectedVersion,
        [Parameter()][switch]$SkipProcessCheck,
        [Parameter()][switch]$AllowDirtyManaged
    )
    $configDir = Join-Path $RepoRoot "scripts\config"
    $confPath = Join-Path $configDir "ue_path.conf"
    $localConfPath = Join-Path $configDir "ue_path.local.conf"
    $uprojectPath = Join-Path $RepoRoot "Alis.uproject"
    $devManifestPath = Join-Path $RepoRoot `
        "Plugins\Boot\Orchestrator\Data\dev_manifest.json"
    $blockers = @()
    $notes = @()

    # Engine roots + derived versions (Build.version is the authority)
    $launcherInfo = $null; $sourceInfo = $null
    try {
        $launcherInfo = Get-UEEngineVersionInfo $LauncherRoot
        $edCmd = Join-Path $LauncherRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
        if (-not (Test-Path $edCmd)) {
            $blockers += "launcher root has no UnrealEditor-Cmd.exe: $LauncherRoot"
        }
    } catch { $blockers += $_.Exception.Message }
    if ($SourceRoot) {
        try {
            $sourceInfo = Get-UEEngineVersionInfo $SourceRoot
            if ($launcherInfo -and $sourceInfo.Line -ne $launcherInfo.Line) {
                $blockers += ("source engine line $($sourceInfo.Line) does not " +
                    "match launcher line $($launcherInfo.Line) (Major.Minor hard fail)")
            } elseif ($launcherInfo -and $sourceInfo.Patch -ne $launcherInfo.Patch) {
                $notes += ("WARN: source patch $($sourceInfo.Patch) differs from " +
                    "launcher patch $($launcherInfo.Patch) - allowed; source gate " +
                    "proves compatibility (see docs/ue_engine/version_update.md)")
            }
        } catch { $blockers += $_.Exception.Message }
    }
    if ($ExpectedVersion -and $launcherInfo) {
        $actual = "$($launcherInfo.Major).$($launcherInfo.Minor).$($launcherInfo.Patch)"
        if ($actual -ne $ExpectedVersion) {
            $blockers += "-ExpectedVersion $ExpectedVersion but launcher Build.version is $actual"
        }
    }

    # Current SOT values
    . (Join-Path $configDir "Resolve-UEConfig.ps1")
    $conf = $null
    try { $conf = Resolve-UEConfig -ConfigDir $configDir }
    catch { $blockers += "conf grammar error: $($_.Exception.Message)" }
    $assoc = Get-UEProjectAssociation $uprojectPath

    # Managed tracked files must not carry unrelated uncommitted edits
    # (EAP local-continue: git warnings on stderr are not errors)
    $prevEap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $dirty = & git -C $RepoRoot status --porcelain -- `
        "scripts/config/ue_path.conf" "Alis.uproject" `
        "Plugins/Boot/Orchestrator/Data/dev_manifest.json" 2>$null
    $ErrorActionPreference = $prevEap
    if ($dirty) {
        if ($AllowDirtyManaged) {
            $notes += ("WARN: managed files carry uncommitted edits, admitted " +
                "via -AllowDirtyManaged (backups capture pre-apply state): " +
                (($dirty | ForEach-Object { $_.Trim() }) -join "; "))
        } else {
            $blockers += ("managed files have uncommitted edits (commit or " +
                "revert first, or pass -AllowDirtyManaged when the edits are " +
                "intentionally part of this update session): " +
                (($dirty | ForEach-Object { $_.Trim() }) -join "; "))
        }
    }

    # Plugin pin inventory (report-only; expected empty per plan)
    $pins = @()
    $pluginsDir = Join-Path $RepoRoot "Plugins"
    if (Test-Path $pluginsDir) {
        foreach ($up in (Get-ChildItem $pluginsDir -Recurse -Filter "*.uplugin" -ErrorAction SilentlyContinue)) {
            $c = Get-Content $up.FullName -Raw
            if ($c -match '"EngineVersion"\s*:\s*"([^"]*)"') {
                $pins += @{ Plugin = $up.FullName; Pin = $Matches[1] }
            }
        }
    }
    if ($pins.Count -gt 0) {
        $notes += "REPORT: $($pins.Count) .uplugin EngineVersion pin(s) found (never auto-rewritten)"
    }

    # Editor / UBT processes holding the project (locked DLLs, UBT mutex)
    $persistentPid = $null
    $pidFile = Join-Path $RepoRoot "scripts\ue\artifacts\persistent\editor.pid"
    if (Test-Path $pidFile) {
        $persistentPid = (Get-Content $pidFile | Select-Object -First 1).ToString().Trim()
        if (-not (Get-Process -Id $persistentPid -ErrorAction SilentlyContinue)) {
            $persistentPid = $null
        }
    }
    if (-not $SkipProcessCheck) {
        $procFilter = "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe' OR Name = 'dotnet.exe'"
        $procs = Get-CimInstance Win32_Process -Filter $procFilter |
            Where-Object { $_.CommandLine -match [regex]::Escape($RepoRoot) -or
                           ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool') }
        foreach ($p in $procs) {
            if ("$($p.ProcessId)" -eq "$persistentPid") { continue }
            $blockers += ("process holds the project (stop it first): " +
                "$($p.Name) pid=$($p.ProcessId)")
        }
    }

    $manifestBuildId = $null
    try {
        $manifestBuildId = [string](Read-UEJson $devManifestPath).engine_build_id
    } catch {
        $blockers += "invalid dev manifest engine pin: $($_.Exception.Message)"
    }

    $plan = @{
        launcherRoot = $LauncherRoot
        sourceRoot = $SourceRoot
        previousLauncherRoot = $conf.UE_PATH
        previousSourceRoot = $conf.UE_SOURCE_PATH
        launcherVersion = $launcherInfo
        sourceVersion = $sourceInfo
        changes = @(
            @{ file = $confPath; key = "UE_PATH";
               from = $conf.UE_PATH; to = ($LauncherRoot -replace '\\', '/') }
        ) + $(if ($SourceRoot) { ,@{ file = $confPath; key = "UE_SOURCE_PATH";
               from = $conf.UE_SOURCE_PATH; to = ($SourceRoot -replace '\\', '/') } } else { @() }
        ) + @(
            @{ file = $uprojectPath; key = "EngineAssociation";
               from = $assoc; to = $launcherInfo.Line }
            @{ file = $devManifestPath; key = "engine_build_id";
               from = $manifestBuildId; to = $launcherInfo.BuildId }
        ) + $(if (Test-Path $localConfPath) { ,@{ file = $localConfPath;
               key = "(local override, per declared keys)"; from = "current"; to = "new roots" } } else { @() })
        machineLocalSync = "scripts/setup/setup_ue_env.ps1 (env, .vscode, settings.local.json, .mcp.json)"
        pluginPins = $pins
        persistentEditorPid = $persistentPid
        persistentEditorWasRunning = [bool]$persistentPid
    }
    return @{ Blockers = $blockers; Notes = $notes; Plan = $plan }
}

# ------------------------------------------------------------ mutations --

function Update-UEConfKey {
    # Regex line-level key rewrite preserving comments; validates after.
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Key,
        [Parameter(Mandatory)][string]$Value
    )
    $raw = Get-Content -LiteralPath $Path -Raw
    $pattern = "(?m)^$Key=.*$"
    if ($raw -match $pattern) {
        $new = [regex]::Replace($raw, $pattern, "$Key=$Value")
    } else {
        $new = $raw.TrimEnd("`r", "`n") + "`r`n$Key=$Value`r`n"
    }
    if ($new -ne $raw) {
        [IO.File]::WriteAllText($Path, $new,
            (New-Object System.Text.UTF8Encoding($false)))
    }
}

function Invoke-UEDeterministicMutations {
    # Backs up then rewrites the SOT files + invokes the machine-local
    # writer. Idempotent; each file backed up once per run.
    param(
        [Parameter(Mandatory)]$Run,
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$LauncherRoot,
        [Parameter()][string]$SourceRoot,
        [Parameter()][string]$PreviousLauncherRoot,
        [Parameter()][string]$PreviousSourceRoot,
        [Parameter()][ValidateSet("User", "Process")][string]$EnvScope = "User"
    )
    $configDir = Join-Path $RepoRoot "scripts\config"
    $confPath = Join-Path $configDir "ue_path.conf"
    $localConfPath = Join-Path $configDir "ue_path.local.conf"
    $uprojectPath = Join-Path $RepoRoot "Alis.uproject"
    $devManifestPath = Join-Path $RepoRoot `
        "Plugins\Boot\Orchestrator\Data\dev_manifest.json"
    $backups = Join-Path $Run.Dir "backups"

    $launcherInfo = Get-UEEngineVersionInfo $LauncherRoot
    $launcherConfValue = $LauncherRoot -replace '\\', '/'

    foreach ($f in @($confPath, $uprojectPath, $devManifestPath) + $(
            if (Test-Path $localConfPath) { @($localConfPath) } else { @() })) {
        $dest = Join-Path $backups (Split-Path -Leaf $f)
        if (-not (Test-Path $dest)) { Copy-Item $f $dest }
    }

    Update-UEConfKey -Path $confPath -Key "UE_PATH" -Value $launcherConfValue
    if ($SourceRoot) {
        Update-UEConfKey -Path $confPath -Key "UE_SOURCE_PATH" `
            -Value ($SourceRoot -replace '\\', '/')
    }
    if (Test-Path $localConfPath) {
        # Only rewrite keys the local override actually declares.
        $localRaw = Get-Content -LiteralPath $localConfPath -Raw
        if ($localRaw -match '(?m)^UE_PATH=') {
            Update-UEConfKey -Path $localConfPath -Key "UE_PATH" -Value $launcherConfValue
        }
        if ($SourceRoot -and $localRaw -match '(?m)^UE_SOURCE_PATH=') {
            Update-UEConfKey -Path $localConfPath -Key "UE_SOURCE_PATH" `
                -Value ($SourceRoot -replace '\\', '/')
        }
    }

    # Validate the mutated conf against the strict grammar immediately.
    . (Join-Path $configDir "Resolve-UEConfig.ps1")
    Resolve-UEConfig -ConfigDir $configDir | Out-Null

    # uproject association derives from Build.version (never a parameter)
    $raw = Get-Content -LiteralPath $uprojectPath -Raw
    $new = [regex]::Replace($raw,
        '"EngineAssociation"\s*:\s*"[^"]*"',
        ('"EngineAssociation": "' + $launcherInfo.Line + '"'))
    if ($new -ne $raw) {
        [IO.File]::WriteAllText($uprojectPath, $new,
            (New-Object System.Text.UTF8Encoding($false)))
    }

    # The checked-in dev manifest is a derived launcher-engine pin.
    $raw = Get-Content -LiteralPath $devManifestPath -Raw
    $new = [regex]::Replace($raw,
        '"engine_build_id"\s*:\s*"[^"]*"',
        ('"engine_build_id": "' + $launcherInfo.BuildId + '"'), 1)
    if ($new -ne $raw) {
        [IO.File]::WriteAllText($devManifestPath, $new,
            (New-Object System.Text.UTF8Encoding($false)))
    }

    # Machine-local derived state via its designated writer.
    $setup = Join-Path $RepoRoot "scripts\setup\setup_ue_env.ps1"
    & $setup -EnvScope $EnvScope -SkipVsCode:$false `
        -PreviousLauncherRoot $PreviousLauncherRoot `
        -PreviousSourceRoot $PreviousSourceRoot | Out-Null

    $Run.State.managedStateFingerprint = Get-WorkspaceFingerprint $RepoRoot
    Save-UERunState $Run
}

# ------------------------------------------------------------- rollback --

function Invoke-UERollback {
    param(
        [Parameter(Mandatory)]$Run,
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter()][ValidateSet("User", "Process")][string]$EnvScope = "User",
        [Parameter()][switch]$AllowIncompleteManagedState
    )
    $backups = Join-Path $Run.Dir "backups"
    $managed = @("ue_path.conf", "ue_path.local.conf", "Alis.uproject",
        "dev_manifest.json")
    $managedRel = @("scripts/config/ue_path.conf",
        "scripts/config/ue_path.local.conf", "Alis.uproject",
        "Plugins/Boot/Orchestrator/Data/dev_manifest.json")

    $plan = Read-UEJson (Join-Path $Run.Dir "plan.json")
    $current = Get-WorkspaceFingerprint $RepoRoot
    if (-not (Test-UEFingerprintSchema $Run.State.baselineFingerprint)) {
        Set-UERunState $Run "BLOCKED" `
            "rollback blocked: BASELINE uses a stale checkpoint schema"
        throw "rollback requires a current-schema BASELINE checkpoint"
    }
    if (-not $AllowIncompleteManagedState -and
        -not (Test-UEFingerprintSchema $Run.State.managedStateFingerprint)) {
        Set-UERunState $Run "BLOCKED" `
            "rollback blocked: MANAGED-STATE is missing or stale"
        throw "rollback requires a current-schema MANAGED-STATE checkpoint"
    }

    if ($Run.State.baselineFingerprint.Head -ne $current.Head) {
        Set-UERunState $Run "BLOCKED" `
            "rollback blocked: repository HEAD changed after BASELINE"
        throw "rollback cannot cross a repository HEAD change"
    }
    if ($Run.State.baselineFingerprint.SubmoduleStatusHash -ne
        $current.SubmoduleStatusHash) {
        Set-UERunState $Run "BLOCKED" `
            "rollback blocked: submodule state changed after BASELINE"
        throw "rollback cannot cross a submodule-state change"
    }

    if (-not $AllowIncompleteManagedState) {
        $managedChanged = @(Get-UEChangedPathsSince `
            -Baseline $Run.State.managedStateFingerprint -Current $current |
            Where-Object { $managedRel -contains ($_ -replace '\\', '/') })
        if ($managedChanged.Count -gt 0) {
            Set-UERunState $Run "BLOCKED" ("rollback blocked: managed files " +
                "changed after MANAGED-STATE: " + ($managedChanged -join ", "))
            throw ("rollback would overwrite later managed-file edits: " +
                ($managedChanged -join ", "))
        }
    }

    $semantic = @()
    $changed = Get-UEChangedPathsSince -Baseline $Run.State.baselineFingerprint `
        -Current $current
    $semantic = @($changed | Where-Object {
        $managedRel -notcontains ($_ -replace '\\', '/') })

    try {
        foreach ($name in $managed) {
            $src = Join-Path $backups $name
            if (-not (Test-Path $src)) { continue }
            $dest = if ($name -eq "Alis.uproject") {
                Join-Path $RepoRoot $name
            } elseif ($name -eq "dev_manifest.json") {
                Join-Path $RepoRoot "Plugins\Boot\Orchestrator\Data\$name"
            } else { Join-Path $RepoRoot "scripts\config\$name" }
            Copy-Item $src $dest -Force
        }
        # Regeneration stays the ONLY writer for generated files; derived
        # machine-local state goes through its designated writer.
        $setup = Join-Path $RepoRoot "scripts\setup\setup_ue_env.ps1"
        & $setup -EnvScope $EnvScope `
            -PreviousLauncherRoot $plan.launcherRoot `
            -PreviousSourceRoot $plan.sourceRoot | Out-Null
        $generationOutput = & make -C $RepoRoot generate
        $generationExit = $LASTEXITCODE
        $generationOutput | ForEach-Object { Write-Host $_ }
        if ($generationExit -ne 0) {
            throw "make generate failed during rollback (exit $generationExit)"
        }
        if ($Run.State.persistentEditorWasRunning) {
            $restartOutput = & (Join-Path $RepoRoot `
                "scripts\ue\test\unit\persistent_editor_start.ps1")
            $restartExit = $LASTEXITCODE
            $restartOutput | ForEach-Object { Write-Host $_ }
            if ($restartExit -ne 0) {
                throw "persistent editor restart failed during rollback"
            }
        }
    } catch {
        Set-UERunState $Run "ROLLBACK_FAILED" "rollback error: $($_.Exception.Message)"
        throw
    }

    if ($semantic.Count -gt 0) {
        Set-UERunState $Run "ROLLED_BACK" ("deterministic state restored; " +
            "semantic changes remain (revert or adapt manually, then verify): " +
            ($semantic -join ", ") + ". Project files regenerated; previous-engine " +
            "rebuild NOT attempted.")
    } else {
        Set-UERunState $Run "ROLLED_BACK" ("deterministic state restored; no " +
            "semantic changes since -Apply. Project files regenerated; rebuild via " +
            "scripts/ue/standalone/build.ps1")
    }
    return $semantic
}

# --------------------------------------------------------- phase engine --

function Invoke-UEPhaseList {
    # Generic runner. Phase shape:
    #   @{ Name; Kind = "mutation"|"verification"|"action_required";
    #      AlwaysRun = bool (optional external-input verification);
    #      Run = scriptblock (throws or returns nonzero-exit info);
    #      ResumeHint = string }
    # Completed mutations and verifications rerun as one conservative
    # closure whenever the workspace changed. Verifiable external actions
    # remain ACTION_REQUIRED until their real probe succeeds.
    param(
        [Parameter(Mandatory)]$Run,
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][array]$Phases,
        [Parameter(Mandatory)][string]$ResumeCommand,
        [Parameter()][switch]$ForceRerun
    )

    $completed = @{}
    foreach ($p in $Run.State.phases) {
        if ($p.status -eq "ok") { $completed[$p.name] = $p }
    }

    # Invalidate verification results when the tree changed since the
    # last verification checkpoint (any agent repair -> rerun ALL).
    $current = Get-WorkspaceFingerprint $RepoRoot
    $lastVerify = $Run.State.lastVerificationFingerprint
    $rerunVerifications = $ForceRerun -or (-not $lastVerify) -or
        (-not (Compare-UEFingerprint $lastVerify $current))

    foreach ($phase in $Phases) {
        $isDone = $completed.ContainsKey($phase.Name)
        $alwaysRun = [bool]$phase.AlwaysRun
        if ($isDone -and $phase.Kind -eq "mutation" -and
            -not $rerunVerifications) { continue }
        if ($isDone -and $phase.Kind -eq "verification" -and
            -not $rerunVerifications -and -not $alwaysRun) { continue }

        if ($phase.Kind -eq "action_required") {
            if ($isDone -and -not $rerunVerifications) { continue }
            $pending = @($Run.State.phases | Where-Object {
                $_.name -eq $phase.Name -and $_.status -eq "pending"
            } | Select-Object -Last 1)
            $hasProbe = $phase.ContainsKey("Verify")
            $shouldProbe = $hasProbe -and
                ($pending.Count -gt 0 -or ($isDone -and $rerunVerifications))
            if (-not $shouldProbe) {
                $Run.State.phases += @{ name = $phase.Name; status = "pending";
                    at = (Get-Date -Format "o") }
                Set-UERunState $Run "ACTION_REQUIRED" ($phase.ResumeHint +
                    " Resume with: $ResumeCommand")
                return @{ Stopped = $true; Phase = $phase.Name;
                    State = "ACTION_REQUIRED" }
            }

            $logPath = Join-Path $Run.Dir ("logs\" + $phase.Name + ".log")
            try {
                $global:LASTEXITCODE = 0
                $output = & $phase.Verify 2>&1
                $output | Out-File -FilePath $logPath -Encoding utf8
                if ($LASTEXITCODE -is [int] -and $LASTEXITCODE -ne 0) {
                    throw "external health probe exited $LASTEXITCODE"
                }
            } catch {
                $message = $_.Exception.Message
                $message | Out-File -FilePath $logPath -Append -Encoding utf8
                $Run.State.phases += @{ name = $phase.Name; status = "pending";
                    at = (Get-Date -Format "o"); error = $message; log = $logPath }
                Set-UERunState $Run "ACTION_REQUIRED" ($phase.ResumeHint +
                    " Probe failed: $message. Resume with: $ResumeCommand")
                return @{ Stopped = $true; Phase = $phase.Name;
                    State = "ACTION_REQUIRED" }
            }
            $Run.State.phases += @{ name = $phase.Name; status = "ok";
                at = (Get-Date -Format "o"); log = $logPath }
            Save-UERunState $Run
            continue
        }

        $logPath = Join-Path $Run.Dir ("logs\" + $phase.Name + ".log")
        Write-Host "[phase] $($phase.Name) ..." -ForegroundColor Cyan
        $failed = $false
        $errText = ""
        try {
            # Reset so a pure-PowerShell phase never inherits a stale
            # native exit code from an earlier phase.
            $global:LASTEXITCODE = 0
            $output = & $phase.Run 2>&1
            $output | Out-File -FilePath $logPath -Encoding utf8
            if ($LASTEXITCODE -is [int] -and $LASTEXITCODE -ne 0) {
                $failed = $true
                $errText = "exit code $LASTEXITCODE"
            }
        } catch {
            $failed = $true
            $errText = $_.Exception.Message
            $errText | Out-File -FilePath $logPath -Append -Encoding utf8
        }

        if ($failed) {
            $tail = (Get-Content $logPath -Tail 20 -ErrorAction SilentlyContinue) -join "`n"
            $Run.State.phases += @{ name = $phase.Name; status = "failed";
                at = (Get-Date -Format "o"); error = $errText; log = $logPath }
            Set-UERunState $Run "BLOCKED" ("phase '$($phase.Name)' failed: $errText. " +
                "Full log: $logPath. Fix the first causal error, then: $ResumeCommand")
            Write-Host "[phase] FAILED: $($phase.Name) - $errText" -ForegroundColor Red
            Write-Host "  log: $logPath" -ForegroundColor Yellow
            Write-Host "  diagnostic candidate (full log is authoritative):" -ForegroundColor Yellow
            Write-Host $tail
            return @{ Stopped = $true; Phase = $phase.Name; State = "BLOCKED" }
        }

        $Run.State.phases += @{ name = $phase.Name; status = "ok";
            at = (Get-Date -Format "o"); log = $logPath }
        if ($phase.Kind -eq "verification") {
            $Run.State.lastVerificationFingerprint = Get-WorkspaceFingerprint $RepoRoot
        }
        Save-UERunState $Run
    }
    return @{ Stopped = $false }
}

Export-ModuleMember -Function @(
    "Get-UESha256", "Save-UEJson", "Read-UEJson", "Get-UEUpdateRoot",
    "Enter-UEUpdateLock", "Exit-UEUpdateLock",
    "Get-WorkspaceFingerprint", "Test-UEFingerprintSchema",
    "Compare-UEFingerprint", "Get-UEChangedPathsSince",
    "Get-UEEngineVersionInfo", "Get-UEProjectAssociation",
    "Get-UERuns", "Get-UENonTerminalRuns", "Select-UERun", "New-UERun",
    "Save-UERunState", "Set-UERunState",
    "Invoke-UEPreflight", "Update-UEConfKey", "Invoke-UEDeterministicMutations",
    "Invoke-UERollback", "Invoke-UEPhaseList"
)
