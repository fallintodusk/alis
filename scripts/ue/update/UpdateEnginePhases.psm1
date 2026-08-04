# UpdateEnginePhases.psm1 - PRODUCTION phase list for the engine-update
# orchestrator. Every phase calls an EXISTING project route; the
# orchestrator adds no bespoke build logic. Phase engine + state live in
# UpdateEngineCore.psm1.
#
# Kinds: mutation (skip when already completed), verification (rerun
# whenever the workspace changed since the last verification checkpoint),
# action_required (pause for an external process the script cannot own).

function Get-UESourceCandidateProblems {
    param(
        [Parameter(Mandatory)][string]$SourceRoot,
        [Parameter(Mandatory)][string]$ExpectedLine
    )

    $problems = @()
    try {
        $info = Get-UEEngineVersionInfo $SourceRoot
        if ($info.Line -ne $ExpectedLine) {
            $problems += "source engine line $($info.Line) does not match target $ExpectedLine"
        }
    } catch {
        $problems += $_.Exception.Message
    }
    $editor = Join-Path $SourceRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (-not (Test-Path -LiteralPath $editor)) {
        $problems += "source editor is not built: $editor"
    }
    $installedMarker = Join-Path $SourceRoot "Engine\Build\InstalledBuild.txt"
    if (Test-Path -LiteralPath $installedMarker) {
        $problems += "source root is an installed engine: $SourceRoot"
    }
    return $problems
}

function Get-UEUpdatePhases {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)]$Plan
    )

    $ue = ($Plan.launcherRoot -replace '/', '\')
    $pkgOut = Join-Path $RepoRoot "Saved\PackageRelease\EngineUpdate"

    return @(
        @{ Name = "sot-conformance"; Kind = "verification"
           ResumeHint = "resolver/config conformance failed."
           Run = {
               $previousEap = $ErrorActionPreference
               try {
                   # Cargo warnings use stderr even when the aggregate gate
                   # exits zero. Preserve them in the log; trust the exit code.
                   $ErrorActionPreference = "Continue"
                   powershell.exe -NoProfile -ExecutionPolicy Bypass `
                       -File "$RepoRoot\scripts\config\test\run_conformance.ps1" `
                       -RepoRoot $RepoRoot
               } finally {
                   $ErrorActionPreference = $previousEap
               }
           }.GetNewClosure() },

        @{ Name = "validate-all"; Kind = "verification"
           ResumeHint = "project validation failed."
           Run = {
               & "$env:SystemRoot\System32\cmd.exe" /d /c `
                   "cd /d `"$RepoRoot`" && scripts\ue\check\validate_all.bat"
           }.GetNewClosure() },

        @{ Name = "generate-project-files"; Kind = "mutation"
           ResumeHint = "project file generation failed."
           Run = {
               & make -C $RepoRoot generate
           }.GetNewClosure() },

        @{ Name = "build-editor"; Kind = "verification"
           ResumeHint = ("editor build failed - fix the FIRST causal compile " +
               "error (full log is authoritative), then resume.")
           Run = {
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\standalone\build.ps1"
           }.GetNewClosure() },

        @{ Name = "smoke-boot"; Kind = "verification"
           ResumeHint = "editor boot smoke failed."
           Run = {
               & "$env:SystemRoot\System32\cmd.exe" /d /c `
                   "cd /d `"$RepoRoot`" && scripts\ue\test\smoke\boot_test.bat"
           }.GetNewClosure() },

        @{ Name = "unit-tests"; Kind = "verification"
           ResumeHint = "unit test tier failed."
           Run = {
               & "$env:SystemRoot\System32\cmd.exe" /d /c `
                   "cd /d `"$RepoRoot`" && scripts\ue\test\test.bat --unit"
           }.GetNewClosure() },

        @{ Name = "mcp-config-verify"; Kind = "verification"
           ResumeHint = ("MCP config verification failed - check .mcp.json " +
               "and env UE_PATH; if `${UE_PATH} expansion is unsupported, " +
               "switch setup to absolute-path mode per version_update.md.")
           Run = {
               $mcp = Join-Path $RepoRoot ".mcp.json"
               if (Test-Path $mcp) {
                   $doc = Get-Content $mcp -Raw | ConvertFrom-Json
                   foreach ($srv in $doc.mcpServers.PSObject.Properties) {
                       $envNode = $srv.Value.PSObject.Properties["env"]
                       if (-not $envNode) { continue }
                       $cmdProp = $envNode.Value.PSObject.Properties["UE_EDITOR_CMD"]
                       if (-not $cmdProp) { continue }
                       $resolved = $cmdProp.Value -replace '\$\{UE_PATH\}', $env:UE_PATH
                       if (-not (Test-Path $resolved)) {
                           throw "UE_EDITOR_CMD does not resolve: $($cmdProp.Value) -> $resolved"
                       }
                   }
               }
               "mcp config resolves against env UE_PATH=$env:UE_PATH"
            }.GetNewClosure() },

        @{ Name = "dev-package"; Kind = "verification"
           ResumeHint = "development cook/package on UE_PATH failed."
           Run = {
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\package\package_release.ps1" `
                   -EngineRoot $ue -OutputDir $pkgOut `
                   -ClientConfig Development
           }.GetNewClosure() },

        @{ Name = "package-boot"; Kind = "verification"
           ResumeHint = "packaged build boot smoke failed."
           Run = {
               $exe = Join-Path $pkgOut "Windows\Alis.exe"
               if (-not (Test-Path $exe)) {
                   throw "packaged exe not found: $exe"
               }
               $packageRoot = [System.IO.Path]::GetFullPath($pkgOut).TrimEnd('\') + '\'
               $runtimeSaved = [System.IO.Path]::GetFullPath(
                   (Join-Path (Split-Path -Parent $exe) "Alis\Saved"))
               if (-not $runtimeSaved.StartsWith(
                   $packageRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                   throw "runtime state escapes package root: $runtimeSaved"
               }
               if (Test-Path $runtimeSaved) {
                   Remove-Item -LiteralPath $runtimeSaved -Recurse -Force
               }
               $logDir = Join-Path $runtimeSaved "Logs"
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\test\smoke\packaged_boot_test.ps1" `
                   -ExePath $exe -LogDir $logDir -SecondRun
           }.GetNewClosure() },

        @{ Name = "restore-persistent-editor"; Kind = "mutation"
           ResumeHint = "persistent editor restoration failed."
           Run = {
               if (-not $Plan.persistentEditorWasRunning) {
                   "persistent editor was not running before the update"
                   return
               }
               $pidFile = Join-Path $RepoRoot `
                   "scripts\ue\artifacts\persistent\editor.pid"
               $isRunning = $false
               if (Test-Path $pidFile) {
                   $editorPid = Get-Content $pidFile | Select-Object -First 1
                   $isRunning = [bool](Get-Process -Id $editorPid `
                       -ErrorAction SilentlyContinue)
               }
               if (-not $isRunning) {
                   & "$RepoRoot\scripts\ue\test\unit\persistent_editor_start.ps1"
                   if ($LASTEXITCODE -ne 0) {
                       throw "persistent editor restart failed (exit $LASTEXITCODE)"
                   }
               }
               $global:LASTEXITCODE = 0
               "persistent editor restored"
           }.GetNewClosure() },

        @{ Name = "restart-externals"; Kind = "action_required"
           ResumeHint = ("Reload VS Code and its MCP host so they inherit the " +
               "new engine paths, then resume. Resuming is the operator's " +
               "attestation that this external action occurred; shared VS Code " +
               "process replacement is not reliably machine-verifiable. A fresh " +
               "client must also call BlueprintMCP server_status through the " +
               "configured MCP stdio transport.")
           Verify = {
               $mcpPath = Join-Path $RepoRoot ".mcp.json"
               $mcp = Get-Content $mcpPath -Raw | ConvertFrom-Json
               $server = $mcp.mcpServers.'blueprint-mcp'
               if (-not $server) { throw "blueprint-mcp is not configured" }
               $node = [string]$server.command
               $probe = Join-Path $RepoRoot `
                   "scripts\ue\update\probe_blueprint_mcp.mjs"
               $output = & $node $probe $mcpPath
               if ($LASTEXITCODE -ne 0) {
                   throw "BlueprintMCP protocol probe failed (exit $LASTEXITCODE)"
               }
               $output
           }.GetNewClosure() }
    )
}

function Get-UESourceCompletionPhases {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)]$Plan
    )

    $source = [string]$Plan.sourceRoot
    $launcher = [string]$Plan.launcherRoot
    $targetLine = [string]$Plan.launcherVersion.Line
    $pkgOut = [string]$Plan.sourcePackageOutput

    return @(
        @{ Name = "source-candidate-identity"; Kind = "verification"; AlwaysRun = $true
           ResumeHint = "source candidate identity or built-editor gate failed."
           Run = {
               $problems = @(Get-UESourceCandidateProblems `
                   -SourceRoot $source -ExpectedLine $targetLine)
               if ($problems.Count -gt 0) {
                   throw ($problems -join "; ")
               }
               "source candidate is built and matches target line $targetLine"
           }.GetNewClosure() },

        @{ Name = "source-shipping-package"; Kind = "verification"
           ResumeHint = "source-engine Shipping package/archive failed."
           Run = {
               $allowedRoot = [System.IO.Path]::GetFullPath(
                   (Join-Path $RepoRoot "Saved\PackageRelease\EngineUpdateSource")
               ).TrimEnd('\') + '\'
               $resolvedOutput = [System.IO.Path]::GetFullPath($pkgOut)
               if (-not $resolvedOutput.StartsWith(
                   $allowedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                   throw "source package output escapes engine-update root: $resolvedOutput"
               }
               if (Test-Path $resolvedOutput) {
                   Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
               }
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\package\package_release.ps1" `
                   -EngineRoot $source -OutputDir $resolvedOutput `
                   -ClientConfig Shipping -CreateReleaseArchive
           }.GetNewClosure() },

        @{ Name = "source-package-boot"; Kind = "verification"
           ResumeHint = "source-engine packaged build boot smoke failed."
           Run = {
               $exe = Join-Path $pkgOut "Windows\Alis.exe"
               if (-not (Test-Path $exe)) {
                   throw "packaged exe not found: $exe"
               }
               $packageRoot = [System.IO.Path]::GetFullPath($pkgOut).TrimEnd('\') + '\'
               $runtimeSaved = [System.IO.Path]::GetFullPath(
                   (Join-Path (Split-Path -Parent $exe) "Alis\Saved"))
               if (-not $runtimeSaved.StartsWith(
                   $packageRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                   throw "runtime state escapes package root: $runtimeSaved"
               }
               if (Test-Path $runtimeSaved) {
                   Remove-Item -LiteralPath $runtimeSaved -Recurse -Force
               }
               $logDir = Join-Path $runtimeSaved "Logs"
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\test\smoke\packaged_boot_test.ps1" `
                   -ExePath $exe -LogDir $logDir -SecondRun
           }.GetNewClosure() },

        @{ Name = "restore-launcher-editor-binaries"; Kind = "verification"
           ResumeHint = "launcher editor-binary restoration failed."
           Run = {
               # Source UBT writes project module manifests into shared Binaries/.
               # Restore the daily launcher BuildId before editor/MCP finalization.
               $env:UE_PATH = $launcher
               powershell.exe -NoProfile -ExecutionPolicy Bypass `
                   -File "$RepoRoot\scripts\ue\standalone\build.ps1"
           }.GetNewClosure() }
    )
}

Export-ModuleMember -Function Get-UEUpdatePhases, Get-UESourceCompletionPhases, `
    Get-UESourceCandidateProblems
