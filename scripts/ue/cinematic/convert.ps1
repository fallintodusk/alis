#Requires -Version 5.1
<#
.SYNOPSIS
    Batch-convert ProRes .mov renders to NVENC HEVC .mp4 deliverables.

.DESCRIPTION
    Scans <InputDir> for .mov files. For each one, if <name><Suffix>.mp4 already
    exists, skips it. Otherwise verifies the .mov is stable (not mid-render) and
    encodes it with the production-blessed NVENC HEVC CQ 19 command documented in
    docs/cinematics/ffmpeg.md.

    Safety: never globs blindly. Every .mov goes through a size-stability check
    over -StabilityWaitSeconds before being touched. Mid-render files are skipped
    with a clear message instead of corrupted. See the standing rule in
    docs/cinematics/troubleshooting.md.

.PARAMETER InputDir
    Folder to scan. Defaults to Saved/MovieRenders relative to the project root.

.PARAMETER Suffix
    Appended to the basename to derive the .mp4 output name. Default: _enc.

.PARAMETER StabilityWaitSeconds
    Seconds to wait between two size samples to confirm a .mov is not still
    being written. Default: 5.

.PARAMETER FfmpegPath
    Explicit path to ffmpeg.exe. If omitted, tries Get-Command ffmpeg first,
    then the Kdenlive-bundled fallback at C:\Program Files\Kdenlive\bin\ffmpeg.exe.

.EXAMPLE
    .\convert.ps1
    Convert every new .mov in Saved/MovieRenders.

.EXAMPLE
    .\convert.ps1 -InputDir "E:\some\other\folder" -StabilityWaitSeconds 10
    Same with custom folder and longer stability window.
#>

param(
    [string]$InputDir = "",
    [string]$Suffix = "_enc",
    [int]$StabilityWaitSeconds = 5,
    [int]$MinAgeSeconds = 60,
    [string]$FfmpegPath = ""
)

$ErrorActionPreference = "Stop"

# ============================================================
# Resolve ffmpeg
# ============================================================

if (-not $FfmpegPath) {
    $cmd = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($cmd) {
        $FfmpegPath = $cmd.Source
    } else {
        $fallback = 'C:\Program Files\Kdenlive\bin\ffmpeg.exe'
        if (Test-Path $fallback) {
            $FfmpegPath = $fallback
        } else {
            Write-Host "[ERROR] ffmpeg not found. Install ffmpeg or pass -FfmpegPath." -ForegroundColor Red
            exit 1
        }
    }
}

# ============================================================
# Resolve InputDir
# ============================================================

if (-not $InputDir) {
    # Default: <repo-root>/Saved/MovieRenders. Script lives at scripts/ue/cinematic/convert.ps1.
    $repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
    $InputDir = Join-Path $repoRoot "Saved\MovieRenders"
}

if (-not (Test-Path $InputDir)) {
    Write-Host "[ERROR] InputDir not found: $InputDir" -ForegroundColor Red
    exit 1
}

$InputDir = (Resolve-Path $InputDir).Path

Write-Host "ffmpeg:    $FfmpegPath"
Write-Host "input dir: $InputDir"
Write-Host "suffix:    $Suffix"
Write-Host ""

# ============================================================
# Enumerate work
# ============================================================

$movs = Get-ChildItem -Path $InputDir -Filter *.mov -File | Sort-Object Name
if ($movs.Count -eq 0) {
    Write-Host "No .mov files in $InputDir. Nothing to do."
    exit 0
}

$results = @()   # objects with Name, SourceMB, EncodedMB, Ratio, Seconds, Status
$anyFailed = $false

foreach ($mov in $movs) {
    $outName = $mov.BaseName + $Suffix + ".mp4"
    $outPath = Join-Path $InputDir $outName
    $srcMB   = [math]::Round($mov.Length / 1MB, 1)

    # Stale output check: only skip if the existing _enc.mp4 is non-empty AND
    # newer than the source .mov. Otherwise it's leftover from a crashed encode
    # or a stale render and we re-encode.
    if (Test-Path -LiteralPath $outPath) {
        $out = Get-Item -LiteralPath $outPath
        $src = Get-Item -LiteralPath $mov.FullName
        if ($out.Length -gt 0 -and $out.LastWriteTimeUtc -ge $src.LastWriteTimeUtc) {
            Write-Host "[SKIP ] $($mov.Name) -> $outName already exists" -ForegroundColor DarkGray
            $results += [pscustomobject]@{ Name=$mov.Name; SourceMB=$srcMB; EncodedMB=$null; Ratio=$null; Seconds=$null; Status="already-encoded" }
            continue
        }
        Write-Host "[REENC] $outName is empty or older than source; replacing it." -ForegroundColor Yellow
        Remove-Item -LiteralPath $outPath -Force
    }

    # Stability check - skip mid-render files. Size alone is not enough: a slow
    # MRQ frame can take longer than $StabilityWaitSeconds, leaving the file
    # unchanged-but-still-being-written across the sample window. Require BOTH
    # size and LastWriteTime stable, source age >= $MinAgeSeconds, AND that no
    # other process holds an exclusive lock on it.
    $item1 = Get-Item -LiteralPath $mov.FullName
    $size1 = $item1.Length
    $write1 = $item1.LastWriteTimeUtc
    Start-Sleep -Seconds $StabilityWaitSeconds
    $item2 = Get-Item -LiteralPath $mov.FullName
    $ageSeconds = ((Get-Date).ToUniversalTime() - $item2.LastWriteTimeUtc).TotalSeconds
    if ($item2.Length -ne $size1 -or $item2.LastWriteTimeUtc -ne $write1 -or $ageSeconds -lt $MinAgeSeconds) {
        Write-Host "[SKIP ] $($mov.Name) is not stable yet (size/mtime moving, or age $([int]$ageSeconds)s < ${MinAgeSeconds}s). Try again after render finishes." -ForegroundColor Yellow
        $results += [pscustomobject]@{ Name=$mov.Name; SourceMB=$srcMB; EncodedMB=$null; Ratio=$null; Seconds=$null; Status="mid-render" }
        continue
    }
    try {
        $fs = [System.IO.File]::Open($mov.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
        $fs.Close()
    } catch {
        Write-Host "[SKIP ] $($mov.Name) is still locked/open by another process." -ForegroundColor Yellow
        $results += [pscustomobject]@{ Name=$mov.Name; SourceMB=$srcMB; EncodedMB=$null; Ratio=$null; Seconds=$null; Status="locked" }
        continue
    }

    Write-Host "[ENC  ] $($mov.Name) ($srcMB MB) -> $outName" -ForegroundColor Cyan
    # Encode to a temp file and atomically rename on success. A partial .mp4
    # from a crashed ffmpeg would otherwise look complete on the next run and
    # get [SKIP]ped silently.
    $tmpPath = Join-Path $InputDir ($mov.BaseName + $Suffix + ".tmp.mp4")
    if (Test-Path -LiteralPath $tmpPath) {
        Remove-Item -LiteralPath $tmpPath -Force
    }
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    & $FfmpegPath -hide_banner -loglevel warning -stats -y `
        -i $mov.FullName `
        -c:v hevc_nvenc -preset p7 -tune hq -rc vbr -cq 19 -b:v 0 `
        -spatial_aq 1 -temporal_aq 1 `
        -pix_fmt p010le -tag:v hvc1 -an `
        $tmpPath

    $sw.Stop()
    $exit = $LASTEXITCODE

    $tmpOk = (Test-Path -LiteralPath $tmpPath) -and ((Get-Item -LiteralPath $tmpPath).Length -gt 0)
    if ($exit -ne 0 -or -not $tmpOk) {
        if (Test-Path -LiteralPath $tmpPath) {
            Remove-Item -LiteralPath $tmpPath -Force -ErrorAction SilentlyContinue
        }
        Write-Host "[FAIL ] $($mov.Name) - ffmpeg exit $exit" -ForegroundColor Red
        $anyFailed = $true
        $results += [pscustomobject]@{ Name=$mov.Name; SourceMB=$srcMB; EncodedMB=$null; Ratio=$null; Seconds=[int]$sw.Elapsed.TotalSeconds; Status="failed" }
        continue
    }

    Move-Item -LiteralPath $tmpPath -Destination $outPath -Force
    $dstMB = [math]::Round((Get-Item -LiteralPath $outPath).Length / 1MB, 1)
    $ratio = if ($dstMB -gt 0) { [math]::Round($srcMB / $dstMB, 1) } else { 0 }
    $results += [pscustomobject]@{ Name=$mov.Name; SourceMB=$srcMB; EncodedMB=$dstMB; Ratio=$ratio; Seconds=[int]$sw.Elapsed.TotalSeconds; Status="encoded" }
}

# ============================================================
# Summary
# ============================================================

Write-Host ""
Write-Host "=== SUMMARY ===" -ForegroundColor Cyan
$fmt = "{0,-32} {1,10} {2,10} {3,8} {4,7}  {5}"
Write-Host ($fmt -f "name", "source MB", "enc MB", "ratio", "sec", "status")
foreach ($r in $results) {
    $encStr   = if ($null -ne $r.EncodedMB) { "$($r.EncodedMB)" } else { "-" }
    $ratioStr = if ($null -ne $r.Ratio)     { "$($r.Ratio)x" }    else { "-" }
    $secStr   = if ($null -ne $r.Seconds)   { "$($r.Seconds)" }   else { "-" }
    Write-Host ($fmt -f $r.Name, $r.SourceMB, $encStr, $ratioStr, $secStr, $r.Status)
}

$encoded = $results | Where-Object { $_.Status -eq "encoded" }
if ($encoded.Count -gt 0) {
    $totalSrc  = ($encoded | Measure-Object -Property SourceMB  -Sum).Sum
    $totalDst  = ($encoded | Measure-Object -Property EncodedMB -Sum).Sum
    $totalSecs = ($encoded | Measure-Object -Property Seconds   -Sum).Sum
    $totalRatio = if ($totalDst -gt 0) { [math]::Round($totalSrc / $totalDst, 1) } else { 0 }
    Write-Host ""
    Write-Host ("Encoded $($encoded.Count) file(s): $([math]::Round($totalSrc,1)) MB -> $([math]::Round($totalDst,1)) MB ($($totalRatio)x smaller) in ${totalSecs}s")
}

if ($anyFailed) { exit 1 } else { exit 0 }
