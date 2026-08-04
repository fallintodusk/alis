# UpdateEngineFingerprint.ps1 - exact Git workspace checkpoints.
#
# Dot-sourced by UpdateEngineCore.psm1 so checkpoint logic stays isolated
# from orchestration state and remains available through the core module.

function Get-UEGitStatusNul {
    param([Parameter(Mandatory)][string]$RepoRoot)

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "git.exe"
    $psi.Arguments = "-c core.quotepath=false status --porcelain=v1 -z --untracked-files=all"
    $psi.WorkingDirectory = $RepoRoot
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "git status failed: $stderr"
    }
    return $stdout
}

function Get-UEGitHash {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & git -C $RepoRoot @Arguments 2>$null
        if ($LASTEXITCODE -ne 0) { return $null }
        return (($output | Select-Object -First 1) -join "")
    } finally {
        $ErrorActionPreference = $previous
    }
}

function New-UEPathFingerprintRecord {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$IndexStatus,
        [Parameter(Mandatory)][string]$WorktreeStatus,
        [Parameter(Mandatory)][string]$TrackedState,
        [Parameter()][string]$RenameFrom,
        [Parameter()][string]$RenameTo
    )

    $indexHash = Get-UEGitHash -RepoRoot $RepoRoot `
        -Arguments @("rev-parse", "--verify", ":$Path")
    $full = Join-Path $RepoRoot $Path
    $worktreeHash = if (Test-Path -LiteralPath $full -PathType Leaf) {
        Get-UEGitHash -RepoRoot $RepoRoot `
            -Arguments @("hash-object", "--no-filters", "--", $Path)
    } elseif (Test-Path -LiteralPath $full -PathType Container) {
        "DIRECTORY"
    } else {
        "DELETED"
    }

    return [ordered]@{
        TrackedState = $TrackedState
        IndexStatus = $IndexStatus
        IndexHash = $indexHash
        WorktreeStatus = $WorktreeStatus
        WorktreeHash = $worktreeHash
        RenameFrom = $RenameFrom
        RenameTo = $RenameTo
    }
}

function ConvertTo-UEFingerprintFileMap {
    param([Parameter()][AllowNull()]$Files)

    $map = @{}
    if ($null -eq $Files) { return $map }
    if ($Files -is [System.Collections.IDictionary]) {
        foreach ($key in $Files.Keys) { $map[[string]$key] = $Files[$key] }
        return $map
    }
    foreach ($property in $Files.PSObject.Properties) {
        $map[$property.Name] = $property.Value
    }
    return $map
}

function Get-WorkspaceFingerprint {
    param([Parameter(Mandatory)][string]$RepoRoot)

    $head = Get-UEGitHash -RepoRoot $RepoRoot `
        -Arguments @("rev-parse", "HEAD")
    if (-not $head) { throw "cannot fingerprint a repository without HEAD" }

    $files = @{}
    $records = @((Get-UEGitStatusNul $RepoRoot) -split "`0")
    for ($i = 0; $i -lt $records.Count; $i++) {
        $record = $records[$i]
        if (-not $record) { continue }
        if ($record.Length -lt 4) { throw "invalid NUL-delimited git status record" }
        $indexStatus = $record.Substring(0, 1)
        $worktreeStatus = $record.Substring(1, 1)
        $path = $record.Substring(3)
        $renameFrom = $null
        if ($indexStatus -in @("R", "C") -or
            $worktreeStatus -in @("R", "C")) {
            $i++
            if ($i -ge $records.Count -or -not $records[$i]) {
                throw "rename record is missing its original path"
            }
            $renameFrom = $records[$i]
        }
        $trackedState = if ($indexStatus -eq "?" -and $worktreeStatus -eq "?") {
            "untracked"
        } else {
            "tracked"
        }
        $files[$path] = New-UEPathFingerprintRecord -RepoRoot $RepoRoot `
            -Path $path -IndexStatus $indexStatus `
            -WorktreeStatus $worktreeStatus -TrackedState $trackedState `
            -RenameFrom $renameFrom

        if ($renameFrom) {
            $oldIndex = if ($indexStatus -eq "R") { "D" } else { " " }
            $oldWorktree = if ($worktreeStatus -eq "R") { "D" } else { " " }
            $files[$renameFrom] = New-UEPathFingerprintRecord -RepoRoot $RepoRoot `
                -Path $renameFrom -IndexStatus $oldIndex `
                -WorktreeStatus $oldWorktree -TrackedState "tracked" -RenameTo $path
        }
    }

    $hasSubmodules = Test-Path (Join-Path $RepoRoot ".gitmodules")
    $submoduleStatus = if ($hasSubmodules) {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try { (& git -C $RepoRoot submodule status 2>$null) -join "`n" }
        finally { $ErrorActionPreference = $previous }
    } else {
        ""
    }
    $pathLines = foreach ($path in ($files.Keys | Sort-Object)) {
        $json = $files[$path] | ConvertTo-Json -Compress -Depth 8
        "$path`0$json"
    }
    $submoduleStatusHash = Get-UESha256 $submoduleStatus
    $combined = Get-UESha256 ("3`n$head`n" +
        (($pathLines -join "`n")) + "`n" + $submoduleStatusHash)

    return [ordered]@{
        SchemaVersion = 3
        Head = $head
        Combined = $combined
        Files = $files
        HasSubmodules = $hasSubmodules
        SubmoduleStatusHash = $submoduleStatusHash
    }
}

function Test-UEFingerprintSchema {
    param([Parameter()][AllowNull()]$Fingerprint)
    return ($null -ne $Fingerprint -and
        [int]$Fingerprint.SchemaVersion -eq 3 -and
        -not [string]::IsNullOrWhiteSpace([string]$Fingerprint.Combined) -and
        -not [string]::IsNullOrWhiteSpace(
            [string]$Fingerprint.SubmoduleStatusHash))
}

function Compare-UEFingerprint {
    param([Parameter(Mandatory)]$A, [Parameter(Mandatory)]$B)
    return ((Test-UEFingerprintSchema $A) -and
        (Test-UEFingerprintSchema $B) -and
        $A.Combined -eq $B.Combined)
}

function Get-UEChangedPathsSince {
    param([Parameter(Mandatory)]$Baseline, [Parameter(Mandatory)]$Current)

    if (-not (Test-UEFingerprintSchema $Baseline) -or
        -not (Test-UEFingerprintSchema $Current)) {
        throw "workspace checkpoint schema is stale; create a fresh verification checkpoint"
    }
    $baselineFiles = ConvertTo-UEFingerprintFileMap $Baseline.Files
    $currentFiles = ConvertTo-UEFingerprintFileMap $Current.Files
    $paths = @($baselineFiles.Keys) + @($currentFiles.Keys) | Sort-Object -Unique
    $changed = foreach ($path in $paths) {
        if (-not $baselineFiles.ContainsKey($path) -or
            -not $currentFiles.ContainsKey($path)) {
            $path
            continue
        }
        $before = $baselineFiles[$path] | ConvertTo-Json -Compress -Depth 8
        $after = $currentFiles[$path] | ConvertTo-Json -Compress -Depth 8
        if ($before -ne $after) { $path }
    }
    return @($changed | Sort-Object -Unique)
}
