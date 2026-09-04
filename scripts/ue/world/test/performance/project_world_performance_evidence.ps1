# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

#Requires -Version 5.1

function Test-ProjectWorldPerformanceNumber {
    param([object]$Value)
    return $Value -is [byte] -or $Value -is [sbyte] -or
        $Value -is [int16] -or $Value -is [uint16] -or
        $Value -is [int32] -or $Value -is [uint32] -or
        $Value -is [int64] -or $Value -is [uint64] -or
        $Value -is [single] -or $Value -is [double] -or $Value -is [decimal]
}

function Test-ProjectWorldPerformanceEnvelopeReceipt {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][object]$Receipt)

    $required = @(
        'vsync',
        'max_fps',
        'smooth_frame_rate',
        'use_fixed_frame_rate',
        'fixed_frame_rate',
        'use_fixed_time_step',
        'fixed_delta_time_seconds',
        'benchmarking',
        'dynamic_resolution_operation_mode',
        'dynamic_resolution_status',
        'dynamic_resolution_enabled'
    )
    foreach ($name in $required) {
        if ($null -eq $Receipt.PSObject.Properties[$name]) {
            return $false
        }
    }

    foreach ($name in @(
            'vsync',
            'max_fps',
            'fixed_frame_rate',
            'fixed_delta_time_seconds',
            'dynamic_resolution_operation_mode')) {
        if (-not (Test-ProjectWorldPerformanceNumber `
                    -Value $Receipt.PSObject.Properties[$name].Value)) {
            return $false
        }
    }
    foreach ($name in @(
            'smooth_frame_rate',
            'use_fixed_frame_rate',
            'use_fixed_time_step',
            'benchmarking',
            'dynamic_resolution_enabled')) {
        if ($Receipt.PSObject.Properties[$name].Value -isnot [bool]) {
            return $false
        }
    }
    if ($Receipt.dynamic_resolution_status -isnot [string]) {
        return $false
    }

    $fixedFrameRate = [double]$Receipt.fixed_frame_rate
    $fixedDelta = [double]$Receipt.fixed_delta_time_seconds
    if ([double]::IsNaN($fixedFrameRate) -or [double]::IsInfinity($fixedFrameRate) -or
        [double]::IsNaN($fixedDelta) -or [double]::IsInfinity($fixedDelta)) {
        return $false
    }
    return [int]$Receipt.vsync -eq 0 -and
        [double]$Receipt.max_fps -eq 0.0 -and
        -not [bool]$Receipt.smooth_frame_rate -and
        -not [bool]$Receipt.use_fixed_frame_rate -and
        -not [bool]$Receipt.use_fixed_time_step -and
        -not [bool]$Receipt.benchmarking -and
        [int]$Receipt.dynamic_resolution_operation_mode -eq 0 -and
        ([string]$Receipt.dynamic_resolution_status -ceq 'disabled' -or
            [string]$Receipt.dynamic_resolution_status -ceq 'unsupported') -and
        -not [bool]$Receipt.dynamic_resolution_enabled
}

function Get-ProjectWorldPerformanceFileHash {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Performance evidence file is missing: $Path"
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProjectWorldPerformanceChildOutcome {
    param(
        [Parameter(Mandatory = $true)][object]$Receipt,
        [Parameter(Mandatory = $true)][int]$ProcessExitCode,
        [double]$FrameP95BudgetMilliseconds = 16.67
    )
    $errors = @()
    if ($null -ne $Receipt.PSObject.Properties['errors'] -and
        $null -ne $Receipt.errors) {
        $errors = @($Receipt.errors)
    }
    $individualAcceptance = [string]$Receipt.status -ceq 'accepted' -and
        [double]$Receipt.frame_p95_ms -le $FrameP95BudgetMilliseconds -and
        $errors.Count -eq 0 -and $ProcessExitCode -eq 0
    $performanceOnlyRejection = [string]$Receipt.status -ceq 'rejected' -and
        [double]$Receipt.frame_p95_ms -gt $FrameP95BudgetMilliseconds -and
        $errors.Count -eq 1 -and
        [string]$errors[0].code -ceq 'performance_hard_gate_failed' -and
        ($ProcessExitCode -eq 0 -or $ProcessExitCode -eq 10)
    return [pscustomobject][ordered]@{
        individual_acceptance = $individualAcceptance
        performance_only_rejection = $performanceOnlyRejection
        valid = $individualAcceptance -or $performanceOnlyRejection
        error_count = $errors.Count
    }
}

function Get-ProjectWorldPackagePayloadDigest {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string[]]$RuntimeStateRelativePaths = @(
            'Windows/Alis/LocalAppData',
            'Windows/Alis/Saved',
            'Windows/Engine/Saved'
        )
    )
    $root = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Package root is missing: $root"
    }
    $runtimeStates = @($RuntimeStateRelativePaths | ForEach-Object {
            $_.Trim('\', '/').Replace('\', '/')
        })
    $lines = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
            $isRuntimeState = @($runtimeStates | Where-Object {
                    $relative.Equals($_, [StringComparison]::OrdinalIgnoreCase) -or
                    $relative.StartsWith($_ + '/', [StringComparison]::OrdinalIgnoreCase)
                }).Count -gt 0
            if ($isRuntimeState) {
                return
            }
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            '{0}|{1}|{2}' -f $relative, $_.Length, $hash
        })
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Import-ProjectWorldPerformanceSamples {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$ExpectedCount
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Exact performance sample file is missing: $Path"
    }
    $rows = @(Import-Csv -LiteralPath $Path)
    if ($rows.Count -ne $ExpectedCount) {
        throw "Exact performance sample count mismatch: expected $ExpectedCount, found $($rows.Count)."
    }
    $fields = @('FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime')
    $samples = [Collections.Generic.List[object]]::new()
    foreach ($row in $rows) {
        $values = [ordered]@{}
        foreach ($field in $fields) {
            if ($null -eq $row.PSObject.Properties[$field]) {
                throw "Exact performance sample field is missing: $field"
            }
            $number = 0.0
            if (-not [double]::TryParse(
                    [string]$row.PSObject.Properties[$field].Value,
                    [Globalization.NumberStyles]::Float,
                    [Globalization.CultureInfo]::InvariantCulture,
                    [ref]$number) -or
                [double]::IsNaN($number) -or [double]::IsInfinity($number) -or
                $number -lt 0.0) {
                throw "Exact performance sample is invalid: $field"
            }
            $values[$field] = $number
        }
        $samples.Add([pscustomobject]$values)
    }
    return @($samples)
}

function Get-ProjectWorldPerformancePercentile {
    param(
        [Parameter(Mandatory = $true)][object[]]$Samples,
        [Parameter(Mandatory = $true)][string]$Field,
        [Parameter(Mandatory = $true)][double]$Quantile
    )
    if ($Samples.Count -eq 0) {
        return 0.0
    }
    $values = [Collections.Generic.List[double]]::new()
    foreach ($sample in $Samples) {
        $values.Add([double]$sample.PSObject.Properties[$Field].Value)
    }
    $values.Sort()
    $index = [Math]::Max(0, [Math]::Min(
            $values.Count - 1,
            [Math]::Ceiling($Quantile * $values.Count) - 1))
    return $values[$index]
}

function Get-ProjectWorldPerformanceStatistics {
    param([Parameter(Mandatory = $true)][object[]]$Samples)
    return [pscustomobject][ordered]@{
        sample_count = $Samples.Count
        frame_p95_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'FrameTime' -Quantile 0.95
        frame_p99_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'FrameTime' -Quantile 0.99
        frame_max_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'FrameTime' -Quantile 1.0
        game_p95_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'GameThreadTime' -Quantile 0.95
        game_p99_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'GameThreadTime' -Quantile 0.99
        render_p95_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'RenderThreadTime' -Quantile 0.95
        render_p99_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'RenderThreadTime' -Quantile 0.99
        gpu_p95_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'GPUTime' -Quantile 0.95
        gpu_p99_ms = Get-ProjectWorldPerformancePercentile `
            -Samples $Samples -Field 'GPUTime' -Quantile 0.99
    }
}

function Test-ProjectWorldPerformanceClose {
    param([double]$Actual, [double]$Expected)
    return [Math]::Abs($Actual - $Expected) -le 0.000001
}

function New-ProjectWorldPerformanceAggregate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][object[]]$Children,
        [Parameter(Mandatory = $true)][string]$OperationId,
        [Parameter(Mandatory = $true)][string]$SourceRevision,
        [Parameter(Mandatory = $true)][string]$SourceStateSha256,
        [Parameter(Mandatory = $true)][string]$RuntimeProfileSha256,
        [Parameter(Mandatory = $true)][string]$ExpectedExecutable,
        [Parameter(Mandatory = $true)][string]$ExpectedExecutableSha256,
        [Parameter(Mandatory = $true)][string]$ExpectedPackage,
        [Parameter(Mandatory = $true)][string]$ExpectedPackageSha256,
        [double]$FrameP95BudgetMilliseconds = 16.67
    )

    if ($Children.Count -ne 3) {
        throw 'Performance aggregation requires exactly three predetermined child runs.'
    }
    foreach ($hash in @(
            $SourceStateSha256,
            $RuntimeProfileSha256,
            $ExpectedExecutableSha256,
            $ExpectedPackageSha256)) {
        if ($hash -cnotmatch '^[a-f0-9]{64}$') {
            throw 'Performance aggregation received an invalid SHA-256 identity.'
        }
    }
    if ($SourceRevision -cnotmatch '^[a-f0-9]{40}$') {
        throw 'Performance aggregation received an invalid source revision.'
    }
    $resolvedExecutable = [IO.Path]::GetFullPath($ExpectedExecutable)
    $resolvedPackage = [IO.Path]::GetFullPath($ExpectedPackage)
    if ((Get-ProjectWorldPerformanceFileHash -Path $resolvedExecutable) -cne
        $ExpectedExecutableSha256) {
        throw 'Development executable bytes changed before performance aggregation.'
    }

    $allSamples = [Collections.Generic.List[object]]::new()
    $childEvidence = [Collections.Generic.List[object]]::new()
    $peakProcessPhysicalBytes = [Int64]0
    $peakGpuLocalBytes = [Int64]0
    foreach ($child in $Children) {
        foreach ($name in @(
                'ExpectedOperationId', 'ExpectedProcessExitCode', 'ReceiptPath',
                'SamplePath', 'RichCsvPath')) {
            if ($null -eq $child.PSObject.Properties[$name] -or
                [string]::IsNullOrWhiteSpace([string]$child.PSObject.Properties[$name].Value)) {
                throw "Performance child is missing required identity: $name"
            }
        }
        $receipt = Get-Content -LiteralPath $child.ReceiptPath -Raw | ConvertFrom-Json
        if (-not (Test-ProjectWorldPerformanceEnvelopeReceipt -Receipt $receipt)) {
            throw 'Performance child has a missing or invalid runtime envelope.'
        }
        $receiptExecutable = [IO.Path]::GetFullPath([string]$receipt.executable)
        if ([string]$receipt.operation_id -cne [string]$child.ExpectedOperationId -or
            [string]$receipt.runtime_profile_sha256 -cne $RuntimeProfileSha256 -or
            -not $receiptExecutable.Equals(
                $resolvedExecutable, [StringComparison]::OrdinalIgnoreCase) -or
            [string]$receipt.build_configuration -cne 'Development' -or
            [string]$receipt.correctness_status -cne 'accepted' -or
            [string]$receipt.machine_profile_id -cne 'rtx4070_primary' -or
            [string]$receipt.gpu_adapter -cne 'NVIDIA GeForce RTX 4070' -or
            [string]$receipt.rhi -cne 'D3D12' -or
            [string]$receipt.quality_preset -cne 'High' -or
            [int]$receipt.resolution_x -ne 2560 -or
            [int]$receipt.resolution_y -ne 1440 -or
            -not [bool]$receipt.playable_tour -or
            [int]$receipt.streaming_failures -ne 0) {
            throw 'Performance child identity, product, or execution envelope was rejected.'
        }
        $samplePath = [IO.Path]::GetFullPath([string]$child.SamplePath)
        $richCsvPath = [IO.Path]::GetFullPath([string]$child.RichCsvPath)
        if ($null -eq $receipt.PSObject.Properties['raw_sample_capture'] -or
            -not ([IO.Path]::GetFullPath([string]$receipt.raw_sample_capture)).Equals(
                $samplePath, [StringComparison]::OrdinalIgnoreCase) -or
            -not ([IO.Path]::GetFullPath([string]$receipt.csv_capture)).Equals(
                $richCsvPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Performance child sample or diagnostic CSV authority was rejected.'
        }
        $samples = @(Import-ProjectWorldPerformanceSamples -Path $samplePath `
                -ExpectedCount ([int]$receipt.sample_count))
        if ($samples.Count -lt 300) {
            throw 'Performance child did not satisfy the minimum sample count.'
        }
        $statistics = Get-ProjectWorldPerformanceStatistics -Samples $samples
        foreach ($field in @(
                'frame_p95_ms', 'frame_p99_ms', 'frame_max_ms',
                'game_p95_ms', 'render_p95_ms', 'gpu_p95_ms')) {
            if (-not (Test-ProjectWorldPerformanceClose `
                    -Actual ([double]$statistics.PSObject.Properties[$field].Value) `
                    -Expected ([double]$receipt.PSObject.Properties[$field].Value))) {
                throw "Performance child raw samples do not reproduce $field."
            }
        }
        $outcome = Get-ProjectWorldPerformanceChildOutcome -Receipt $receipt `
            -ProcessExitCode ([int]$child.ExpectedProcessExitCode) `
            -FrameP95BudgetMilliseconds $FrameP95BudgetMilliseconds
        if (-not $outcome.valid) {
            throw ('Performance child failed for a non-aggregatable reason: ' +
                "status=$($receipt.status) frame_p95_ms=$($receipt.frame_p95_ms) " +
                "exit=$($child.ExpectedProcessExitCode) error_count=$($outcome.error_count).")
        }

        foreach ($sample in $samples) {
            $allSamples.Add($sample)
        }
        $peakProcessPhysicalBytes = [Math]::Max(
            $peakProcessPhysicalBytes, [Int64]$receipt.peak_process_physical_bytes)
        $peakGpuLocalBytes = [Math]::Max(
            $peakGpuLocalBytes, [Int64]$receipt.peak_gpu_local_bytes)
        $childEvidence.Add([pscustomobject][ordered]@{
                operation_id = [string]$child.ExpectedOperationId
                status = [string]$receipt.status
                process_exit_code = [int]$child.ExpectedProcessExitCode
                receipt = [IO.Path]::GetFullPath([string]$child.ReceiptPath)
                receipt_sha256 = Get-ProjectWorldPerformanceFileHash -Path $child.ReceiptPath
                raw_sample_capture = $samplePath
                raw_sample_sha256 = Get-ProjectWorldPerformanceFileHash -Path $samplePath
                diagnostic_csv = $richCsvPath
                diagnostic_csv_sha256 = Get-ProjectWorldPerformanceFileHash -Path $richCsvPath
                sample_count = $statistics.sample_count
                frame_p95_ms = $statistics.frame_p95_ms
                frame_p99_ms = $statistics.frame_p99_ms
                game_p95_ms = $statistics.game_p95_ms
                game_p99_ms = $statistics.game_p99_ms
                render_p95_ms = $statistics.render_p95_ms
                render_p99_ms = $statistics.render_p99_ms
                gpu_p95_ms = $statistics.gpu_p95_ms
                gpu_p99_ms = $statistics.gpu_p99_ms
                streaming_failures = [int]$receipt.streaming_failures
                peak_process_physical_bytes = [Int64]$receipt.peak_process_physical_bytes
                peak_gpu_local_bytes = [Int64]$receipt.peak_gpu_local_bytes
            })
    }

    $pooled = Get-ProjectWorldPerformanceStatistics -Samples @($allSamples)
    $accepted = [double]$pooled.frame_p95_ms -le $FrameP95BudgetMilliseconds
    return [pscustomobject][ordered]@{
        schema_version = 1
        status = if ($accepted) { 'accepted' } else { 'rejected' }
        operation_id = $OperationId
        source_revision = $SourceRevision
        source_state_sha256 = $SourceStateSha256
        runtime_profile_sha256 = $RuntimeProfileSha256
        development_package = $resolvedPackage
        development_package_sha256 = $ExpectedPackageSha256
        development_package_digest_scope = 'payload_excluding_runtime_state'
        development_package_excluded_runtime_state = @(
            'Windows/Alis/LocalAppData',
            'Windows/Alis/Saved',
            'Windows/Engine/Saved'
        )
        development_executable = $resolvedExecutable
        development_executable_sha256 = $ExpectedExecutableSha256
        execution_count = 3
        frame_p95_budget_ms = $FrameP95BudgetMilliseconds
        children = @($childEvidence)
        total_sample_count = $pooled.sample_count
        frame_p95_ms = $pooled.frame_p95_ms
        frame_p99_ms = $pooled.frame_p99_ms
        frame_max_ms = $pooled.frame_max_ms
        game_p95_ms = $pooled.game_p95_ms
        game_p99_ms = $pooled.game_p99_ms
        render_p95_ms = $pooled.render_p95_ms
        render_p99_ms = $pooled.render_p99_ms
        gpu_p95_ms = $pooled.gpu_p95_ms
        gpu_p99_ms = $pooled.gpu_p99_ms
        peak_process_physical_bytes = $peakProcessPhysicalBytes
        peak_gpu_local_bytes = $peakGpuLocalBytes
        streaming_failures = 0
        acceptance_reason = if ($accepted) { '' } else {
            'Pooled Frame p95 {0:F3} ms exceeded the {1:F3} ms budget.' -f `
                $pooled.frame_p95_ms, $FrameP95BudgetMilliseconds
        }
    }
}
