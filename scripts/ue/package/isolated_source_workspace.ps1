#Requires -Version 5.1
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Get-ProjectIsolatedFileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProjectIsolatedTextSha256 {
    param([Parameter(Mandatory = $true)][string[]]$Lines)

    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes(($Lines -join "`n"))
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Invoke-ProjectIsolatedReadOnlyGit {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $previous = $env:GIT_OPTIONAL_LOCKS
    try {
        $env:GIT_OPTIONAL_LOCKS = '0'
        $output = @(& git -C $Root @Arguments)
        $exitCode = $LASTEXITCODE
        return [pscustomobject]@{ ExitCode = $exitCode; Output = $output }
    }
    finally {
        if ($null -eq $previous) {
            Remove-Item Env:GIT_OPTIONAL_LOCKS -ErrorAction SilentlyContinue
        }
        else {
            $env:GIT_OPTIONAL_LOCKS = $previous
        }
    }
}

function Assert-ProjectIsolatedPathUnderRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $resolvedPath = [IO.Path]::GetFullPath($Path)
    $resolvedRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not $resolvedPath.StartsWith(
            $resolvedRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain under $resolvedRoot`: $resolvedPath"
    }
    return $resolvedPath
}

function ConvertTo-ProjectIsolatedRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $root = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $resolved = [IO.Path]::GetFullPath((Join-Path $root $Path))
    if (-not $resolved.StartsWith(
            $root + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Candidate path escapes the project root: $Path"
    }
    $relative = $resolved.Substring($root.Length + 1).Replace('\', '/')
    if ($relative.StartsWith('-') -or $relative.Contains('..')) {
        throw "Candidate path is unsafe: $Path"
    }
    return $relative
}

function Test-ProjectIsolatedExcludedPath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $segments = @($RelativePath.Replace('\', '/').Split('/'))
    return @($segments | Where-Object {
        $_ -in @('.git', '.vs', 'Binaries', 'DerivedDataCache', 'Intermediate', 'Saved', '__pycache__')
    }).Count -gt 0
}

function Get-ProjectIsolatedCandidateFiles {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string[]]$CandidatePaths
    )

    $files = [Collections.Generic.List[string]]::new()
    foreach ($candidate in $CandidatePaths) {
        $relative = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $ProjectRoot -Path $candidate
        $absolute = Join-Path $ProjectRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if (Test-Path -LiteralPath $absolute -PathType Leaf) {
            if (-not (Test-ProjectIsolatedExcludedPath -RelativePath $relative)) {
                $files.Add($relative)
            }
            continue
        }
        if (Test-Path -LiteralPath $absolute -PathType Container) {
            foreach ($file in @(Get-ChildItem -LiteralPath $absolute -Recurse -File | Sort-Object FullName)) {
                $child = $file.FullName.Substring([IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/').Length + 1).
                    Replace('\', '/')
                if (-not (Test-ProjectIsolatedExcludedPath -RelativePath $child)) {
                    $files.Add($child)
                }
            }
        }
    }
    return @($files | Sort-Object -Unique)
}

function Assert-ProjectIsolatedRequiredFiles {
    param(
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)][string[]]$RequiredPaths
    )

    foreach ($required in $RequiredPaths) {
        $relative = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $WorkspaceRoot -Path $required
        $absolute = Join-Path $WorkspaceRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $files = if (Test-Path -LiteralPath $absolute -PathType Container) {
            @(Get-ChildItem -LiteralPath $absolute -Recurse -File)
        }
        elseif (Test-Path -LiteralPath $absolute -PathType Leaf) {
            @(Get-Item -LiteralPath $absolute)
        }
        else {
            throw "Required isolated source path is missing: $relative"
        }
        foreach ($file in $files) {
            $stream = [IO.File]::OpenRead($file.FullName)
            try {
                $length = [Math]::Min(200, [int]$stream.Length)
                $bytes = New-Object byte[] $length
                [void]$stream.Read($bytes, 0, $length)
                $prefix = [Text.Encoding]::ASCII.GetString($bytes)
            }
            finally {
                $stream.Dispose()
            }
            if ($prefix.StartsWith('version https://git-lfs.github.com/spec/v1')) {
                throw "Required isolated source remains a Git LFS pointer: $relative"
            }
        }
    }
}

function New-ProjectIsolatedSourceOverlay {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$SourceCommit,
        [Parameter(Mandatory = $true)][string[]]$CandidatePaths,
        [Parameter(Mandatory = $true)][string[]]$RequiredPaths,
        [Parameter(Mandatory = $true)][string]$OverlayRoot
    )

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $OverlayRoot = Assert-ProjectIsolatedPathUnderRoot -Path $OverlayRoot `
        -Root (Join-Path $ProjectRoot 'tmp') -Label 'Overlay root'
    if (Test-Path -LiteralPath $OverlayRoot) {
        throw "Overlay root already exists: $OverlayRoot"
    }
    $resolve = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments @('rev-parse', '--verify', "$SourceCommit`^{commit}")
    $resolvedCommit = (@($resolve.Output) -join '').Trim()
    if ($resolve.ExitCode -ne 0 -or $resolvedCommit -notmatch '^[0-9a-f]{40}$') {
        throw "Source commit does not resolve to one exact commit: $SourceCommit"
    }
    $candidateRoots = @($CandidatePaths | ForEach-Object {
        ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $ProjectRoot -Path $_
    } | Sort-Object -Unique)
    $requiredRoots = @($RequiredPaths | ForEach-Object {
        ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $ProjectRoot -Path $_
    } | Sort-Object -Unique)
    $candidateFiles = @(Get-ProjectIsolatedCandidateFiles `
        -ProjectRoot $ProjectRoot -CandidatePaths $candidateRoots)
    New-Item -ItemType Directory -Path (Join-Path $OverlayRoot 'files') -Force | Out-Null

    $patchPath = Join-Path $OverlayRoot 'tracked.patch'
    $patchArguments = @(
        '-c', 'core.quotepath=false', 'diff', '--binary', '--full-index',
        "--output=$patchPath", $resolvedCommit, '--') + $candidateRoots
    $patchResult = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments $patchArguments
    if ($patchResult.ExitCode -ne 0) {
        throw 'Cannot create the tracked candidate patch.'
    }
    $trackedArguments = @(
        '-c', 'core.quotepath=false', 'diff', '--name-only', $resolvedCommit, '--') + $candidateRoots
    $trackedResult = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments $trackedArguments
    $trackedChanges = @($trackedResult.Output |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
    if ($trackedResult.ExitCode -ne 0) {
        throw 'Cannot enumerate tracked candidate changes.'
    }

    $untracked = [Collections.Generic.List[object]]::new()
    foreach ($relative in $candidateFiles) {
        $matchResult = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
            -Arguments @('-c', 'core.quotepath=false', 'ls-files', '--', $relative)
        $trackedMatch = @($matchResult.Output)
        if ($matchResult.ExitCode -ne 0) {
            throw "Cannot classify candidate path ownership: $relative"
        }
        if ($relative -in $trackedMatch) {
            continue
        }
        $source = Join-Path $ProjectRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $blob = Join-Path (Join-Path $OverlayRoot 'files') `
            $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        New-Item -ItemType Directory -Path (Split-Path -Parent $blob) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $blob
        $untracked.Add([pscustomobject][ordered]@{
            path = $relative
            length = (Get-Item -LiteralPath $source).Length
            sha256 = Get-ProjectIsolatedFileSha256 -Path $source
        })
    }

    $resultPaths = @($trackedChanges + @($untracked | ForEach-Object { $_.path }) | Sort-Object -Unique)
    $resultEntries = [Collections.Generic.List[object]]::new()
    foreach ($relative in $resultPaths) {
        $source = Join-Path $ProjectRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            $resultEntries.Add([pscustomobject][ordered]@{
                path = $relative
                state = 'file'
                length = (Get-Item -LiteralPath $source).Length
                sha256 = Get-ProjectIsolatedFileSha256 -Path $source
            })
        }
        else {
            $resultEntries.Add([pscustomobject][ordered]@{
                path = $relative
                state = 'absent'
                length = 0
                sha256 = 'none'
            })
        }
    }
    $patchSha256 = Get-ProjectIsolatedFileSha256 -Path $patchPath
    $identityLines = @("base|$resolvedCommit", "patch|$patchSha256") + @(
        $resultEntries | Sort-Object path | ForEach-Object {
            "result|$($_.path)|$($_.state)|$($_.length)|$($_.sha256)"
        })
    $sourceIdentity = Get-ProjectIsolatedTextSha256 -Lines $identityLines
    $receipt = [ordered]@{
        schema = 'project-isolated-source-overlay:v1'
        project_root = $ProjectRoot.Replace('\', '/')
        source_commit = $resolvedCommit
        candidate_roots = $candidateRoots
        required_paths = $requiredRoots
        tracked_patch = [ordered]@{
            path = 'tracked.patch'
            length = (Get-Item -LiteralPath $patchPath).Length
            sha256 = $patchSha256
        }
        tracked_changed_paths = $trackedChanges
        untracked_files = @($untracked)
        result_inventory = @($resultEntries)
        source_identity_sha256 = $sourceIdentity
    }
    $receiptPath = Join-Path $OverlayRoot 'overlay-receipt.json'
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8
    return Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
}

function Test-ProjectIsolatedOwnerAlive {
    param([Parameter(Mandatory = $true)][object]$Receipt)

    $process = Get-Process -Id ([int]$Receipt.owner_process_id) -ErrorAction SilentlyContinue
    if ($null -eq $process) {
        return $false
    }
    try {
        return $process.StartTime.ToUniversalTime().ToString('o') -ceq `
            [string]$Receipt.owner_process_start_utc
    }
    catch {
        return $false
    }
}

function Remove-ProjectIsolatedSourceWorkspace {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot
    )

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $parent = Join-Path $ProjectRoot 'tmp\release\projection'
    $WorkspaceRoot = Assert-ProjectIsolatedPathUnderRoot -Path $WorkspaceRoot `
        -Root $parent -Label 'Workspace root'
    $ownerPath = Join-Path $WorkspaceRoot 'workspace-owner.json'
    if (-not (Test-Path -LiteralPath $ownerPath -PathType Leaf)) {
        throw "Workspace cleanup requires its owner receipt: $WorkspaceRoot"
    }
    $owner = Get-Content -LiteralPath $ownerPath -Raw | ConvertFrom-Json
    if ([string]$owner.schema -cne 'project-isolated-source-workspace-owner:v1' -or
        [IO.Path]::GetFullPath([string]$owner.workspace_root) -cne $WorkspaceRoot -or
        [IO.Path]::GetFullPath([string]$owner.project_root) -cne $ProjectRoot) {
        throw "Workspace owner receipt does not match the cleanup target: $WorkspaceRoot"
    }
    $checkout = Join-Path $WorkspaceRoot 'repo'
    if (Test-Path -LiteralPath $checkout) {
        & git -c core.longpaths=true -C $ProjectRoot worktree remove --force $checkout | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Git refused isolated workspace removal: $checkout"
        }
    }
    if (Test-Path -LiteralPath $WorkspaceRoot) {
        Remove-Item -LiteralPath $WorkspaceRoot -Recurse -Force
    }
}

function Remove-StaleProjectIsolatedSourceWorkspaces {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $parent = Join-Path $ProjectRoot 'tmp\release\projection'
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        return
    }
    foreach ($directory in @(Get-ChildItem -LiteralPath $parent -Directory)) {
        $ownerPath = Join-Path $directory.FullName 'workspace-owner.json'
        if (-not (Test-Path -LiteralPath $ownerPath -PathType Leaf)) {
            continue
        }
        $owner = Get-Content -LiteralPath $ownerPath -Raw | ConvertFrom-Json
        if (-not (Test-ProjectIsolatedOwnerAlive -Receipt $owner)) {
            Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $ProjectRoot `
                -WorkspaceRoot $directory.FullName
        }
    }
}

function Assert-ProjectIsolatedSourceWorkspace {
    param(
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)][string]$OverlayRoot
    )

    $WorkspaceRoot = [IO.Path]::GetFullPath($WorkspaceRoot)
    $OverlayRoot = [IO.Path]::GetFullPath($OverlayRoot)
    $checkout = Join-Path $WorkspaceRoot 'repo'
    $overlay = Get-Content -LiteralPath (Join-Path $OverlayRoot 'overlay-receipt.json') -Raw |
        ConvertFrom-Json
    $head = (& git -C $checkout rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cne [string]$overlay.source_commit) {
        throw 'Isolated workspace HEAD does not match the overlay base commit.'
    }
    $actualEntries = [Collections.Generic.List[object]]::new()
    foreach ($entry in @($overlay.result_inventory)) {
        $path = Join-Path $checkout ([string]$entry.path).Replace('/', [IO.Path]::DirectorySeparatorChar)
        if ([string]$entry.state -ceq 'absent') {
            if (Test-Path -LiteralPath $path) {
                throw "Materialized overlay retained an expected deletion: $($entry.path)"
            }
            $actualEntries.Add($entry)
            continue
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
            (Get-Item -LiteralPath $path).Length -ne [long]$entry.length -or
            (Get-ProjectIsolatedFileSha256 -Path $path) -cne [string]$entry.sha256) {
            throw "Materialized overlay byte identity mismatch: $($entry.path)"
        }
        $actualEntries.Add($entry)
    }
    $expectedUntracked = @($overlay.untracked_files | ForEach-Object { [string]$_.path } | Sort-Object)
    $actualUntracked = @(& git -C $checkout -c core.quotepath=false ls-files --others --exclude-standard |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object)
    $unexpected = @($actualUntracked | Where-Object { $_ -notin $expectedUntracked })
    $omitted = @($expectedUntracked | Where-Object { $_ -notin $actualUntracked })
    if ($unexpected.Count -gt 0 -or $omitted.Count -gt 0) {
        throw "Materialized overlay untracked inventory mismatch; extra=$($unexpected -join ',') omitted=$($omitted -join ',')"
    }
    Assert-ProjectIsolatedRequiredFiles -WorkspaceRoot $checkout `
        -RequiredPaths @($overlay.required_paths)
    $identityLines = @("base|$($overlay.source_commit)", "patch|$($overlay.tracked_patch.sha256)") + @(
        $actualEntries | Sort-Object path | ForEach-Object {
            "result|$($_.path)|$($_.state)|$($_.length)|$($_.sha256)"
        })
    $identity = Get-ProjectIsolatedTextSha256 -Lines $identityLines
    if ($identity -cne [string]$overlay.source_identity_sha256) {
        throw 'Materialized overlay aggregate source identity mismatch.'
    }
    return $identity
}

function New-ProjectIsolatedSourceWorkspace {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$OverlayRoot
    )

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $OverlayRoot = [IO.Path]::GetFullPath($OverlayRoot)
    $overlay = Get-Content -LiteralPath (Join-Path $OverlayRoot 'overlay-receipt.json') -Raw |
        ConvertFrom-Json
    if ([IO.Path]::GetFullPath([string]$overlay.project_root) -cne $ProjectRoot) {
        throw 'Overlay project identity does not match the requested project.'
    }
    Remove-StaleProjectIsolatedSourceWorkspaces -ProjectRoot $ProjectRoot
    $parent = Join-Path $ProjectRoot 'tmp\release\projection'
    $workspace = Join-Path $parent ([Guid]::NewGuid().ToString('N'))
    $checkout = Join-Path $workspace 'repo'
    New-Item -ItemType Directory -Path $workspace -Force | Out-Null
    $ownerProcess = Get-Process -Id $PID
    [ordered]@{
        schema = 'project-isolated-source-workspace-owner:v1'
        project_root = $ProjectRoot
        workspace_root = $workspace
        source_commit = [string]$overlay.source_commit
        owner_process_id = $PID
        owner_process_start_utc = $ownerProcess.StartTime.ToUniversalTime().ToString('o')
        overlay_root = $OverlayRoot
    } | ConvertTo-Json -Depth 5 | Set-Content `
        -LiteralPath (Join-Path $workspace 'workspace-owner.json') -Encoding utf8
    try {
        & git -c core.longpaths=true -C $ProjectRoot worktree add --detach $checkout `
            ([string]$overlay.source_commit) |
            Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw 'Cannot create the detached comparison worktree.'
        }
        & git -C $checkout config core.longpaths true
        if ($LASTEXITCODE -ne 0) {
            throw 'Cannot persist long-path support in the detached comparison worktree.'
        }
        $patchPath = Join-Path $OverlayRoot ([string]$overlay.tracked_patch.path)
        if ((Get-Item -LiteralPath $patchPath).Length -gt 0) {
            & git -C $checkout apply --binary --whitespace=nowarn $patchPath | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw 'Cannot apply the authenticated tracked candidate patch.'
            }
        }
        foreach ($entry in @($overlay.untracked_files)) {
            $source = Join-Path (Join-Path $OverlayRoot 'files') `
                ([string]$entry.path).Replace('/', [IO.Path]::DirectorySeparatorChar)
            $destination = Join-Path $checkout `
                ([string]$entry.path).Replace('/', [IO.Path]::DirectorySeparatorChar)
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination
        }
        if (@($overlay.required_paths).Count -gt 0) {
            & git -C $checkout lfs checkout -- @($overlay.required_paths) | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw 'Git LFS checkout failed for required isolated source paths.'
            }
        }
        $identity = Assert-ProjectIsolatedSourceWorkspace `
            -WorkspaceRoot $workspace -OverlayRoot $OverlayRoot
        [ordered]@{
            schema = 'project-isolated-source-workspace:v1'
            status = 'accepted'
            workspace_root = $workspace.Replace('\', '/')
            checkout_root = $checkout.Replace('\', '/')
            source_commit = [string]$overlay.source_commit
            source_identity_sha256 = $identity
            overlay_receipt_sha256 = Get-ProjectIsolatedFileSha256 `
                -Path (Join-Path $OverlayRoot 'overlay-receipt.json')
        } | ConvertTo-Json -Depth 5 | Set-Content `
            -LiteralPath (Join-Path $workspace 'workspace-receipt.json') -Encoding utf8
        return Get-Content -LiteralPath (Join-Path $workspace 'workspace-receipt.json') -Raw |
            ConvertFrom-Json
    }
    catch {
        if (Test-Path -LiteralPath (Join-Path $workspace 'workspace-owner.json')) {
            Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $ProjectRoot -WorkspaceRoot $workspace
        }
        throw
    }
}
