# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RequestPath,
    [Parameter(Mandatory)][ValidateRange(1, 86400)][int]$TimeoutSeconds
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'owned_test_process.ps1')
$request = Get-Content -LiteralPath $RequestPath -Raw | ConvertFrom-Json
$info = New-Object Diagnostics.ProcessStartInfo
$info.FileName = [string]$request.executable
$info.Arguments = [string]$request.arguments
$info.WorkingDirectory = [string]$request.cwd
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
$result = Invoke-ProjectOwnedTestProcess -StartInfo $info -TimeoutSeconds $TimeoutSeconds `
    -LogPath ([string]$request.log) -ScratchRoot (Join-Path (Split-Path $RequestPath -Parent) 'owned-process-compile')
$result | ConvertTo-Json -Compress | Write-Output
exit $result.ExitCode
