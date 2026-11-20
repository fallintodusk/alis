#Requires -Version 5.1

$ErrorActionPreference = "Stop"
$ProjectRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel 2>$null | Out-String).Trim()
if (-not $ProjectRoot) {
    throw "Could not detect the ALIS repository root."
}

$TestRoot = Join-Path $ProjectRoot ("tmp\definition-authority\inventory-freshness\" + [Guid]::NewGuid().ToString("N"))
$Repo = Join-Path $TestRoot "repo"
$FakeUe = Join-Path $TestRoot "FakeUE"
$FakeEditorDir = Join-Path $FakeUe "Engine\Binaries\Win64"
$FakeEditor = Join-Path $FakeEditorDir "UnrealEditor-Cmd.exe"

try {
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $Repo "scripts\git\mirror"), `
        (Join-Path $Repo "scripts\config"), `
        (Join-Path $Repo "Saved\Inspection"), `
        $FakeEditorDir | Out-Null

    Copy-Item -LiteralPath (Join-Path $ProjectRoot "scripts\git\mirror\refresh_developer_asset_authority.ps1") `
        -Destination (Join-Path $Repo "scripts\git\mirror\refresh_developer_asset_authority.ps1")
    Copy-Item -LiteralPath (Join-Path $ProjectRoot "scripts\git\mirror\refresh_developer_asset_authority.py") `
        -Destination (Join-Path $Repo "scripts\git\mirror\refresh_developer_asset_authority.py")
    Copy-Item -LiteralPath (Join-Path $ProjectRoot "scripts\config\Resolve-UEConfig.ps1") `
        -Destination (Join-Path $Repo "scripts\config\Resolve-UEConfig.ps1")
    $UePath = $FakeUe.Replace("\", "/")
    "UE_PATH=$UePath" | Set-Content -LiteralPath (Join-Path $Repo "scripts\config\ue_path.conf") -Encoding Ascii

    "fixture project" | Set-Content -LiteralPath (Join-Path $Repo "Alis.uproject") -Encoding Ascii
    '{"schema_version":1,"assets":[]}' | Set-Content -LiteralPath `
        (Join-Path $Repo "Saved\Inspection\generated_asset_inventory.json") -Encoding Ascii

    $FakeSource = @"
public static class FakeEditor
{
    public static int Main(string[] args) { return 0; }
}
"@
    Add-Type -TypeDefinition $FakeSource -OutputAssembly $FakeEditor -OutputType ConsoleApplication

    Push-Location $Repo
    try {
        & git init -q
        if ($LASTEXITCODE -ne 0) { throw "git init failed for the fixture repository" }
    }
    finally {
        Pop-Location
    }

    $Script = Join-Path $Repo "scripts\git\mirror\refresh_developer_asset_authority.ps1"
    $StdOut = Join-Path $TestRoot "child.out"
    $StdErr = Join-Path $TestRoot "child.err"
    $Process = Start-Process -FilePath "powershell.exe" `
        -ArgumentList @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $Script) `
        -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $StdOut -RedirectStandardError $StdErr
    $Output = ((Get-Content -LiteralPath $StdOut -Raw -ErrorAction SilentlyContinue) + "`n" +
        (Get-Content -LiteralPath $StdErr -Raw -ErrorAction SilentlyContinue))
    $ExitCode = $Process.ExitCode
    if ($ExitCode -eq 0) {
        throw "The refresh accepted an editor exit 0 without a fresh inventory.`n$Output"
    }
    if ($Output -notmatch "fresh inventory") {
        throw "The refresh failed for the wrong reason. Expected a fresh-inventory refusal.`n$Output"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Repo "Saved\Inspection\generated_asset_inventory.json"))) {
        throw "The stale fixed inventory was removed instead of being ignored."
    }

    $FreshInventories = @(Get-ChildItem -LiteralPath (Join-Path $Repo "Saved\Inspection") `
        -Filter "generated_asset_inventory.*.json" -ErrorAction SilentlyContinue)
    if ($FreshInventories.Count -ne 0) {
        throw "The failed export left a per-run inventory that could be mistaken for fresh evidence."
    }

    Write-Host "[OK] Missing fresh inventory was refused and the stale fixed inventory was not reused."
}
finally {
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
