# UpdateEngine.Tests.ps1 - synthetic state-machine tests for the
# engine-update orchestrator. Uses a temp git repo
# with fake engine roots and injected synthetic phases; the real
# pipeline phases are never executed here.
#
# Run: Invoke-Pester scripts/ue/update/test/UpdateEngine.Tests.ps1

BeforeAll {
    $script:UpdateDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $script:RealRepo = (Resolve-Path (Join-Path $script:UpdateDir "..\..\..")).Path
    Import-Module (Join-Path $script:UpdateDir "UpdateEngineCore.psm1") -Force
    Import-Module (Join-Path $script:UpdateDir "UpdateEnginePhases.psm1") -Force

    function New-FakeEngine([string]$Root, [int]$Major, [int]$Minor, [int]$Patch) {
        $bin = Join-Path $Root "Engine\Binaries\Win64"
        New-Item -ItemType Directory -Path $bin -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $Root "Engine\Build") -Force | Out-Null
        Set-Content (Join-Path $bin "UnrealEditor-Cmd.exe") "stub"
        Set-Content (Join-Path $Root "Engine\Build\Build.version") (@{
            MajorVersion = $Major; MinorVersion = $Minor; PatchVersion = $Patch
            Changelist = if ($Minor -eq 8) { 56057345 } else { 48512491 }
            BranchName = "++UE$Major+Release-$Major.$Minor"
        } | ConvertTo-Json)
    }

    function New-SyntheticRepo {
        $repo = Join-Path $TestDrive ("repo_" + [IO.Path]::GetRandomFileName())
        $cfg = Join-Path $repo "scripts\config"
        $setup = Join-Path $repo "scripts\setup"
        New-Item -ItemType Directory -Path $cfg, $setup -Force | Out-Null

        $engOld = Join-Path $TestDrive ("engOld_" + [IO.Path]::GetRandomFileName())
        $engNew = Join-Path $TestDrive ("engNew_" + [IO.Path]::GetRandomFileName())
        New-FakeEngine $engOld 5 7 4
        New-FakeEngine $engNew 5 8 1

        Copy-Item (Join-Path $script:RealRepo "scripts\config\Resolve-UEConfig.ps1") $cfg
        Copy-Item (Join-Path $script:RealRepo "scripts\setup\setup_ue_env.ps1") $setup
        Copy-Item (Join-Path $script:RealRepo "scripts\setup\UEEnvSync.psm1") $setup

        Set-Content (Join-Path $cfg "ue_path.conf") @(
            "UE_PATH=$($engOld -replace '\\', '/')",
            "UE_SOURCE_PATH=G:/FakeSource-5.7"
        )
        Set-Content (Join-Path $repo "Alis.uproject") @'
{
	"FileVersion": 3,
	"EngineAssociation": "5.7",
	"Category": "",
	"Description": ""
}
'@
        $manifestDir = Join-Path $repo "Plugins\Boot\Orchestrator\Data"
        New-Item -ItemType Directory -Path $manifestDir -Force | Out-Null
        Set-Content (Join-Path $manifestDir "dev_manifest.json") @'
{
  "engine_build_id": "++UE5+Release-5.7-CL-48512491"
}
'@
        Set-Content (Join-Path $repo ".gitignore") "Saved/`n"
        Set-Content (Join-Path $repo "Makefile") "generate:`n`t@echo generated`n"
        Set-Content (Join-Path $repo "source_file.cpp") "// original"

        git -C $repo init -q 2>$null | Out-Null
        git -C $repo add -A 2>$null | Out-Null
        git -C $repo -c user.email=t@t -c user.name=t commit -qm init 2>$null | Out-Null

        return @{ Repo = $repo; EngineOld = $engOld; EngineNew = $engNew }
    }

    # Synthetic phases: A and B pass; C fails while the flag exists.
    # This proves a later repair invalidates every earlier verification.
    $script:PhasesScriptPath = Join-Path $TestDrive "phases.ps1"
    Set-Content $script:PhasesScriptPath @'
param([string]$RepoRoot, $Plan)
$log = Join-Path $RepoRoot "Saved\phases_executed.log"
$phases = @()
if ($Plan.completionKind -ne "source") {
    $phases += @{ Name = "syn-generate"; Kind = "mutation"
        ResumeHint = "generation failed."
        Run = { Add-Content $log "G"; "G ok" }.GetNewClosure() }
}
$phases + @(
    @{ Name = "syn-A"; Kind = "verification"; ResumeHint = "A failed."
       AlwaysRun = ($Plan.completionKind -eq "source")
       Run = { Add-Content $log "A"; "A ok" }.GetNewClosure() },
    @{ Name = "syn-B"; Kind = "verification"; ResumeHint = "B failed."
       Run = { Add-Content $log "B"; "B ok" }.GetNewClosure() },
    @{ Name = "syn-C"; Kind = "verification"; ResumeHint = "C failed."
       Run = {
           Add-Content $log "C"
           if (Test-Path (Join-Path $RepoRoot "Saved\failB.flag")) { throw "synthetic C failure" }
           "C ok"
       }.GetNewClosure() }
)
'@

    function Invoke-UpdateEngine([hashtable]$Ctx, [string[]]$Cli) {
        $phasePath = if ($Ctx.PhasesScript) {
            $Ctx.PhasesScript
        } else { $script:PhasesScriptPath }
        $args = @(
            "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", (Join-Path $script:UpdateDir "update_engine.ps1"),
            "-RepoRoot", $Ctx.Repo, "-EnvScope", "Process",
            "-SkipProcessCheck", "-PhasesScript", $phasePath
        ) + $Cli
        $out = & powershell.exe @args 2>&1
        return @{ ExitCode = $LASTEXITCODE; Out = ($out -join "`n") }
    }

    function Invoke-UpdateEngineWithDefaultRoot([hashtable]$Ctx, [string[]]$Cli) {
        $localUpdateDir = Join-Path $Ctx.Repo "scripts\ue\update"
        if (-not (Test-Path $localUpdateDir)) {
            New-Item -ItemType Directory -Path $localUpdateDir -Force | Out-Null
            Copy-Item (Join-Path $script:UpdateDir "update_engine.ps1") $localUpdateDir
            Copy-Item (Join-Path $script:UpdateDir "UpdateEngineCore.psm1") $localUpdateDir
            Copy-Item (Join-Path $script:UpdateDir "UpdateEngineFingerprint.ps1") `
                $localUpdateDir
            Copy-Item (Join-Path $script:UpdateDir "UpdateEnginePhases.psm1") $localUpdateDir
        }
        $args = @(
            "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", (Join-Path $localUpdateDir "update_engine.ps1"),
            "-EnvScope", "Process", "-SkipProcessCheck",
            "-PhasesScript", $script:PhasesScriptPath
        ) + $Cli
        $out = & powershell.exe @args 2>&1
        return @{ ExitCode = $LASTEXITCODE; Out = ($out -join "`n") }
    }

    function Get-RunStates([hashtable]$Ctx) {
        @(Get-UERuns $Ctx.Repo | ForEach-Object { $_.State.state })
    }
}

Describe "lock" {
    It "second invocation cannot run while the lock is held; released after" {
        $ctx = New-SyntheticRepo
        $lock = Enter-UEUpdateLock $ctx.Repo
        try {
            $r = Invoke-UpdateEngine $ctx @("-Status")
            $r.ExitCode | Should -Not -Be 0
            $r.Out | Should -Match "holds the lock"
        } finally { Exit-UEUpdateLock $lock }
        (Invoke-UpdateEngine $ctx @("-Status")).ExitCode | Should -Be 0
    }

    It "stale lock metadata does not block a later run" {
        $ctx = New-SyntheticRepo
        $lockPath = Join-Path (Get-UEUpdateRoot $ctx.Repo) "update.lock"
        New-Item -ItemType Directory -Path (Split-Path $lockPath) -Force | Out-Null
        Set-Content $lockPath "pid=999999 start=long-ago cmd=dead"
        (Invoke-UpdateEngine $ctx @("-Status")).ExitCode | Should -Be 0
    }
}

Describe "workspace fingerprint" {
    It "records both rename paths from NUL-delimited Git status" {
        $ctx = New-SyntheticRepo
        git -C $ctx.Repo mv "source_file.cpp" "renamed file.cpp"
        $fingerprint = Get-WorkspaceFingerprint $ctx.Repo

        $fingerprint.SchemaVersion | Should -Be 3
        $fingerprint.SubmoduleStatusHash | Should -Not -BeNullOrEmpty
        $fingerprint.Files.ContainsKey("renamed file.cpp") | Should -BeTrue
        $fingerprint.Files.ContainsKey("source_file.cpp") | Should -BeTrue
        $fingerprint.Files["renamed file.cpp"].RenameFrom |
            Should -Be "source_file.cpp"
        $fingerprint.Files["source_file.cpp"].RenameTo |
            Should -Be "renamed file.cpp"
    }

    It "distinguishes staged index content from later worktree content" {
        $ctx = New-SyntheticRepo
        Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// staged"
        git -C $ctx.Repo add source_file.cpp
        Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// worktree"
        $record = (Get-WorkspaceFingerprint $ctx.Repo).Files["source_file.cpp"]

        $record.TrackedState | Should -Be "tracked"
        $record.IndexStatus | Should -Be "M"
        $record.WorktreeStatus | Should -Be "M"
        $record.IndexHash | Should -Not -Be $record.WorktreeHash
    }
}

Describe "verified external action" {
    It "stays ACTION_REQUIRED until its real probe succeeds" {
        $ctx = New-SyntheticRepo
        $fingerprint = Get-WorkspaceFingerprint $ctx.Repo
        $run = New-UERun -RepoRoot $ctx.Repo -Plan @{} `
            -BaselineFingerprint $fingerprint
        $marker = Join-Path $ctx.Repo "Saved\external-ready.marker"
        $phase = @{ Name = "external"; Kind = "action_required"
            ResumeHint = "restart external"
            Verify = {
                if (-not (Test-Path $marker)) { throw "external is not healthy" }
                "healthy"
            }.GetNewClosure() }

        (Invoke-UEPhaseList -Run $run -RepoRoot $ctx.Repo -Phases @($phase) `
            -ResumeCommand "resume").State | Should -Be "ACTION_REQUIRED"
        (Invoke-UEPhaseList -Run $run -RepoRoot $ctx.Repo -Phases @($phase) `
            -ResumeCommand "resume").State | Should -Be "ACTION_REQUIRED"
        Set-Content $marker "ok"
        (Invoke-UEPhaseList -Run $run -RepoRoot $ctx.Repo -Phases @($phase) `
            -ResumeCommand "resume").Stopped | Should -BeFalse
        $run.State.phases[-1].status | Should -Be "ok"
    }

    It "states that restart is attested while protocol health is verified" {
        $phase = Get-UEUpdatePhases -RepoRoot $script:RealRepo `
            -Plan @{ launcherRoot = "C:\FakeUE"; persistentEditorWasRunning = $false } |
            Where-Object { $_.Name -eq "restart-externals" }

        $phase.ResumeHint | Should -Match "operator's attestation"
        $phase.ResumeHint | Should -Match "not reliably machine-verifiable"
        $phase.ResumeHint | Should -Match "server_status"
    }
}

Describe "BlueprintMCP protocol probe" {
    It "calls server_status through the configured stdio transport" {
        $sdk = (Resolve-Path (Join-Path $script:RealRepo `
            "Plugins\Local\BlueprintMCP\Tools\node_modules\@modelcontextprotocol\sdk\dist\esm")).Path `
            -replace '\\', '/'
        $serverScript = Join-Path $TestDrive "fake-mcp-server.mjs"
        Set-Content $serverScript @"
import { McpServer } from "file:///$sdk/server/mcp.js";
import { StdioServerTransport } from "file:///$sdk/server/stdio.js";
const server = new McpServer({ name: "fake", version: "1.0.0" });
server.tool("server_status", "status", {}, async () => ({
  content: [{ type: "text", text: "UE5 Blueprint server is running (test mode)." }],
}));
await server.connect(new StdioServerTransport());
"@
        $node = (Get-Command node).Source
        $config = Join-Path $TestDrive "fake-mcp.json"
        @{
            mcpServers = @{
                "blueprint-mcp" = @{ command = $node; args = @($serverScript);
                    cwd = $TestDrive; env = @{} }
            }
        } | ConvertTo-Json -Depth 8 | Set-Content $config

        $output = & $node (Join-Path $script:UpdateDir `
            "probe_blueprint_mcp.mjs") $config 2>&1

        $LASTEXITCODE | Should -Be 0 -Because ($output -join "`n")
        ($output -join "`n") | Should -Match "running \(test mode\)"
    }
}

Describe "dry-run and admission" {
    It "dry-run prints the plan, derives versions from Build.version, creates NO run" {
        $ctx = New-SyntheticRepo
        $r = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew)
        $r.ExitCode | Should -Be 0
        $r.Out | Should -Match "Build\.version 5\.8\.1"
        $r.Out | Should -Match "EngineAssociation.*5\.7.*->.*5\.8"
        $r.Out | Should -Match "dry-run only"
        (Get-UERuns $ctx.Repo).Count | Should -Be 0
    }

    It "-ExpectedVersion is an assertion, not an authority" {
        $ctx = New-SyntheticRepo
        $r = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew,
            "-ExpectedVersion", "5.9.0")
        $r.ExitCode | Should -Be 2
        $r.Out | Should -Match "BLOCKERS"
    }

    It "reserves source-root activation for the completed development child" {
        $ctx = New-SyntheticRepo
        $r = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew,
            "-SourceRoot", $ctx.EngineNew, "-Apply")
        $r.ExitCode | Should -Not -Be 0
        $r.Out | Should -Match "valid only with -CompleteSource after DEV_COMPLETE"
        (Get-UERuns $ctx.Repo).Count | Should -Be 0
        (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
            Should -Match "UE_SOURCE_PATH=G:/FakeSource-5.7"
    }

    It "a new -Apply is rejected while a nonterminal run exists" {
        $ctx = New-SyntheticRepo
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
            Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = $null
            $r1 = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
            $r1.ExitCode | Should -Be 3
            (Get-RunStates $ctx) | Should -Contain "BLOCKED"
            $r2 = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
            $r2.ExitCode | Should -Not -Be 0
            $r2.Out | Should -Match "nonterminal update run exists"
        } finally { $Env:UE_PATH = $saved }
    }

    It "blocks -Apply when managed files carry uncommitted edits" {
        $ctx = New-SyntheticRepo
        Add-Content (Join-Path $ctx.Repo "Alis.uproject") "// dirty"
        $r = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $r.ExitCode | Should -Be 2
        $r.Out | Should -Match "uncommitted edits"
    }
}

Describe "apply -> fail -> repair -> resume (invalidation) -> DEV_COMPLETE" {
    It "runs the full synthetic repair loop with verification invalidation" {
        $ctx = New-SyntheticRepo
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = $null
            New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
            Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"

            # -Apply: mutations succeed, syn-A ok, syn-B fails -> BLOCKED
            $r1 = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
            $r1.ExitCode | Should -Be 3

            # Deterministic mutations landed + were backed up
            $conf = Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw
            $conf | Should -Match ([regex]::Escape(($ctx.EngineNew -replace '\\', '/')))
            (Get-Content (Join-Path $ctx.Repo "Alis.uproject") -Raw) |
                Should -Match '"EngineAssociation": "5.8"'
            (Get-Content (Join-Path $ctx.Repo `
                "Plugins\Boot\Orchestrator\Data\dev_manifest.json") -Raw) |
                Should -Match '"engine_build_id": "\+\+UE5\+Release-5.8-CL-56057345"'
            $run = @(Get-UERuns $ctx.Repo)[0]
            Test-Path (Join-Path $run.Dir "backups\ue_path.conf") | Should -BeTrue
            Test-Path (Join-Path $run.Dir "backups\dev_manifest.json") | Should -BeTrue
            $run.State.state | Should -Be "BLOCKED"

            # Agent repair: fix flag + edit a source file (workspace change)
            Remove-Item (Join-Path $ctx.Repo "Saved\failB.flag")
            Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// repaired"

            # Simulate a fresh agent process inheriting the old derived cache.
            $Env:UE_PATH = $ctx.EngineOld
            $r2 = Invoke-UpdateEngine $ctx @("-Resume")
            $r2.ExitCode | Should -Be 0
            $r2.Out | Should -Match "Development engine update: COMPLETE"
            $r2.Out | Should -Match "FROZEN"
            (Get-RunStates $ctx) | Should -Contain "DEV_COMPLETE"

            # Invalidation: syn-A must have rerun after the repair
            $log = Get-Content (Join-Path $ctx.Repo "Saved\phases_executed.log")
            ($log | Where-Object { $_ -eq "A" }).Count | Should -Be 2
            ($log | Where-Object { $_ -eq "B" }).Count | Should -Be 2
            ($log | Where-Object { $_ -eq "C" }).Count | Should -Be 2
            ($log | Where-Object { $_ -eq "G" }).Count | Should -Be 2
        } finally { $Env:UE_PATH = $saved }
    }

    It "resume without changes does NOT rerun completed verification" {
        $ctx = New-SyntheticRepo
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = $null
            $r1 = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
            $r1.ExitCode | Should -Be 0
            # DEV_COMPLETE run is terminal for -Resume
            $r2 = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", @(Get-UERuns $ctx.Repo)[0].Id)
            $r2.ExitCode | Should -Not -Be 0
            $r2.Out | Should -Match "nothing to resume"
        } finally { $Env:UE_PATH = $saved }
    }
}

Describe "cold closure finalization" {
    It "reruns cold gates when editor finalization changes tracked state" {
        $ctx = New-SyntheticRepo
        $phaseScript = Join-Path $TestDrive "finalization-phases.ps1"
        Set-Content $phaseScript @'
param([string]$RepoRoot, $Plan)
$log = Join-Path $RepoRoot "Saved\finalization.log"
@(
    @{ Name = "cold-check"; Kind = "verification"; ResumeHint = "cold failed"
       Run = { Add-Content $log "cold"; "cold ok" }.GetNewClosure() },
    @{ Name = "restore-persistent-editor"; Kind = "mutation"; ResumeHint = "restore failed"
       Run = {
           Set-Content (Join-Path $RepoRoot "source_file.cpp") "// editor normalized"
           "editor restored"
       }.GetNewClosure() },
    @{ Name = "restart-externals"; Kind = "action_required"; ResumeHint = "restart"
       Verify = {
           if (-not (Test-Path (Join-Path $RepoRoot "Saved\mcp-ok.flag"))) {
               throw "MCP not ready"
           }
           "MCP ready"
       }.GetNewClosure() }
)
'@
        $ctx.PhasesScript = $phaseScript
        $first = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $first.ExitCode | Should -Be 3
        $run = @(Get-UERuns $ctx.Repo)[0]
        Set-Content (Join-Path $ctx.Repo "Saved\mcp-ok.flag") "ok"

        $second = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", $run.Id)
        $second.ExitCode | Should -Be 3
        $third = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", $run.Id)

        $third.ExitCode | Should -Be 0 -Because $third.Out
        (Get-Content (Join-Path $ctx.Repo "Saved\finalization.log")).Count |
            Should -Be 2
        $completed = Get-UERuns $ctx.Repo | Where-Object { $_.Id -eq $run.Id }
        $completed.State.completionFingerprint.Combined |
            Should -Be (Get-WorkspaceFingerprint $ctx.Repo).Combined
    }
}

Describe "DEV_COMPLETE -> source child -> SOURCE_COMPLETE" {
    It "restores launcher editor binaries after source package gates" {
        $plan = @{
            sourceRoot = "G:\FakeSource-5.8"
            launcherRoot = "C:\FakeLauncher-5.8"
            launcherVersion = @{ Line = "5.8" }
            sourcePackageOutput = "E:\FakePackage"
        }
        $names = @(Get-UESourceCompletionPhases -RepoRoot $script:RealRepo `
            -Plan $plan | ForEach-Object { $_.Name })

        [array]::IndexOf($names, "restore-launcher-editor-binaries") |
            Should -BeGreaterThan ([array]::IndexOf($names, "source-package-boot"))
    }

    It "requires a completed development parent" {
        $ctx = New-SyntheticRepo
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") | Out-Null
        $parent = @(Get-UERuns $ctx.Repo)[0]

        $r = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")

        $r.ExitCode | Should -Not -Be 0
        $r.Out | Should -Match "requires a DEV_COMPLETE parent"
    }

    It "rejects a project moved beyond the parent engine line" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        $project = Join-Path $ctx.Repo "Alis.uproject"
        (Get-Content $project -Raw).Replace(
            '"EngineAssociation": "5.8"', '"EngineAssociation": "5.9"') |
            Set-Content $project

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")

        $source.ExitCode | Should -Be 2
        $source.Out | Should -Match "LINEAGE BLOCKERS"
        $source.Out | Should -Match "association 5\.9"
        @(Get-UERuns $ctx.Repo).Count | Should -Be 1
    }

    It "reruns the full dev closure after same-line workspace drift" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        $parentStateBefore = Get-Content (Join-Path $parent.Dir "state.json") -Raw
        Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// later work"

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")

        $source.ExitCode | Should -Be 0 -Because $source.Out
        $source.Out | Should -Match "rerun in immutable-parent source child"
        (Get-UERuns $ctx.Repo | Where-Object { $_.Id -eq $parent.Id }).State.state |
            Should -Be "DEV_COMPLETE"
        (Get-Content (Join-Path $parent.Dir "state.json") -Raw) |
            Should -Be $parentStateBefore
        $child = Get-UERuns $ctx.Repo |
            Where-Object { $_.State.parentRunId -eq $parent.Id }
        $childPlan = Read-UEJson (Join-Path $child.Dir "plan.json")
        $childPlan.developmentClosureRequired | Should -BeTrue
        $child.State.developmentClosureReason | Should -Be "parent-drift"
        $childPlan.parentCompletionFingerprint.Combined |
            Should -Be $parent.State.completionFingerprint.Combined
        $executed = Get-Content (Join-Path $ctx.Repo "Saved\phases_executed.log")
        ($executed | Where-Object { $_ -eq "G" }).Count | Should -Be 2
        ($executed | Where-Object { $_ -eq "A" }).Count | Should -Be 3
    }

    It "creates one linked child and reaches SOURCE_COMPLETE" {
        $ctx = New-SyntheticRepo
        $savedPath = $Env:UE_PATH
        $savedSourcePath = $Env:UE_SOURCE_PATH
        try {
            $Env:UE_PATH = $null
            $Env:UE_SOURCE_PATH = $null
            $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
            $dev.ExitCode | Should -Be 0
            $parent = @(Get-UERuns $ctx.Repo)[0]

            $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
                "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")

            $source.ExitCode | Should -Be 0
            $source.Out | Should -Match "Source engine update: COMPLETE"
            $runs = @(Get-UERuns $ctx.Repo)
            $runs.Count | Should -Be 2
            $child = $runs | Where-Object { $_.State.parentRunId -eq $parent.Id }
            $child.State.state | Should -Be "SOURCE_COMPLETE"
            $childPlan = Read-UEJson (Join-Path $child.Dir "plan.json")
            $childPlan.completionKind | Should -Be "source"
            $childPlan.sourcePackageOutput | Should -Match "EngineUpdateSource"
            (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
                Should -Match ([regex]::Escape(($ctx.EngineNew -replace '\\', '/')))

            $again = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
                "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")
            $again.ExitCode | Should -Not -Be 0
            $again.Out | Should -Match "source child already exists"
        } finally {
            $Env:UE_PATH = $savedPath
            $Env:UE_SOURCE_PATH = $savedSourcePath
        }
    }

    It "resumes the source pipeline without dispatching the dev pipeline" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")
        $source.ExitCode | Should -Be 3
        $child = @(Get-UERuns $ctx.Repo) |
            Where-Object { $_.State.parentRunId -eq $parent.Id }
        $child.State.state | Should -Be "BLOCKED"
        (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
            Should -Match "UE_SOURCE_PATH=G:/FakeSource-5.7"

        Remove-Item (Join-Path $ctx.Repo "Saved\failB.flag")
        $resume = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", $child.Id)
        $resume.ExitCode | Should -Be 0
        $resume.Out | Should -Match "Source engine update: COMPLETE"
        (Get-UERuns $ctx.Repo | Where-Object { $_.Id -eq $child.Id }).State.state |
            Should -Be "SOURCE_COMPLETE"
        $executed = Get-Content (Join-Path $ctx.Repo "Saved\phases_executed.log")
        ($executed | Where-Object { $_ -eq "A" }).Count | Should -Be 3
        ($executed | Where-Object { $_ -eq "B" }).Count | Should -Be 2
    }

    It "latches development closure after a source-child repair" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        Clear-Content (Join-Path $ctx.Repo "Saved\phases_executed.log")
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")
        $source.ExitCode | Should -Be 3
        $child = @(Get-UERuns $ctx.Repo) |
            Where-Object { $_.State.parentRunId -eq $parent.Id }
        (Read-UEJson (Join-Path $child.Dir "plan.json")).developmentClosureRequired |
            Should -BeFalse

        Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// source repair"
        $blocked = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", $child.Id)
        $blocked.ExitCode | Should -Be 3
        $blockedState = (Get-UERuns $ctx.Repo |
            Where-Object { $_.Id -eq $child.Id }).State
        $blockedState.state | Should -Be "BLOCKED"
        $blockedState.developmentClosure | Should -Be "rerun-required"
        $blockedState.developmentClosureReason | Should -Be "source-child-repair"
        @(Get-Content (Join-Path $ctx.Repo "Saved\phases_executed.log"))[-4..-1] |
            Should -Be @("G", "A", "B", "C")

        Remove-Item (Join-Path $ctx.Repo "Saved\failB.flag")
        $resume = Invoke-UpdateEngine $ctx @("-Resume", "-RunId", $child.Id)
        $resume.ExitCode | Should -Be 0 -Because $resume.Out
        $completed = (Get-UERuns $ctx.Repo |
            Where-Object { $_.Id -eq $child.Id }).State
        $completed.state | Should -Be "SOURCE_COMPLETE"
        $completed.developmentClosure | Should -Be "rerun-complete"
        @(Get-Content (Join-Path $ctx.Repo "Saved\phases_executed.log"))[-7..-1] |
            Should -Be @("G", "A", "B", "C", "A", "B", "C")
    }

    It "activates source when RepoRoot is derived from the script location" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngineWithDefaultRoot $ctx `
            @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0 -Because $dev.Out
        $parent = @(Get-UERuns $ctx.Repo)[0]

        $source = Invoke-UpdateEngineWithDefaultRoot $ctx @(
            "-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged"
        )

        $source.ExitCode | Should -Be 0 -Because $source.Out
        $source.Out | Should -Match "Source engine update: COMPLETE"
        (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
            Should -Match ([regex]::Escape(
                "UE_SOURCE_PATH=$($ctx.EngineNew -replace '\\', '/')"))
    }

    It "rejects an installed engine before creating or mutating a source child" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        Set-Content (Join-Path $ctx.EngineNew "Engine\Build\InstalledBuild.txt") "installed"

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")

        $source.ExitCode | Should -Be 2
        $source.Out | Should -Match "source root is an installed engine"
        @(Get-UERuns $ctx.Repo).Count | Should -Be 1
        (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
            Should -Match "UE_SOURCE_PATH=G:/FakeSource-5.7"
    }

    It "requires the source child to roll back before its development parent" {
        $ctx = New-SyntheticRepo
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0
        $parent = @(Get-UERuns $ctx.Repo)[0]
        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")
        $source.ExitCode | Should -Be 0
        $child = @(Get-UERuns $ctx.Repo) |
            Where-Object { $_.State.parentRunId -eq $parent.Id }

        $wrongOrder = Invoke-UpdateEngine $ctx @("-Rollback", "-RunId", $parent.Id)
        $wrongOrder.ExitCode | Should -Not -Be 0
        $wrongOrder.Out | Should -Match "Roll back the child first"
        (Invoke-UpdateEngine $ctx @("-Rollback", "-RunId", $child.Id)).ExitCode |
            Should -Be 0
        (Invoke-UpdateEngine $ctx @("-Rollback", "-RunId", $parent.Id)).ExitCode |
            Should -Be 0
    }

    It "owns stop-gates-activation-restore with parent drift=<Drift>" -TestCases @(
        @{ Drift = $false }, @{ Drift = $true }
    ) {
        param([bool]$Drift)
        $ctx = New-SyntheticRepo
        $ctx.PhasesScript = Join-Path $TestDrive `
            ("source_lifecycle_" + [IO.Path]::GetRandomFileName() + ".ps1")
        Set-Content $ctx.PhasesScript @'
param([string]$RepoRoot, $Plan)
$log = Join-Path $RepoRoot "Saved\source_lifecycle.log"
$kind = [string]$Plan.completionKind
$gateName = if ($kind -eq "source") { "source-gate" } `
    elseif ($kind -eq "development-revalidation") { "dev-revalidation" } `
    else { "dev-parent" }
$gate = @{ Name = $gateName; Kind = "verification"; ResumeHint = "gate failed"
    Run = {
        $sourceLine = Get-Content (Join-Path $RepoRoot "scripts\config\ue_path.conf") |
            Where-Object { $_ -like "UE_SOURCE_PATH=*" }
        Add-Content $log "$gateName`:$sourceLine"
        "gate ok"
    }.GetNewClosure() }
$restore = @{ Name = "restore-persistent-editor"; Kind = "mutation"
    ResumeHint = "restore failed"
    Run = {
        if ($Plan.persistentEditorWasRunning) {
            $sourceLine = Get-Content (Join-Path $RepoRoot "scripts\config\ue_path.conf") |
                Where-Object { $_ -like "UE_SOURCE_PATH=*" }
            Add-Content $log "restore`:$sourceLine"
        }
        "restore ok"
    }.GetNewClosure() }
$phases = @($gate)
if ($kind -eq "source") {
    $phases += @{ Name = "restore-launcher-editor-binaries"; Kind = "verification"
        ResumeHint = "launcher restore failed"
        Run = {
            $sourceLine = Get-Content (Join-Path $RepoRoot "scripts\config\ue_path.conf") |
                Where-Object { $_ -like "UE_SOURCE_PATH=*" }
            Add-Content $log "launcher-artifacts`:$sourceLine"
            "launcher artifacts restored"
        }.GetNewClosure() }
}
$phases + @($restore)
'@
        $unitDir = Join-Path $ctx.Repo "scripts\ue\test\unit"
        $pidDir = Join-Path $ctx.Repo "scripts\ue\artifacts\persistent"
        New-Item -ItemType Directory -Path $unitDir, $pidDir -Force | Out-Null
        Add-Content (Join-Path $ctx.Repo ".gitignore") `
            "scripts/ue/artifacts/persistent/"
        Set-Content (Join-Path $unitDir "persistent_editor_stop.ps1") @'
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
Add-Content (Join-Path $repo "Saved\source_lifecycle.log") "stop"
Remove-Item (Join-Path $repo "scripts\ue\artifacts\persistent\editor.pid") `
    -ErrorAction SilentlyContinue
'@
        git -C $ctx.Repo add .gitignore scripts/ue/test/unit/persistent_editor_stop.ps1
        git -C $ctx.Repo -c user.email=t@t -c user.name=t commit -qm lifecycle-fixture
        $dev = Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply")
        $dev.ExitCode | Should -Be 0 -Because $dev.Out
        $parent = @(Get-UERuns $ctx.Repo)[0]
        if ($Drift) { Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// drift" }

        Set-Content (Join-Path $pidDir "editor.pid") $PID

        $source = Invoke-UpdateEngine $ctx @("-CompleteSource", "-RunId", $parent.Id,
            "-SourceRoot", $ctx.EngineNew, "-AllowDirtyManaged")
        $source.ExitCode | Should -Be 0 -Because $source.Out
        $child = @(Get-UERuns $ctx.Repo) |
            Where-Object { $_.State.parentRunId -eq $parent.Id }
        $child.State.persistentEditorWasRunning | Should -BeTrue
        $events = @(Get-Content (Join-Path $ctx.Repo "Saved\source_lifecycle.log"))
        $gateEvent = "source-gate:UE_SOURCE_PATH=G:/FakeSource-5.7"
        $stopIndex = [array]::IndexOf($events, "stop")
        $gateIndex = [array]::IndexOf($events, $gateEvent)
        $stopIndex | Should -BeLessThan $gateIndex
        $newSource = "restore:UE_SOURCE_PATH=" + ($ctx.EngineNew -replace '\\', '/')
        $launcherArtifacts = "launcher-artifacts:UE_SOURCE_PATH=" +
            ($ctx.EngineNew -replace '\\', '/')
        $launcherIndex = [array]::IndexOf($events, $launcherArtifacts)
        $restoreIndex = [array]::IndexOf($events, $newSource)
        $launcherIndex | Should -BeGreaterThan $gateIndex
        $restoreIndex | Should -BeGreaterThan $launcherIndex
        ($events -like "dev-revalidation:*").Count | Should -Be ([int]$Drift)
    }
}

Describe "rollback honesty" {
    It "blocks when HEAD changed after BASELINE" {
        $ctx = New-SyntheticRepo
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// committed later"
        git -C $ctx.Repo add source_file.cpp 2>$null | Out-Null
        git -C $ctx.Repo -c user.email=t@t -c user.name=t commit -qm later 2>$null |
            Out-Null

        $rollback = Invoke-UpdateEngine $ctx @("-Rollback")

        $rollback.ExitCode | Should -Not -Be 0
        $rollback.Out | Should -Match "cannot cross a repository HEAD change"
    }

    It "blocks after a clean checkout to a different revision" {
        $ctx = New-SyntheticRepo
        Set-Content (Join-Path $ctx.Repo "baseline_marker.txt") "baseline"
        git -C $ctx.Repo add baseline_marker.txt 2>$null | Out-Null
        git -C $ctx.Repo -c user.email=t@t -c user.name=t commit -qm baseline 2>$null |
            Out-Null
        $otherRevision = git -C $ctx.Repo rev-parse HEAD^
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") |
            Out-Null
        git -C $ctx.Repo add -A 2>$null | Out-Null
        git -C $ctx.Repo -c user.email=t@t -c user.name=t commit -qm applied 2>$null |
            Out-Null
        git -C $ctx.Repo checkout -q --detach $otherRevision 2>$null | Out-Null

        $rollback = Invoke-UpdateEngine $ctx @("-Rollback")

        $rollback.ExitCode | Should -Not -Be 0
        $rollback.Out | Should -Match "cannot cross a repository HEAD change"
    }

    It "blocks before overwriting a managed file edited after MANAGED-STATE" {
        $ctx = New-SyntheticRepo
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") |
            Out-Null
        $project = Join-Path $ctx.Repo "Alis.uproject"
        Add-Content $project "// independent later edit"

        $rollback = Invoke-UpdateEngine $ctx @("-Rollback")

        $rollback.ExitCode | Should -Not -Be 0
        $rollback.Out | Should -Match "would overwrite later managed-file edits"
        (Get-Content $project -Raw) | Should -Match "independent later edit"
        (Get-RunStates $ctx) | Should -Contain "BLOCKED"
    }

    It "reports ROLLBACK_FAILED when project regeneration fails" {
        $ctx = New-SyntheticRepo
        New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
        Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") |
            Out-Null
        Set-Content (Join-Path $ctx.Repo "Makefile") `
            "generate:`n`t@exit 1`n"

        $rollback = Invoke-UpdateEngine $ctx @("-Rollback")

        $rollback.ExitCode | Should -Not -Be 0
        (Get-RunStates $ctx) | Should -Contain "ROLLBACK_FAILED"
    }

    It "restores managed state; reports semantic changes; never touches git" {
        $ctx = New-SyntheticRepo
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = $null
            New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
            Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
            Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") | Out-Null

            # Semantic repair happened before the user decides to roll back
            Set-Content (Join-Path $ctx.Repo "source_file.cpp") "// semantic fix"

            $r = Invoke-UpdateEngine $ctx @("-Rollback")
            $r.ExitCode | Should -Be 0
            $r.Out | Should -Match "Deterministic engine state restored"
            $r.Out | Should -Match "Semantic migration changes remain"
            $r.Out | Should -Match "source_file.cpp"
            $r.Out | Should -Match "rebuild NOT attempted"

            # Managed files back to the OLD engine
            (Get-Content (Join-Path $ctx.Repo "scripts\config\ue_path.conf") -Raw) |
                Should -Match ([regex]::Escape(($ctx.EngineOld -replace '\\', '/')))
            (Get-Content (Join-Path $ctx.Repo "Alis.uproject") -Raw) |
                Should -Match '"EngineAssociation": "5.7"'
            (Get-Content (Join-Path $ctx.Repo `
                "Plugins\Boot\Orchestrator\Data\dev_manifest.json") -Raw) |
                Should -Match '"engine_build_id": "\+\+UE5\+Release-5.7-CL-48512491"'
            # Semantic edit untouched (no git reset/checkout)
            (Get-Content (Join-Path $ctx.Repo "source_file.cpp") -Raw) |
                Should -Match "semantic fix"
            (Get-RunStates $ctx) | Should -Contain "ROLLED_BACK"
        } finally { $Env:UE_PATH = $saved }
    }

    It "clean rollback (no semantic changes) recommends rebuild directly" {
        $ctx = New-SyntheticRepo
        $saved = $Env:UE_PATH
        try {
            $Env:UE_PATH = $null
            New-Item -ItemType Directory -Path (Join-Path $ctx.Repo "Saved") -Force | Out-Null
            Set-Content (Join-Path $ctx.Repo "Saved\failB.flag") "x"
            Invoke-UpdateEngine $ctx @("-LauncherRoot", $ctx.EngineNew, "-Apply") | Out-Null
            Remove-Item (Join-Path $ctx.Repo "Saved\failB.flag")

            $r = Invoke-UpdateEngine $ctx @("-Rollback")
            $r.ExitCode | Should -Be 0
            $r.Out | Should -Match "No semantic changes"
        } finally { $Env:UE_PATH = $saved }
    }
}

Describe "run selection" {
    It "fails with a candidate list when no RunId and nothing nonterminal" {
        $ctx = New-SyntheticRepo
        $r = Invoke-UpdateEngine $ctx @("-Resume")
        $r.ExitCode | Should -Not -Be 0
        $r.Out | Should -Match "no candidate run"
    }
}
