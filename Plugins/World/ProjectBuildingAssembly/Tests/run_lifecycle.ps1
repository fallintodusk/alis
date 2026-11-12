param([Parameter(Mandatory=$true)][string]$CaseRoot,[switch]$ResumeReconstruction,[string]$ResumeRemovalSnapshot)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$case=[IO.Path]::GetFullPath($CaseRoot)
if (-not $case.StartsWith((Join-Path $repo 'tmp/world/'),[StringComparison]::OrdinalIgnoreCase)) {throw 'Case must be under repository tmp/world.'}
$scripts=Join-Path $repo 'scripts/ue/world'
. (Join-Path $scripts 'generated_content_transaction.ps1')
. (Join-Path $scripts 'generated_manifest.ps1')
$profilePath=Join-Path $case 'realization.json'
$profile=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$inputFiles=Get-Content -LiteralPath (Join-Path $case 'case-inputs.json') -Raw | ConvertFrom-Json
function Resolve-DisposableInput([string]$Relative) {
    $path=[IO.Path]::GetFullPath([IO.Path]::Combine($repo,$Relative))
    $inputRoot=[IO.Path]::GetFullPath((Join-Path $repo 'tmp/world/inputs')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if (-not $path.StartsWith($inputRoot,[StringComparison]::OrdinalIgnoreCase) -or -not [IO.File]::Exists($path)) {
        throw 'Lifecycle inputs must be existing files strictly below tmp/world/inputs/.'
    }
    return $path
}
$kitInput=Resolve-DisposableInput ([string]$inputFiles.kit)
$selectionInput=Resolve-DisposableInput ([string]$inputFiles.selection)
if ($kitInput.Equals($selectionInput,[StringComparison]::OrdinalIgnoreCase)) {throw 'Kit and selection inputs must be distinct.'}
if ($profile.world_data_plugin -cne 'ProjectWorldTestData') {throw 'Only disposable TestData is supported.'}
$roots=Resolve-ProjectWorldDataRoots -ProjectRoot $repo -PluginName ProjectWorldTestData
$layerPaths=@($profile.layers | ForEach-Object {Join-Path $roots.ContentRoot ([string]$_.artifact_root).Substring($roots.MountRoot.Length)})
$evidence=Join-Path $case 'lifecycle'
$replay=$ResumeReconstruction -or -not [string]::IsNullOrEmpty($ResumeRemovalSnapshot)
if ($ResumeReconstruction -and $ResumeRemovalSnapshot) {throw 'Choose one replay boundary.'}
if ((Test-Path -LiteralPath $evidence) -and -not $replay) {throw 'Lifecycle evidence must be fresh.'}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$manifest=Join-Path $evidence 'manifests'
$powershell=(Get-Process -Id $PID).Path
$runs=[System.Collections.Generic.List[object]]::new()
if ($replay) {
    $priorRuns=Get-Content -LiteralPath (Join-Path $evidence 'runs.json') -Raw | ConvertFrom-Json
    foreach ($row in $priorRuns) {$runs.Add($row)}
}
$lock=$null;$records=@();$delegation=$null;$beforeContent=$null

function Assert([bool]$Condition,[string]$Message) {if (-not $Condition) {throw $Message}}
function Layer($Run,[string]$Id) {return @($Run.layer_inventories | Where-Object layer_id -CEQ $Id)[0]}
function Census([switch]$ContentOnly) {
    $all=@($layerPaths)+@(Get-ProjectWorldGeneratedPaths -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot)+@((Join-Path $roots.ContentRoot 'Authored'))
    if (-not $ContentOnly) {$all+=@((Join-Path $manifest 'active_set.json'))}
    $hashes=[ordered]@{}
    foreach ($path in $all | Sort-Object -Unique) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {$hashes[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;continue}
        if (Test-Path -LiteralPath $path -PathType Container) {
            foreach ($f in Get-ChildItem -LiteralPath $path -File -Recurse | Sort-Object FullName) {$hashes[$f.FullName]=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash}
        }
    }
    return ($hashes | ConvertTo-Json -Depth 12 -Compress)
}
function Run([string]$Name,[string[]]$Extra=@(),[bool]$Accepted=$true) {
    $output=Join-Path $evidence "$Name.json";$log=Join-Path $evidence "$Name.log"
    Assert (-not (Test-Path -LiteralPath $output) -and -not (Test-Path -LiteralPath $log)) 'A replay must retain existing evidence.'
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $scripts 'realize_canonical_world.ps1'),
        '-CompileResult',(Join-Path $case 'canonical/compile_result.json'),'-Map',[string]$profile.map_package,
        '-PresentationProfile',(Join-Path $repo 'Plugins/World/ProjectWorldTestData/Data/Presentation/synthetic_representative_v1.json'),
        '-AuthoredOverlayProfile',(Join-Path $repo 'Plugins/World/ProjectWorldTestData/Data/Authored/building_neighborhood.json'),'-RealizationProfile',$profilePath,
        '-TransientRealizationProfileRoot',$case,'-ManifestRoot',$manifest,'-EvidencePath',$output,'-NonInteractive','-Mode','Apply')+$Extra
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $quoted=($arguments | ForEach-Object {'"'+([string]$_).Replace('"','\"')+'"'}) -join ' '
    $process=Start-Process -FilePath $powershell -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.stderr"
    $code=$process.ExitCode;$watch.Stop()
    Assert (Test-Path -LiteralPath $output) "Native result missing for $Name; inspect $log"
    $result=Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
    $runs.Add([ordered]@{name=$Name;exit_code=$code;seconds=$watch.Elapsed.TotalSeconds;status=$result.status;result=$output})
    $runs | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $evidence 'runs.json') -Encoding ASCII
    Assert (($code -eq 0 -and $result.status -ceq 'accepted') -eq $Accepted) "Unexpected $Name result ($code/$($result.status)); inspect $log"
    return $result
}
function Semantics($Result) {
    return (@($Result.layer_inventories | Where-Object layer_id -CEQ 'buildings' | Sort-Object layer_id | ForEach-Object {
        $owner=$_.layer_id
        $_.artifacts | Sort-Object path | ForEach-Object {"$owner|$($_.path)|$($_.semantic_sha256)"}
    }) -join "`n")
}
try {
    $lock=Enter-ProjectWorldContentLock -ProjectRoot $repo
    $delegation=Enable-ProjectGeneratedContentLockDelegation -Lock $lock
    $snapshotName=if ($replay) {'outer-snapshot-resume-'+[Guid]::NewGuid().ToString('N')} else {'outer-snapshot'}
    $records=@(New-ProjectWorldGeneratedSnapshot -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot -SnapshotRoot (Join-Path $evidence $snapshotName) -AdditionalPaths $layerPaths)
    $beforeContent=Census -ContentOnly
    $beforeContent | Set-Content -LiteralPath (Join-Path $evidence 'initial-content-census.json') -Encoding ASCII
    if (-not $replay) {
    $first=Run '01-first' @('-EnrollManifests')
    $b=Layer $first buildings
    Assert ($b.generator_id -ceq 'project_building_massing' -and $b.metrics.assembly_actors -eq 1 -and $b.metrics.shared_kit_meshes -eq 4) 'Normal buildings layer did not admit one assembly and four shared kit meshes.'
    $stable=Census
    $noop=Run '02-noop'
    Assert ((Census) -ceq $stable) 'No-op changed package bytes or active authority.'
    foreach ($l in $noop.layer_inventories) {Assert (@($l.final_dirty_units).Count -eq 0) "No-op dirtied $($l.layer_id)"}
    $kitPath=$kitInput;$kit=Get-Content -LiteralPath $kitPath -Raw | ConvertFrom-Json
    $kit.parameters.window_every=3;$kit | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $kitPath -Encoding ASCII
    $changed=Run '03-kit-change' @('-ExecuteLayer','buildings')
    $b=Layer $changed buildings
    Assert (@($b.final_dirty_units).Count -eq 1 -and $b.metrics.assembly_actors_written -eq 1 -and $b.metrics.kit_packages_written -eq 0) 'Kit-only change exceeded its selected unit or rebuilt shared geometry.'
    $priorState=$stable | ConvertFrom-Json;$currentState=(Census) | ConvertFrom-Json
    $priorProperties=@($priorState.PSObject.Properties);$currentProperties=@($currentState.PSObject.Properties)
    $activePath=Join-Path $manifest 'active_set.json'
    $changedPaths=@($priorState.PSObject.Properties | Where-Object {$_.Name -cne $activePath -and $currentState.PSObject.Properties[$_.Name].Value -cne $_.Value} | ForEach-Object Name)
    Assert ($priorProperties.Count -eq $currentProperties.Count -and $changedPaths.Count -eq 1 -and $changedPaths[0] -match '[\\/]__ExternalActors__[\\/]') 'Kit-only change mutated outputs beyond one selected external actor.'
    [ordered]@{changed_output_paths=$changedPaths;unchanged_output_count=($priorProperties.Count-2);native_seconds=$changed.duration_seconds} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidence 'kit-locality.json') -Encoding ASCII
    $stable=Census
    $kit.parameters.window_every=4;$kit | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $kitPath -Encoding ASCII
    $rejected=Run '04-after-save-rollback' @('-ExecuteLayer','buildings','-TestFailAfterSelfSavedActor') $false
    Assert ((Census) -ceq $stable) 'Rejected apply changed accepted packages or authority.'
    Assert ($rejected.changes.self_saved_actor_mutations -gt 0) 'Fault did not reach a real saved actor mutation.'
    $kit.parameters.window_every=3;$kit | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $kitPath -Encoding ASCII
    $selectionPath=$selectionInput;$selection=Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
    foreach ($id in @('alis:building_fixture:way:2002','alis:building_fixture:way:2003')) {
        $selection.assemblies+=@([pscustomobject]@{feature_id=$id;kit_source=$selection.assemblies[0].kit_source})
    }
    $selection | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $selectionPath -Encoding ASCII
    $batch=Run '05-three-buildings' @('-ExecuteLayer','buildings')
    $b=Layer $batch buildings
    Assert ($b.metrics.assembly_actors -eq 3 -and $b.metrics.shared_kit_meshes -eq 4 -and $b.metrics.kit_packages_written -eq 0) 'Three-building generation duplicated kit meshes.'
    $semantics=Semantics $batch
    Remove-ProjectWorldGeneratedPaths -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot
    foreach ($path in $layerPaths) {if (Test-Path -LiteralPath $path) {Remove-Item -LiteralPath (Assert-ProjectWorldOwnedPath -ContentRoot $roots.ContentRoot -Path $path) -Recurse -Force}}
    } else {
        foreach ($name in @('01-first','02-noop','03-kit-change','05-three-buildings')) {
            $prior=Get-Content -LiteralPath (Join-Path $evidence "$name.json") -Raw | ConvertFrom-Json
            Assert ($prior.status -ceq 'accepted') "Cannot reuse rejected upstream evidence: $name"
        }
        $batch=Get-Content -LiteralPath (Join-Path $evidence '05-three-buildings.json') -Raw | ConvertFrom-Json
        Assert ($batch.input_sha256 -ceq (Get-FileHash -LiteralPath (Join-Path $case 'canonical/compile_result.json') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Reconstruction replay changed canonical authority.'
        Assert ($batch.realization_profile_sha256 -ceq (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash.ToLowerInvariant()) 'Reconstruction replay changed the profile.'
        $semantics=Semantics $batch
        $selectionPath=$selectionInput;$selection=Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
    }
    if ($ResumeRemovalSnapshot) {
        $saved=[IO.Path]::GetFullPath($ResumeRemovalSnapshot)
        Assert ($saved.StartsWith($evidence+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Replay snapshot must belong to this case.'
        Assert ((Get-FileHash -LiteralPath (Join-Path $saved 'active_set.json')).Hash -ceq (Get-FileHash -LiteralPath (Join-Path $manifest 'active_set.json')).Hash) 'Replay authority differs from the admitted snapshot.'
        $admitted=Get-Content -LiteralPath (Join-Path $saved 'records.json') -Raw | ConvertFrom-Json
        $relative=([string]$profile.map_package).Substring($roots.MountRoot.Length)
        $mapBase=Join-Path $roots.ContentRoot $relative
        $expected=@($layerPaths)+@("$mapBase.umap","${mapBase}_BuiltData.uasset",(Join-Path $roots.ContentRoot "__ExternalActors__/$relative"),(Join-Path $roots.ContentRoot "__ExternalObjects__/$relative"))
        $expected=@($expected | ForEach-Object {[IO.Path]::GetFullPath($_).TrimEnd('\','/')})
        $seenSources=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($record in $admitted) {
            $source=[IO.Path]::GetFullPath($record.Source).TrimEnd('\','/')
            $backup=[IO.Path]::GetFullPath($record.Backup)
            Assert ($expected -contains $source) 'Replay snapshot escapes the selected generated scope.'
            Assert ($seenSources.Add($source)) 'Replay snapshot repeats an owned source.'
            Assert ($backup.StartsWith($saved+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Replay backup escapes its snapshot.'
        }
        Restore-ProjectWorldGeneratedSnapshot -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot -Records $admitted
        Copy-Item -LiteralPath (Join-Path $saved 'kit.json') -Destination $kitInput -Force
        $selection=Get-Content -LiteralPath (Join-Path $saved 'selection.json') -Raw | ConvertFrom-Json
    } else {
    $coldName=if ($ResumeReconstruction) {'06-cold-reconstruction-retry-'+[Guid]::NewGuid().ToString('N')} else {'06-cold-reconstruction'}
    $cold=Run $coldName @('-Reconstruct')
    Assert ((Semantics $cold) -ceq $semantics) 'Cold reconstruction changed admitted geometry semantics.'
    foreach ($old in $batch.layer_inventories) {
        $current=Layer $cold $old.layer_id
        Assert (($old.canonical_inputs | ConvertTo-Json -Depth 8 -Compress) -ceq ($current.canonical_inputs | ConvertTo-Json -Depth 8 -Compress)) 'Reconstruction replay changed consumed inputs.'
    }
    $admittedSnapshot=Join-Path $evidence ('admitted-cold-state-'+[Guid]::NewGuid().ToString('N'))
    $admitted=@(New-ProjectWorldGeneratedSnapshot -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot -SnapshotRoot $admittedSnapshot -AdditionalPaths $layerPaths)
    $admitted | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $admittedSnapshot 'records.json') -Encoding ASCII
    Copy-Item -LiteralPath $selectionPath -Destination (Join-Path $admittedSnapshot 'selection.json')
    Copy-Item -LiteralPath $kitInput -Destination (Join-Path $admittedSnapshot 'kit.json')
    Copy-Item -LiteralPath (Join-Path $manifest 'active_set.json') -Destination (Join-Path $admittedSnapshot 'active_set.json')
    }
    $selection.assemblies=@();$selection | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $selectionPath -Encoding ASCII
    $removalName=if ($ResumeRemovalSnapshot) {'07-remove-selection-retry-'+[Guid]::NewGuid().ToString('N')} else {'07-remove-selection'}
    $removed=Run $removalName @('-ExecuteLayer','buildings')
    $b=Layer $removed buildings
    Assert ($b.metrics.assembly_actors -eq 0 -and $b.metrics.shared_kit_meshes -eq 0 -and @($b.artifacts | Where-Object path -Match '/Kits/').Count -eq 0) 'Deselection failed to retire assembly/kit ownership.'
    [ordered]@{status='accepted';scope='Disposable registered buildings lifecycle, one then three selections';runs=$runs;restoration='pending'} | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $evidence 'receipt.json') -Encoding ASCII
}
finally {
    try {
        if ($records.Count -gt 0) {Restore-ProjectWorldGeneratedSnapshot -ContentRoot $roots.ContentRoot -MapPackage $profile.map_package -GeneratedPackageRoot $roots.GeneratedPackageRoot -Records $records}
        if ($null -ne $beforeContent) {
            $afterContent=Census -ContentOnly
            Assert ($afterContent -ceq $beforeContent) 'Outer restoration changed generated or protected authored bytes.'
            $receiptPath=Join-Path $evidence 'receipt.json'
            if (Test-Path -LiteralPath $receiptPath) {
                $receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
                $receipt.restoration=[ordered]@{before_sha256=(Get-ProjectWorldBytesHash -Bytes ([Text.Encoding]::UTF8.GetBytes($beforeContent)));after_sha256=(Get-ProjectWorldBytesHash -Bytes ([Text.Encoding]::UTF8.GetBytes($afterContent)))}
                $receipt | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $receiptPath -Encoding ASCII
            }
        }
    } finally {
        if ($null -ne $lock) {Disable-ProjectGeneratedContentLockDelegation -Prior $delegation;$lock.Dispose()}
    }
}
