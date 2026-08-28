# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [string]$RequestPath = 'scripts\ue\cinematic\requests\kazan_release_v1.json'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$driver = Join-Path $PSScriptRoot 'release_capture_editor.py'
$releaseBinding = Join-Path $PSScriptRoot 'release_binding.ps1'
$runId = [Guid]::NewGuid().ToString('N')
$operationId = "project_cinematic_release_capture_$runId"
$ownerRoot = Join-Path $projectRoot 'tmp\cinematic\release_capture'
$workRoot = Join-Path $ownerRoot $runId
$renderRoot = Join-Path $workRoot 'render'
$statusPath = Join-Path $workRoot 'status.json'
$finalRoot = Join-Path $projectRoot 'Saved\CinematicRelease\Kazan'
$currentRoot = Join-Path $finalRoot 'Current'
$previousRoot = Join-Path $finalRoot 'Previous'
$candidateRoot = Join-Path $finalRoot "Candidate-$runId"
$failureRoot = Join-Path $projectRoot "Saved\Validation\CinematicRelease\Failures\$runId"
$promoted = $false

function Assert-Capture {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Get-CaptureSourceStateDigest {
    $parts = [Collections.Generic.List[string]]::new()
    $parts.Add((@(& git -C $projectRoot diff --binary --no-ext-diff HEAD) -join "`n"))
    Assert-Capture ($LASTEXITCODE -eq 0) 'Unable to read tracked source state.'
    $untracked = @(& git -C $projectRoot ls-files --others --exclude-standard | Sort-Object)
    Assert-Capture ($LASTEXITCODE -eq 0) 'Unable to read untracked source state.'
    foreach ($relative in $untracked) {
        $path = Join-Path $projectRoot $relative
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        $parts.Add("$relative|$hash")
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($parts -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Resolve-CaptureOwnedPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Owner
    )
    $target = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $root = [IO.Path]::GetFullPath($Owner).TrimEnd('\', '/')
    Assert-Capture ($target.StartsWith(
            $root + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) `
        "Capture path escaped its owner: $target"
    return $target
}

function Remove-CaptureOwnedTree {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Owner
    )
    $target = Resolve-CaptureOwnedPath -Path $Path -Owner $Owner
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

function Get-CaptureTool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Fallback
    )
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    if (Test-Path -LiteralPath $Fallback -PathType Leaf) {
        return $Fallback
    }
    throw "$Name is unavailable."
}

$resolvedRequest = if ([IO.Path]::IsPathRooted($RequestPath)) {
    [IO.Path]::GetFullPath($RequestPath)
}
else {
    [IO.Path]::GetFullPath((Join-Path $projectRoot $RequestPath))
}
$resolvedProject = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\', '/')
Assert-Capture ($resolvedRequest.StartsWith(
        $resolvedProject + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) `
    'Capture request must live inside the project.'
Assert-Capture (Test-Path -LiteralPath $resolvedRequest -PathType Leaf) `
    "Capture request does not exist: $resolvedRequest"
Assert-Capture (Test-Path -LiteralPath $driver -PathType Leaf) `
    "Editor capture driver does not exist: $driver"
Assert-Capture (Test-Path -LiteralPath $releaseBinding -PathType Leaf) `
    "Release binding helper does not exist: $releaseBinding"
. $releaseBinding

$requestText = Get-Content -LiteralPath $resolvedRequest -Raw
Assert-Capture ($requestText -match '^\s*\{\s*"\$schema"\s*:') `
    'Capture request must declare $schema as its first field.'
$request = $requestText | ConvertFrom-Json
foreach ($field in @(
        'schema_version', 'capture_id', 'map_package', 'sequence', 'preset',
        'camera_class', 'playback_start', 'playback_end', 'scalability_quality',
        'timeout_seconds', 'release_acceptance', 'package_root')) {
    Assert-Capture ($null -ne $request.$field) "Capture request is missing '$field'."
}
Assert-Capture ($request.schema_version -eq 2) 'Unsupported capture request version.'
Assert-Capture ($request.scalability_quality -eq 2) `
    'Release capture must match the accepted High scalability tier.'
Assert-Capture ($request.playback_end -gt $request.playback_start) `
    'Capture playback range is empty.'

$expectedPackageRoot = 'Saved\PackageRelease\KazanPlayableTour\Candidate'
Assert-Capture ([string]$request.package_root -ceq `
        'Saved/PackageRelease/KazanPlayableTour/Candidate') `
    'Release capture package root must be the accepted playable-tour Candidate.'
$sourceStateHash = Get-CaptureSourceStateDigest
$binding = Test-ProjectCinematicReleaseBinding `
    -ProjectRoot $projectRoot `
    -ReleaseAcceptancePath ([string]$request.release_acceptance) `
    -ExpectedPackageRoot $expectedPackageRoot `
    -CurrentSourceStateSha256 $sourceStateHash
$releaseCompositePath = $binding.release_composite_path
$packageRoot = $binding.package_root
$releaseComposite = $binding.release_composite

$existingEditors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -match [regex]::Escape($projectRoot) })
Assert-Capture ($existingEditors.Count -eq 0) `
    'A project editor is already running. Close it before the isolated release capture.'

. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
$editor = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor.exe'
Assert-Capture (Test-Path -LiteralPath $editor -PathType Leaf) `
    'Launcher UnrealEditor.exe is unavailable.'

New-Item -ItemType Directory -Path $renderRoot -Force | Out-Null
$environmentBefore = @{}
foreach ($name in @(
        'PROJECT_CINEMATIC_CAPTURE_REQUEST',
        'PROJECT_CINEMATIC_CAPTURE_STATUS',
        'PROJECT_CINEMATIC_CAPTURE_OUTPUT',
        'PROJECT_CINEMATIC_CAPTURE_OPERATION')) {
    $environmentBefore[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

$env:PROJECT_CINEMATIC_CAPTURE_REQUEST = $resolvedRequest
$env:PROJECT_CINEMATIC_CAPTURE_STATUS = $statusPath
$env:PROJECT_CINEMATIC_CAPTURE_OUTPUT = $renderRoot
$env:PROJECT_CINEMATIC_CAPTURE_OPERATION = $operationId
$launchUtc = [DateTime]::UtcNow
$arguments = @(
    '/c',
    'scripts\ue\run\run_editor.bat',
    $request.map_package,
    ('-ExecCmds="py {0}"' -f $driver),
    '-unattended',
    '-RenderOffscreen',
    '-NoSplash',
    '-NoSound',
    '-log'
)
$hostProcess = Start-Process -FilePath 'cmd.exe' -ArgumentList $arguments `
    -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
foreach ($name in $environmentBefore.Keys) {
    [Environment]::SetEnvironmentVariable($name, $environmentBefore[$name], 'Process')
}

$deadline = [DateTime]::UtcNow.AddSeconds([int]$request.timeout_seconds)
$editorProcess = $null
do {
    Start-Sleep -Milliseconds 500
    $editorProcess = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
        Where-Object {
            $_.CreationDate.ToUniversalTime() -ge $launchUtc.AddSeconds(-2) -and
            $_.CommandLine -match [regex]::Escape($request.map_package)
        } |
        Select-Object -First 1
} while (-not $editorProcess -and [DateTime]::UtcNow -lt $deadline)
Assert-Capture ($null -ne $editorProcess) 'Dedicated capture editor did not start.'

$editorPid = [int]$editorProcess.ProcessId
$peakWorkingSet = [int64]0
$lastState = ''
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $process = Get-Process -Id $editorPid -ErrorAction SilentlyContinue
        if ($process) {
            $peakWorkingSet = [Math]::Max($peakWorkingSet, [int64]$process.WorkingSet64)
            if ($process.MainWindowTitle -eq 'Restore Packages') {
                throw 'Unreal recovery dialog blocked the unattended capture.'
            }
        }
        if (Test-Path -LiteralPath $statusPath -PathType Leaf) {
            try {
                $status = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
                if ($status.state -ne $lastState) {
                    Write-Host "[ProjectCinematic] state=$($status.state)"
                    $lastState = $status.state
                }
                if ($status.state -in @('finished', 'error')) {
                    break
                }
            }
            catch [System.ArgumentException] {
            }
        }
        if (-not $process -and -not (Test-Path -LiteralPath $statusPath)) {
            throw 'Capture editor exited before publishing status.'
        }
        Start-Sleep -Seconds 1
    }
    Assert-Capture ([DateTime]::UtcNow -lt $deadline) 'Release capture timed out.'
    Assert-Capture (Test-Path -LiteralPath $statusPath -PathType Leaf) `
        'Release capture did not publish a status file.'
    $status = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
    Assert-Capture ($status.state -ceq 'finished' -and $status.success) `
        "Release capture failed: $($status.reason) $($status.error_reason) $(@($status.output_errors) -join '; ')"
    Assert-Capture ((Get-CaptureSourceStateDigest) -ceq $sourceStateHash) `
        'Source state changed during the release capture.'

    $movFiles = @(Get-ChildItem -LiteralPath $renderRoot -Recurse -File -Filter '*.mov')
    Assert-Capture ($movFiles.Count -eq 1) `
        "Expected exactly one ProRes master; found $($movFiles.Count)."
    Assert-Capture ($movFiles[0].Length -gt 1MB) 'ProRes master is unexpectedly small.'

    $ffprobe = Get-CaptureTool -Name 'ffprobe' `
        -Fallback 'C:\Program Files\Kdenlive\bin\ffprobe.exe'
    $probeJson = @(& $ffprobe -v error -count_frames -select_streams v:0 `
        -show_entries 'stream=nb_read_frames,duration,width,height' -of json $movFiles[0].FullName)
    Assert-Capture ($LASTEXITCODE -eq 0) 'ffprobe could not inspect the ProRes master.'
    $probe = ($probeJson -join "`n") | ConvertFrom-Json
    Assert-Capture ($probe.streams.Count -eq 1) 'ProRes master has no video stream.'
    Assert-Capture ([int]$probe.streams[0].width -gt 0 -and [int]$probe.streams[0].height -gt 0) `
        'ProRes master has invalid dimensions.'
    Assert-Capture ([int]$probe.streams[0].nb_read_frames -ge 180) `
        'ProRes master is missing expected frames.'

    New-Item -ItemType Directory -Path $candidateRoot -Force | Out-Null
    $masterTarget = Join-Path $candidateRoot 'KazanRelease_v1.mov'
    Copy-Item -LiteralPath $movFiles[0].FullName -Destination $masterTarget
    $ffmpeg = Get-CaptureTool -Name 'ffmpeg' `
        -Fallback 'C:\Program Files\Kdenlive\bin\ffmpeg.exe'
    $preview = Join-Path $candidateRoot 'preview.png'
    & $ffmpeg -hide_banner -loglevel error -ss 4 -i $masterTarget -frames:v 1 -y $preview
    Assert-Capture ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $preview -PathType Leaf)) `
        'Could not extract the visual review frame.'

    Copy-Item -LiteralPath $statusPath -Destination (Join-Path $candidateRoot 'editor-status.json')
    $editorLog = Join-Path $projectRoot 'Saved\Logs\Alis.log'
    if (Test-Path -LiteralPath $editorLog -PathType Leaf) {
        Copy-Item -LiteralPath $editorLog -Destination (Join-Path $candidateRoot 'editor.log')
    }
    $receipt = [ordered]@{
        schema_version = 1
        status = 'technically_accepted'
        operation_id = $operationId
        capture_id = $request.capture_id
        source_state_sha256 = $sourceStateHash
        request_sha256 = (Get-FileHash -LiteralPath $resolvedRequest -Algorithm SHA256).Hash.ToLowerInvariant()
        sequence_sha256 = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Content\Cinematics\Kazan\LS_KazanRelease_v1.uasset') -Algorithm SHA256).Hash.ToLowerInvariant()
        preset_sha256 = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Content\Cinematics\MP_Config_Dev.uasset') -Algorithm SHA256).Hash.ToLowerInvariant()
        release_operation_id = $releaseComposite.operation_id
        release_acceptance_sha256 = (Get-FileHash -LiteralPath $binding.acceptance_path -Algorithm SHA256).Hash.ToLowerInvariant()
        release_composite_sha256 = $binding.release_composite_sha256
        shipping_executable_sha256 = $binding.shipping_executable_sha256
        shipping_package_sha256 = $binding.package_tree_sha256
        master_sha256 = (Get-FileHash -LiteralPath $masterTarget -Algorithm SHA256).Hash.ToLowerInvariant()
        preview_sha256 = (Get-FileHash -LiteralPath $preview -Algorithm SHA256).Hash.ToLowerInvariant()
        peak_editor_working_set_bytes = $peakWorkingSet
        ffprobe = $probe.streams[0]
        package_root = $packageRoot
        cleanup_result = 'owner_tmp_cleaned_after_promotion'
    }
    [IO.File]::WriteAllText(
        (Join-Path $candidateRoot 'receipt.json'),
        ($receipt | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))

    if (Test-Path -LiteralPath $previousRoot) {
        Remove-CaptureOwnedTree -Path $previousRoot -Owner $finalRoot
    }
    if (Test-Path -LiteralPath $currentRoot) {
        Move-Item -LiteralPath $currentRoot -Destination $previousRoot
    }
    Move-Item -LiteralPath $candidateRoot -Destination $currentRoot
    $promoted = $true
    Write-Output (Get-Content -LiteralPath (Join-Path $currentRoot 'receipt.json') -Raw)
}
finally {
    $process = Get-Process -Id $editorPid -ErrorAction SilentlyContinue
    if ($process) {
        Stop-Process -Id $editorPid -Force
    }
    if ($hostProcess -and -not $hostProcess.HasExited) {
        Stop-Process -Id $hostProcess.Id -Force
    }
    if (Test-Path -LiteralPath $candidateRoot) {
        Remove-CaptureOwnedTree -Path $candidateRoot -Owner $finalRoot
    }
    if (-not $promoted -and (Test-Path -LiteralPath $statusPath -PathType Leaf)) {
        New-Item -ItemType Directory -Path $failureRoot -Force | Out-Null
        Copy-Item -LiteralPath $statusPath -Destination (Join-Path $failureRoot 'status.json')
        $editorLog = Join-Path $projectRoot 'Saved\Logs\Alis.log'
        if (Test-Path -LiteralPath $editorLog -PathType Leaf) {
            Copy-Item -LiteralPath $editorLog -Destination (Join-Path $failureRoot 'editor.log')
        }
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-CaptureOwnedTree -Path $workRoot -Owner $ownerRoot
    }
}
