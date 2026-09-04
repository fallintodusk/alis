param(
    [Parameter(Mandatory = $true)]
    [string]$ReferencePath,

    [Parameter(Mandatory = $true)]
    [string]$RepeatPath,

    [Parameter(Mandatory = $true)]
    [string]$ReceiptPath,

    [int]$MinimumBluePixels = 4096,
    [double]$MaximumBlueClassificationFlipRatio = 0.001,
    [double]$MaximumMeanBlueChannelDelta = 1.5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function Read-BgraImage {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    $source = [System.Drawing.Bitmap]::FromFile($resolved)
    $bitmap = $null
    try {
        # SceneCapture base-color PNGs carry meaningful RGB with alpha 0. Drawing
        # them through GDI+ alpha-composites those pixels to black; Clone keeps the
        # source bytes intact for geometry/material classification.
        $bitmap = $source.Clone(
            (New-Object System.Drawing.Rectangle(0, 0, $source.Width, $source.Height)),
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $rectangle = New-Object System.Drawing.Rectangle(0, 0, $bitmap.Width, $bitmap.Height)
        $data = $bitmap.LockBits(
            $rectangle,
            [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $rowBytes = [Math]::Abs($data.Stride)
            $bytes = New-Object byte[] ($rowBytes * $bitmap.Height)
            [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        }
        finally {
            $bitmap.UnlockBits($data)
        }

        return [pscustomobject]@{
            Path = $resolved
            Width = $bitmap.Width
            Height = $bitmap.Height
            Stride = $data.Stride
            RowBytes = $rowBytes
            Bytes = $bytes
        }
    }
    finally {
        if ($null -ne $bitmap) {
            $bitmap.Dispose()
        }
        $source.Dispose()
    }
}

if ($MinimumBluePixels -le 0 -or
    $MaximumBlueClassificationFlipRatio -lt 0.0 -or
    $MaximumMeanBlueChannelDelta -lt 0.0) {
    throw 'Water temporal thresholds must be non-negative and MinimumBluePixels must be positive.'
}

$reference = Read-BgraImage -Path $ReferencePath
$repeat = Read-BgraImage -Path $RepeatPath
if ($reference.Width -ne $repeat.Width -or $reference.Height -ne $repeat.Height) {
    throw 'Water temporal images must have identical dimensions.'
}

[long]$blueUnion = 0
[long]$blueBoth = 0
[long]$blueClassificationFlips = 0
[double]$blueChannelDeltaSum = 0.0
[int]$maximumBlueChannelDelta = 0

for ($y = 0; $y -lt $reference.Height; ++$y) {
    $referenceRow = if ($reference.Stride -ge 0) { $y } else { $reference.Height - 1 - $y }
    $repeatRow = if ($repeat.Stride -ge 0) { $y } else { $repeat.Height - 1 - $y }
    $referenceRowOffset = $referenceRow * $reference.RowBytes
    $repeatRowOffset = $repeatRow * $repeat.RowBytes
    for ($x = 0; $x -lt $reference.Width; ++$x) {
        $referenceOffset = $referenceRowOffset + ($x * 4)
        $repeatOffset = $repeatRowOffset + ($x * 4)

        $referenceBlue = [int]$reference.Bytes[$referenceOffset]
        $referenceGreen = [int]$reference.Bytes[$referenceOffset + 1]
        $referenceRed = [int]$reference.Bytes[$referenceOffset + 2]
        $repeatBlue = [int]$repeat.Bytes[$repeatOffset]
        $repeatGreen = [int]$repeat.Bytes[$repeatOffset + 1]
        $repeatRed = [int]$repeat.Bytes[$repeatOffset + 2]

        $referenceIsBlue = $referenceBlue -ge 80 -and
            $referenceBlue -ge ($referenceRed + 25) -and
            $referenceBlue -ge ($referenceGreen + 25)
        $repeatIsBlue = $repeatBlue -ge 80 -and
            $repeatBlue -ge ($repeatRed + 25) -and
            $repeatBlue -ge ($repeatGreen + 25)

        if ($referenceIsBlue -or $repeatIsBlue) {
            ++$blueUnion
        }
        if ($referenceIsBlue -xor $repeatIsBlue) {
            ++$blueClassificationFlips
        }
        if ($referenceIsBlue -and $repeatIsBlue) {
            ++$blueBoth
            $redDelta = [Math]::Abs($referenceRed - $repeatRed)
            $greenDelta = [Math]::Abs($referenceGreen - $repeatGreen)
            $blueDelta = [Math]::Abs($referenceBlue - $repeatBlue)
            $blueChannelDeltaSum += $redDelta + $greenDelta + $blueDelta
            $maximumBlueChannelDelta = [Math]::Max(
                $maximumBlueChannelDelta,
                [Math]::Max($redDelta, [Math]::Max($greenDelta, $blueDelta)))
        }
    }
}

$flipRatio = if ($blueUnion -gt 0) {
    $blueClassificationFlips / [double]$blueUnion
} else {
    1.0
}
$meanBlueChannelDelta = if ($blueBoth -gt 0) {
    $blueChannelDeltaSum / ([double]$blueBoth * 3.0)
} else {
    [double]::PositiveInfinity
}
$failures = @()
if ($blueUnion -lt $MinimumBluePixels) {
    $failures += "blue pixel union $blueUnion is below $MinimumBluePixels"
}
if ($flipRatio -gt $MaximumBlueClassificationFlipRatio) {
    $failures += "blue classification flip ratio $flipRatio exceeds $MaximumBlueClassificationFlipRatio"
}
if ($meanBlueChannelDelta -gt $MaximumMeanBlueChannelDelta) {
    $failures += "mean blue channel delta $meanBlueChannelDelta exceeds $MaximumMeanBlueChannelDelta"
}

$receiptDirectory = Split-Path -Parent $ReceiptPath
if (![string]::IsNullOrWhiteSpace($receiptDirectory)) {
    New-Item -ItemType Directory -Force -Path $receiptDirectory | Out-Null
}
$receipt = [ordered]@{
    schema_version = 1
    status = if ($failures.Count -eq 0) { 'accepted' } else { 'rejected' }
    reference = [ordered]@{
        path = $reference.Path
        sha256 = (Get-FileHash -LiteralPath $reference.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    repeat = [ordered]@{
        path = $repeat.Path
        sha256 = (Get-FileHash -LiteralPath $repeat.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    dimensions = [ordered]@{
        width = $reference.Width
        height = $reference.Height
    }
    metrics = [ordered]@{
        blue_pixel_union = $blueUnion
        blue_pixel_both = $blueBoth
        blue_classification_flips = $blueClassificationFlips
        blue_classification_flip_ratio = $flipRatio
        mean_blue_channel_delta = $meanBlueChannelDelta
        maximum_blue_channel_delta = $maximumBlueChannelDelta
    }
    gates = [ordered]@{
        minimum_blue_pixels = $MinimumBluePixels
        maximum_blue_classification_flip_ratio = $MaximumBlueClassificationFlipRatio
        maximum_mean_blue_channel_delta = $MaximumMeanBlueChannelDelta
    }
    failures = @($failures)
}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ReceiptPath -Encoding Ascii

if ($failures.Count -ne 0) {
    throw "Water temporal stability rejected: $($failures -join '; ')"
}

Write-Host "Water temporal stability accepted: flips=$blueClassificationFlips ratio=$flipRatio mean_delta=$meanBlueChannelDelta"
