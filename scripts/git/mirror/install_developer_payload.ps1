#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [string]$ReleaseDir,
    [string]$ManifestPath,
    [switch]$RequireReleaseSignature
)

$ErrorActionPreference = "Stop"

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Resolve-SafeProjectPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $Normalized = $RelativePath.Replace("\", "/")
    if ([string]::IsNullOrWhiteSpace($Normalized) -or
        $Normalized.StartsWith("/") -or
        $Normalized -match "^[A-Za-z]:" -or
        $Normalized.Split("/") -contains "..") {
        throw "Unsafe project-relative path: $RelativePath"
    }

    $ResolvedRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd("\", "/")
    $Candidate = [System.IO.Path]::GetFullPath((Join-Path $ResolvedRoot $Normalized.Replace("/", [System.IO.Path]::DirectorySeparatorChar)))
    $Prefix = $ResolvedRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $Candidate.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes the project root: $RelativePath"
    }
    return $Candidate
}

function Assert-FileIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][long]$ByteSize,
        [Parameter(Mandatory = $true)][string]$Sha256
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    $Item = Get-Item -LiteralPath $Path
    if ($Item.Length -ne $ByteSize) {
        throw "File size mismatch: $Path"
    }
    if ((Get-Sha256 -Path $Path) -ne $Sha256.ToLowerInvariant()) {
        throw "File hash mismatch: $Path"
    }
}

if ($ReleaseDir) {
    $ReleaseRoot = (Resolve-Path -LiteralPath $ReleaseDir).Path
} elseif ($ManifestPath) {
    $ReleaseRoot = Split-Path -Parent (Resolve-Path -LiteralPath $ManifestPath).Path
} else {
    $ReleaseRoot = $PSScriptRoot
}

$TrustedReleaseVerifier = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\ue\package\verify_release.ps1"))
$HasSignedManifest = (Test-Path -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt")) -and
    (Test-Path -LiteralPath (Join-Path $ReleaseRoot "SHA256SUMS.txt.asc"))
if ($HasSignedManifest) {
    if (-not (Test-Path -LiteralPath $TrustedReleaseVerifier -PathType Leaf)) {
        throw "Run the installer from the matching trusted source checkout, not the downloaded release copy."
    }
    & $TrustedReleaseVerifier -ReleaseDir $ReleaseRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Release signature verification failed."
    }
} elseif ($RequireReleaseSignature) {
    throw "This folder has no signed SHA256SUMS.txt release manifest."
} else {
    Write-Warning "Release signature was not required; payload hashes will still be verified."
}

if ($ManifestPath) {
    $ManifestPath = (Resolve-Path -LiteralPath $ManifestPath).Path
    $ReleasePrefix = [System.IO.Path]::GetFullPath($ReleaseRoot).TrimEnd("\", "/") +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $ManifestPath.StartsWith($ReleasePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "ManifestPath must be inside the authenticated release directory."
    }
}

if (-not $ManifestPath) {
    $Candidates = @(Get-ChildItem -LiteralPath $ReleaseRoot -Filter "*.developer-payload.json" -File)
    if ($Candidates.Count -ne 1) {
        throw "Expected exactly one *.developer-payload.json next to this installer."
    }
    $ManifestPath = $Candidates[0].FullName
}
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

if ($Manifest.schema_version -ne 2 -or -not $Manifest.payload_id -or
    -not $Manifest.public_source.tag -or $Manifest.public_source.revision -notmatch "^[0-9a-f]{40}$") {
    throw "Unsupported developer payload manifest."
}
if (-not $ProjectRoot) {
    $SourceCheckout = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
    if (Test-Path -LiteralPath (Join-Path $SourceCheckout "Alis.uproject") -PathType Leaf) {
        $ProjectRoot = $SourceCheckout
    } else {
        $ProjectRoot = (Get-Location).Path
    }
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "Project root does not exist: $ProjectRoot"
}

foreach ($Marker in $Manifest.project_markers) {
    $MarkerPath = Resolve-SafeProjectPath -Root $ProjectRoot -RelativePath $Marker.path
    if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
        throw "This is not the matching ALIS project root. Missing: $($Marker.path)"
    }
}

$GitRoot = (& git -C $ProjectRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or -not $GitRoot) {
    throw "ProjectRoot must be an exact public Git checkout."
}
$ResolvedGitRoot = [System.IO.Path]::GetFullPath($GitRoot).TrimEnd("\", "/")
if (-not $ResolvedGitRoot.Equals($ProjectRoot.TrimEnd("\", "/"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "ProjectRoot must be the public Git checkout root: $ResolvedGitRoot"
}
$ActualRevision = (& git -C $ProjectRoot rev-parse HEAD 2>$null | Out-String).Trim().ToLowerInvariant()
if ($LASTEXITCODE -ne 0 -or $ActualRevision -ne $Manifest.public_source.revision) {
    throw "Public source revision mismatch. Expected $($Manifest.public_source.revision), found $ActualRevision."
}
$TaggedRevision = (& git -C $ProjectRoot rev-parse "refs/tags/$($Manifest.public_source.tag)^{commit}" 2>$null | Out-String).Trim().ToLowerInvariant()
if ($LASTEXITCODE -ne 0 -or $TaggedRevision -ne $ActualRevision) {
    throw "Public source tag $($Manifest.public_source.tag) does not identify the checked-out revision."
}
$SourceStatus = @(& git -C $ProjectRoot status --porcelain=v1 --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0 -or ($SourceStatus | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
    throw "Public source checkout must be clean before developer payload installation."
}

$TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("alis-developer-payload-" + [guid]::NewGuid().ToString("N"))
$TempArchive = Join-Path $TempRoot $Manifest.archive.logical_name
$StagingRoot = Join-Path $TempRoot "staging"
$InstalledPaths = New-Object System.Collections.Generic.List[string]

try {
    New-Item -ItemType Directory -Path $StagingRoot -Force | Out-Null
    $ArchiveStream = [System.IO.File]::Open($TempArchive, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write)
    try {
        foreach ($Part in $Manifest.archive.parts) {
            $PartPath = Join-Path $ReleaseRoot $Part.name
            Assert-FileIdentity -Path $PartPath -ByteSize $Part.byte_size -Sha256 $Part.sha256
            $PartStream = [System.IO.File]::OpenRead($PartPath)
            try {
                $PartStream.CopyTo($ArchiveStream)
            } finally {
                $PartStream.Dispose()
            }
        }
    } finally {
        $ArchiveStream.Dispose()
    }
    Assert-FileIdentity -Path $TempArchive -ByteSize $Manifest.archive.byte_size -Sha256 $Manifest.archive.sha256

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Expected = @{}
    foreach ($Entry in $Manifest.entries) {
        $Relative = $Entry.path.Replace("\", "/")
        if ($Expected.ContainsKey($Relative)) {
            throw "Duplicate manifest entry: $Relative"
        }
        $Expected[$Relative] = $Entry
    }

    $Archive = [System.IO.Compression.ZipFile]::OpenRead($TempArchive)
    try {
        if ($Archive.Entries.Count -ne $Expected.Count) {
            throw "Archive entry count does not match the payload manifest."
        }
        foreach ($ZipEntry in $Archive.Entries) {
            $Relative = $ZipEntry.FullName.Replace("\", "/")
            if (-not $Expected.ContainsKey($Relative)) {
                throw "Unexpected archive entry: $Relative"
            }
            if ($ZipEntry.Length -ne $Expected[$Relative].byte_size) {
                throw "Archive entry size mismatch: $Relative"
            }
            $StagePath = Resolve-SafeProjectPath -Root $StagingRoot -RelativePath $Relative
            $StageParent = Split-Path -Parent $StagePath
            New-Item -ItemType Directory -Path $StageParent -Force | Out-Null
            $InputStream = $ZipEntry.Open()
            $OutputStream = [System.IO.File]::Open($StagePath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write)
            try {
                $InputStream.CopyTo($OutputStream)
            } finally {
                $OutputStream.Dispose()
                $InputStream.Dispose()
            }
        }
    } finally {
        $Archive.Dispose()
    }

    foreach ($Entry in $Manifest.entries) {
        $StagePath = Resolve-SafeProjectPath -Root $StagingRoot -RelativePath $Entry.path
        Assert-FileIdentity -Path $StagePath -ByteSize $Entry.byte_size -Sha256 $Entry.sha256
        $TargetPath = Resolve-SafeProjectPath -Root $ProjectRoot -RelativePath $Entry.path
        if (Test-Path -LiteralPath $TargetPath -PathType Leaf) {
            Assert-FileIdentity -Path $TargetPath -ByteSize $Entry.byte_size -Sha256 $Entry.sha256
        } elseif (Test-Path -LiteralPath $TargetPath) {
            throw "Payload target is not a file: $($Entry.path)"
        }
    }

    $Installed = 0
    foreach ($Entry in $Manifest.entries) {
        $TargetPath = Resolve-SafeProjectPath -Root $ProjectRoot -RelativePath $Entry.path
        if (Test-Path -LiteralPath $TargetPath -PathType Leaf) {
            continue
        }
        $TargetParent = Split-Path -Parent $TargetPath
        New-Item -ItemType Directory -Path $TargetParent -Force | Out-Null
        $StagePath = Resolve-SafeProjectPath -Root $StagingRoot -RelativePath $Entry.path
        $InstalledPaths.Add($TargetPath) | Out-Null
        [System.IO.File]::Copy($StagePath, $TargetPath, $false)
        Assert-FileIdentity -Path $TargetPath -ByteSize $Entry.byte_size -Sha256 $Entry.sha256
        $Installed++
    }

    $ReceiptRoot = Join-Path $ProjectRoot "Saved\DeveloperPayload"
    New-Item -ItemType Directory -Path $ReceiptRoot -Force | Out-Null
    $Receipt = [ordered]@{
        schema_version = 1
        payload_id = $Manifest.payload_id
        release_version = $Manifest.release_version
        public_source = $Manifest.public_source
        installed_file_count = $Installed
        verified_entry_count = $Expected.Count
        installed_at_utc = [DateTime]::UtcNow.ToString("o")
    }
    $ReceiptPath = Join-Path $ReceiptRoot ($Manifest.payload_id + ".json")
    ($Receipt | ConvertTo-Json -Depth 4) + "`n" | Set-Content -LiteralPath $ReceiptPath -Encoding UTF8
    Write-Host "[OK] ALIS developer payload installed and verified."
    Write-Host "[OK] Project root: $ProjectRoot"
    Write-Host "[OK] Receipt: $ReceiptPath"
} catch {
    foreach ($InstalledPath in $InstalledPaths) {
        Remove-Item -LiteralPath $InstalledPath -Force -ErrorAction SilentlyContinue
    }
    throw
} finally {
    if (Test-Path -LiteralPath $TempRoot) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
