BeforeAll {
    Add-Type -AssemblyName System.Drawing
    $script:Verifier = Join-Path $PSScriptRoot 'water_temporal_stability.ps1'
    $script:Scratch = Join-Path $PSScriptRoot '../../../../tmp/world/water_temporal/tests'
    if (Test-Path -LiteralPath $script:Scratch) {
        [System.IO.Directory]::Delete((Resolve-Path -LiteralPath $script:Scratch).Path, $true)
    }
    New-Item -ItemType Directory -Force -Path $script:Scratch | Out-Null

    function Write-WaterTestImage {
        param(
            [string]$Path,
            [System.Drawing.Color]$WaterColor,
            [switch]$HalfOccluded
        )

        $bitmap = New-Object System.Drawing.Bitmap(32, 32)
        try {
            for ($y = 0; $y -lt 32; ++$y) {
                for ($x = 0; $x -lt 32; ++$x) {
                    $color = if ($HalfOccluded -and $x -ge 16) {
                        [System.Drawing.Color]::Black
                    } else {
                        $WaterColor
                    }
                    $bitmap.SetPixel($x, $y, $color)
                }
            }
            $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $bitmap.Dispose()
        }
    }
}

AfterAll {
    if (Test-Path -LiteralPath $script:Scratch) {
        [System.IO.Directory]::Delete((Resolve-Path -LiteralPath $script:Scratch).Path, $true)
    }
}

Describe 'Water temporal stability verifier' {
    It 'accepts a fixed native visibility mask with zero alpha' {
        $reference = Join-Path $script:Scratch 'stable_reference.png'
        $repeat = Join-Path $script:Scratch 'stable_repeat.png'
        $receipt = Join-Path $script:Scratch 'stable_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::FromArgb(0, 0, 0, 255))
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::FromArgb(0, 0, 0, 255))

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Not -Throw
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'accepted'
    }

    It 'rejects water-to-occluder visibility flips' {
        $reference = Join-Path $script:Scratch 'flip_reference.png'
        $repeat = Join-Path $script:Scratch 'flip_repeat.png'
        $receipt = Join-Path $script:Scratch 'flip_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::Blue)
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::Blue) -HalfOccluded

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Throw '*classification flip ratio*'
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'rejected'
    }

    It 'refuses colored beauty pixels instead of treating albedo as coverage' {
        $reference = Join-Path $script:Scratch 'drift_reference.png'
        $repeat = Join-Path $script:Scratch 'drift_repeat.png'
        $receipt = Join-Path $script:Scratch 'drift_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::FromArgb(10, 70, 210))
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::FromArgb(30, 100, 230))

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Throw '*visibility mask encoding*'
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'rejected'
    }

    It 'refuses a stable empty mask rather than accepting zero observed Water' {
        $reference = Join-Path $script:Scratch 'empty_reference.png'
        $repeat = Join-Path $script:Scratch 'empty_repeat.png'
        $receipt = Join-Path $script:Scratch 'empty_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::Black)
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::Black)
        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Throw '*pixel union*'
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'rejected'
    }
}
