#Requires -Version 5.1
# License terms: see repository root LICENSE.

$ErrorActionPreference = 'Stop'
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageRoot = Split-Path -Parent $ScriptRoot
. (Join-Path $PackageRoot 'isolated_source_workspace.ps1')
$ProjectRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PackageRoot))
$TestParent = Join-Path $ProjectRoot 'tmp\world\mesh_terrain_comparison\workspace-tests'
$TestRoot = Join-Path $TestParent ([Guid]::NewGuid().ToString('N'))
$Repo = Join-Path $TestRoot 'source'
$LfsRepo = Join-Path $TestRoot 'source-lfs'
$Overlay = Join-Path $Repo 'tmp\overlay\candidate'
$Workspaces = [Collections.Generic.List[string]]::new()
$LfsWorkspace = $null
$CommittedWorkspace = $null
$FetchedWorkspace = $null
$FetchedFileWorkspace = $null

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-ThrowsLike {
    param([scriptblock]$Action, [string]$Pattern, [string]$Message)
    $accepted = $false
    try {
        & $Action
    }
    catch {
        $accepted = $_.Exception.Message -like $Pattern
    }
    if (-not $accepted) {
        throw $Message
    }
}

function Get-LiveIdentity {
    $status = (@(& git -C $Repo -c core.quotepath=false status --porcelain=v1 --untracked-files=all) -join "`n")
    $index = Get-ProjectIsolatedTextSha256 -Lines @(
        & git -C $Repo -c core.quotepath=false ls-files --stage)
    $binary = Get-ProjectIsolatedFileSha256 -Path (Join-Path $Repo 'tracked.bin')
    $candidate = Get-ProjectIsolatedFileSha256 -Path (Join-Path $Repo 'candidate\new.bin')
    return "$index|$binary|$candidate|$status"
}

try {
    New-Item -ItemType Directory -Path $Repo -Force | Out-Null
    & git -C $Repo init --quiet
    & git -C $Repo config user.name 'Project Workspace Test'
    & git -C $Repo config user.email 'workspace-test@invalid.local'
    & git -C $Repo config core.autocrlf false
    [IO.File]::WriteAllBytes((Join-Path $Repo 'tracked.bin'), [byte[]](0, 1, 2, 3, 4))
    'baseline' | Set-Content -LiteralPath (Join-Path $Repo 'tracked.txt') -Encoding Ascii
    'tmp/' | Set-Content -LiteralPath (Join-Path $Repo '.gitignore') -Encoding Ascii
    & git -C $Repo add .gitignore tracked.bin tracked.txt
    & git -C $Repo commit --quiet -m 'baseline'
    $Base = (& git -C $Repo rev-parse HEAD).Trim()

    [IO.File]::WriteAllBytes((Join-Path $Repo 'tracked.bin'), [byte[]](0, 1, 9, 3, 4, 5))
    'candidate' | Set-Content -LiteralPath (Join-Path $Repo 'tracked.txt') -Encoding Ascii
    New-Item -ItemType Directory -Path (Join-Path $Repo 'candidate') -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $Repo 'candidate\new.bin'), [byte[]](8, 7, 6, 5))
    'staged candidate' | Set-Content -LiteralPath (Join-Path $Repo 'staged_new.txt') -Encoding Ascii
    & git -C $Repo add staged_new.txt
    $LiveBefore = Get-LiveIdentity

    $overlayReceipt = New-ProjectIsolatedSourceOverlay -ProjectRoot $Repo `
        -SourceCommit $Base -CandidatePaths @('tracked.bin', 'tracked.txt', 'candidate', 'staged_new.txt') `
        -RequiredPaths @('tracked.bin', 'candidate/new.bin', 'staged_new.txt') -OverlayRoot $Overlay
    Assert-True ($overlayReceipt.source_commit -ceq $Base) 'Overlay did not bind the exact source commit.'
    Assert-True (@($overlayReceipt.untracked_files).Count -eq 1) `
        'Overlay did not record the exact untracked candidate inventory.'
    Assert-True ('staged_new.txt' -in @($overlayReceipt.tracked_changed_paths)) `
        'Overlay omitted a staged addition from the tracked candidate patch.'
    Assert-True ([string]$overlayReceipt.tracked_patch.sha256 -match '^[0-9a-f]{64}$') `
        'Overlay did not authenticate its tracked binary patch.'

    $first = New-ProjectIsolatedSourceWorkspace -ProjectRoot $Repo -OverlayRoot $Overlay
    $Workspaces.Add([string]$first.workspace_root)
    $firstRoot = [IO.Path]::GetFullPath([string]$first.workspace_root)
    $firstCheckout = [IO.Path]::GetFullPath([string]$first.checkout_root)
    Assert-True ((& git -C $firstCheckout config --bool core.longpaths).Trim() -ceq 'true') `
        'Detached comparison worktree did not persist Windows long-path support.'
    Assert-True ((Get-ProjectIsolatedFileSha256 -Path (Join-Path $firstCheckout 'tracked.bin')) -ceq `
        (Get-ProjectIsolatedFileSha256 -Path (Join-Path $Repo 'tracked.bin'))) `
        'Tracked binary candidate bytes did not survive materialization.'
    Assert-True ((Get-ProjectIsolatedFileSha256 -Path (Join-Path $firstCheckout 'candidate\new.bin')) -ceq `
        (Get-ProjectIsolatedFileSha256 -Path (Join-Path $Repo 'candidate\new.bin'))) `
        'Untracked candidate bytes did not survive materialization.'
    Assert-True ((Get-ProjectIsolatedFileSha256 -Path (Join-Path $firstCheckout 'staged_new.txt')) -ceq `
        (Get-ProjectIsolatedFileSha256 -Path (Join-Path $Repo 'staged_new.txt'))) `
        'Staged new candidate bytes did not survive materialization.'

    $candidateInCheckout = Join-Path $firstCheckout 'candidate\new.bin'
    $candidateBlob = Join-Path $Overlay 'files\candidate\new.bin'
    Remove-Item -LiteralPath $candidateInCheckout -Force
    Assert-ThrowsLike {
        Assert-ProjectIsolatedSourceWorkspace -WorkspaceRoot $firstRoot -OverlayRoot $Overlay | Out-Null
    } '*byte identity mismatch*' 'An omitted candidate file was not rejected.'
    Copy-Item -LiteralPath $candidateBlob -Destination $candidateInCheckout
    'extra' | Set-Content -LiteralPath (Join-Path $firstCheckout 'candidate\extra.txt') -Encoding Ascii
    Assert-ThrowsLike {
        Assert-ProjectIsolatedSourceWorkspace -WorkspaceRoot $firstRoot -OverlayRoot $Overlay | Out-Null
    } '*untracked inventory mismatch*' 'An extra candidate file was not rejected.'
    Remove-Item -LiteralPath (Join-Path $firstCheckout 'candidate\extra.txt') -Force
    $identity = Assert-ProjectIsolatedSourceWorkspace -WorkspaceRoot $firstRoot -OverlayRoot $Overlay
    Assert-True ($identity -ceq [string]$overlayReceipt.source_identity_sha256) `
        'Materialized workspace identity does not match the overlay receipt.'

    $pointer = Join-Path $firstCheckout 'candidate\pointer.bin'
    @(
        'version https://git-lfs.github.com/spec/v1',
        'oid sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'size 4'
    ) | Set-Content -LiteralPath $pointer -Encoding Ascii
    Assert-ThrowsLike {
        Assert-ProjectIsolatedRequiredFiles -WorkspaceRoot $firstCheckout `
            -RequiredPaths @('candidate/pointer.bin')
    } '*Git LFS pointer*' 'A required Git LFS pointer was not rejected.'
    Remove-Item -LiteralPath $pointer -Force

    $second = New-ProjectIsolatedSourceWorkspace -ProjectRoot $Repo -OverlayRoot $Overlay
    $Workspaces.Add([string]$second.workspace_root)
    Assert-True ([string]$second.workspace_root -cne [string]$first.workspace_root) `
        'Concurrent comparison workspaces shared one mutable root.'
    Assert-True ((Test-Path -LiteralPath $firstRoot) -and
        (Test-Path -LiteralPath ([string]$second.workspace_root))) `
        'Creating a concurrent workspace removed an active workspace.'

    New-Item -ItemType Directory -Path $LfsRepo -Force | Out-Null
    & git -C $LfsRepo init --quiet
    & git -C $LfsRepo config user.name 'Project Workspace Test'
    & git -C $LfsRepo config user.email 'workspace-test@invalid.local'
    & git -C $LfsRepo config core.autocrlf false
    & git -C $LfsRepo lfs install --local | Out-Null
    'tmp/' | Set-Content -LiteralPath (Join-Path $LfsRepo '.gitignore') -Encoding Ascii
    '*.lfsbin filter=lfs diff=lfs merge=lfs -text' |
        Set-Content -LiteralPath (Join-Path $LfsRepo '.gitattributes') -Encoding Ascii
    $lfsFolder = Join-Path $LfsRepo 'payload'
    New-Item -ItemType Directory -Path $lfsFolder -Force | Out-Null
    $lfsPath = Join-Path $lfsFolder 'candidate.lfsbin'
    $retiredPath = Join-Path $lfsFolder 'retired.lfsbin'
    [IO.File]::WriteAllBytes($lfsPath, [byte[]]::new(1024))
    [IO.File]::WriteAllBytes($retiredPath, [byte[]]::new(1024))
    & git -C $LfsRepo add .gitignore .gitattributes payload
    & git -C $LfsRepo commit --quiet -m 'baseline LFS payload'
    $lfsBase = (& git -C $LfsRepo rev-parse HEAD).Trim()
    [IO.File]::WriteAllBytes($lfsPath, [byte[]](@(2) * 1024))
    Remove-Item -LiteralPath $retiredPath -Force
    $lfsExpected = Get-ProjectIsolatedFileSha256 -Path $lfsPath
    $lfsOverlay = Join-Path $LfsRepo 'tmp\overlay\candidate'
    $lfsReceipt = New-ProjectIsolatedSourceOverlay -ProjectRoot $LfsRepo `
        -SourceCommit $lfsBase -CandidatePaths @('payload') `
        -RequiredPaths @('payload') -OverlayRoot $lfsOverlay
    [IO.File]::WriteAllBytes($lfsPath, [byte[]](@(3) * 1024))
    $priorSkip = $env:GIT_LFS_SKIP_SMUDGE
    try {
        $env:GIT_LFS_SKIP_SMUDGE = '1'
        $LfsWorkspace = New-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -OverlayRoot $lfsOverlay
    }
    finally {
        if ($null -eq $priorSkip) { Remove-Item Env:GIT_LFS_SKIP_SMUDGE -ErrorAction SilentlyContinue }
        else { $env:GIT_LFS_SKIP_SMUDGE = $priorSkip }
    }
    $lfsActual = Get-ProjectIsolatedFileSha256 -Path (Join-Path ([string]$LfsWorkspace.checkout_root) 'payload\candidate.lfsbin')
    Assert-True ($lfsActual -ceq $lfsExpected -and
        [string]$LfsWorkspace.source_identity_sha256 -ceq [string]$lfsReceipt.source_identity_sha256) `
        'Unstaged LFS candidate did not materialize as the verified raw payload.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path ([string]$LfsWorkspace.checkout_root) 'payload\retired.lfsbin'))) `
        'LFS checkout recreated a deleted candidate payload.'

    Assert-ThrowsLike {
        New-ProjectIsolatedCommittedWorkspace -ProjectRoot $LfsRepo `
            -SourceCommit 'HEAD' -RequiredPaths @('payload') | Out-Null
    } '*full 40-digit commit*' 'A symbolic release source identity was accepted.'
    $CommittedWorkspace = New-ProjectIsolatedCommittedWorkspace -ProjectRoot $LfsRepo `
        -SourceCommit $lfsBase -RequiredPaths @('payload')
    $committedCheckout = [string]$CommittedWorkspace.checkout_root
    Assert-True ((Get-ProjectIsolatedFileSha256 -Path (Join-Path $committedCheckout 'payload\candidate.lfsbin')) -ceq `
        (Get-ProjectIsolatedFileSha256 -Path (Join-Path $committedCheckout 'payload\retired.lfsbin'))) `
        'Committed release workspace did not materialize the exact LFS payloads.'
    [IO.File]::WriteAllBytes((Join-Path $committedCheckout 'payload\candidate.lfsbin'), [byte[]](1, 2, 3))
    Assert-ThrowsLike {
        Assert-ProjectIsolatedRequiredLfsObjects -WorkspaceRoot $committedCheckout `
            -RequiredPaths @('payload')
    } '*wrong object identity*' 'A corrupted committed LFS payload was accepted.'
    Remove-Item -LiteralPath (Join-Path $committedCheckout 'payload\candidate.lfsbin') -Force
    & git -C $committedCheckout lfs checkout -- payload | Out-Null
    Assert-ProjectIsolatedRequiredLfsObjects -WorkspaceRoot $committedCheckout `
        -RequiredPaths @('payload')
    $ownerFile = Join-Path ([string]$CommittedWorkspace.workspace_root) 'workspace-owner.json'
    $ownerOriginal = Get-Content -LiteralPath $ownerFile -Raw
    $tampered = $ownerOriginal | ConvertFrom-Json
    $tampered.source_commit = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
    $tampered | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ownerFile -Encoding utf8
    Assert-ThrowsLike {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -WorkspaceRoot ([string]$CommittedWorkspace.workspace_root)
    } '*HEAD or detached state*' 'Cleanup accepted a mismatched source commit receipt.'
    $ownerOriginal | Set-Content -LiteralPath $ownerFile -Encoding utf8
    Assert-True (Test-Path -LiteralPath $committedCheckout) `
        'Mismatched cleanup removed the protected checkout.'

    $remote = Join-Path $TestRoot 'lfs-remote.git'
    & git init --bare --quiet $remote
    & git -C $LfsRepo remote add origin $remote
    & git -C $LfsRepo lfs push origin $lfsBase | Out-Null
    Assert-True ($LASTEXITCODE -eq 0) 'Disposable LFS remote was not populated.'
    $lfsListing = @(& git -C $LfsRepo lfs ls-files --long)
    Assert-True ($lfsListing[0] -match '^([0-9a-f]{64}) ') 'Cannot read fixture LFS object ID.'
    $oid = $Matches[1]
    $localObject = Join-Path $LfsRepo ('.git\lfs\objects\' + $oid.Substring(0, 2) +
        '\' + $oid.Substring(2, 2) + '\' + $oid)
    Remove-Item -LiteralPath $localObject -Force
    $FetchedWorkspace = New-ProjectIsolatedCommittedWorkspace -ProjectRoot $LfsRepo `
        -SourceCommit $lfsBase -RequiredPaths @('payload')
    Assert-ProjectIsolatedRequiredLfsObjects `
        -WorkspaceRoot ([string]$FetchedWorkspace.checkout_root) -RequiredPaths @('payload')
    Remove-Item -LiteralPath $localObject -Force
    $FetchedFileWorkspace = New-ProjectIsolatedCommittedWorkspace -ProjectRoot $LfsRepo `
        -SourceCommit $lfsBase -RequiredPaths @('payload/candidate.lfsbin')
    Assert-ProjectIsolatedRequiredLfsObjects `
        -WorkspaceRoot ([string]$FetchedFileWorkspace.checkout_root) `
        -RequiredPaths @('payload/candidate.lfsbin')

    $signal = Join-Path $TestRoot 'forced-workspace.txt'
    $childScript = Join-Path $TestRoot 'forced-owner.ps1'
    @'
param($Helper, $ProjectRoot, $OverlayRoot, $Signal)
. $Helper
$workspace = New-ProjectIsolatedSourceWorkspace -ProjectRoot $ProjectRoot -OverlayRoot $OverlayRoot
[string]$workspace.workspace_root | Set-Content -LiteralPath $Signal -Encoding Ascii
while ($true) { Start-Sleep -Seconds 1 }
'@ | Set-Content -LiteralPath $childScript -Encoding Ascii
    $child = Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -PassThru -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $childScript,
        '-Helper', (Join-Path $PackageRoot 'isolated_source_workspace.ps1'),
        '-ProjectRoot', $Repo, '-OverlayRoot', $Overlay, '-Signal', $signal
    )
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while (-not (Test-Path -LiteralPath $signal) -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
    }
    Assert-True (Test-Path -LiteralPath $signal) 'Forced-interruption child did not create its workspace.'
    $forcedRoot = (Get-Content -LiteralPath $signal -Raw).Trim()
    Stop-Process -Id $child.Id -Force
    $child.WaitForExit()
    Remove-StaleProjectIsolatedSourceWorkspaces -ProjectRoot $Repo
    Assert-True (-not (Test-Path -LiteralPath $forcedRoot)) `
        'A dead owner left its isolated workspace after recovery.'

    $LiveAfter = Get-LiveIdentity
    Assert-True ($LiveAfter -ceq $LiveBefore) `
        "Comparison workspace lifecycle changed the live candidate tree or Git index. before=$LiveBefore after=$LiveAfter"
    Write-Host '[OK] Isolated source workspace preserves authenticated candidates and live-tree isolation.'
}
finally {
    if ($null -ne $FetchedFileWorkspace -and
        (Test-Path -LiteralPath ([string]$FetchedFileWorkspace.workspace_root))) {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -WorkspaceRoot ([string]$FetchedFileWorkspace.workspace_root)
    }
    if ($null -ne $FetchedWorkspace -and
        (Test-Path -LiteralPath ([string]$FetchedWorkspace.workspace_root))) {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -WorkspaceRoot ([string]$FetchedWorkspace.workspace_root)
    }
    if ($null -ne $CommittedWorkspace -and
        (Test-Path -LiteralPath ([string]$CommittedWorkspace.workspace_root))) {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -WorkspaceRoot ([string]$CommittedWorkspace.workspace_root)
    }
    if ($null -ne $LfsWorkspace -and (Test-Path -LiteralPath ([string]$LfsWorkspace.workspace_root))) {
        Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $LfsRepo `
            -WorkspaceRoot ([string]$LfsWorkspace.workspace_root)
    }
    foreach ($workspace in @($Workspaces)) {
        if (Test-Path -LiteralPath $workspace) {
            Remove-ProjectIsolatedSourceWorkspace -ProjectRoot $Repo -WorkspaceRoot $workspace
        }
    }
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
}
