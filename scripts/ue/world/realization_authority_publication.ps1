# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
Set-StrictMode -Version Latest

function Publish-ProjectWorldRealizationAuthority {
    param([string]$ProjectRoot,[string]$ManifestRoot,[object]$ActiveSet,[string]$TransactionId,
        [AllowEmptyCollection()][object[]]$Candidates,[AllowEmptyCollection()][string[]]$Retired,
        [object]$CanonicalOperation=$null)
    $journalPath=Join-Path $ManifestRoot 'journal.json'
    $compatibility=if ($null -ne $CanonicalOperation) {$CanonicalOperation.Compatibility} else {$null}
    return Publish-ProjectWorldActiveSet -ManifestRoot $ManifestRoot -ProjectRoot $ProjectRoot `
        -TransactionId $TransactionId -OperationId $TransactionId -CandidateManifests $Candidates `
        -RetiredScopeIds $Retired -PriorActiveSet $ActiveSet -CanonicalInputCompatibility $compatibility -BeforeCommit {
            param($expectedSha)
            $journal=Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
            $journal.phase='publishing';$journal.expected_active_set_sha256=$expectedSha
            Write-ProjectWorldTransactionJournal -ManifestRoot $ManifestRoot -Journal $journal
            if ($null -ne $CanonicalOperation -and $null -ne $CanonicalOperation.Transition) {
                $selected=Invoke-ProjectWorldCanonicalSelection -ProjectRoot $ProjectRoot `
                    -Transition $CanonicalOperation.Transition -Action publish
                if ([string]$selected.state -cne 'selected') {throw 'Canonical selection did not activate before World publication.'}
            }
        }
}
