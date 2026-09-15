#Requires -Version 5.1

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageDir = Split-Path -Parent $ScriptDir
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageDir))
$TestParent = Join-Path $ProjectRoot "tmp\release\player-installer-test"
$TestRoot = Join-Path $TestParent ([guid]::NewGuid().ToString("N"))
$PlayerDir = Join-Path $TestRoot "release"
$PackageRoot = Join-Path $TestRoot "package"
$Destination = Join-Path $TestRoot "installed"

New-Item -ItemType Directory -Path $PlayerDir, $PackageRoot -Force | Out-Null

function Write-RenderedInstaller {
    param([string]$ReportPath)

    $Marker = "__ALIS_PLAYER_ARCHIVE_MANIFEST_JSON__"
    $Template = Get-Content -LiteralPath (Join-Path $PackageDir "install_player_release.ps1") -Raw
    if (-not $Template.Contains(
            'Read-Host "Install location [$DefaultDestination] (press Enter to accept)"')) {
        throw "Player installer prompt does not explain how to accept the default location."
    }
    if ($Template.IndexOf($Marker) -lt 0 -or $Template.IndexOf($Marker) -ne $Template.LastIndexOf($Marker)) {
        throw "Player installer template marker is not unique."
    }
    $ManifestJson = (Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json |
        ConvertTo-Json -Depth 10 -Compress).Replace("'", "''")
    $Template.Replace($Marker, $ManifestJson) |
        Set-Content -LiteralPath (Join-Path $PlayerDir "INSTALL_ALIS_PLAYER.ps1") -Encoding UTF8
}

try {
    "fixture executable" | Set-Content -LiteralPath (Join-Path $PackageRoot "Alis.exe") -Encoding Ascii
    New-Item -ItemType Directory -Path (Join-Path $PackageRoot "Windows\Alis\Content") -Force | Out-Null
    "fixture content" | Set-Content -LiteralPath (Join-Path $PackageRoot "Windows\Alis\Content\fixture.bin") -Encoding Ascii

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Archive = Join-Path $TestRoot "ALIS_Win64_v9.8.7.zip"
    [IO.Compression.ZipFile]::CreateFromDirectory($PackageRoot, $Archive)
    $Bytes = [IO.File]::ReadAllBytes($Archive)
    $Middle = [Math]::Floor($Bytes.Length / 2)
    $PartOne = Join-Path $PlayerDir "ALIS_Win64_v9.8.7.zip.001"
    $PartTwo = Join-Path $PlayerDir "ALIS_Win64_v9.8.7.zip.002"
    [IO.File]::WriteAllBytes($PartOne, $Bytes[0..($Middle - 1)])
    [IO.File]::WriteAllBytes($PartTwo, $Bytes[$Middle..($Bytes.Length - 1)])
    $Parts = @($PartOne, $PartTwo) | ForEach-Object {
        $Item = Get-Item -LiteralPath $_
        [ordered]@{
            name = $Item.Name
            byte_size = $Item.Length
            sha256 = (Get-FileHash -LiteralPath $Item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $ReportPath = Join-Path $PlayerDir "player-archive.json"
    [ordered]@{
        schema = "alis-player-archive-v1"
        status = "accepted"
        parts = $Parts
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding Ascii
    Write-RenderedInstaller -ReportPath $ReportPath
    Remove-Item -LiteralPath $ReportPath

    & (Join-Path $PlayerDir "INSTALL_ALIS_PLAYER.ps1") -Destination $Destination
    if (-not (Test-Path -LiteralPath (Join-Path $Destination "Alis.exe") -PathType Leaf)) {
        throw "Player installer did not extract Alis.exe."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Destination "Windows\Alis\Content\fixture.bin") -PathType Leaf)) {
        throw "Player installer did not extract the complete archive."
    }

    [IO.File]::WriteAllBytes($PartTwo, [byte[]](1, 2, 3))
    $Rejected = $false
    $RejectedDestination = Join-Path $TestRoot "rejected"
    try {
        & (Join-Path $PlayerDir "INSTALL_ALIS_PLAYER.ps1") -Destination $RejectedDestination
    } catch {
        $Rejected = $_.Exception.Message -like "*identity mismatch*"
    }
    if (-not $Rejected -or (Test-Path -LiteralPath $RejectedDestination)) {
        throw "Corrupted player archive part was not rejected before installation."
    }

    $UnsafeArchive = Join-Path $PlayerDir "ALIS_Win64_v9.8.7.zip"
    $UnsafeStream = [IO.File]::Open($UnsafeArchive, [IO.FileMode]::Create, [IO.FileAccess]::ReadWrite)
    $UnsafeZip = New-Object IO.Compression.ZipArchive($UnsafeStream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $UnsafeEntry = $UnsafeZip.CreateEntry("../escaped.txt")
        $Writer = New-Object IO.StreamWriter($UnsafeEntry.Open())
        try {
            $Writer.Write("unsafe")
        } finally {
            $Writer.Dispose()
        }
    } finally {
        $UnsafeZip.Dispose()
        $UnsafeStream.Dispose()
    }
    $UnsafeItem = Get-Item -LiteralPath $UnsafeArchive
    [ordered]@{
        schema = "alis-player-archive-v1"
        status = "accepted"
        parts = @([ordered]@{
            name = $UnsafeItem.Name
            byte_size = $UnsafeItem.Length
            sha256 = (Get-FileHash -LiteralPath $UnsafeItem.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding Ascii
    Write-RenderedInstaller -ReportPath $ReportPath
    Remove-Item -LiteralPath $ReportPath
    $UnsafeRejected = $false
    $UnsafeDestination = Join-Path $TestRoot "unsafe"
    try {
        & (Join-Path $PlayerDir "INSTALL_ALIS_PLAYER.ps1") -Destination $UnsafeDestination
    } catch {
        $UnsafeRejected = $_.Exception.Message -like "*unsafe path*"
    }
    if (-not $UnsafeRejected -or
        (Test-Path -LiteralPath $UnsafeDestination) -or
        (Test-Path -LiteralPath (Join-Path $TestRoot "escaped.txt"))) {
        throw "Unsafe player archive path was not rejected before extraction."
    }

    Write-Host "[OK] Player release installer joined, verified, and extracted split ZIP parts"
} finally {
    if (Test-Path -LiteralPath $TestRoot) {
        $ResolvedParent = [IO.Path]::GetFullPath($TestParent).TrimEnd('\', '/')
        $ResolvedTestRoot = (Resolve-Path -LiteralPath $TestRoot).Path
        if (-not $ResolvedTestRoot.StartsWith(
                $ResolvedParent + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected test path: $ResolvedTestRoot"
        }
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
