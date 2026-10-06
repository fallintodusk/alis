# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
#
# The ProjectMaterial host's recovery state: delegated runs, the operation id a
# caller binds before mutation, RestorePrevious and its bundle authentication,
# validate-before-destroy restores, and the structured receipt of every run.
# The real host script runs from a copy under a fake project root in TestDrive,
# so its lock, test root, and evidence are that root's; the commandlet launch is
# replaced by a fake surface compiler and no Unreal process starts.

BeforeAll {
    $script:MaterialScripts = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $script:LockFile = (Resolve-Path (Join-Path $PSScriptRoot '..\..\generated_content\generated_content_mutation_lock.ps1')).Path
    . $script:LockFile
    . (Join-Path $script:MaterialScripts 'material_host_recovery.ps1')
    $script:Variable = Get-ProjectGeneratedContentLockTokenVariable

    function script:New-HostProject {
        $root = Join-Path $TestDrive ([System.Guid]::NewGuid().ToString('N'))
        $materialRoot = Join-Path $root 'scripts\ue\material'
        $lockRoot = Join-Path $root 'scripts\ue\generated_content'
        $configRoot = Join-Path $root 'scripts\config'
        $editor = Join-Path $root 'ue\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        New-Item -ItemType Directory -Path $materialRoot, $lockRoot, $configRoot, (Split-Path -Parent $editor) -Force | Out-Null
        foreach ($name in @('run_material_generation.ps1', 'material_host_recovery.ps1')) {
            Copy-Item -LiteralPath (Join-Path $script:MaterialScripts $name) -Destination $materialRoot
        }
        Copy-Item -LiteralPath $script:LockFile -Destination $lockRoot
        Set-Content -LiteralPath (Join-Path $configRoot 'Resolve-UEConfig.ps1') -Value (
            "function Resolve-UEConfig { param([string]`$ConfigDir) return @{ UE_PATH = '$root\ue' } }")
        Set-Content -LiteralPath $editor -Value '' -NoNewline
        $testRoot = Join-Path $root 'tmp\material\generation\host'
        $recipes = Join-Path $testRoot 'recipes'
        New-Item -ItemType Directory -Path $recipes -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $recipes 'a.txt') -Value '1' -NoNewline
        Set-Content -LiteralPath (Join-Path $recipes 'b.txt') -Value '1' -NoNewline
        return [pscustomobject]@{
            Root = $root
            HostScript = Join-Path $materialRoot 'run_material_generation.ps1'
            TestRoot = $testRoot
            Recipes = $recipes
            Output = Join-Path $testRoot 'content\Surfaces'
            Manifests = Join-Path $testRoot 'manifests'
            Journal = Join-Path $testRoot 'transaction\journal.json'
            Transaction = Join-Path $testRoot 'transaction'
            Rollback = Join-Path $testRoot 'RollbackPrevious'
            Evidence = Join-Path $testRoot 'evidence'
        }
    }

    function script:Get-HostState {
        # Independent of the host's own digest: relative path and SHA-256 of every file.
        param([object]$Project)
        $lines = foreach ($root in @($Project.Output, $Project.Manifests)) {
            if (-not (Test-Path -LiteralPath $root)) { "absent:$root"; continue }
            foreach ($file in @(Get-ChildItem -LiteralPath $root -Recurse -File)) {
                '{0}|{1}' -f $file.FullName, (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            }
        }
        return (@($lines | Sort-Object) -join "`n")
    }

    function script:Invoke-FakeSurfaceCommandlet {
        # Stands in for ProjectMaterialSurfaceGenerate: one surface per recipe, an accepted
        # manifest of all recipes, the post-save failure injection, and an authenticated receipt.
        param([string[]]$Arguments)
        $values = @{}
        foreach ($argument in $Arguments) {
            if ($argument -match '^-(?<name>[a-z]+)=(?<value>.*)$') {
                $values[$Matches['name']] = $Matches['value'].Trim('"')
            }
        }
        $testRoot = $values['testroot']
        $recipes = @(Get-ChildItem -LiteralPath (Join-Path $testRoot 'recipes') -Filter '*.txt' | Sort-Object Name)
        $output = Join-Path $testRoot 'content\Surfaces'
        $manifestPath = Join-Path $testRoot 'manifests\accepted.surface-manifest.json'
        $expected = (@($recipes | ForEach-Object { '{0}={1}' -f $_.BaseName, [System.IO.File]::ReadAllText($_.FullName) }) -join "`n") + "`n"
        $status = 'accepted'
        $failure = ''
        $generated = 0
        $skipped = 0
        if ($values['mode'] -eq 'validate') {
            $current = if (Test-Path -LiteralPath $manifestPath) { [System.IO.File]::ReadAllText($manifestPath) } else { '' }
            if ($current -cne $expected) {
                $status = 'rejected'
                $failure = "Accepted surface manifest is stale: $manifestPath"
            }
        }
        else {
            New-Item -ItemType Directory -Path $output -Force | Out-Null
            foreach ($recipe in $recipes) {
                $asset = Join-Path $output "$($recipe.BaseName).uasset"
                $bytes = 'surface:' + [System.IO.File]::ReadAllText($recipe.FullName)
                if ((Test-Path -LiteralPath $asset) -and [System.IO.File]::ReadAllText($asset) -ceq $bytes) {
                    ++$skipped
                }
                else {
                    [System.IO.File]::WriteAllText($asset, $bytes)
                    ++$generated
                }
            }
            if ($values['injectfailure'] -eq 'post-save') {
                $status = 'rejected'
                $failure = 'Injected failure after surface save and before manifest promotion.'
            }
            else {
                New-Item -ItemType Directory -Path (Split-Path -Parent $manifestPath) -Force | Out-Null
                [System.IO.File]::WriteAllText($manifestPath, $expected)
            }
        }
        $manifestSha = if (Test-Path -LiteralPath $manifestPath) {
            (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { '' }
        $receipt = [ordered]@{
            schema_version = '1'
            operation_id = $values['operation']
            status = $status
            generated = $generated
            skipped = $skipped
            shader_compiles = 0
            manifest_sha256 = $manifestSha
            error = $failure
            retained_orphans = @()
            authentication_sha256 = Get-StringSha256 -Value (
                "operation=$($values['operation'])|status=$status|manifest=$manifestSha|generated=$generated|skipped=$skipped")
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $values['receipt']) -Force | Out-Null
        [System.IO.File]::WriteAllText($values['receipt'], ($receipt | ConvertTo-Json))
        [System.IO.File]::WriteAllText($values['abslog'], "fake surface commandlet mode=$($values['mode']) status=$status")
        $process = [pscustomobject]@{ HasExited = $true; ExitCode = $(if ($status -eq 'accepted') { 0 } else { 1 }); Id = 0 }
        $process | Add-Member -MemberType ScriptMethod -Name Refresh -Value { }
        $process | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { }
        return $process
    }

    function script:Invoke-Host {
        param([object]$Project, [hashtable]$Parameters)
        $arguments = @{ Domain = 'Surface'; TestRoot = $Project.TestRoot }
        foreach ($key in $Parameters.Keys) { $arguments[$key] = $Parameters[$key] }
        $output = @(& $Project.HostScript @arguments)
        return ($output[-1] | ConvertFrom-Json)
    }

    function script:Invoke-HostChild {
        param([object]$Project, [string[]]$Arguments)
        $ErrorActionPreference = 'Continue'
        $output = & (Get-Process -Id $PID).Path -NoProfile -NonInteractive -ExecutionPolicy Bypass `
            -File $Project.HostScript @Arguments 2>&1
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Lines = @($output | ForEach-Object { [string]$_ } | Where-Object { $_ })
        }
    }

    function script:Set-Recipe {
        param([object]$Project, [string]$Name, [string]$Value)
        Set-Content -LiteralPath (Join-Path $Project.Recipes "$Name.txt") -Value $Value -NoNewline
    }
}

Describe 'ProjectMaterial host recovery state' {
    BeforeEach {
        $ErrorActionPreference = 'Stop'
        $project = New-HostProject
        Mock Start-Process { Invoke-FakeSurfaceCommandlet -Arguments $ArgumentList } -ParameterFilter {
            $FilePath -like '*UnrealEditor-Cmd.exe'
        }
    }

    AfterEach {
        [Environment]::SetEnvironmentVariable($script:Variable, [NullString]::Value, 'Process')
    }

    It 'admits a host child that carries the live token and refuses one without it' {
        $owner = Enter-ProjectGeneratedContentMutationLock -ProjectRoot $project.Root -RequireOwnership
        try {
            $prior = Enable-ProjectGeneratedContentLockDelegation -Lock $owner
            $arguments = @('-Domain', 'Surface', '-Mode', 'RestorePrevious', '-TestRoot', $project.TestRoot,
                '-RestoreOperationId', ('a' * 32))
            $child = Invoke-HostChild -Project $project -Arguments $arguments
            $child.ExitCode | Should -Be 0
            ($child.Lines[-1] | ConvertFrom-Json).source | Should -Be 'none'

            Disable-ProjectGeneratedContentLockDelegation -Prior $prior
            $child = Invoke-HostChild -Project $project -Arguments $arguments
            $child.ExitCode | Should -Not -Be 0
            ($child.Lines -join "`n") | Should -BeLike '*Another operation holds*'
        }
        finally { $owner.Dispose() }
    }

    It 'retains the replaced state bound to the caller operation id and restores it by that id' {
        (Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }).status | Should -Be 'accepted'
        $firstState = Get-HostState -Project $project
        $reference = Join-Path $project.Root 'reference'
        New-Item -ItemType Directory -Path $reference | Out-Null
        Copy-Item -LiteralPath $project.Output -Destination (Join-Path $reference 'output') -Recurse
        Copy-Item -LiteralPath $project.Manifests -Destination (Join-Path $reference 'manifests') -Recurse

        Set-Recipe -Project $project -Name 'a' -Value '2'
        $operation = [System.Guid]::NewGuid().ToString('N')
        $second = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = $operation }
        $second.operation_id | Should -Be $operation
        $second.generated | Should -Be 1
        Get-HostState -Project $project | Should -Not -Be $firstState
        $bundle = Get-Content -LiteralPath (Join-Path $project.Rollback 'rollback.receipt.json') -Raw | ConvertFrom-Json
        $bundle.schema_version | Should -Be '2'
        $bundle.replaced_by_operation_id | Should -Be $operation
        $bundle.output_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root (Join-Path $reference 'output'))
        $bundle.manifest_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root (Join-Path $reference 'manifests'))

        $restore = Invoke-Host -Project $project -Parameters @{ Mode = 'RestorePrevious'; RestoreOperationId = $operation }
        $restore.source | Should -Be 'bundle'
        $restore.restored_operation_id | Should -Be $operation
        Get-HostState -Project $project | Should -Be $firstState
        Test-Path -LiteralPath $project.Journal | Should -BeFalse
    }

    It 'restores nothing for an operation id the retained bundle is not bound to' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        Set-Recipe -Project $project -Name 'a' -Value '2'
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = ('b' * 32) }
        $state = Get-HostState -Project $project
        $restore = Invoke-Host -Project $project -Parameters @{ Mode = 'RestorePrevious'; RestoreOperationId = ('c' * 32) }
        $restore.source | Should -Be 'none'
        Get-HostState -Project $project | Should -Be $state
    }

    It 'refuses a bundle whose <Name> and changes nothing' -ForEach @(
        @{ Name = 'snapshot file changed'; Expected = '*does not match its recorded digest*'; Damage = {
                param($p)
                $file = @(Get-ChildItem -LiteralPath (Join-Path $p.Rollback 'snapshot\output') -Recurse -File)[0]
                Add-Content -LiteralPath $file.FullName -Value 'tampered' -NoNewline
            } },
        @{ Name = 'snapshot copy is missing'; Expected = '*recorded presence*'; Damage = {
                param($p)
                Remove-Item -LiteralPath (Join-Path $p.Rollback 'snapshot\manifests') -Recurse -Force
            } },
        @{ Name = 'receipt binds no content digests'; Expected = '*only a schema 2 bundle binds content digests*'; Damage = {
                param($p)
                $path = Join-Path $p.Rollback 'rollback.receipt.json'
                $receipt = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
                $legacy = [ordered]@{
                    schema_version = '1'
                    replaced_by_operation_id = $receipt.replaced_by_operation_id
                    output_was_present = $receipt.output_was_present
                    manifest_was_present = $receipt.manifest_was_present
                }
                Set-Content -LiteralPath $path -Value ($legacy | ConvertTo-Json)
            } }
    ) {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        Set-Recipe -Project $project -Name 'a' -Value '2'
        $operation = 'd' * 32
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = $operation }
        & $Damage $project
        $state = Get-HostState -Project $project
        { Invoke-Host -Project $project -Parameters @{ Mode = 'RestorePrevious'; RestoreOperationId = $operation } } |
            Should -Throw $Expected
        Get-HostState -Project $project | Should -Be $state
        Test-Path -LiteralPath $project.Journal | Should -BeFalse
    }

    It 'restores the pending journal of the named operation after a held file blocked its rollback' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        $accepted = Get-HostState -Project $project
        Set-Recipe -Project $project -Name 'a' -Value '2'
        $operation = [System.Guid]::NewGuid().ToString('N')
        # A reader without delete sharing blocks moving the live output aside.
        $handle = [System.IO.File]::Open((Join-Path $project.Output 'a.uasset'), 'Open', 'Read', 'ReadWrite')
        try {
            { Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = $operation; InjectFailure = 'post-save' } } |
                Should -Throw '*rejected and its rollback failed*'
            (Get-Content -LiteralPath $project.Journal -Raw | ConvertFrom-Json).operation_id | Should -Be $operation
            Test-Path -LiteralPath (Join-Path $project.Transaction "$operation\snapshot") | Should -BeTrue
        }
        finally { $handle.Dispose() }

        $restore = Invoke-Host -Project $project -Parameters @{ Mode = 'RestorePrevious'; RestoreOperationId = $operation }
        $restore.source | Should -Be 'journal'
        Get-HostState -Project $project | Should -Be $accepted
        Test-Path -LiteralPath $project.Journal | Should -BeFalse
    }

    It 'rolls back a run that fails after retention and before its commit' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        $accepted = Get-HostState -Project $project
        Set-Recipe -Project $project -Name 'a' -Value '2'
        { Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; InjectFailure = 'pre-commit' } } |
            Should -Throw '*rejected*'
        Get-HostState -Project $project | Should -Be $accepted
        Test-Path -LiteralPath $project.Journal | Should -BeFalse
    }

    It 'refuses a pending journal whose snapshot is missing before deleting anything' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        $accepted = Get-HostState -Project $project
        $operation = [System.Guid]::NewGuid().ToString('N')
        New-Item -ItemType Directory -Path $project.Transaction -Force | Out-Null
        $journal = [ordered]@{
            schema_version = '1'
            operation_id = $operation
            mode = 'regenerate'
            output_root = $project.Output
            manifest_root = $project.Manifests
            snapshot_root = Join-Path $project.Transaction "$operation\snapshot"
            output_was_present = $true
            manifest_was_present = $true
        }
        Set-Content -LiteralPath $project.Journal -Value ($journal | ConvertTo-Json)
        { Invoke-Host -Project $project -Parameters @{ Mode = 'Validate' } } | Should -Throw '*snapshot is missing*'
        Get-HostState -Project $project | Should -Be $accepted
        Test-Path -LiteralPath $project.Journal | Should -BeTrue
    }

    It 'refuses to reuse an operation id the retained bundle is bound to' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        Set-Recipe -Project $project -Name 'a' -Value '2'
        $operation = 'e' * 32
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = $operation }
        $state = Get-HostState -Project $project
        Set-Recipe -Project $project -Name 'a' -Value '3'
        { Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate'; OperationId = $operation } } |
            Should -Throw '*already bound to the retained RollbackPrevious bundle*'
        Get-HostState -Project $project | Should -Be $state
        Test-Path -LiteralPath $project.Journal | Should -BeFalse
        Test-Path -LiteralPath (Join-Path $project.Transaction $operation) | Should -BeFalse
    }

    It 'writes a structured rejected receipt bound to the caller operation id' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        Set-Recipe -Project $project -Name 'b' -Value '2'
        $operation = [System.Guid]::NewGuid().ToString('N')
        { Invoke-Host -Project $project -Parameters @{ Mode = 'Validate'; OperationId = $operation } } |
            Should -Throw '*rejected*'
        $receipt = Get-Content -LiteralPath (Join-Path $project.Evidence 'Rejected\host.receipt.json') -Raw | ConvertFrom-Json
        $receipt.operation_id | Should -Be $operation
        $receipt.status | Should -Be 'rejected'
        $receipt.commandlet_status | Should -Be 'rejected'
        $receipt.commandlet_error | Should -BeLike '*manifest is stale*'
        $receipt.manifest_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root $project.Manifests)
        $receipt.output_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root $project.Output)
        $receipt.manifest_tree_sha256 | Should -Match '^[a-f0-9]{64}$'
    }

    It 'writes a structured accepted receipt with content digests' {
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Regenerate' }
        $operation = [System.Guid]::NewGuid().ToString('N')
        $null = Invoke-Host -Project $project -Parameters @{ Mode = 'Validate'; OperationId = $operation }
        $receipt = Get-Content -LiteralPath (Join-Path $project.Evidence 'Current\host.receipt.json') -Raw | ConvertFrom-Json
        $receipt.operation_id | Should -Be $operation
        $receipt.mode | Should -Be 'validate'
        $receipt.status | Should -Be 'accepted'
        $receipt.commandlet_status | Should -Be 'accepted'
        $receipt.manifest_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root $project.Manifests)
        $receipt.output_tree_sha256 | Should -Be (Get-MaterialTreeSha256 -Root $project.Output)
    }

    It 'refuses RestorePrevious <Name>' -ForEach @(
        @{ Name = 'without an operation to restore'; Parameters = @{ Mode = 'RestorePrevious' }; Expected = '*requires -RestoreOperationId*' },
        @{ Name = 'of its own operation id'; Parameters = @{ Mode = 'RestorePrevious'; RestoreOperationId = ('f' * 32); OperationId = ('f' * 32) }; Expected = '*its own operation id*' },
        @{ Name = 'with commandlet options'; Parameters = @{ Mode = 'RestorePrevious'; RestoreOperationId = ('f' * 32); InjectFailure = 'post-save' }; Expected = '*runs no commandlet*' }
    ) {
        { Invoke-Host -Project $project -Parameters $Parameters } | Should -Throw $Expected
    }
}
