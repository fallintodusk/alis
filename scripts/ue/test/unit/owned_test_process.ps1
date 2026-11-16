# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

function Assert-ProjectColdTestAvailable {
    $blockers = @(Get-Process -ErrorAction Stop | Where-Object {
        $_.ProcessName -like 'UnrealEditor*' -or $_.ProcessName -like 'UEBuildWorker*' -or
        $_.ProcessName -like 'ShaderCompileWorker*'
    })
    if ($blockers.Count) {
        $identities = ($blockers | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ', '
        throw "Cold test refused while pre-existing Unreal processes are running: $identities. Close them or use the established persistent-editor route."
    }
}

. (Join-Path $PSScriptRoot '../lib/owned_test_process.ps1')
