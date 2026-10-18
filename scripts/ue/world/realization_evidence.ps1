# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Resolve-ProjectWorldEvidenceLocation {
    param([string]$ProjectRoot, [string]$EvidencePath, [string]$ModeName,
        [System.Collections.IDictionary]$InvocationIdentity)
    if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes(($InvocationIdentity | ConvertTo-Json -Compress))
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try { $identity = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $result = Join-Path $ProjectRoot "Saved/Validation/WorldRealization/$identity/$ModeName.json"
    }
    else { $result = [System.IO.Path]::GetFullPath($EvidencePath) }
    $result = [System.IO.Path]::GetFullPath($result)
    $allowed = $false
    foreach ($relative in @('Saved/Validation/WorldRealization', 'tmp/world')) {
        $prefix = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $relative)) + [System.IO.Path]::DirectorySeparatorChar
        if ($result.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) { $allowed = $true }
    }
    if (-not $allowed) { throw 'Evidence path must stay under Saved/Validation/WorldRealization or tmp/world.' }
    return [pscustomobject]@{ ResultPath=$result; Directory=(Split-Path -Parent $result);
        LogPath=[System.IO.Path]::ChangeExtension($result, '.log') }
}

function Assert-ProjectWorldEvidenceInputIsolation {
    param([Parameter(Mandatory = $true)][string]$ResultPath,
        [Parameter(Mandatory = $true)][string]$CompileResultPath,
        [AllowEmptyCollection()][string[]]$InputPaths = @(),
        [AllowEmptyCollection()][string[]]$ProtectedRoots = @(),
        [AllowEmptyCollection()][string[]]$AuxiliaryOutputPaths = @())
    $result = [System.IO.Path]::GetFullPath($ResultPath)
    $roots = @((Split-Path -Parent $CompileResultPath)) + $ProtectedRoots
    $seen = @{}
    foreach ($rawOutput in (@($result, [System.IO.Path]::ChangeExtension($result, '.log'), ($result + '.tmp')) + $AuxiliaryOutputPaths)) {
        $output = [System.IO.Path]::GetFullPath($rawOutput)
        if ($seen.ContainsKey($output)) { throw 'Realization output files alias each other.' }
        $seen[$output] = $true
        foreach ($root in $roots) {
            $fullRoot = [System.IO.Path]::GetFullPath($root).TrimEnd('\', '/')
            $prefix = $fullRoot + [System.IO.Path]::DirectorySeparatorChar
            if ($output.Equals($fullRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
                $output.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw 'Realization evidence overlaps the protected canonical input tree or manifest authority.'
            }
        }
        foreach ($protectedPath in $InputPaths) {
            if (-not [string]::IsNullOrWhiteSpace($protectedPath) -and
                $output.Equals([System.IO.Path]::GetFullPath($protectedPath), [System.StringComparison]::OrdinalIgnoreCase)) {
                throw 'Realization evidence or a sidecar overlaps a requested input.'
            }
        }
    }
}
