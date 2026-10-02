Describe 'World realization runtime profile contract' {
    BeforeAll {
        $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
        $fixtureRoot = Join-Path $repoRoot 'tmp/world/runtime_profile_contract/fixture'
        $canonicalRoot = Join-Path $fixtureRoot 'canonical'
        New-Item -ItemType Directory -Path $canonicalRoot -Force | Out-Null
        $coveragePath = Join-Path $canonicalRoot 'coverage.json'
        [System.IO.File]::WriteAllText(
            $coveragePath, '{"world_data_plugin":"ProjectWorldData"}',
            [System.Text.Encoding]::UTF8)
        $coverageHash = (Get-FileHash -LiteralPath $coveragePath -Algorithm SHA256).Hash.ToLowerInvariant()
        $compilePath = Join-Path $fixtureRoot 'compile_result.json'
        $compileResult = @{
            profile_id = 'kazan_territory_v1'
            outputs = @(@{
                path = 'canonical/coverage.json'
                sha256 = $coverageHash
                byte_size = (Get-Item -LiteralPath $coveragePath).Length
            })
        }
        [System.IO.File]::WriteAllText(
            $compilePath, ($compileResult | ConvertTo-Json -Depth 5),
            [System.Text.Encoding]::UTF8)
        $worldScript = Join-Path $repoRoot 'scripts/ue/world/realize_canonical_world.ps1'
        $dataRoot = Join-Path $repoRoot 'Plugins/World/ProjectWorldData/Data'
        # Every arm uses an evidence path the wrapper refuses before its first
        # write, so an admitted runtime profile stops at that later refusal.
        $refusedEvidence = Join-Path $repoRoot (
            'tmp/world/runtime_profile_contract/' + [System.Guid]::NewGuid().ToString('N') + '/validate.json')

        function Invoke-KazanValidate([string]$RuntimeProfile) {
            & $worldScript `
                -CompileResult $compilePath `
                -Mode Validate `
                -Map '/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory' `
                -PresentationProfile (Join-Path $dataRoot 'Presentation/kazan_representative_v1.json') `
                -RuntimeProfile $RuntimeProfile `
                -AuthoredOverlayProfile (Join-Path $dataRoot 'Authored/kazan_slice0_v1.json') `
                -RealizationProfile (Join-Path $dataRoot 'Profiles/Realization/kazan_territory_v1.realization.json') `
                -EvidencePath $refusedEvidence `
                -NonInteractive
        }
    }

    It 'rejects a missing runtime profile declared by the realization profile' {
        { Invoke-KazanValidate '' } | Should -Throw '*requires runtime profile*'
    }

    It 'rejects a runtime profile other than the declared one' {
        {
            Invoke-KazanValidate (Join-Path $dataRoot 'Runtime/manhattan_showcase_512_1536_v1.json')
        } | Should -Throw "*requires runtime profile 'kazan_territory_512_1536_v1'; got 'manhattan_showcase_512_1536_v1'*"
    }

    It 'admits the declared runtime profile without writing evidence' {
        {
            Invoke-KazanValidate (Join-Path $dataRoot 'Runtime/kazan_territory_512_1536_v1.json')
        } | Should -Throw '*Evidence path must stay under*'
        Test-Path -LiteralPath (Split-Path -Parent $refusedEvidence) | Should -BeFalse
    }
}
