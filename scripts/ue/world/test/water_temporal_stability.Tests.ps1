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
            [switch]$HalfGreen
        )

        $bitmap = New-Object System.Drawing.Bitmap(32, 32)
        try {
            for ($y = 0; $y -lt 32; ++$y) {
                for ($x = 0; $x -lt 32; ++$x) {
                    $color = if ($HalfGreen -and $x -ge 16) {
                        [System.Drawing.Color]::FromArgb(40, 145, 40)
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
    It 'accepts a fixed UE base-color blue surface with zero alpha' {
        $reference = Join-Path $script:Scratch 'stable_reference.png'
        $repeat = Join-Path $script:Scratch 'stable_repeat.png'
        $receipt = Join-Path $script:Scratch 'stable_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::FromArgb(0, 33, 118, 211))
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::FromArgb(0, 33, 118, 211))

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Not -Throw
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'accepted'
    }

    It 'rejects blue-to-ground classification flips' {
        $reference = Join-Path $script:Scratch 'flip_reference.png'
        $repeat = Join-Path $script:Scratch 'flip_repeat.png'
        $receipt = Join-Path $script:Scratch 'flip_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::FromArgb(10, 70, 210))
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::FromArgb(10, 70, 210)) -HalfGreen

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Throw '*classification flip ratio*'
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'rejected'
    }

    It 'rejects temporal color drift on pixels that remain blue' {
        $reference = Join-Path $script:Scratch 'drift_reference.png'
        $repeat = Join-Path $script:Scratch 'drift_repeat.png'
        $receipt = Join-Path $script:Scratch 'drift_receipt.json'
        Write-WaterTestImage -Path $reference -WaterColor ([System.Drawing.Color]::FromArgb(10, 70, 210))
        Write-WaterTestImage -Path $repeat -WaterColor ([System.Drawing.Color]::FromArgb(30, 100, 230))

        { & $script:Verifier -ReferencePath $reference -RepeatPath $repeat -ReceiptPath $receipt `
                -MinimumBluePixels 100 } | Should -Throw '*mean blue channel delta*'
        (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).status | Should -Be 'rejected'
    }
}
