#Requires -Version 5.1
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Get-ProjectWorldProjectionDigest {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Records
    )

    $lines = [Collections.Generic.List[string]]::new()
    foreach ($record in @($Records | Sort-Object Source)) {
        $source = [IO.Path]::GetFullPath([string]$record.Source)
        if (-not (Test-Path -LiteralPath $source)) {
            $lines.Add("absent|$source")
            continue
        }
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            $item = Get-Item -LiteralPath $source
            $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
            $lines.Add("file|$source|$($item.Length)|$hash")
            continue
        }
        $lines.Add("directory|$source")
        foreach ($file in @(Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName)) {
            $relative = $file.FullName.Substring($source.TrimEnd('\', '/').Length + 1).Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $lines.Add("entry|$relative|$($file.Length)|$hash")
        }
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Convert-ProjectWorldPackageRootToContentPath {
    param(
        [Parameter(Mandatory = $true)][string]$ContentRoot,
        [Parameter(Mandatory = $true)][string]$PackageRoot
    )

    if ($PackageRoot -notmatch '^/ProjectWorldData/(Generated/[A-Za-z0-9_/]+)/$') {
        throw "Unsupported public World artifact root: $PackageRoot"
    }
    return Join-Path $ContentRoot $Matches[1].Replace('/', [IO.Path]::DirectorySeparatorChar)
}

function Invoke-WithProjectWorldPublicProjection {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorkRoot,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
    $WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
    $expectedWorkRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'tmp\release\work')).TrimEnd('\', '/')
    if (-not $WorkRoot.StartsWith(
            $expectedWorkRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Public World projection work must stay under $expectedWorkRoot`: $WorkRoot"
    }

    $worldScriptRoot = Join-Path $ProjectRoot 'scripts\ue\world'
    . (Join-Path $worldScriptRoot 'generated_content_transaction.ps1')
    $contentRoot = Join-Path $ProjectRoot 'Plugins\World\ProjectWorldData\Content'
    $dataRoot = Join-Path $ProjectRoot 'Plugins\World\ProjectWorldData\Data'
    $generatedPackageRoot = '/ProjectWorldData/Generated/'
    $presentation = Join-Path $dataRoot 'Presentation\kazan_representative_v1.json'
    $compilerBootstrap = Join-Path $ProjectRoot 'tools\World\CanonicalCompilation\bootstrap.py'
    $realizer = Join-Path $worldScriptRoot 'realize_canonical_world.ps1'
    $manifestRoot = Join-Path $WorkRoot 'public-world-authority'
    $snapshotParent = Join-Path $WorkRoot 'world-projection-rollback'
    $snapshotRoot = Join-Path $snapshotParent ([Guid]::NewGuid().ToString('N'))
    $evidenceRoot = Join-Path $ProjectRoot `
        ('Saved\Validation\WorldRealization\public-release\' + [Guid]::NewGuid().ToString('N'))

    $worlds = @(
        [ordered]@{
            Name = 'kazan'
            Compiler = Join-Path $dataRoot 'Profiles\CanonicalCompilation\kazan_territory_v1.compile.json'
            Realization = Join-Path $dataRoot 'Profiles\Realization\kazan_territory_public_v1.realization.json'
            PrivateRealization = Join-Path $dataRoot 'Profiles\Realization\kazan_territory_v1.realization.json'
            Authored = Join-Path $dataRoot 'Authored\kazan_territory_public_v1.json'
            Map = '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory'
        },
        [ordered]@{
            Name = 'manhattan'
            Compiler = Join-Path $dataRoot 'Profiles\CanonicalCompilation\manhattan_showcase_v1.compile.json'
            Realization = Join-Path $dataRoot 'Profiles\Realization\manhattan_showcase_public_v1.realization.json'
            PrivateRealization = Join-Path $dataRoot 'Profiles\Realization\manhattan_showcase_v1.realization.json'
            Authored = Join-Path $dataRoot 'Authored\manhattan_showcase_v1.json'
            Map = '/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase'
        }
    )

    $additionalPaths = [Collections.Generic.List[string]]::new()
    foreach ($world in $worlds) {
        foreach ($profilePath in @($world.Realization, $world.PrivateRealization)) {
            $profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
            if ([string]$profile.map_package -cne [string]$world.Map -or
                [string]$profile.world_data_plugin -cne 'ProjectWorldData') {
                throw "World realization profile identity drift: $profilePath"
            }
            foreach ($layer in @($profile.layers)) {
                $additionalPaths.Add((Convert-ProjectWorldPackageRootToContentPath `
                    -ContentRoot $contentRoot -PackageRoot ([string]$layer.artifact_root)))
            }
        }
    }
    foreach ($path in @(Get-ProjectWorldGeneratedPaths `
            -ContentRoot $contentRoot -MapPackage $worlds[1].Map `
            -GeneratedPackageRoot $generatedPackageRoot -IncludePresentation $false)) {
        $additionalPaths.Add($path)
    }

    $records = @()
    $beforeDigest = ''
    $restored = $false
    try {
        $records = @(New-ProjectWorldGeneratedSnapshot `
            -ContentRoot $contentRoot -MapPackage $worlds[0].Map `
            -GeneratedPackageRoot $generatedPackageRoot -SnapshotRoot $snapshotRoot `
            -AdditionalPaths @($additionalPaths | Sort-Object -Unique))
        $beforeDigest = Get-ProjectWorldProjectionDigest -Records $records
        foreach ($record in $records) {
            if (Test-Path -LiteralPath $record.Source) {
                Remove-Item -LiteralPath $record.Source -Recurse -Force
            }
        }
        New-Item -ItemType Directory -Path $manifestRoot, $evidenceRoot -Force | Out-Null

        foreach ($world in $worlds) {
            $materializeOutput = @(& python -S $compilerBootstrap materialize --profile $world.Compiler)
            if ($LASTEXITCODE -ne 0 -or $materializeOutput.Count -eq 0) {
                throw "Canonical materialization failed for $($world.Name)."
            }
            $materialized = $materializeOutput[-1] | ConvertFrom-Json
            if ([string]$materialized.status -cne 'accepted') {
                throw "Canonical materialization was not accepted for $($world.Name)."
            }
            $compileResult = [string]$materialized.result_path
            if (-not [IO.Path]::IsPathRooted($compileResult)) {
                $compileResult = Join-Path $ProjectRoot $compileResult
            }
            $receipt = Join-Path $evidenceRoot "$($world.Name).json"
            & $realizer -CompileResult $compileResult -Mode Apply -Map $world.Map `
                -WorldDataPlugin ProjectWorldData -PresentationProfile $presentation `
                -AuthoredOverlayProfile $world.Authored -RealizationProfile $world.Realization `
                -ManifestRoot $manifestRoot -EnrollManifests -NonInteractive `
                -RequireLandscape -EvidencePath $receipt
            if ($LASTEXITCODE -ne 0) {
                throw "Public World realization failed for $($world.Name)."
            }
            $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
            if ([string]$result.status -cne 'accepted') {
                throw "Public World realization was not accepted for $($world.Name)."
            }
        }

        & $Action $manifestRoot $ProjectRoot
    }
    finally {
        if ($records.Count -gt 0) {
            Remove-ProjectWorldGeneratedPaths -ContentRoot $contentRoot `
                -MapPackage $worlds[1].Map -GeneratedPackageRoot $generatedPackageRoot `
                -IncludePresentation $false
            Restore-ProjectWorldGeneratedSnapshot -ContentRoot $contentRoot `
                -MapPackage $worlds[0].Map -GeneratedPackageRoot $generatedPackageRoot `
                -Records $records
            $afterDigest = Get-ProjectWorldProjectionDigest -Records $records
            if ($afterDigest -cne $beforeDigest) {
                throw 'Private ProjectWorldData generated content was not restored byte-for-byte.'
            }
            $restored = $true
        }
        if ($restored -and (Test-Path -LiteralPath $snapshotParent)) {
            Remove-Item -LiteralPath $snapshotParent -Recurse -Force
        }
        if ($restored -and (Test-Path -LiteralPath $manifestRoot)) {
            Remove-Item -LiteralPath $manifestRoot -Recurse -Force
        }
        if (Test-Path -LiteralPath $evidenceRoot) {
            Remove-Item -LiteralPath $evidenceRoot -Recurse -Force
        }
    }
}
