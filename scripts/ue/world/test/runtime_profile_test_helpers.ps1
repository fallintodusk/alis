# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

Set-StrictMode -Version Latest

function Set-ProjectWorldTransientSchemaReference {
    param(
        [Parameter(Mandatory = $true)][object]$Document,
        [Parameter(Mandatory = $true)][string]$DocumentPath,
        [Parameter(Mandatory = $true)][string]$SchemaPath
    )

    $separator = [System.IO.Path]::DirectorySeparatorChar
    $documentRoot = [System.IO.Path]::GetFullPath(
        (Split-Path -Parent $DocumentPath)).TrimEnd('\', '/') + $separator
    $fromUri = [System.Uri]::new($documentRoot)
    $toUri = [System.Uri]::new([System.IO.Path]::GetFullPath($SchemaPath))
    $Document.'$schema' = [System.Uri]::UnescapeDataString(
        $fromUri.MakeRelativeUri($toUri).ToString()).Replace('\', '/')
}
