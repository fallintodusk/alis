# One process-held lock serializes every repository generator that mutates
# persistent Unreal content. The file remains at its established location so
# ProjectWorld and newer generators contend on the same OS handle.

function Enter-ProjectGeneratedContentMutationLock {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [string]$OwnerName = 'project generated-content',
        [string]$DelegationEnvironmentVariable = ''
    )

    $lockDir = Join-Path $ProjectRoot 'tmp\world\world_realization'
    New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
    $lockPath = Join-Path $lockDir 'content_mutation.lock'
    $delegatedToken = if ([string]::IsNullOrWhiteSpace($DelegationEnvironmentVariable)) {
        $null
    }
    else {
        [Environment]::GetEnvironmentVariable($DelegationEnvironmentVariable)
    }

    if (-not [string]::IsNullOrWhiteSpace($delegatedToken)) {
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
    $token = [System.Guid]::NewGuid().ToString('N')
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($token)
    $stream.SetLength(0)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    return $stream
}
