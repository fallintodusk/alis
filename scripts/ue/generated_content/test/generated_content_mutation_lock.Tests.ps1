# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# The shared generated-content lock: its one delegation variable, ownership
# refusals, the outer-recovery marker's presence rule, and outer recovery's
# quiescence proof. Every case uses a lock under its own fake root in TestDrive.

BeforeAll {
    . (Join-Path $PSScriptRoot '..\generated_content_mutation_lock.ps1')
    $script:Variable = Get-ProjectGeneratedContentLockTokenVariable

    function script:New-LockRoot {
        $root = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $root | Out-Null
        return $root
    }

    function script:Read-LockToken {
        param([string]$Root)
        $reader = [System.IO.File]::Open((Get-ProjectGeneratedContentLockPath -ProjectRoot $Root), 'Open', 'Read', 'ReadWrite')
        try {
            $buffer = New-Object byte[] 256
            $count = $reader.Read($buffer, 0, $buffer.Length)
            return [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).Trim()
        }
        finally { $reader.Dispose() }
    }

    function script:Get-LockBytes {
        param([string]$Root)
        return [System.Convert]::ToBase64String([System.IO.File]::ReadAllBytes((Get-ProjectGeneratedContentLockPath -ProjectRoot $Root)))
    }
}

Describe 'Generated-content lock delegation' {
    BeforeEach {
        $ErrorActionPreference = 'Stop'
        $root = New-LockRoot
        $projectFile = Join-Path $root 'Alis.uproject'
        New-Item -ItemType Directory -Path (Split-Path -Parent (Get-ProjectGeneratedContentLockPath -ProjectRoot $root)) -Force | Out-Null
    }

    AfterEach {
        [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
    }

    It 'exports the live owner token and restores the prior value' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        try {
            $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
            $prior | Should -BeNullOrEmpty
            $token = [Environment]::GetEnvironmentVariable($script:Variable)
            $token | Should -Match '^[a-f0-9]{32}$'
            $token | Should -Be (Read-LockToken -Root $root)
            $delegate = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
            try {
                $delegate.CanWrite | Should -BeFalse
                Enable-ProjectGeneratedContentLockDelegation -Lock $delegate | Should -Be $token
            }
            finally { $delegate.Dispose() }
            Disable-ProjectGeneratedContentLockDelegation -Prior $prior
            [Environment]::GetEnvironmentVariable($script:Variable) | Should -BeNullOrEmpty
        }
        finally { $owner.Dispose() }
    }

    It 'refuses a caller that requires ownership while a delegated token is set' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        try {
            $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
            { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root -OwnerName 'Workspace cleanup' -RequireOwnership } |
                Should -Throw '*Workspace cleanup must run as the content-lock owner*'
        }
        finally { $owner.Dispose() }
    }

    It 'refuses every non-delegated acquisition while the <Name> exists and leaves the lock file untouched' -ForEach @(
        @{ Name = 'marker'; Suffix = '' },
        @{ Name = 'marker staging file'; Suffix = '.tmp' }
    ) {
        (Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root).Dispose()
        $before = Get-LockBytes -Root $root
        Set-Content -LiteralPath ((Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) + $Suffix) -Value '{}'
        Test-ProjectGeneratedContentRecoveryPending -ProjectRoot $root | Should -BeTrue
        { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root } | Should -Throw '*outer recovery is pending*'
        { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root -RequireOwnership } | Should -Throw '*outer recovery is pending*'
        Get-LockBytes -Root $root | Should -Be $before
    }

    It 'lets delegated children of the live owner proceed while the marker exists' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        try {
            $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
            Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) -Value '{}'
            $delegate = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
            $delegate.CanWrite | Should -BeFalse
            $delegate.Dispose()
        }
        finally { $owner.Dispose() }
    }

    It 'refuses outer recovery while a delegated token is set' {
        Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) -Value '{}'
        [Environment]::SetEnvironmentVariable($script:Variable, ('f' * 32), 'Process')
        { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile } |
            Should -Throw '*runs top-level*'
    }

    It 'refuses outer recovery when no marker is pending' {
        { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile } |
            Should -Throw '*no outer-recovery marker is pending*'
    }

    It 'refuses outer recovery while the owner or an orphaned delegated reader holds the lock' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
        $reader = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
        Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) -Value '{}'
        try {
            { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile } |
                Should -Throw '*participant still holds the content lock*'
            # The owner dies; the delegated reader it started is still open.
            $owner.Dispose()
            { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile } |
                Should -Throw '*participant still holds the content lock*'
        }
        finally {
            $owner.Dispose()
            $reader.Dispose()
        }
        $recovery = Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile
        $recovery.CanWrite | Should -BeTrue
        $recovery.Dispose()
    }

    It 'stamps a fresh token inside the quiescence proof so a delegate from before recovery fails closed' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root
        $null = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
        $stale = [Environment]::GetEnvironmentVariable($script:Variable)
        Disable-ProjectGeneratedContentLockDelegation -Prior $null
        Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) -Value '{}'
        $owner.Dispose()
        $recovery = Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile
        try {
            Read-LockToken -Root $root | Should -Not -Be $stale
            [Environment]::SetEnvironmentVariable($script:Variable, $stale, 'Process')
            { Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root } |
                Should -Throw '*does not match the live lock owner*'
        }
        finally { $recovery.Dispose() }
    }

    It 'refuses outer recovery while this project has an Unreal process and leaves the lock file untouched' {
        (Enter-ProjectGeneratedContentMutationLock -ProjectRoot $root).Dispose()
        $before = Get-LockBytes -Root $root
        Set-Content -LiteralPath (Get-ProjectGeneratedContentRecoveryMarkerPath -ProjectRoot $root) -Value '{}'
        $needle = $projectFile
        Mock Get-CimInstance {
            @(
                [pscustomobject]@{ ProcessId = 4242; CommandLine = "UnrealEditor-Cmd.exe `"$needle`" -run=ProjectWorldRealize" },
                [pscustomobject]@{ ProcessId = 5151; CommandLine = 'UnrealEditor.exe "D:\Other\Other.uproject"' }
            )
        }
        { Enter-ProjectGeneratedContentRecoveryLock -ProjectRoot $root -ProjectFile $projectFile } |
            Should -Throw "*Outer recovery refused while this project's Unreal process is running: pid=4242"
        Get-LockBytes -Root $root | Should -Be $before
    }
}
