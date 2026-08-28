# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [switch]$IncludePublic,
    [switch]$Remove,
    [Parameter(DontShow)]
    [switch]$ElevatedChild,
    [Parameter(DontShow)]
    [string]$OperationId
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    if ($ElevatedChild) {
        throw '[ALIS Firewall] Elevation was requested but the child process is not Administrator.'
    }

    $powerShellPath = (Get-Process -Id $PID).Path
    $OperationId = [Guid]::NewGuid().ToString('N')
    $scratchRoot = Join-Path $projectRoot "tmp\setup\ue_firewall\$OperationId"
    $errorPath = Join-Path $scratchRoot 'child-error.txt'
    New-Item -ItemType Directory -Path $scratchRoot -Force | Out-Null

    $escapedScript = $PSCommandPath.Replace("'", "''")
    $escapedError = $errorPath.Replace("'", "''")
    $childFlags = "-ElevatedChild -OperationId '$OperationId'"
    if ($IncludePublic) { $childFlags += ' -IncludePublic' }
    if ($Remove) { $childFlags += ' -Remove' }
    $childCommand = "try { & '$escapedScript' $childFlags; exit 0 } " +
        "catch { [IO.File]::WriteAllText('$escapedError', (`$_ | Out-String)); exit 1 }"
    $encodedCommand = [Convert]::ToBase64String(
        [Text.Encoding]::Unicode.GetBytes($childCommand))
    $arguments = @('-NoProfile', '-NonInteractive', '-WindowStyle', 'Hidden',
        '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encodedCommand)

    Write-Host '[ALIS Firewall] Windows will ask once for Administrator consent.'
    try {
        $child = Start-Process -FilePath $powerShellPath -Verb RunAs `
            -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    }
    catch {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        throw "[ALIS Firewall] Elevation was cancelled or failed: $($_.Exception.Message)"
    }
    if ($child.ExitCode -ne 0) {
        $detail = if (Test-Path -LiteralPath $errorPath) {
            (Get-Content -Raw -LiteralPath $errorPath).Trim()
        } else {
            "child exit code $($child.ExitCode)"
        }
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        throw "[ALIS Firewall] Elevated setup failed: $detail"
    }

    $actualCount = @(Get-NetFirewallRule -DisplayGroup 'ALIS Unreal Engine' `
            -ErrorAction SilentlyContinue).Count
    if ($Remove) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        if ($actualCount -ne 0) {
            throw "[ALIS Firewall] Removal left $actualCount owned rule(s)."
        }
        Write-Host '[ALIS Firewall] Verified all owned rules were removed.'
        return
    }

    $receiptPath = Join-Path $projectRoot 'Saved\Validation\Setup\ue_firewall.json'
    if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        throw '[ALIS Firewall] Elevated setup did not write its receipt.'
    }
    $receipt = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json
    if ($receipt.operation_id -cne $OperationId) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        throw '[ALIS Firewall] Elevated receipt does not match this operation.'
    }
    $expectedCount = @($receipt.rules).Count
    if ($actualCount -ne $expectedCount) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
        throw "[ALIS Firewall] Verification expected $expectedCount rules, found $actualCount."
    }
    Remove-Item -LiteralPath $scratchRoot -Recurse -Force
    Write-Host "[ALIS Firewall] Verified $actualCount durable rule(s) in the original terminal."
    return
}

$OperationId = if ($OperationId) { $OperationId } else { [Guid]::NewGuid().ToString('N') }
$configRoot = Join-Path $projectRoot 'scripts\config'
. (Join-Path $configRoot 'Resolve-UEConfig.ps1')
$config = Resolve-UEConfig -ConfigDir $configRoot
if (-not $config.UE_PATH) {
    throw '[ALIS Firewall] UE_PATH is missing from the repository engine configuration.'
}

$engineRoot = [IO.Path]::GetFullPath($config.UE_PATH)
$candidateTargets = [ordered]@{
    'Editor' = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    'Editor Commandlet' = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    'Playable Tour Launcher' = Join-Path $projectRoot `
        'Saved\PackageRelease\KazanPlayableTour\Candidate\Windows\Alis.exe'
    'Playable Tour Shipping' = Join-Path $projectRoot `
        'Saved\PackageRelease\KazanPlayableTour\Candidate\Windows\Alis\Binaries\Win64\Alis-Win64-Shipping.exe'
    'Survival Candidate Launcher' = Join-Path $projectRoot `
        'Saved\PackageRelease\KazanSurvival\Candidate\Windows\Alis.exe'
    'Survival Candidate Shipping' = Join-Path $projectRoot `
        'Saved\PackageRelease\KazanSurvival\Candidate\Windows\Alis\Binaries\Win64\Alis-Win64-Shipping.exe'
}
$targets = [ordered]@{}
foreach ($target in $candidateTargets.GetEnumerator()) {
    $resolved = [IO.Path]::GetFullPath($target.Value)
    if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        $targets[$target.Key] = $resolved
    }
}
if ($targets.Count -lt 2) {
    throw '[ALIS Firewall] Launcher Editor executables were not resolved.'
}
$targetPaths = @($targets.Values)
$profiles = @('Domain', 'Private')
if ($IncludePublic) {
    $profiles += 'Public'
}

# The setup owns only its display group. External Block rules are operator
# policy: report a conflict and stop instead of weakening that policy.
$ownedRules = @(Get-NetFirewallRule -DisplayGroup 'ALIS Unreal Engine' `
    -ErrorAction SilentlyContinue)
$conflictingBlocks = [Collections.Generic.List[object]]::new()
foreach ($rule in @(Get-NetFirewallRule -PolicyStore PersistentStore)) {
    if ($ownedRules.Name -contains $rule.Name -or $rule.Action.ToString() -cne 'Block') {
        continue
    }
    $application = $rule | Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue
    if (-not $application.Program -or $application.Program -ceq 'Any') {
        continue
    }
    try {
        $program = [IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables(
                [string]$application.Program))
    }
    catch {
        continue
    }
    if ($targetPaths -contains $program) {
        $conflictingBlocks.Add($rule)
    }
}
if ($conflictingBlocks.Count -gt 0) {
    throw "[ALIS Firewall] Found $($conflictingBlocks.Count) external Block rule(s). No policy was changed."
}
if ($Remove) {
    foreach ($rule in $ownedRules) {
        Remove-NetFirewallRule -Name $rule.Name
    }
    Write-Host '[ALIS Firewall] Removed only ALIS Unreal Engine rules.'
    return
}

$newRuleNames = [Collections.Generic.List[string]]::new()
try {
    $targetIndex = 0
    foreach ($target in $targets.GetEnumerator()) {
        foreach ($protocol in @('TCP', 'UDP')) {
            $ruleName = "ALIS-UE-$OperationId-$targetIndex-$protocol"
            New-NetFirewallRule -Name $ruleName `
                -DisplayName "ALIS UE - $($target.Key) - $protocol" `
                -Group 'ALIS Unreal Engine' -Direction Inbound -Action Allow `
                -Enabled True -Profile $profiles -Program $target.Value `
                -Protocol $protocol | Out-Null
            $newRuleNames.Add($ruleName)
        }
        $targetIndex++
    }
}
catch {
    foreach ($ruleName in $newRuleNames) {
        Remove-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    }
    throw
}

$installed = [Collections.Generic.List[object]]::new()
foreach ($ruleName in $newRuleNames) {
    $rule = Get-NetFirewallRule -Name $ruleName
    $application = $rule | Get-NetFirewallApplicationFilter
    $matchingTarget = @($targets.Values | Where-Object {
            $_.Equals([string]$application.Program, [StringComparison]::OrdinalIgnoreCase)
        })
    $port = $rule | Get-NetFirewallPortFilter
    if ($matchingTarget.Count -eq 1 -and $port.Protocol -in @('TCP', 'UDP') -and
        $rule.Enabled.ToString() -ceq 'True' -and
        $rule.Direction.ToString() -ceq 'Inbound' -and
        $rule.Action.ToString() -ceq 'Allow') {
        $installed.Add([ordered]@{
                display_name = $rule.DisplayName
                program = [string]$application.Program
                direction = $rule.Direction.ToString()
                action = $rule.Action.ToString()
                profile = $rule.Profile.ToString()
                protocol = $port.Protocol.ToString()
            })
    }
}
if ($installed.Count -ne $targets.Count * 2) {
    foreach ($ruleName in $newRuleNames) {
        Remove-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    }
    throw "[ALIS Firewall] Expected $($targets.Count * 2) exact allow rules, found $($installed.Count)."
}

# Replace the previous generation only after the complete new generation is
# installed and verified. A failed add therefore leaves the old policy intact.
foreach ($rule in $ownedRules) {
    Remove-NetFirewallRule -Name $rule.Name
}
$finalCount = @(Get-NetFirewallRule -DisplayGroup 'ALIS Unreal Engine').Count
if ($finalCount -ne $installed.Count) {
    throw "[ALIS Firewall] Final verification found $finalCount rule(s), expected $($installed.Count)."
}

$receiptPath = Join-Path $projectRoot 'Saved\Validation\Setup\ue_firewall.json'
New-Item -ItemType Directory -Path (Split-Path -Parent $receiptPath) -Force | Out-Null
$receipt = [ordered]@{
    schema_version = 1
    status = 'accepted'
    operation_id = $OperationId
    installed_at = [DateTimeOffset]::Now.ToString('o')
    display_group = 'ALIS Unreal Engine'
    owned_rules_replaced = $ownedRules.Count
    external_block_conflicts = $conflictingBlocks.Count
    public_profile_opt_in = [bool]$IncludePublic
    rules = @($installed)
}
[IO.File]::WriteAllText(
    $receiptPath,
    ($receipt | ConvertTo-Json -Depth 5) + "`n",
    [Text.UTF8Encoding]::new($false))

Write-Host "[ALIS Firewall] Installed $($installed.Count) durable inbound TCP/UDP rules."
Write-Host "[ALIS Firewall] Profiles: $($profiles -join ', ')."
Write-Host '[ALIS Firewall] External firewall policy was not modified.'
Write-Host "[ALIS Firewall] Receipt: $receiptPath"
