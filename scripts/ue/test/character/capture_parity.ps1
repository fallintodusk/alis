# Character Parity Capture
# Runs automation tests for definition-driven character regression validation.
# Uses -ProjectSkipFrontEnd to bypass menu travel and boot directly into gameplay.
# Regenerates ObjectDefinition assets first so Hero.json changes reach Hero.uasset
# before the definition-driven pawn is spawned.
# Output: Saved/Validation/CharacterDebug/ (JSON sidecars with unique RunId)
#
# Usage: .\capture_parity.ps1 [-TimeoutSeconds 180] [-Map "..."]

param(
    [int]$TimeoutSeconds = 300,
    [string]$Map = "/Game/Project/Maps/Test/ClipMatrix_CleanMap.ClipMatrix_CleanMap",
    [string]$TestFilter = "ProjectIntegrationTests.Character.Parity",
    [switch]$SkipDefinitionGeneration = $false
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..\").Path
$testScript = Join-Path $PSScriptRoot "..\unit\run_cpp_tests_safe.ps1"
$configDir = Join-Path $projectRoot "scripts\config"

function Invoke-ObjectDefinitionGeneration {
    param(
        [string]$ProjectRoot,
        [string]$ConfigDir
    )

    . (Join-Path $ConfigDir "Resolve-UEConfig.ps1")
    $config = Resolve-UEConfig -ConfigDir $ConfigDir
    $uePath = $config.UE_PATH.Replace("/", "\")
    if (-not $uePath) {
        throw "UE_PATH not found. Create scripts\\config\\ue_path.local.conf"
    }

    $editorCmdPath = Join-Path $uePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (-not (Test-Path $editorCmdPath)) {
        throw "UnrealEditor-Cmd not found: $editorCmdPath"
    }

    $projectPath = Join-Path $ProjectRoot "Alis.uproject"
    $stepN = $env:OVERNIGHT_STEP
    if (-not $stepN) { $stepN = "manual" }
    $logDir = Join-Path $ProjectRoot "scripts\ue\artifacts\overnight\step-$stepN"
    New-Item -Force -ItemType Directory -Path $logDir | Out-Null
    $generatorLog = Join-Path $logDir "generate_definitions.log"

    Write-Host "[pre] Cleaning existing UE processes for definition generation..." -ForegroundColor Yellow
    Get-Process | Where-Object {
        $_.ProcessName -like "UnrealEditor*" -or
        $_.ProcessName -like "UEBuildWorker*" -or
        $_.ProcessName -like "ShaderCompileWorker*"
    } | Stop-Process -Force -ErrorAction SilentlyContinue

    Write-Host "[pre] Regenerating ObjectDefinition assets..." -ForegroundColor Yellow
    Write-Host "      Commandlet: GenerateDefinitions -type=Object" -ForegroundColor Gray

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $editorCmdPath
    $psi.Arguments = "`"$projectPath`" -run=GenerateDefinitions -type=Object -unattended -nopause -nosplash -nosound -stdout -FullStdOutLogOutput -log"
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $proc.Start() | Out-Null

    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $proc.WaitForExit()

    $output = $stdoutTask.Result + "`n`n--- STDERR ---`n" + $stderrTask.Result
    Set-Content -Path $generatorLog -Value $output

    if ($proc.ExitCode -ne 0) {
        Write-Host "      FAILED: definition generation exited with code $($proc.ExitCode)" -ForegroundColor Red
        Write-Host "      Log: $generatorLog" -ForegroundColor Red
        exit $proc.ExitCode
    }

    Write-Host "      Done" -ForegroundColor Green
    Write-Host "      Log: $generatorLog" -ForegroundColor Gray
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Character Parity Capture" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Project: $projectRoot"
Write-Host "Map:     $Map"
Write-Host "Timeout: $TimeoutSeconds seconds"
Write-Host ""

if (-not (Test-Path $testScript)) {
    Write-Host "ERROR: Test runner not found: $testScript" -ForegroundColor Red
    exit 2
}

$outputDir = Join-Path $projectRoot "Saved\Validation\CharacterDebug"

if (-not $SkipDefinitionGeneration) {
    Invoke-ObjectDefinitionGeneration -ProjectRoot $projectRoot -ConfigDir $configDir
    Write-Host ""
}
else {
    Write-Host "Skipping ObjectDefinition generation preflight" -ForegroundColor Yellow
    Write-Host ""
}

$captureRunId = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff")
$knownParityTests = @(
    "ProjectIntegrationTests.Character.Parity.IdleSnapshot",
    "ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix",
    "ProjectIntegrationTests.Character.Parity.CameraYawTimeline",
    "ProjectIntegrationTests.Character.Parity.LocomotionTimeline",
    "ProjectIntegrationTests.Character.Parity.SimpleAnimSanity"
)

$expectedTests = if ($TestFilter.Contains("*")) {
    @($knownParityTests | Where-Object { $_ -like $TestFilter })
} else {
    @($knownParityTests | Where-Object {
        $_ -eq $TestFilter -or $_.StartsWith("$TestFilter.", [System.StringComparison]::Ordinal)
    })
}

$requiredArtifacts = @()
foreach ($expectedTest in $expectedTests) {
    switch ($expectedTest) {
        "ProjectIntegrationTests.Character.Parity.IdleSnapshot" {
            $requiredArtifacts += [PSCustomObject]@{
                Test = $expectedTest
                Pattern = "*idle_definition_$captureRunId*.json"
            }
        }
        "ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix" {
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_clean_path_timeline_$captureRunId.jsonl" }
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_clean_path_summary_$captureRunId.json" }
        }
        "ProjectIntegrationTests.Character.Parity.CameraYawTimeline" {
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_camera_yaw_timeline_$captureRunId.jsonl" }
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_camera_yaw_summary_$captureRunId.json" }
        }
        "ProjectIntegrationTests.Character.Parity.LocomotionTimeline" {
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_locomotion_timeline_$captureRunId.jsonl" }
            $requiredArtifacts += [PSCustomObject]@{ Test = $expectedTest; Pattern = "definition_locomotion_summary_$captureRunId.json" }
        }
    }
}

Write-Host "Capture RunId: $captureRunId" -ForegroundColor Cyan
if ($expectedTests.Count -gt 0) {
    Write-Host "Expected tests:" -ForegroundColor Gray
    foreach ($expectedTest in $expectedTests) {
        Write-Host "  $expectedTest" -ForegroundColor Gray
    }
}

$testArgs = @{
    TestFilter = $TestFilter
    Mode = "Gate"
    Map = $Map
    Game = $true
    PreExecCmds = "Module Load ProjectIntegrationTests"
    RequiredLogPatterns = @(
        "LogProjectIntegrationTestsPersistent: Display: \[PersistentEditor\] Ready to start automation"
    )
    UseTestExitOnly = $true
    TimeoutSeconds = $TimeoutSeconds
    ExtraArgs = "-ProjectSkipFrontEnd -CharacterCaptureRunId=$captureRunId"
}
if ($expectedTests.Count -gt 0) {
    $testArgs.ExpectedTestNames = $expectedTests
}

& $testScript @testArgs

$testExitCode = $LASTEXITCODE

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan

if ($testExitCode -eq 124) {
    Write-Host "TIMEOUT: Test did not complete in ${TimeoutSeconds}s" -ForegroundColor Red
    exit 124
}
elseif ($testExitCode -eq 2) {
    Write-Host "WARNING: No tests found (check test name)" -ForegroundColor Yellow
    exit 2
}
elseif ($testExitCode -ne 0) {
    Write-Host "FAILED: Test exited with code $testExitCode" -ForegroundColor Red
}
else {
    Write-Host "Test completed" -ForegroundColor Green
}

# Select artifacts by the explicit RunId passed to every character test. This
# prevents files from overlapping runs or unrelated JSON schemas from being
# presented as comparable output.
$currentRunJsonFiles = @(Get-ChildItem -Path $outputDir -Filter "*$captureRunId*.json" -ErrorAction SilentlyContinue |
    Sort-Object Name)
$captureFiles = @($currentRunJsonFiles | Where-Object { $_.Name -notmatch "_summary_" })
$summaryFiles = @($currentRunJsonFiles | Where-Object { $_.Name -match "_summary_" })
$timelineFiles = @(Get-ChildItem -Path $outputDir -Filter "*$captureRunId*.jsonl" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match "_timeline_" } |
    Sort-Object Name)

$missingArtifacts = @()
foreach ($requiredArtifact in $requiredArtifacts) {
    $matchingNonEmptyFiles = @(Get-ChildItem -Path $outputDir -Filter $requiredArtifact.Pattern -ErrorAction SilentlyContinue |
        Where-Object { $_.Length -gt 0 })
    if ($matchingNonEmptyFiles.Count -eq 0) {
        $missingArtifacts += $requiredArtifact
    }
}

if ($missingArtifacts.Count -gt 0) {
    Write-Host "REQUIRED ARTIFACTS MISSING OR EMPTY:" -ForegroundColor Red
    foreach ($missingArtifact in $missingArtifacts) {
        Write-Host "  $($missingArtifact.Test): $($missingArtifact.Pattern)" -ForegroundColor Red
    }
    if ($testExitCode -eq 0) {
        $testExitCode = 1
    }
}

if ($captureFiles.Count -gt 0) {
    Write-Host "CAPTURES:" -ForegroundColor Green
    foreach ($f in $captureFiles) {
        Write-Host "  $($f.Name)" -ForegroundColor Gray
    }
}

# Show timeline files from this run when available.
if ($timelineFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "TIMELINES:" -ForegroundColor Green
    foreach ($f in $timelineFiles) {
        $lineCount = (Get-Content $f.FullName | Measure-Object -Line).Lines
        Write-Host "  $($f.Name) ($lineCount samples)" -ForegroundColor Gray
    }
}

# Show summary files from this run when available.
if ($summaryFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "SUMMARIES:" -ForegroundColor Green
    foreach ($f in $summaryFiles) {
        Write-Host "  $($f.Name)" -ForegroundColor Gray
    }
}

if ($captureFiles.Count -eq 0 -and $timelineFiles.Count -eq 0 -and $summaryFiles.Count -eq 0) {
    Write-Host "No JSON artifacts found for RunId $captureRunId in: $outputDir" -ForegroundColor Gray
}

Write-Host "========================================" -ForegroundColor Cyan
exit $testExitCode
