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

function Assert-ProjectIsolatedRequiredLfsObjects {
    param([string]$WorkspaceRoot, [string[]]$RequiredPaths)

    $listed = @(& git -C $WorkspaceRoot lfs ls-files --long)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect required Git LFS object identities.' }
    foreach ($line in $listed) {
        if ($line -notmatch '^([0-9a-f]{64}) [*\-] (.+)$') { continue }
        $oid = $Matches[1]
        $path = $Matches[2]
        $required = @($RequiredPaths | Where-Object {
            $path -ceq $_ -or $path.StartsWith($_.TrimEnd('/') + '/', [StringComparison]::Ordinal)
        }).Count -gt 0
        if (-not $required) { continue }
        $file = Join-Path $WorkspaceRoot $path.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or
            (Get-ProjectIsolatedFileSha256 -Path $file) -cne $oid) {
            throw "Required Git LFS payload has wrong object identity: $path"
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
        '-c', 'core.quotepath=false', 'diff', '--no-renames', '--name-only', $resolvedCommit, '--') + $candidateRoots
    $trackedResult = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments $trackedArguments
    $trackedChanges = @($trackedResult.Output |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
    if ($trackedResult.ExitCode -ne 0) {
        throw 'Cannot enumerate tracked candidate changes.'
    }

    $trackedFilesResult = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments (@('-c', 'core.quotepath=false', 'ls-files', '--') + $candidateRoots)
    if ($trackedFilesResult.ExitCode -ne 0) {
        throw 'Cannot classify candidate path ownership.'
    }
    $trackedFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in @($trackedFilesResult.Output)) {
        if (-not [string]::IsNullOrWhiteSpace($path)) {
            [void]$trackedFiles.Add([string]$path)
        }
    }
    $untracked = [Collections.Generic.List[object]]::new()
    foreach ($relative in $candidateFiles) {
        if ($trackedFiles.Contains($relative)) {
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
            $entry = [pscustomobject][ordered]@{
                path = $relative
                state = 'file'
                length = (Get-Item -LiteralPath $source).Length
                sha256 = Get-ProjectIsolatedFileSha256 -Path $source
            }
            $blob = Join-Path (Join-Path $OverlayRoot 'files') `
                $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
            if (-not (Test-Path -LiteralPath $blob -PathType Leaf)) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $blob) -Force | Out-Null
                Copy-Item -LiteralPath $source -Destination $blob
            }
            if ((Get-Item -LiteralPath $blob).Length -ne [long]$entry.length -or
                (Get-ProjectIsolatedFileSha256 -Path $blob) -cne [string]$entry.sha256) {
                throw "Candidate changed during source snapshot: $relative"
            }
            $resultEntries.Add($entry)
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
        -not [IO.Path]::GetFullPath([string]$owner.workspace_root).Equals(
            $WorkspaceRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFullPath([string]$owner.project_root).Equals(
            $ProjectRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [string]$owner.source_commit -notmatch '^[0-9a-f]{40}$') {
        throw "Workspace owner receipt does not match the cleanup target: $WorkspaceRoot"
    }
    if ((Test-ProjectIsolatedOwnerAlive -Receipt $owner) -and
        [int]$owner.owner_process_id -ne $PID) {
        throw "Workspace belongs to another live process: $WorkspaceRoot"
    }
    $checkout = Join-Path $WorkspaceRoot 'repo'
    foreach ($path in @((Join-Path $ProjectRoot 'tmp'),
            (Join-Path $ProjectRoot 'tmp\release'), $parent, $WorkspaceRoot, $checkout)) {
        if (Test-Path -LiteralPath $path) {
            $item = Get-Item -LiteralPath $path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Workspace cleanup refuses a reparse point: $path"
            }
        }
    }
    $listed = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments @('worktree', 'list', '--porcelain')
    if ($listed.ExitCode -ne 0) {
        throw 'Cannot verify registered worktrees before isolated cleanup.'
    }
    $entries = @((@($listed.Output) -join "`n") -split "`n`n")
    $registeredEntries = @($entries | Where-Object {
        $_ -match '(?m)^worktree (.+)$' -and
        [IO.Path]::GetFullPath($Matches[1]).Equals($checkout, [StringComparison]::OrdinalIgnoreCase)
    })
    if ($registeredEntries.Count -gt 1 -or
        ($registeredEntries.Count -eq 0 -and (Test-Path -LiteralPath $checkout))) {
        throw "Workspace checkout registration disagrees with its receipt: $checkout"
    }
    if ($registeredEntries.Count -eq 1 -and -not (Test-Path -LiteralPath $checkout)) {
        throw "Registered workspace checkout is missing on disk: $checkout"
    }
    if ($registeredEntries.Count -eq 1) {
        if ($registeredEntries[0] -notmatch '(?m)^HEAD ([0-9a-f]{40})$' -or
            $Matches[1] -cne [string]$owner.source_commit -or
            $registeredEntries[0] -notmatch '(?m)^detached$') {
            throw "Workspace checkout HEAD or detached state disagrees with its receipt: $checkout"
        }
    }
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

function Initialize-ProjectIsolatedCommittedFiles {
    param([string]$CheckoutRoot, [string]$SourceCommit, [string[]]$RequiredPaths)

    if ($RequiredPaths.Count -eq 0) { return }
    foreach ($required in $RequiredPaths) {
        $tracked = Invoke-ProjectIsolatedReadOnlyGit -Root $CheckoutRoot `
            -Arguments @('ls-files', '--', $required)
        if ($tracked.ExitCode -ne 0 -or @($tracked.Output).Count -eq 0) {
            throw "Required committed source has no tracked files: $required"
        }
        foreach ($path in @($tracked.Output)) {
            if (-not (Test-Path -LiteralPath (Join-Path $CheckoutRoot $path) -PathType Leaf)) {
                throw "Required committed source file is missing: $path"
            }
        }
    }
    & git -C $CheckoutRoot lfs checkout -- @RequiredPaths | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot materialize local Git LFS objects.' }
    try {
        Assert-ProjectIsolatedRequiredFiles -WorkspaceRoot $CheckoutRoot -RequiredPaths $RequiredPaths
    }
    catch {
        $include = (@($RequiredPaths | ForEach-Object {
            $_.TrimEnd('/'); $_.TrimEnd('/') + '/**'
        }) -join ',')
        & git -C $CheckoutRoot lfs fetch origin $SourceCommit "--include=$include" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch required release Git LFS objects.' }
        & git -C $CheckoutRoot lfs checkout -- @RequiredPaths | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Cannot materialize fetched Git LFS objects.' }
        Assert-ProjectIsolatedRequiredFiles -WorkspaceRoot $CheckoutRoot -RequiredPaths $RequiredPaths
    }
    Assert-ProjectIsolatedRequiredLfsObjects -WorkspaceRoot $CheckoutRoot -RequiredPaths $RequiredPaths
}

function New-ProjectIsolatedCommittedWorkspace {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$SourceCommit,
        [string[]]$RequiredPaths = @()
    )

    $ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    if ($SourceCommit -cnotmatch '^[0-9a-f]{40}$') {
        throw 'SourceCommit must be one full 40-digit commit object ID.'
    }
    $resolved = Invoke-ProjectIsolatedReadOnlyGit -Root $ProjectRoot `
        -Arguments @('rev-parse', '--verify', "$SourceCommit`^{commit}")
    if ($resolved.ExitCode -ne 0 -or (@($resolved.Output) -join '').Trim() -cne $SourceCommit) {
        throw "SourceCommit is not an available commit: $SourceCommit"
    }
    Remove-StaleProjectIsolatedSourceWorkspaces -ProjectRoot $ProjectRoot
    $workspace = Join-Path $ProjectRoot ('tmp\release\projection\' + [Guid]::NewGuid().ToString('N'))
    $checkout = Join-Path $workspace 'repo'
    New-Item -ItemType Directory -Path $workspace -Force | Out-Null
    $ownerProcess = Get-Process -Id $PID
    [ordered]@{
        schema = 'project-isolated-source-workspace-owner:v1'
        project_root = $ProjectRoot
        workspace_root = $workspace
        source_commit = $SourceCommit
        owner_process_id = $PID
        owner_process_start_utc = $ownerProcess.StartTime.ToUniversalTime().ToString('o')
        mode = 'committed-release'
    } | ConvertTo-Json -Depth 5 | Set-Content `
        -LiteralPath (Join-Path $workspace 'workspace-owner.json') -Encoding utf8
    try {
        $priorSkipSmudge = $env:GIT_LFS_SKIP_SMUDGE
        try {
            $env:GIT_LFS_SKIP_SMUDGE = '1'
            & git -c core.longpaths=true -C $ProjectRoot worktree add --detach $checkout $SourceCommit | Out-Null
        }
        finally {
            if ($null -eq $priorSkipSmudge) {
                Remove-Item Env:GIT_LFS_SKIP_SMUDGE -ErrorAction SilentlyContinue
            }
            else { $env:GIT_LFS_SKIP_SMUDGE = $priorSkipSmudge }
        }
        if ($LASTEXITCODE -ne 0) { throw 'Cannot create committed release worktree.' }
        & git -C $checkout config core.longpaths true
        if ($LASTEXITCODE -ne 0) { throw 'Cannot configure release worktree long paths.' }
        Initialize-ProjectIsolatedCommittedFiles -CheckoutRoot $checkout `
            -SourceCommit $SourceCommit -RequiredPaths $RequiredPaths
        return [pscustomobject]@{
            workspace_root = $workspace
            checkout_root = $checkout
            source_commit = $SourceCommit
        }
    }
    catch {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $ProjectRoot -WorkspaceRoot $workspace
        throw
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

function Copy-ProjectIsolatedCandidateFiles {
    param(
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)][string]$OverlayRoot,
        [Parameter(Mandatory = $true)][object]$Overlay
    )

    foreach ($entry in @($Overlay.result_inventory)) {
        if ([string]$entry.state -cne 'file') { continue }
        $relative = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $WorkspaceRoot `
            -Path ([string]$entry.path)
        $target = Join-Path $WorkspaceRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $source = Join-Path (Join-Path $OverlayRoot 'files') `
            $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
            (Get-Item -LiteralPath $source).Length -ne [long]$entry.length -or
            (Get-ProjectIsolatedFileSha256 -Path $source) -cne [string]$entry.sha256) {
            throw "Authenticated candidate bytes are unavailable: $relative"
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
}

function Remove-ProjectIsolatedCandidateDeletions {
    param(
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)][object]$Overlay
    )

    foreach ($entry in @($Overlay.result_inventory)) {
        if ([string]$entry.state -cne 'absent') { continue }
        $relative = ConvertTo-ProjectIsolatedRelativePath -ProjectRoot $WorkspaceRoot `
            -Path ([string]$entry.path)
        $target = Join-Path $WorkspaceRoot $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if (Test-Path -LiteralPath $target -PathType Container) {
            throw "Deleted candidate path became a directory: $relative"
        }
        if (Test-Path -LiteralPath $target -PathType Leaf) {
            Remove-Item -LiteralPath $target -Force
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
    $cachedResult = Invoke-ProjectIsolatedReadOnlyGit -Root $checkout `
        -Arguments @('-c', 'core.quotepath=false', 'ls-files', '--cached')
    $otherResult = Invoke-ProjectIsolatedReadOnlyGit -Root $checkout `
        -Arguments @('-c', 'core.quotepath=false', 'ls-files', '--others', '--exclude-standard')
    if ($cachedResult.ExitCode -ne 0 -or $otherResult.ExitCode -ne 0) {
        throw 'Cannot classify the isolated checkout inventory.'
    }
    $cached = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in @($cachedResult.Output)) {
        if (-not [string]::IsNullOrWhiteSpace($path)) { [void]$cached.Add([string]$path) }
    }
    $expectedUntracked = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($entry in @($overlay.result_inventory)) {
        if ([string]$entry.state -ceq 'file' -and -not $cached.Contains([string]$entry.path)) {
            [void]$expectedUntracked.Add([string]$entry.path)
        }
    }
    foreach ($entry in @($overlay.untracked_files)) {
        if (-not $expectedUntracked.Contains([string]$entry.path)) {
            throw "Overlay untracked path is absent from its result inventory: $($entry.path)"
        }
    }
    $actualUntracked = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in @($otherResult.Output)) {
        if (-not [string]::IsNullOrWhiteSpace($path)) { [void]$actualUntracked.Add([string]$path) }
    }
    $unexpected = @($actualUntracked | Where-Object { -not $expectedUntracked.Contains($_) } | Select-Object -First 5)
    $omitted = @($expectedUntracked | Where-Object { -not $actualUntracked.Contains($_) } | Select-Object -First 5)
    if ($unexpected.Count -gt 0 -or $omitted.Count -gt 0) {
        throw "Materialized overlay untracked inventory mismatch; extra_first=$($unexpected -join ',') omitted_first=$($omitted -join ',')"
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
        Copy-ProjectIsolatedCandidateFiles -WorkspaceRoot $checkout `
            -OverlayRoot $OverlayRoot -Overlay $overlay
        Remove-ProjectIsolatedCandidateDeletions -WorkspaceRoot $checkout -Overlay $overlay
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
