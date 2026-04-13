# Character Parity Capture
# Runs automation tests for character parity and modular camera/body validation.
# Uses -ProjectSkipFrontEnd to bypass menu travel and boot directly into gameplay.
# Regenerates ObjectDefinition assets first so Hero.json changes reach Hero.uasset
# before the modular pawn is spawned.
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
$captureWindowStart = (Get-Date).AddSeconds(-2)

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

& $testScript `
    -TestFilter $TestFilter `
    -Map $Map `
    -Game `
    -UseTestExitOnly `
    -TimeoutSeconds $TimeoutSeconds `
    -ExtraArgs "-ProjectSkipFrontEnd"

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

# Show captures produced by this run first. Fall back to latest files only if the
# current-run window is empty.
$currentRunJsonFiles = Get-ChildItem -Path $outputDir -Filter "*.json" -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $captureWindowStart } |
    Sort-Object LastWriteTime -Descending

$latestFiles = $currentRunJsonFiles | Select-Object -First 2
if ($latestFiles.Count -eq 0) {
    $latestFiles = Get-ChildItem -Path $outputDir -Filter "*.json" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 2
}

if ($latestFiles.Count -ge 2) {
    Write-Host "CAPTURED:" -ForegroundColor Green
    foreach ($f in $latestFiles) {
        Write-Host "  $($f.Name)" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "Diff:" -ForegroundColor Yellow
    Write-Host "  diff `"$($latestFiles[1].FullName)`" `"$($latestFiles[0].FullName)`"" -ForegroundColor White
}
elseif ($latestFiles.Count -eq 1) {
    Write-Host "PARTIAL: Only one capture found" -ForegroundColor Yellow
    Write-Host "  $($latestFiles[0].Name)" -ForegroundColor Gray
}
else {
    Write-Host "No capture files found in: $outputDir" -ForegroundColor Red
}

# Show timeline files from this run when available.
$timelineFiles = Get-ChildItem -Path $outputDir -Filter "*timeline*.jsonl" -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $captureWindowStart } |
    Sort-Object LastWriteTime -Descending
if ($timelineFiles.Count -eq 0) {
    $timelineFiles = Get-ChildItem -Path $outputDir -Filter "*timeline*.jsonl" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 4
}
if ($timelineFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "TIMELINES:" -ForegroundColor Green
    foreach ($f in $timelineFiles) {
        $lineCount = (Get-Content $f.FullName | Measure-Object -Line).Lines
        Write-Host "  $($f.Name) ($lineCount samples)" -ForegroundColor Gray
    }
}

# Show summary files from this run when available.
$summaryFiles = Get-ChildItem -Path $outputDir -Filter "*summary*.json" -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $captureWindowStart } |
    Sort-Object LastWriteTime -Descending
if ($summaryFiles.Count -eq 0) {
    $summaryFiles = Get-ChildItem -Path $outputDir -Filter "*summary*.json" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 4
}
if ($summaryFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "SUMMARIES:" -ForegroundColor Green
    foreach ($f in $summaryFiles) {
        Write-Host "  $($f.Name)" -ForegroundColor Gray
    }
}

Write-Host "========================================" -ForegroundColor Cyan
exit $testExitCode
