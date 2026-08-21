# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# Operator evidence capture for an already-realized territory.
#
# READ-ONLY with respect to generated content: this loads the territory map, renders the
# planned vantages through a transient scene capture, and never saves. It therefore takes no
# content lock and opens no generated-content transaction - there is nothing to roll back.
#
# The route deliberately does NOT run inside Automation RunTests. Under GIsAutomationTesting
# the screenshot path waits on COMPARISON against absent ground truth instead of completing on
# capture. Contract and rationale: tools/World/VisualVerification/README.md.
#
# It also does not run as a commandlet. That envelope was tried and abandoned on measured
# evidence: with a real D3D12 SM6 RHI, a valid scene, 378 loaded actors, 566 registered visible
# primitives, a DirectionalLight and an explicit CommandletHelpers::TickEngine frame, the scene
# capture still produced only the render target's clear colour. The live editor runs the
# ordinary frame loop the renderer expects, so the capture runs there as a console command.

#Requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Map,

    # Produced by tools/World/VisualVerification/app/plan_vantages.py. It carries the poses AND
    # the capture width/height/FOV the altitude solve assumed; they are never passed separately.
    [Parameter(Mandatory = $true)]
    [string]$VantagePlan,

    [string]$WorldDataPlugin = "ProjectWorldData",

    [string]$OutputDirectory = "",

    [string]$ReceiptPath = ""
)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDirectory))
$configDirectory = Join-Path (Join-Path $projectRoot "scripts") "config"
. (Join-Path $configDirectory "Resolve-UEConfig.ps1")
. (Join-Path $scriptDirectory "execution_envelope.ps1")

$config = Resolve-UEConfig -ConfigDir $configDirectory
$editorCommand = Join-Path $config.UE_PATH "Engine\Binaries\Win64\UnrealEditor.exe"
if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
    throw "UnrealEditor.exe does not exist under the configured UE_PATH."
}
$projectFile = Join-Path $projectRoot "Alis.uproject"

# Git Bash rewrites a leading /Plugin/... argument into a native Windows path, so callers from
# that shell must escape it as //Plugin/... . Accept the escaped form and hand the commandlet a
# real long package name; the commandlet itself stays strict about what it will load.
if ($Map.StartsWith('//')) {
    $Map = $Map.Substring(1)
}

$vantagePlanPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $VantagePlan))
if (-not (Test-Path -LiteralPath $vantagePlanPath -PathType Leaf)) {
    $vantagePlanPath = [System.IO.Path]::GetFullPath($VantagePlan)
}
if (-not (Test-Path -LiteralPath $vantagePlanPath -PathType Leaf)) {
    throw "Vantage plan does not exist: $VantagePlan"
}

# Scratch output lives under the project tmp/ tree per the AGENTS.md rule.
$captureRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Join-Path $projectRoot "tmp\world\visual_verification\screenshots"
}
else {
    [System.IO.Path]::GetFullPath($OutputDirectory)
}
$receipt = if ([string]::IsNullOrWhiteSpace($ReceiptPath)) {
    Join-Path $projectRoot "tmp\world\visual_verification\receipts\capture.json"
}
else {
    [System.IO.Path]::GetFullPath($ReceiptPath)
}
New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $receipt) -Force | Out-Null
$logPath = [System.IO.Path]::ChangeExtension($receipt, ".log")

# The console tokenizes command arguments on whitespace, so a path containing a space would
# silently split into several arguments and land in the wrong parameter. Refuse instead of
# writing evidence to a mis-parsed path.
foreach ($pathArgument in @($vantagePlanPath, $captureRoot, $receipt)) {
    if ($pathArgument -match '\s') {
        throw "Capture paths must not contain spaces; the console splits arguments on whitespace: $pathArgument"
    }
}

# -RenderOffscreen keeps a real RHI and a real frame loop while opening no window, so the
# capture never fights the operator for foreground focus and never depends on it either.
$unrealArguments = @(
    $projectFile
    $Map
    "-ExecCmds=ProjectWorld.CaptureEvidence $vantagePlanPath $captureRoot $receipt"
    "-EnablePlugins=$WorldDataPlugin"
    "-abslog=$logPath"
    "-RenderOffscreen"
    "-unattended"
    "-nop4"
    "-nosplash"
    "-nosound"
    "-FullStdOutLogOutput"
)
# A scene capture renders the scene, so this is a render-required step. The envelope helper
# owns the flags; -NullRHI here would produce black frames that still look like files on disk.
$captureRendering = 'Required'
# The editor is not a commandlet, so it needs no -AllowCommandletRendering; the envelope guard
# is still asserted to keep -NullRHI out of a render-required step.
Assert-ProjectWorldExecutionEnvelope -Rendering $captureRendering -Arguments (
    $unrealArguments + @('-AllowCommandletRendering'))

Write-Host "[WorldCapture] rendering=$captureRendering"
Write-Host "[WorldCapture] map=$Map"
Write-Host "[WorldCapture] plan=$vantagePlanPath"
Write-Host "[WorldCapture] output=$captureRoot"

& $editorCommand @unrealArguments | Out-Host
$engineExit = $LASTEXITCODE

if (-not (Test-Path -LiteralPath $receipt -PathType Leaf)) {
    Write-Host "[WorldCapture] REFUSED: the capture produced no receipt. Log: $logPath"
    exit 1
}
$record = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
Write-Host "[WorldCapture] status=$($record.status) views=$($record.views.Count) engine_exit=$engineExit"
# The command captures whatever world is open. A startup map that failed to load leaves the
# editor on a fallback world, which would otherwise produce a perfectly authenticated capture
# set OF THE WRONG TERRITORY. Operator review must be bound to the requested candidate.
if ($record.map_package -ne $Map) {
    Write-Host "[WorldCapture] REFUSED: requested map '$Map' but the receipt records '$($record.map_package)'."
    exit 1
}
Write-Host "[WorldCapture] receipt=$receipt"
if ($record.status -ne 'accepted' -or $engineExit -ne 0) {
    Write-Host "[WorldCapture] $($record.message)"
    exit 1
}
Write-Host "[WorldCapture] Captures are operator EVIDENCE, not an acceptance gate. Verify them with:"
Write-Host "  python tools/World/VisualVerification/app/verify_capture.py $receipt"
exit 0
