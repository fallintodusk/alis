# UEEnvSync.psm1 - machine-local derived-state writer for UE engine roots.
#
# THE designated writer for machine-local derived files (persistent env,
# .vscode, .claude/settings.local.json, .mcp.json). Invoked by
# setup_ue_env.ps1 (fresh clone / manual sync) and by the engine-update
# orchestrator. SOT stays scripts/config/ue_path.conf; this module only
# derives.
#
# Mutation safety contract:
# - parse all JSON inputs BEFORE changing anything (invalid input = block,
#   nothing written)
# - preserve unknown fields (parse -> targeted transform -> reserialize)
# - temp file + atomic rename; one-time .engine-sync.bak backup
# - idempotent; reports exactly which files changed
#
# Engine-root rewriting is selection-bound: only the previous selected
# launcher/source roots may move. Other engine grants stay untouched.

function Convert-EngineRootStyle {
    # Render canonical root (e.g. C:/UnrealEngine/UE_5.8) in the slash
    # style of a sample occurrence.
    param(
        [Parameter(Mandatory)][string]$CanonRoot,
        [Parameter(Mandatory)][string]$StyleSample
    )
    $root = $CanonRoot -replace '\\', '/'
    if ($root -notmatch '^([A-Za-z]):(/.*)$') { return $CanonRoot }
    $drive = $Matches[1]; $rest = $Matches[2]
    if ($StyleSample.StartsWith('//')) {
        return "//$($drive.ToUpper())$rest"
    }
    if ($StyleSample -match '^/[a-z]/') {
        return "/$($drive.ToLower())$rest"
    }
    if ($StyleSample.Contains('\')) {
        return ($drive.ToUpper() + ':' + $rest) -replace '/', '\'
    }
    return $drive.ToUpper() + ':' + $rest
}

function Update-EngineRootsInString {
    # Rewrite exact/beneath occurrences of the selected previous roots.
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory)][string]$NewLauncherRoot,
        [Parameter()][string]$NewSourceRoot,
        [Parameter()][string]$PreviousLauncherRoot,
        [Parameter()][string]$PreviousSourceRoot
    )

    function Get-RootVariants([string]$Root) {
        if ([string]::IsNullOrWhiteSpace($Root)) { return @() }
        $canon = ($Root -replace '\\', '/').TrimEnd('/')
        $variants = @($canon, ($canon -replace '/', '\'))
        if ($canon -match '^([A-Za-z]):(/.*)$') {
            $drive = $Matches[1]
            $rest = $Matches[2]
            $variants += "/$($drive.ToLower())$rest"
            $variants += "//$($drive.ToUpper())$rest"
        }
        return @($variants | Select-Object -Unique |
            Sort-Object { $_.Length } -Descending)
    }

    function Replace-SelectedRoot(
            [string]$Value, [string]$PreviousRoot, [string]$NewRoot) {
        if ([string]::IsNullOrWhiteSpace($PreviousRoot) -or
            [string]::IsNullOrWhiteSpace($NewRoot)) { return $Value }
        $out = $Value
        foreach ($variant in (Get-RootVariants $PreviousRoot)) {
            $pattern = '(?i)' + [regex]::Escape($variant) +
                '(?=$|[/\\\s)"' + "'" + ',;])'
            $regex = New-Object regex $pattern
            $builder = New-Object System.Text.StringBuilder
            $last = 0
            foreach ($match in $regex.Matches($out)) {
                [void]$builder.Append($out.Substring(
                    $last, $match.Index - $last))
                [void]$builder.Append((Convert-EngineRootStyle `
                    -CanonRoot $NewRoot -StyleSample $match.Value))
                $last = $match.Index + $match.Length
            }
            if ($last -gt 0) {
                [void]$builder.Append($out.Substring($last))
                $out = $builder.ToString()
            }
        }
        return $out
    }

    $out = Replace-SelectedRoot -Value $Text `
        -PreviousRoot $PreviousLauncherRoot -NewRoot $NewLauncherRoot
    $out = Replace-SelectedRoot -Value $out `
        -PreviousRoot $PreviousSourceRoot -NewRoot $NewSourceRoot
    return @{ Text = $out; Blockers = @() }
}

function Update-JsonTreeEngineRoots {
    # Recursive walk over parsed JSON (PSCustomObject/array), rewriting
    # every string. Collects blockers with JSON-path locations.
    param(
        [Parameter()][AllowNull()]$Node,
        [Parameter(Mandatory)][string]$NewLauncherRoot,
        [Parameter()][string]$NewSourceRoot,
        [Parameter()][string]$PreviousLauncherRoot,
        [Parameter()][string]$PreviousSourceRoot,
        [Parameter()][string]$JsonPath = '$',
        [Parameter(Mandatory)][AllowEmptyCollection()]
        [System.Collections.ArrayList]$Blockers
    )
    if ($null -eq $Node) { return $null }
    if ($Node -is [string]) {
        $r = Update-EngineRootsInString -Text $Node `
            -NewLauncherRoot $NewLauncherRoot -NewSourceRoot $NewSourceRoot `
            -PreviousLauncherRoot $PreviousLauncherRoot `
            -PreviousSourceRoot $PreviousSourceRoot
        foreach ($b in $r.Blockers) {
            [void]$Blockers.Add("${JsonPath}: $b")
        }
        return $r.Text
    }
    if ($Node -is [System.Collections.IList]) {
        for ($i = 0; $i -lt $Node.Count; $i++) {
            $Node[$i] = Update-JsonTreeEngineRoots -Node $Node[$i] `
                -NewLauncherRoot $NewLauncherRoot -NewSourceRoot $NewSourceRoot `
                -PreviousLauncherRoot $PreviousLauncherRoot `
                -PreviousSourceRoot $PreviousSourceRoot `
                -JsonPath "$JsonPath[$i]" -Blockers $Blockers
        }
        return , $Node
    }
    if ($Node -is [System.Management.Automation.PSCustomObject]) {
        foreach ($prop in $Node.PSObject.Properties) {
            $prop.Value = Update-JsonTreeEngineRoots -Node $prop.Value `
                -NewLauncherRoot $NewLauncherRoot -NewSourceRoot $NewSourceRoot `
                -PreviousLauncherRoot $PreviousLauncherRoot `
                -PreviousSourceRoot $PreviousSourceRoot `
                -JsonPath "$JsonPath.$($prop.Name)" -Blockers $Blockers
        }
        return $Node
    }
    return $Node
}

function Get-UEMachineLocalJsonPlan {
    # Pure planner for .claude/settings.local.json + .mcp.json.
    param(
        [Parameter(Mandatory)][string]$NewLauncherRoot,
        [Parameter()][string]$NewSourceRoot,
        [Parameter()][string]$PreviousLauncherRoot,
        [Parameter()][string]$PreviousSourceRoot,
        [Parameter(Mandatory)][string]$SettingsPath,
        [Parameter(Mandatory)][string]$McpPath
    )

    $blockers = New-Object System.Collections.ArrayList
    $mutations = @()

    foreach ($item in @(
        @{ Path = $SettingsPath; Kind = "settings" },
        @{ Path = $McpPath; Kind = "mcp" }
    )) {
        if (-not (Test-Path $item.Path)) { continue }
        $raw = Get-Content -LiteralPath $item.Path -Raw
        try { $doc = $raw | ConvertFrom-Json }
        catch {
            [void]$blockers.Add("$($item.Path): invalid JSON - $($_.Exception.Message)")
            continue
        }

        $doc = Update-JsonTreeEngineRoots -Node $doc `
            -NewLauncherRoot $NewLauncherRoot -NewSourceRoot $NewSourceRoot `
            -PreviousLauncherRoot $PreviousLauncherRoot `
            -PreviousSourceRoot $PreviousSourceRoot `
            -JsonPath $item.Path -Blockers $blockers

        if ($item.Kind -eq "settings" -and $doc.PSObject.Properties["env"]) {
            $envNode = $doc.env
            # Unify env var name on UE_PATH (drop UE_INSTALL_LOCATION).
            if ($envNode.PSObject.Properties["UE_INSTALL_LOCATION"]) {
                $envNode.PSObject.Properties.Remove("UE_INSTALL_LOCATION")
            }
            $winRoot = ($NewLauncherRoot -replace '/', '\')
            if ($envNode.PSObject.Properties["UE_PATH"]) {
                $envNode.UE_PATH = $winRoot
            } else {
                $envNode | Add-Member -NotePropertyName "UE_PATH" `
                    -NotePropertyValue $winRoot
            }
        }
        if ($item.Kind -eq "mcp" -and $doc.PSObject.Properties["mcpServers"]) {
            # Expansion branch (single derivation path): UE_EDITOR_CMD uses
            # ${UE_PATH}; env comes from settings env + persistent user env.
            $mcpProjectRoot = (Split-Path -Parent `
                ([IO.Path]::GetFullPath($item.Path))) -replace '\\', '/'
            foreach ($srv in $doc.mcpServers.PSObject.Properties) {
                $envNode = $srv.Value.PSObject.Properties["env"]
                if ($envNode -and $envNode.Value.PSObject.Properties["UE_EDITOR_CMD"]) {
                    $envNode.Value.UE_EDITOR_CMD =
                        '${UE_PATH}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
                }
                if ($srv.Name -eq "blueprint-mcp" -and $envNode) {
                    if ($envNode.Value.PSObject.Properties["UE_PROJECT_DIR"]) {
                        $envNode.Value.UE_PROJECT_DIR = $mcpProjectRoot
                    } else {
                        $envNode.Value | Add-Member `
                            -NotePropertyName "UE_PROJECT_DIR" `
                            -NotePropertyValue $mcpProjectRoot
                    }
                }
            }
        }

        $newRaw = ($doc | ConvertTo-Json -Depth 32) + "`n"
        # Compare parsed forms to stay idempotent across formatting.
        $oldNorm = ($raw | ConvertFrom-Json | ConvertTo-Json -Depth 32)
        $newNorm = ($newRaw | ConvertFrom-Json | ConvertTo-Json -Depth 32)
        if ($oldNorm -ne $newNorm) {
            $mutations += @{ Path = $item.Path; Old = $raw; New = $newRaw }
        }
    }

    return @{ Mutations = $mutations; Blockers = @($blockers) }
}

function Invoke-UEFileMutationPlan {
    # Prepare every replacement before applying any. If a later replace
    # fails, already-replaced files are restored from the in-memory plan.
    param([Parameter(Mandatory)][AllowEmptyCollection()][array]$Mutations)

    $prepared = @()
    try {
        foreach ($mutation in $Mutations) {
            $path = [string]$mutation.Path
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "machine-local destination is missing: $path"
            }
            if ((Get-Item -LiteralPath $path).IsReadOnly) {
                throw "machine-local destination is read-only: $path"
            }
            $tmp = "$path.tmp-$([IO.Path]::GetRandomFileName())"
            [IO.File]::WriteAllText($tmp, [string]$mutation.New,
                (New-Object System.Text.UTF8Encoding($false)))
            $prepared += @{ Path = $path; Old = [string]$mutation.Old;
                New = [string]$mutation.New; Temp = $tmp }
        }
    } catch {
        foreach ($entry in $prepared) {
            if (Test-Path -LiteralPath $entry.Temp) {
                Remove-Item -LiteralPath $entry.Temp -Force
            }
        }
        throw
    }

    $applied = @()
    try {
        foreach ($entry in $prepared) {
            $backup = "$($entry.Path).engine-sync.bak"
            if (-not (Test-Path -LiteralPath $backup)) {
                Copy-Item -LiteralPath $entry.Path -Destination $backup
            }
            Move-Item -LiteralPath $entry.Temp -Destination $entry.Path -Force
            $applied += $entry
        }
    } catch {
        $reverseApplied = @($applied)
        [array]::Reverse($reverseApplied)
        foreach ($entry in $reverseApplied) {
            $restore = "$($entry.Path).restore-$([IO.Path]::GetRandomFileName())"
            [IO.File]::WriteAllText($restore, $entry.Old,
                (New-Object System.Text.UTF8Encoding($false)))
            Move-Item -LiteralPath $restore -Destination $entry.Path -Force
        }
        throw
    } finally {
        foreach ($entry in $prepared) {
            if (Test-Path -LiteralPath $entry.Temp) {
                Remove-Item -LiteralPath $entry.Temp -Force
            }
        }
    }
    return $prepared
}

function Restore-UEFileMutationPlan {
    param([Parameter(Mandatory)][AllowEmptyCollection()][array]$Mutations)
    $reverseMutations = @($Mutations)
    [array]::Reverse($reverseMutations)
    foreach ($entry in $reverseMutations) {
        $restore = "$($entry.Path).restore-$([IO.Path]::GetRandomFileName())"
        [IO.File]::WriteAllText($restore, [string]$entry.Old,
            (New-Object System.Text.UTF8Encoding($false)))
        Move-Item -LiteralPath $restore -Destination $entry.Path -Force
    }
}

function Sync-UEMachineLocalJson {
    param(
        [Parameter(Mandatory)][string]$NewLauncherRoot,
        [Parameter()][string]$NewSourceRoot,
        [Parameter()][string]$PreviousLauncherRoot,
        [Parameter()][string]$PreviousSourceRoot,
        [Parameter(Mandatory)][string]$SettingsPath,
        [Parameter(Mandatory)][string]$McpPath
    )
    $plan = Get-UEMachineLocalJsonPlan @PSBoundParameters
    if ($plan.Blockers.Count -gt 0) {
        return @{ Changed = @(); Blockers = $plan.Blockers }
    }
    try {
        $applied = @(Invoke-UEFileMutationPlan -Mutations $plan.Mutations)
    } catch {
        return @{ Changed = @(); Blockers = @($_.Exception.Message) }
    }
    return @{ Changed = @($applied | ForEach-Object { $_.Path }); Blockers = @() }
}

function Sync-UEUserEnv {
    # Persistent env cache writer. Process scope exists for tests and for
    # the orchestrator's same-session consistency.
    param(
        [Parameter(Mandatory)][string]$NewLauncherRoot,
        [Parameter()][ValidateSet("User", "Process")][string]$Scope = "User"
    )
    $winRoot = ($NewLauncherRoot -replace '/', '\')
    if ($Scope -eq "Process") {
        $Env:UE_PATH = $winRoot
    } else {
        [Environment]::SetEnvironmentVariable("UE_PATH", $winRoot, "User")
        $Env:UE_PATH = $winRoot
    }
    return $winRoot
}

Export-ModuleMember -Function @(
    "Convert-EngineRootStyle",
    "Update-EngineRootsInString",
    "Update-JsonTreeEngineRoots",
    "Get-UEMachineLocalJsonPlan",
    "Invoke-UEFileMutationPlan",
    "Restore-UEFileMutationPlan",
    "Sync-UEMachineLocalJson",
    "Sync-UEUserEnv"
)
