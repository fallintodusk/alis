#Requires -Version 5.1
# License terms: see repository root LICENSE.

[CmdletBinding()]
param(
    [ValidateSet("Status", "Accept")]
    [string]$Mode = "Accept",
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string]$ReleaseVersion
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$EvidenceRoot = Join-Path $ProjectRoot "Saved\Validation\WorldRealization\playable-tour"
$AcceptanceRoot = Join-Path $EvidenceRoot "Candidate"
$AcceptancePath = Join-Path $AcceptanceRoot "operator-acceptance.json"
$PackageRoot = Join-Path $ProjectRoot "Saved\PackageRelease\KazanPlayableTour\Candidate"
$ShippingExecutable = Join-Path $PackageRoot "Windows\Alis\Binaries\Win64\Alis-Win64-Shipping.exe"
$IdentityTool = Join-Path $ProjectRoot "scripts\ue\package\prepare_release.py"
$PerformanceEvidence = Join-Path $ScriptDir "test\performance\project_world_performance_evidence.ps1"
. $PerformanceEvidence

function Get-PythonValue {
    param([string[]]$Arguments)

    $Output = @(& python $IdentityTool @Arguments)
    if ($LASTEXITCODE -ne 0 -or $Output.Count -ne 1) {
        throw "Release identity command failed: $($Arguments -join ' ')"
    }
    return $Output[0]
}

function Resolve-RecordedPath {
    param([string]$Path)

    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $ProjectRoot $Path))
}

function Test-CurrentAcceptance {
    param(
        [object]$Acceptance,
        [string]$Revision,
        [string]$SourceState,
        [string]$PackageTree,
        [string]$ExecutableHash
    )

    if (-not $Acceptance) { return $false }
    try {
        $CompositePath = Resolve-RecordedPath -Path ([string]$Acceptance.release_composite)
        return (
            [int]$Acceptance.schema_version -eq 1 -and
            [string]$Acceptance.status -ceq "operator_accepted" -and
            [string]$Acceptance.product_decision -ceq "accepted" -and
            [string]$Acceptance.source_revision -ceq $Revision -and
            [string]$Acceptance.source_state_sha256 -ceq $SourceState -and
            [string]$Acceptance.package_tree_sha256 -ceq $PackageTree -and
            [string]$Acceptance.shipping_executable_sha256 -ceq $ExecutableHash -and
            (Test-Path -LiteralPath $CompositePath -PathType Leaf) -and
            [string]$Acceptance.release_composite_sha256 -ceq
                (Get-FileHash -LiteralPath $CompositePath -Algorithm SHA256).Hash.ToLowerInvariant()
        )
    }
    catch {
        return $false
    }
}

if (-not (Test-Path -LiteralPath $PackageRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $ShippingExecutable -PathType Leaf)) {
    $Result = @{ schema = "alis-player-acceptance-status-v1"; state = "candidate_missing" }
    if ($Mode -eq "Status") {
        $Result | ConvertTo-Json -Compress
        exit 0
    }
    throw "The machine-accepted Shipping Candidate is missing."
}

$Revision = (& git -C $ProjectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Unable to resolve source revision." }
$SourceState = Get-PythonValue -Arguments @("source-state", "--source-root", $ProjectRoot)
$PackageTree = Get-ProjectWorldPackagePayloadDigest -Path $PackageRoot
$ExecutableHash = (Get-FileHash -LiteralPath $ShippingExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
$ExistingAcceptance = $null
if (Test-Path -LiteralPath $AcceptancePath -PathType Leaf) {
    $ExistingAcceptance = Get-Content -LiteralPath $AcceptancePath -Raw | ConvertFrom-Json
}

if (Test-CurrentAcceptance -Acceptance $ExistingAcceptance -Revision $Revision `
        -SourceState $SourceState -PackageTree $PackageTree -ExecutableHash $ExecutableHash) {
    $CompositePath = Resolve-RecordedPath -Path ([string]$ExistingAcceptance.release_composite)
    @{
        schema = "alis-player-acceptance-status-v1"
        state = "accepted"
        receipt = $AcceptancePath
        composite = $CompositePath
    } |
        ConvertTo-Json -Compress
    exit 0
}

$CompositeCandidates = @(
    Get-ChildItem -LiteralPath $EvidenceRoot -Filter "composite.json" -File -Recurse |
        Where-Object { $_.Directory.Name -notin @("Candidate", "PreviousCandidate") } |
        Sort-Object LastWriteTimeUtc -Descending
)
$SelectedComposite = $null
$SelectedDocument = $null
foreach ($Candidate in $CompositeCandidates) {
    try {
        $Document = Get-Content -LiteralPath $Candidate.FullName -Raw | ConvertFrom-Json
        $RecordedPackage = Resolve-RecordedPath -Path ([string]$Document.final_package)
        if ([string]$Document.status -ceq "accepted" -and
            [string]$Document.revision -ceq $Revision -and
            [string]$Document.source_state_sha256 -ceq $SourceState -and
            [string]$Document.shipping_package_sha256 -ceq $PackageTree -and
            [string]$Document.shipping_executable_sha256 -ceq $ExecutableHash -and
            $RecordedPackage.Equals([IO.Path]::GetFullPath($PackageRoot), [StringComparison]::OrdinalIgnoreCase)) {
            $SelectedComposite = $Candidate
            $SelectedDocument = $Document
            break
        }
    }
    catch {
        continue
    }
}

if (-not $SelectedComposite) {
    if ($Mode -eq "Status") {
        @{ schema = "alis-player-acceptance-status-v1"; state = "candidate_stale" } |
            ConvertTo-Json -Compress
        exit 0
    }
    throw "No machine-accepted Candidate matches the current source and package bytes."
}

if ($Mode -eq "Status") {
    @{
        schema = "alis-player-acceptance-status-v1"
        state = "awaiting_operator"
        composite = $SelectedComposite.FullName
    } | ConvertTo-Json -Compress
    exit 0
}

$Confirmation = Read-Host "After the Shipping walkthrough, type PASS $ReleaseVersion"
if ($Confirmation -cne "PASS $ReleaseVersion") {
    throw "Operator Candidate acceptance was not granted."
}

$Receipt = [ordered]@{
    schema_version = 1
    status = "operator_accepted"
    product_decision = "accepted"
    package_root = "Saved/PackageRelease/KazanPlayableTour/Candidate"
    package_tree_sha256 = $PackageTree
    shipping_executable = "Saved/PackageRelease/KazanPlayableTour/Candidate/Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
    shipping_executable_sha256 = $ExecutableHash
    source_revision = $Revision
    source_state_sha256 = $SourceState
    map_package = [string]$SelectedDocument.map_package
    runtime_profile = [string]$SelectedDocument.runtime_profile
    runtime_profile_sha256 = [string]$SelectedDocument.runtime_profile_sha256
    release_operation_id = [string]$SelectedDocument.operation_id
    release_composite = $SelectedComposite.FullName.Substring($ProjectRoot.Length + 1).Replace("\", "/")
    release_composite_sha256 = (Get-FileHash -LiteralPath $SelectedComposite.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    accepted_utc = [DateTime]::UtcNow.ToString("o")
    acceptance_scope = "Operator walkthrough: menu/Escape, PreviewFlight, building collision, blue Water, and perceived streaming."
}
New-Item -ItemType Directory -Path $AcceptanceRoot -Force | Out-Null
$TemporaryPath = "$AcceptancePath.tmp"
$Receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $TemporaryPath -Encoding Ascii
Move-Item -LiteralPath $TemporaryPath -Destination $AcceptancePath -Force
Write-Host "[OK] Operator acceptance recorded: $AcceptancePath" -ForegroundColor Green
