#Requires -Version 5.1
# License terms: see repository root LICENSE.
<#
.SYNOPSIS
    Export the resolved UBT module graph of AlisEditor (Win64 Development) as JSON.

.DESCRIPTION
    Runs UnrealBuildTool -Mode=JsonExport through build.bat, so the engine path
    resolver, the project UBT configuration, and the stable argument contract are
    the same as for a normal build. JsonExport saves no build makefile, so the next
    build of the target stays incremental.

    The World proof-input fitness check reads the export
    (tools/World/EndToEndValidation/app/proof_input_fitness.py) and fails closed
    when it is missing or older than any project Build.cs, Target.cs, .uplugin, or
    the .uproject; rerun this script after editing one of them.

    Output: <repo>/Binaries/Win64/AlisEditor.json (gitignored build output).
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).ProviderPath
$BuildWrapper = Join-Path $PSScriptRoot "build.bat"
$OutputFile = Join-Path $ProjectRoot "Binaries\Win64\AlisEditor.json"
$Cmd = Join-Path $env:SystemRoot "System32\cmd.exe"

# cmd splits unquoted batch arguments at '=', so each UBT option is quoted; /s keeps
# the inner quotes intact. ProcessStartInfo passes the line verbatim.
$StartInfo = New-Object System.Diagnostics.ProcessStartInfo
$StartInfo.FileName = $Cmd
$StartInfo.Arguments = "/d /s /c `"`"$BuildWrapper`" AlisEditor Win64 Development " +
    "`"-Mode=JsonExport`" `"-OutputFile=$OutputFile`" -WaitMutex <NUL`""
$StartInfo.WorkingDirectory = $ProjectRoot
$StartInfo.UseShellExecute = $false

$Started = [DateTime]::UtcNow
$Process = [System.Diagnostics.Process]::Start($StartInfo)
$Process.WaitForExit()
if ($Process.ExitCode -ne 0) {
    throw "UBT JsonExport failed with exit code $($Process.ExitCode)."
}

# The fitness check validates the content; this only proves UBT rewrote the file.
$Export = Get-Item -LiteralPath $OutputFile -ErrorAction SilentlyContinue
if ($null -eq $Export -or $Export.LastWriteTimeUtc -lt $Started) {
    throw "UBT JsonExport did not write $OutputFile."
}
Write-Host "UBT target export: $OutputFile"
