# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

function Assert-ProjectCinematicReleaseBinding {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Resolve-ProjectCinematicProjectPath {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $root = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $resolved = if ([IO.Path]::IsPathRooted($Path)) {
        [IO.Path]::GetFullPath($Path)
    }
    else {
        [IO.Path]::GetFullPath((Join-Path $root $Path))
    }
    Assert-ProjectCinematicReleaseBinding ($resolved.StartsWith(
            $root + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) `
        "$Label escaped the project root: $resolved"
    return $resolved
}

function Get-ProjectCinematicPackageTreeDigest {
    param([Parameter(Mandatory = $true)][string]$Path)
    $root = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $lines = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            '{0}|{1}|{2}' -f $relative, $_.Length, $hash
        })
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash(
                    [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-ProjectCinematicShippingExecutable {
    param([Parameter(Mandatory = $true)][string]$PackageRoot)
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'Windows') `
        -Recurse -File -Filter 'Alis*.exe' |
        Where-Object { $_.FullName -match '[\\/]Alis[\\/]Binaries[\\/]Win64[\\/]' } |
        Sort-Object FullName)
    Assert-ProjectCinematicReleaseBinding ($candidates.Count -eq 1) `
        "Expected exactly one staged Shipping executable under $PackageRoot."
    return $candidates[0].FullName
}

function Test-ProjectCinematicReleaseBinding {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ReleaseAcceptancePath,
        [Parameter(Mandatory = $true)][string]$ExpectedPackageRoot,
        [Parameter(Mandatory = $true)][string]$CurrentSourceStateSha256,
        [Parameter(Mandatory = $true)][string]$CurrentSourceRevision
    )

    $acceptancePath = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path $ReleaseAcceptancePath -Label 'Release acceptance receipt'
    $expectedRoot = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path $ExpectedPackageRoot -Label 'Expected package root'
    Assert-ProjectCinematicReleaseBinding (Test-Path -LiteralPath $acceptancePath -PathType Leaf) `
        'Operator acceptance receipt is unavailable.'
    Assert-ProjectCinematicReleaseBinding (Test-Path -LiteralPath $expectedRoot -PathType Container) `
        'Accepted Shipping Candidate is unavailable.'

    $acceptance = Get-Content -LiteralPath $acceptancePath -Raw | ConvertFrom-Json
    foreach ($field in @(
            'schema_version', 'status', 'product_decision', 'package_root',
            'package_tree_sha256', 'shipping_executable',
            'shipping_executable_sha256', 'source_revision',
            'source_state_sha256', 'map_package', 'runtime_profile',
            'runtime_profile_sha256', 'release_operation_id',
            'release_composite', 'release_composite_sha256')) {
        Assert-ProjectCinematicReleaseBinding ($null -ne $acceptance.$field) `
            "Operator acceptance receipt is missing '$field'."
    }
    Assert-ProjectCinematicReleaseBinding ($acceptance.schema_version -eq 1) `
        'Unsupported operator acceptance receipt version.'
    Assert-ProjectCinematicReleaseBinding ([string]$acceptance.status -ceq 'operator_accepted') `
        'Track V requires an operator_accepted Candidate.'
    Assert-ProjectCinematicReleaseBinding ([string]$acceptance.product_decision -ceq 'accepted') `
        'Track V requires an accepted product decision.'

    $receiptPackageRoot = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path ([string]$acceptance.package_root) `
        -Label 'Receipt package root'
    Assert-ProjectCinematicReleaseBinding ($receiptPackageRoot.Equals(
            $expectedRoot, [StringComparison]::OrdinalIgnoreCase)) `
        'Operator acceptance receipt names a different package root.'

    $shippingExecutable = Get-ProjectCinematicShippingExecutable -PackageRoot $expectedRoot
    $receiptExecutable = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path ([string]$acceptance.shipping_executable) `
        -Label 'Receipt Shipping executable'
    Assert-ProjectCinematicReleaseBinding ($receiptExecutable.Equals(
            $shippingExecutable, [StringComparison]::OrdinalIgnoreCase)) `
        'Operator acceptance receipt names a different Shipping executable.'

    $packageTreeHash = Get-ProjectCinematicPackageTreeDigest -Path $expectedRoot
    $shippingExecutableHash = (Get-FileHash -LiteralPath $shippingExecutable `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-ProjectCinematicReleaseBinding ($packageTreeHash -ceq `
            [string]$acceptance.package_tree_sha256) `
        'Accepted Candidate tree no longer matches the operator receipt.'
    Assert-ProjectCinematicReleaseBinding ($shippingExecutableHash -ceq `
            [string]$acceptance.shipping_executable_sha256) `
        'Accepted Shipping executable no longer matches the operator receipt.'
    Assert-ProjectCinematicReleaseBinding ($CurrentSourceStateSha256 -ceq `
            [string]$acceptance.source_state_sha256) `
        'Current source state differs from the operator-accepted Candidate source.'
    Assert-ProjectCinematicReleaseBinding ($CurrentSourceRevision -ceq `
            [string]$acceptance.source_revision) `
        'Current source revision differs from the operator-accepted Candidate source.'

    $releaseCompositePath = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path ([string]$acceptance.release_composite) `
        -Label 'Release composite'
    Assert-ProjectCinematicReleaseBinding (Test-Path -LiteralPath $releaseCompositePath -PathType Leaf) `
        'Accepted release composite is unavailable.'
    $releaseCompositeHash = (Get-FileHash -LiteralPath $releaseCompositePath `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-ProjectCinematicReleaseBinding ($releaseCompositeHash -ceq `
            [string]$acceptance.release_composite_sha256) `
        'Accepted release composite no longer matches the operator receipt.'

    $releaseComposite = Get-Content -LiteralPath $releaseCompositePath -Raw | ConvertFrom-Json
    Assert-ProjectCinematicReleaseBinding ([string]$releaseComposite.status -ceq 'accepted') `
        'Release composite is not accepted.'
    Assert-ProjectCinematicReleaseBinding ([string]$releaseComposite.operation_id -ceq `
            [string]$acceptance.release_operation_id) `
        'Release operation identity differs from the operator receipt.'
    foreach ($field in @('revision', 'source_state_sha256', 'map_package',
            'runtime_profile', 'runtime_profile_sha256', 'shipping_executable_sha256',
            'shipping_package_sha256')) {
        $acceptanceField = if ($field -ceq 'revision') { 'source_revision' } `
            elseif ($field -ceq 'shipping_package_sha256') { 'package_tree_sha256' } `
            else { $field }
        Assert-ProjectCinematicReleaseBinding ([string]$releaseComposite.$field -ceq `
                [string]$acceptance.$acceptanceField) `
            "Release composite field '$field' differs from operator acceptance."
    }
    $compositePackageRoot = Resolve-ProjectCinematicProjectPath `
        -ProjectRoot $ProjectRoot -Path ([string]$releaseComposite.final_package) `
        -Label 'Composite final package'
    Assert-ProjectCinematicReleaseBinding ($compositePackageRoot.Equals(
            $expectedRoot, [StringComparison]::OrdinalIgnoreCase)) `
        'Release composite names a different final package.'

    return [pscustomobject]@{
        acceptance_path = $acceptancePath
        acceptance = $acceptance
        release_composite_path = $releaseCompositePath
        release_composite = $releaseComposite
        package_root = $expectedRoot
        package_tree_sha256 = $packageTreeHash
        shipping_executable = $shippingExecutable
        shipping_executable_sha256 = $shippingExecutableHash
        release_composite_sha256 = $releaseCompositeHash
    }
}
