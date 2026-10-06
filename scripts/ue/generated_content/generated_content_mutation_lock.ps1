# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# One process-held lock serializes every repository generator that mutates
# persistent Unreal content. The file remains at its established location so
# ProjectWorld and newer generators contend on the same OS handle.
#
# The lock owns its delegation variable: an owner exports its token with
# Enable-ProjectGeneratedContentLockDelegation, and a child that finds the
# variable joins the live owner instead of acquiring. The outer-recovery
# marker beside the lock covers what an OS handle cannot outlive: while it
# exists every non-delegated acquisition fails closed until
# recover_generated_content.ps1 resolves it.

function Get-ProjectGeneratedContentLockTokenVariable {
    # The one delegation variable. EndToEndValidation's execution.py carries the
    # only other copy of the name; its cross-language test keeps the two equal.
    return 'PROJECT_GENERATED_CONTENT_LOCK_TOKEN'
}

function Get-ProjectGeneratedContentLockPath {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    return Join-Path $ProjectRoot 'tmp\world\world_realization\content_mutation.lock'
}

function Get-ProjectGeneratedContentRecoveryMarkerPath {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $lockDirectory = Split-Path -Parent (Get-ProjectGeneratedContentLockPath -ProjectRoot $ProjectRoot)
    return Join-Path $lockDirectory 'outer_recovery.json'
}

function Test-ProjectGeneratedContentRecoveryPending {
    # Presence alone decides and nothing is parsed: the committed marker or its
    # staging file (an interrupted create or replacement) means an outer
    # operation is unresolved.
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $marker = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $ProjectRoot
    return (Test-Path -LiteralPath $marker) -or (Test-Path -LiteralPath "$marker.tmp")
}

function Assert-ProjectGeneratedContentNoProjectUnrealProcess {
    # Single owner of the same-project Unreal check. A commandlet grandchild
    # holds no lock handle, so callers that must prove quiescence also run this.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][string]$Purpose
    )
    $projectNeedle = [System.IO.Path]::GetFullPath($ProjectFile).TrimEnd('\', '/').Replace('\', '/').ToLowerInvariant()
    $found = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" |
        Where-Object {
            $commandLine = ([string]$_.CommandLine).Replace('\', '/').ToLowerInvariant()
            $commandLine.Contains($projectNeedle)
        })
    if ($found.Count -gt 0) {
        $ids = ($found | ForEach-Object { [string]$_.ProcessId }) -join ','
        throw "$Purpose refused while this project's Unreal process is running: pid=$ids"
    }
}

function Enter-ProjectGeneratedContentMutationLock {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [string]$OwnerName = 'project generated-content',
        # Work that must never run inside another operation's envelope
        # (workspace cleanup, an outer coordinator) refuses a delegated token.
        [switch]$RequireOwnership
    )

    $lockPath = Get-ProjectGeneratedContentLockPath -ProjectRoot $ProjectRoot
    New-Item -ItemType Directory -Path (Split-Path -Parent $lockPath) -Force | Out-Null
    $delegatedToken = [Environment]::GetEnvironmentVariable((Get-ProjectGeneratedContentLockTokenVariable))

    if (-not [string]::IsNullOrWhiteSpace($delegatedToken)) {
        if ($RequireOwnership) {
            throw "$OwnerName must run as the content-lock owner; a delegated content-lock token is set."
        }
        $reader = $null
        try {
            $reader = [System.IO.File]::Open(
                $lockPath,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite)
        }
        catch {
            throw "Delegated content-lock token is set but the live lock cannot be read: $lockPath"
        }

        $verified = $false
        try {
            $buffer = New-Object byte[] 256
            $count = $reader.Read($buffer, 0, $buffer.Length)
            $liveToken = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()
            if ($liveToken -ne $delegatedToken.Trim()) {
                throw "Delegated content-lock token does not match the live lock owner: $lockPath"
            }
            $probe = $null
            try {
                $probe = [System.IO.File]::Open(
                    $lockPath,
                    [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::Write,
                    [System.IO.FileShare]::Read)
            }
            catch [System.IO.IOException] { }
            if ($null -ne $probe) {
                $probe.Dispose()
                throw "Delegated content-lock token is set but no live owner holds the lock: $lockPath"
            }
            $verified = $true
        }
        finally {
            if (-not $verified) {
                $reader.Dispose()
            }
        }
        return $reader
    }

    try {
        $stream = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::Read)
    }
    catch [System.IO.IOException] {
        throw "Another operation holds the $OwnerName content mutation lock: $lockPath"
    }
    # Checked while holding the lock: only a live owner writes the marker, so
    # none can appear between this check and the release. Refusal leaves the
    # lock file bytes untouched.
    if (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $ProjectRoot) {
        $stream.Dispose()
        $marker = Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $ProjectRoot
        throw "Generated-content outer recovery is pending ($marker); run scripts/ue/generated_content/recover_generated_content.ps1."
    }
    $token = [System.Guid]::NewGuid().ToString('N')
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($token)
    $stream.SetLength(0)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    return $stream
}

function Enter-ProjectGeneratedContentRecoveryLock {
    # Outer recovery's acquisition. The open with no sharing is the quiescence
    # proof: it fails while any participant, an orphaned delegated child
    # included, still holds a handle. The fresh token is stamped inside that
    # proof, so a delegate carrying a token from before recovery fails closed.
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ProjectFile
    )
    if (-not [string]::IsNullOrWhiteSpace(
            [Environment]::GetEnvironmentVariable((Get-ProjectGeneratedContentLockTokenVariable)))) {
        throw 'Outer recovery refused: it runs top-level, but a delegated content-lock token is set.'
    }
    if (-not (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $ProjectRoot)) {
        throw 'Outer recovery refused: no outer-recovery marker is pending.'
    }
    $lockPath = Get-ProjectGeneratedContentLockPath -ProjectRoot $ProjectRoot
    New-Item -ItemType Directory -Path (Split-Path -Parent $lockPath) -Force | Out-Null
    $proof = $null
    try {
        $proof = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None)
    }
    catch [System.IO.IOException] {
        throw "Outer recovery refused: a generated-content participant still holds the content lock ($lockPath)."
    }
    $token = [System.Guid]::NewGuid().ToString('N')
    try {
        Assert-ProjectGeneratedContentNoProjectUnrealProcess -ProjectFile $ProjectFile -Purpose 'Outer recovery'
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($token)
        $proof.SetLength(0)
        $proof.Write($bytes, 0, $bytes.Length)
        $proof.Flush($true)
    }
    finally {
        $proof.Dispose()
    }
    $owner = $null
    try {
        $owner = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::Read)
    }
    catch [System.IO.IOException] {
        throw "Outer recovery refused: another operation acquired the content lock after the quiescence proof ($lockPath)."
    }
    $buffer = New-Object byte[] 256
    $count = $owner.Read($buffer, 0, $buffer.Length)
    if ([System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim() -cne $token) {
        $owner.Dispose()
        throw "Outer recovery refused: the content lock changed after the quiescence proof ($lockPath)."
    }
    return $owner
}

function Enable-ProjectGeneratedContentLockDelegation {
    # Exports the live owner's token so child processes join this operation;
    # returns the prior value for Disable-ProjectGeneratedContentLockDelegation.
    param([Parameter(Mandatory = $true)][System.IO.FileStream]$Lock)
    if (-not $Lock.Name.EndsWith(
            '\tmp\world\world_realization\content_mutation.lock',
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Content-lock delegation requires the content mutation lock stream: $($Lock.Name)"
    }
    $name = Get-ProjectGeneratedContentLockTokenVariable
    $prior = [Environment]::GetEnvironmentVariable($name)
    if (-not $Lock.CanWrite) {
        # A delegated view re-exports the token it joined with; it never mints one.
        if ([string]::IsNullOrWhiteSpace($prior)) {
            throw "A delegated content-lock view has no token to delegate: $($Lock.Name)"
        }
        return $prior
    }
    $null = $Lock.Seek(0, [System.IO.SeekOrigin]::Begin)
    $buffer = New-Object byte[] 256
    $count = $Lock.Read($buffer, 0, $buffer.Length)
    $token = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()
    if ($token -notmatch '^[a-f0-9]{32}$') {
        throw "The content lock carries no valid owner token: $($Lock.Name)"
    }
    [Environment]::SetEnvironmentVariable($name, $token, [System.EnvironmentVariableTarget]::Process)
    return $prior
}

function Disable-ProjectGeneratedContentLockDelegation {
    param([AllowNull()][AllowEmptyString()][string]$Prior)
    $name = Get-ProjectGeneratedContentLockTokenVariable
    if ([string]::IsNullOrEmpty($Prior)) {
        [Environment]::SetEnvironmentVariable($name, [NullString]::Value, [System.EnvironmentVariableTarget]::Process)
    }
    else {
        [Environment]::SetEnvironmentVariable($name, $Prior, [System.EnvironmentVariableTarget]::Process)
    }
}
