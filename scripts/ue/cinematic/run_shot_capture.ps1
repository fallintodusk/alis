# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Render one raw camera move from a shot plan into Saved/CinematicRaw/<id>/.
#
# These are disposable takes for a human to cut later. This is NOT the release
# route: it binds to no Candidate and touches nothing under Saved/CinematicRelease,
# Saved/PackageRelease or Saved/Validation. Contract: docs/cinematics/raw_capture.md

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PlanPath,

    # Composition iteration: 480x270, one temporal sample, Dev preset. Answers
    # "is the framing and the motion right" in about a third of the time and a
    # twentieth of the disk. Streaming and camera path are untouched, so what a
    # preview shows about void walls and composition holds at production.
    # Promoted into Saved/CinematicRaw/<id>_preview/, never mistaken for a take.
    [switch]$Preview
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$driver = Join-Path $PSScriptRoot 'shot_capture_editor.py'
$runId = [Guid]::NewGuid().ToString('N')
$workRoot = Join-Path $projectRoot "tmp\cinematic\shot_capture\$runId"
$renderRoot = Join-Path $workRoot 'render'
$statusPath = Join-Path $workRoot 'status.json'
$logPath = Join-Path $workRoot 'editor.log'

function Assert-Shot {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-ShotTool {
    param([string]$Name, [string]$Fallback)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $Fallback -PathType Leaf) { return $Fallback }
    throw "$Name is unavailable."
}

$resolvedPlan = if ([IO.Path]::IsPathRooted($PlanPath)) { [IO.Path]::GetFullPath($PlanPath) }
    else { [IO.Path]::GetFullPath((Join-Path $projectRoot $PlanPath)) }
Assert-Shot (Test-Path -LiteralPath $resolvedPlan -PathType Leaf) "Shot plan does not exist: $resolvedPlan"
$plan = Get-Content -LiteralPath $resolvedPlan -Raw | ConvertFrom-Json
foreach ($field in @('id', 'map', 'duration', 'fps', 'fov', 'camera')) {
    Assert-Shot ($null -ne $plan.$field) "Shot plan is missing '$field'."
}
Assert-Shot ($plan.id -cmatch '^[a-z0-9][a-z0-9_]+$') 'id is not a stable lowercase identity.'
Assert-Shot ($plan.map -cmatch '^/[A-Za-z0-9_/-]+$') 'map must be a long package name.'
Assert-Shot (@($plan.camera).Count -ge 2) 'A shot needs at least two camera keys.'
$expectedFrames = [int][Math]::Round([double]$plan.duration * [int]$plan.fps)

if ($plan.map -like '/ProjectWorldData/*') {
    $mapFile = Join-Path $projectRoot ('Plugins\World\ProjectWorldData\Content' +
        (($plan.map -replace '^/ProjectWorldData', '') -replace '/', '\') + '.umap')
    Assert-Shot (Test-Path -LiteralPath $mapFile -PathType Leaf) "Shot map does not exist on disk: $mapFile"
}

$existingEditors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -match [regex]::Escape($projectRoot) })
Assert-Shot ($existingEditors.Count -eq 0) `
    'Capture waits until the editor is closed: a project editor is already running.'

. (Join-Path $projectRoot 'scripts\config\Resolve-UEConfig.ps1')
$config = Resolve-UEConfig -ConfigDir (Join-Path $projectRoot 'scripts\config')
$editor = Join-Path $config.UE_PATH 'Engine\Binaries\Win64\UnrealEditor.exe'
Assert-Shot (Test-Path -LiteralPath $editor -PathType Leaf) 'Launcher UnrealEditor.exe is unavailable.'
$ffprobe = Get-ShotTool -Name 'ffprobe' -Fallback 'C:\Program Files\Kdenlive\bin\ffprobe.exe'
$ffmpeg = Get-ShotTool -Name 'ffmpeg' -Fallback 'C:\Program Files\Kdenlive\bin\ffmpeg.exe'
New-Item -ItemType Directory -Path $renderRoot -Force | Out-Null

if ($Preview) {
    # The driver reads the plan from disk, so the preview preset is handed over
    # in a scratch copy rather than by mutating the committed plan.
    $previewPlan = Join-Path $workRoot 'plan.preview.json'
    $plan | Add-Member -NotePropertyName preset -NotePropertyValue '/Game/Cinematics/MP_Config_Dev' -Force
    [IO.File]::WriteAllText($previewPlan, ($plan | ConvertTo-Json -Depth 8),
        (New-Object Text.UTF8Encoding($false)))
    $resolvedPlan = $previewPlan
    $env:PROJECT_CINEMATIC_SHOT_PREVIEW = '1'
    Write-Host '[ShotCapture] PREVIEW: 480x270, one sample, composition only'
}
$env:PROJECT_CINEMATIC_SHOT_PLAN = $resolvedPlan
$env:PROJECT_CINEMATIC_SHOT_STATUS = $statusPath
$env:PROJECT_CINEMATIC_SHOT_OUTPUT = $renderRoot
$launchUtc = [DateTime]::UtcNow
Write-Host "[ShotCapture] $($plan.id): $expectedFrames frames at $($plan.fps) fps on $($plan.map)"
Start-Process -FilePath 'cmd.exe' -WindowStyle Hidden -WorkingDirectory $projectRoot -ArgumentList @(
    '/c', 'scripts\ue\run\run_editor.bat', $plan.map,
    ('-ExecCmds="py {0}"' -f $driver),
    '-unattended', '-RenderOffscreen', '-NoSplash', '-NoSound',
    ('-abslog={0}' -f $logPath), '-log') | Out-Null
foreach ($name in @('PROJECT_CINEMATIC_SHOT_PLAN', 'PROJECT_CINEMATIC_SHOT_STATUS', 'PROJECT_CINEMATIC_SHOT_PREVIEW',
        'PROJECT_CINEMATIC_SHOT_OUTPUT')) {
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}

$timeout = if ($plan.timeout_seconds) { [int]$plan.timeout_seconds } else { 5400 }
$deadline = [DateTime]::UtcNow.AddSeconds($timeout)
$editorProcess = $null
do {
    Start-Sleep -Milliseconds 500
    $editorProcess = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
        Where-Object {
            $_.CreationDate.ToUniversalTime() -ge $launchUtc.AddSeconds(-2) -and
            $_.CommandLine -match [regex]::Escape($plan.map)
        } | Select-Object -First 1
} while (-not $editorProcess -and [DateTime]::UtcNow -lt $deadline)
Assert-Shot ($null -ne $editorProcess) 'Capture editor did not start.'
$editorPid = [int]$editorProcess.ProcessId

$lastState = ''
while ([DateTime]::UtcNow -lt $deadline) {
    $process = Get-Process -Id $editorPid -ErrorAction SilentlyContinue
    if ($process -and $process.MainWindowTitle -eq 'Restore Packages') {
        throw 'Unreal recovery dialog blocked the unattended capture.'
    }
    if (Test-Path -LiteralPath $statusPath -PathType Leaf) {
        try {
            $status = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
            if ($status.state -ne $lastState) {
                Write-Host "[ShotCapture] state=$($status.state)"
                $lastState = $status.state
            }
            if ($status.state -in @('finished', 'error')) { break }
        }
        catch [System.ArgumentException] { }
    }
    # The editor can die during MRQ finalize, after the master is written but
    # before the driver records a result. Without this the loop would poll a
    # status stuck on 'rendering' until the timeout.
    if (-not $process) {
        throw "Capture editor exited while state was '$lastState'. Log: $logPath"
    }
    Start-Sleep -Seconds 2
}
Assert-Shot (Test-Path -LiteralPath $statusPath -PathType Leaf) 'Capture published no status.'
$status = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
Assert-Shot ($status.state -ceq 'finished' -and $status.success) `
    "Shot capture failed: $($status.reason)"
# The editor opens a fallback world when the requested map fails to load and then
# renders a valid capture of the wrong place. That has happened here.
Assert-Shot ($status.map -ceq [string]$plan.map) `
    "Requested map '$($plan.map)' but the capture recorded '$($status.map)'."

$rendered = @(Get-ChildItem -LiteralPath $renderRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.mov', '.mp4', '.avi') })
Assert-Shot ($rendered.Count -eq 1) "Expected one video master; got $($rendered.Count)."
$master = $rendered[0]

$probe = (& $ffprobe -v error -select_streams v:0 -count_frames `
    -show_entries 'stream=width,height,nb_read_frames:format=duration' -of json $master.FullName |
    Out-String) | ConvertFrom-Json
$measuredFrames = [int]$probe.streams[0].nb_read_frames
$measuredSeconds = [double]$probe.format.duration
Assert-Shot ($measuredFrames -eq $expectedFrames) `
    "Master carries $measuredFrames frames; the plan asked for $expectedFrames."

$finalName = if ($Preview) { "$($plan.id)_preview" } else { [string]$plan.id }
$finalRoot = Join-Path $projectRoot "Saved\CinematicRaw\$finalName"
if (Test-Path -LiteralPath $finalRoot) { Remove-Item -LiteralPath $finalRoot -Recurse -Force }
New-Item -ItemType Directory -Path $finalRoot -Force | Out-Null
$promoted = Join-Path $finalRoot ("{0}{1}" -f $finalName, $master.Extension)
Copy-Item -LiteralPath $master.FullName -Destination $promoted -Force
Copy-Item -LiteralPath $resolvedPlan -Destination (Join-Path $finalRoot 'plan.json') -Force
if (Test-Path -LiteralPath $logPath -PathType Leaf) {
    Copy-Item -LiteralPath $logPath -Destination (Join-Path $finalRoot 'editor.log') -Force
}

# MRQ producing bytes is not evidence that the bytes show anything, so pull three
# frames out for a look.
$framesRoot = Join-Path $finalRoot 'frames'
New-Item -ItemType Directory -Path $framesRoot -Force | Out-Null
foreach ($mark in @{ first = 0.0; middle = [Math]::Round($measuredSeconds / 2, 3)
                     last = [Math]::Round([Math]::Max($measuredSeconds - 0.2, 0), 3) }.GetEnumerator()) {
    & $ffmpeg -loglevel error -y -ss $mark.Value -i $promoted -frames:v 1 `
        (Join-Path $framesRoot "$($mark.Key).png") | Out-Null
}

[IO.File]::WriteAllText(
    (Join-Path $finalRoot 'receipt.json'),
    (([ordered]@{
        id       = $finalName
        map      = [string]$status.map
        seconds  = $measuredSeconds
        frames   = $measuredFrames
        width    = [int]$probe.streams[0].width
        height   = [int]$probe.streams[0].height
        fps      = [int]$plan.fps
        captured = [DateTime]::UtcNow.ToString('o')
    }) | ConvertTo-Json),
    (New-Object Text.UTF8Encoding($false)))

# The driver quits the editor after writing its result, so the process can still
# be shutting down here. Wait it out, or the next capture in a sequence refuses
# because it sees a project editor still holding the project.
for ($i = 0; $i -lt 60 -and (Get-Process -Id $editorPid -ErrorAction SilentlyContinue); $i++) {
    Start-Sleep -Seconds 1
}
if (Test-Path -LiteralPath $workRoot) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host "[ShotCapture] ACCEPTED $($plan.id): $([Math]::Round($measuredSeconds,2))s, $measuredFrames frames, $($probe.streams[0].width)x$($probe.streams[0].height)"
Write-Host "[ShotCapture] $finalRoot"
exit 0
