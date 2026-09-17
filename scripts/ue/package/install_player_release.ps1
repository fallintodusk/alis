#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = "Stop"
$EmbeddedArchiveManifestJson = '__ALIS_PLAYER_ARCHIVE_MANIFEST_JSON__'

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-ArchivePart {
    param(
        [Parameter(Mandatory = $true)]$Part,
        [Parameter(Mandatory = $true)][string]$Directory
    )

    $Name = [string]$Part.name
    if ([string]::IsNullOrWhiteSpace($Name) -or [IO.Path]::GetFileName($Name) -cne $Name) {
        throw "Unsafe player archive part name: $Name"
    }
    $Path = Join-Path $Directory $Name
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Player archive part is missing: $Name"
    }
    $Item = Get-Item -LiteralPath $Path
    if ($Item.Length -ne [long]$Part.byte_size -or (Get-Sha256 -Path $Path) -ne ([string]$Part.sha256).ToLowerInvariant()) {
        throw "Player archive part identity mismatch: $Name"
    }
    return $Item.FullName
}

$ReleaseRoot = $PSScriptRoot
$Report = if ($EmbeddedArchiveManifestJson.StartsWith("__ALIS_PLAYER_")) {
    $ReportPath = Join-Path $ReleaseRoot "player-archive.json"
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
        throw "Player archive manifest is missing: $ReportPath"
    }
    Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
} else {
    $EmbeddedArchiveManifestJson | ConvertFrom-Json
}
if ($Report.schema -notin @("alis-player-archive-v1", "alis-game-archive-v1") -or
    $Report.status -ne "accepted" -or @($Report.parts).Count -lt 1) {
    throw "Unsupported player archive manifest."
}
$PartPaths = @($Report.parts | ForEach-Object {
    Assert-ArchivePart -Part $_ -Directory $ReleaseRoot
})
$FirstName = [string]$Report.parts[0].name
$Version = if ($FirstName -match "ALIS_Win64_v(?<version>[0-9]+\.[0-9]+\.[0-9]+)\.zip") {
    $Matches.version
} else {
    "Release"
}

if ([string]::IsNullOrWhiteSpace($Destination)) {
    $DefaultParent = Split-Path -Parent $ReleaseRoot
    $DefaultDestination = Join-Path $DefaultParent "ALIS_v$Version"
    $Answer = Read-Host "Install location [$DefaultDestination] (press Enter to accept)"
    $Destination = if ([string]::IsNullOrWhiteSpace($Answer)) { $DefaultDestination } else { $Answer }
}

$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) {
    throw "Install destination already exists: $Destination"
}
$DestinationParent = Split-Path -Parent $Destination
New-Item -ItemType Directory -Path $DestinationParent -Force | Out-Null
$OperationId = [guid]::NewGuid().ToString("N")
$Staging = Join-Path $DestinationParent (".alis-player-install-" + $OperationId)
$JoinedArchive = Join-Path $DestinationParent (".alis-player-" + $OperationId + ".zip")
$CreatedJoinedArchive = $false

try {
    $ArchivePath = $PartPaths[0]
    if ($PartPaths.Count -gt 1 -or $FirstName -match "\.zip\.001$") {
        $Output = [IO.File]::Open($JoinedArchive, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        try {
            foreach ($PartPath in $PartPaths) {
                $Input = [IO.File]::OpenRead($PartPath)
                try {
                    $Input.CopyTo($Output)
                } finally {
                    $Input.Dispose()
                }
            }
        } finally {
            $Output.Dispose()
        }
        $ArchivePath = $JoinedArchive
        $CreatedJoinedArchive = $true
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -ItemType Directory -Path $Staging | Out-Null
    $StagingPrefix = $Staging.TrimEnd("\", "/") + [IO.Path]::DirectorySeparatorChar
    $Archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($Entry in $Archive.Entries) {
            if ([string]::IsNullOrWhiteSpace($Entry.FullName)) {
                throw "Player archive contains an empty path."
            }
            $Target = [IO.Path]::GetFullPath((Join-Path $Staging $Entry.FullName))
            if (-not $Target.StartsWith($StagingPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Player archive contains an unsafe path: $($Entry.FullName)"
            }
        }
    } finally {
        $Archive.Dispose()
    }
    [IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $Staging)
    $Executables = @(Get-ChildItem -LiteralPath $Staging -Recurse -File -Filter "Alis.exe")
    if ($Executables.Count -ne 1) {
        throw "Installed player archive must contain exactly one Alis.exe."
    }
    [IO.Directory]::Move($Staging, $Destination)
    $InstalledExecutable = Join-Path $Destination $Executables[0].FullName.Substring($StagingPrefix.Length)
    Write-Host "[OK] ALIS player installed: $Destination"
    Write-Host "[OK] Run: $InstalledExecutable"
} finally {
    if (Test-Path -LiteralPath $Staging) {
        Remove-Item -LiteralPath $Staging -Recurse -Force
    }
    if ($CreatedJoinedArchive -and (Test-Path -LiteralPath $JoinedArchive)) {
        Remove-Item -LiteralPath $JoinedArchive -Force
    }
}
