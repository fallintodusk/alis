# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

$ErrorActionPreference = 'Continue'
$result = Invoke-Pester -Path $PSScriptRoot -PassThru
if ($result.FailedCount -gt 0) {
    exit 1
}
