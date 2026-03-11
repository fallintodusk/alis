# Common.psm1 - Shared utilities for overnight CI scripts
# Provides retry logic, circuit breaker, and state management

<#
.SYNOPSIS
  Invoke action with exponential backoff retry logic

.PARAMETER Action
  Scriptblock to execute

.PARAMETER MaxAttempts
  Maximum retry attempts (default: 3)

.PARAMETER InitialDelay
  Initial backoff delay in seconds (default: 10)

.PARAMETER MaxDelay
  Maximum backoff delay in seconds (default: 120)

.PARAMETER OperationName
  Name of operation for logging

.EXAMPLE
  Invoke-WithExponentialBackoff -Action { Build-Something } -MaxAttempts 3 -InitialDelay 10 -MaxDelay 120
#>
function Invoke-WithExponentialBackoff {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [scriptblock]$Action,

        [Parameter()]
        [int]$MaxAttempts = 3,

        [Parameter()]
        [int]$InitialDelay = 10,

        [Parameter()]
        [int]$MaxDelay = 120,

        [Parameter()]
        [string]$OperationName = "Operation"
    )

    $attempt = 1
    $delay = $InitialDelay

    while ($attempt -le $MaxAttempts) {
        Write-Verbose "[$OperationName] Attempt $attempt/$MaxAttempts..."

        try {
            $result = & $Action
            if ($null -eq $result) {
                $result = @{ ExitCode = 0 }
            }
            if ($result.ExitCode -eq 0) {
                Write-Verbose "[$OperationName] Success on attempt $attempt"
                return $result
            } else {
                throw "$OperationName failed with exit code $($result.ExitCode)"
            }
        } catch {
            Write-Warning "[$OperationName] Failed: $($_.Exception.Message)"

            if ($attempt -ge $MaxAttempts) {
                Write-Error "[$OperationName] All $MaxAttempts attempts exhausted"
                throw $_
            }

            Write-Verbose "[$OperationName] Waiting $delay seconds before retry..."
            Start-Sleep -Seconds $delay

            # Exponential backoff: double delay, cap at MaxDelay
            $delay = [Math]::Min($delay * 2, $MaxDelay)
            $attempt++
        }
    }
}

<#
.SYNOPSIS
  Get circuit breaker state from file

.PARAMETER StateFile
  Path to circuit breaker state JSON file

.EXAMPLE
  $state = Get-CircuitBreakerState -StateFile "circuit_breaker.json"
#>
function Get-CircuitBreakerState {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$StateFile
    )

    if (Test-Path $StateFile) {
        $state = Get-Content $StateFile | ConvertFrom-Json
        return @{
            ConsecutiveFailures = $state.ConsecutiveFailures
            LastFailureTime = [DateTime]::Parse($state.LastFailureTime)
            State = $state.State
        }
    }

    return @{
        ConsecutiveFailures = 0
        LastFailureTime = [DateTime]::MinValue
        State = "CLOSED"
    }
}

<#
.SYNOPSIS
  Set circuit breaker state to file

.PARAMETER StateFile
  Path to circuit breaker state JSON file

.PARAMETER ConsecutiveFailures
  Number of consecutive failures

.PARAMETER LastFailureTime
  DateTime of last failure

.PARAMETER State
  Circuit breaker state: CLOSED, OPEN, HALF_OPEN

.EXAMPLE
  Set-CircuitBreakerState -StateFile "circuit_breaker.json" -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"
#>
function Set-CircuitBreakerState {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$StateFile,

        [Parameter(Mandatory)]
        [int]$ConsecutiveFailures,

        [Parameter(Mandatory)]
        [DateTime]$LastFailureTime,

        [Parameter(Mandatory)]
        [ValidateSet("CLOSED", "OPEN", "HALF_OPEN")]
        [string]$State
    )

    $stateDir = Split-Path -Parent $StateFile
    if (-not (Test-Path $stateDir)) {
        New-Item -ItemType Directory -Path $stateDir -Force | Out-Null
    }

    $stateObj = @{
        ConsecutiveFailures = $ConsecutiveFailures
        LastFailureTime = $LastFailureTime.ToString("o")
        State = $State
    }
    $stateObj | ConvertTo-Json | Set-Content $StateFile
}

<#
.SYNOPSIS
  Test if circuit breaker allows operation

.PARAMETER StateFile
  Path to circuit breaker state JSON file

.PARAMETER Threshold
  Number of consecutive failures before opening circuit

.PARAMETER CooldownMinutes
  Cooldown period in minutes before allowing retry

.EXAMPLE
  if (Test-CircuitBreaker -StateFile "circuit_breaker.json" -Threshold 5 -CooldownMinutes 30) {
      # Operation allowed
  }
#>
function Test-CircuitBreaker {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$StateFile,

        [Parameter()]
        [int]$Threshold = 5,

        [Parameter()]
        [int]$CooldownMinutes = 30
    )

    $state = Get-CircuitBreakerState -StateFile $StateFile

    if ($state.State -eq "OPEN") {
        $timeSinceFailure = (Get-Date) - $state.LastFailureTime
        if ($timeSinceFailure.TotalMinutes -ge $CooldownMinutes) {
            Write-Verbose "Circuit breaker HALF-OPEN (cooldown expired, allowing test attempt)"
            Set-CircuitBreakerState -StateFile $StateFile -ConsecutiveFailures $state.ConsecutiveFailures -LastFailureTime $state.LastFailureTime -State "HALF_OPEN"
            return $true
        } else {
            $remainingMinutes = [Math]::Ceiling($CooldownMinutes - $timeSinceFailure.TotalMinutes)
            Write-Warning "Circuit breaker OPEN: $($state.ConsecutiveFailures) consecutive failures (threshold: $Threshold)"
            Write-Warning "Cooldown remaining: $remainingMinutes minutes"
            return $false
        }
    }

    return $true
}

<#
.SYNOPSIS
  Update circuit breaker state on successful operation

.PARAMETER StateFile
  Path to circuit breaker state JSON file

.EXAMPLE
  Update-CircuitBreakerOnSuccess -StateFile "circuit_breaker.json"
#>
function Update-CircuitBreakerOnSuccess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$StateFile
    )

    Write-Verbose "Circuit breaker CLOSED (operation succeeded, resetting failure count)"
    Set-CircuitBreakerState -StateFile $StateFile -ConsecutiveFailures 0 -LastFailureTime ([DateTime]::MinValue) -State "CLOSED"
}

<#
.SYNOPSIS
  Update circuit breaker state on failed operation

.PARAMETER StateFile
  Path to circuit breaker state JSON file

.PARAMETER Threshold
  Number of consecutive failures before opening circuit

.EXAMPLE
  Update-CircuitBreakerOnFailure -StateFile "circuit_breaker.json" -Threshold 5
#>
function Update-CircuitBreakerOnFailure {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$StateFile,

        [Parameter()]
        [int]$Threshold = 5
    )

    $state = Get-CircuitBreakerState -StateFile $StateFile
    $newFailures = $state.ConsecutiveFailures + 1
    $newState = if ($newFailures -ge $Threshold) { "OPEN" } else { "CLOSED" }

    Write-Verbose "Circuit breaker: $newFailures consecutive failures (threshold: $Threshold)"
    Set-CircuitBreakerState -StateFile $StateFile -ConsecutiveFailures $newFailures -LastFailureTime (Get-Date) -State $newState

    if ($newState -eq "OPEN") {
        Write-Warning "Circuit breaker OPENED: Too many failures, entering cooldown"
    }
}

<#
.SYNOPSIS
  Test if disk has sufficient free space

.PARAMETER Path
  Path to check (defaults to current drive)

.PARAMETER RequiredGB
  Required free space in GB (default: 50GB for UE builds)

.EXAMPLE
  if (Test-DiskSpace -Path "C:\" -RequiredGB 50) {
      # Proceed with build
  }
#>
function Test-DiskSpace {
    [CmdletBinding()]
    param(
        [Parameter()]
        [string]$Path = (Get-Location).Drive.Root,

        [Parameter()]
        [int]$RequiredGB = 50
    )

    try {
        $drive = Get-PSDrive -Name (Split-Path -Qualifier $Path).TrimEnd(':')
        $freeGB = [Math]::Round($drive.Free / 1GB, 2)

        Write-Verbose "Disk space check: $($drive.Name): $freeGB GB free (required: $RequiredGB GB)"

        if ($freeGB -lt $RequiredGB) {
            Write-Warning "Low disk space: $freeGB GB free, $RequiredGB GB required"
            return $false
        }

        return $true
    } catch {
        Write-Warning "Failed to check disk space: $($_.Exception.Message)"
        return $true  # Fail open - don't block builds on monitoring failure
    }
}

<#
.SYNOPSIS
  Get disk space information for a path

.PARAMETER Path
  Path to check (defaults to current drive)

.EXAMPLE
  $info = Get-DiskSpaceInfo -Path "C:\"
#>
function Get-DiskSpaceInfo {
    [CmdletBinding()]
    param(
        [Parameter()]
        [string]$Path = (Get-Location).Drive.Root
    )

    try {
        $drive = Get-PSDrive -Name (Split-Path -Qualifier $Path).TrimEnd(':')

        return @{
            Drive = $drive.Name
            TotalGB = [Math]::Round($drive.Used / 1GB + $drive.Free / 1GB, 2)
            UsedGB = [Math]::Round($drive.Used / 1GB, 2)
            FreeGB = [Math]::Round($drive.Free / 1GB, 2)
            FreePercent = [Math]::Round(($drive.Free / ($drive.Used + $drive.Free)) * 100, 1)
        }
    } catch {
        Write-Warning "Failed to get disk space info: $($_.Exception.Message)"
        return $null
    }
}

<#
.SYNOPSIS
  Find zombie UE processes (running longer than threshold)

.PARAMETER MaxAgeMinutes
  Process age threshold in minutes (default: 60)

.EXAMPLE
  $zombies = Find-ZombieProcesses -MaxAgeMinutes 60
#>
function Find-ZombieProcesses {
    [CmdletBinding()]
    param(
        [Parameter()]
        [int]$MaxAgeMinutes = 60
    )

    $processNames = @(
        "UnrealEditor*",
        "UnrealEditor-Cmd*",
        "UEBuildWorker*",
        "ShaderCompileWorker*",
        "UnrealLightmass*",
        "CrashReportClient*"
    )

    $zombies = @()
    $now = Get-Date

    foreach ($pattern in $processNames) {
        $processes = Get-Process | Where-Object { $_.ProcessName -like $pattern }

        foreach ($proc in $processes) {
            try {
                $age = ($now - $proc.StartTime).TotalMinutes
                if ($age -gt $MaxAgeMinutes) {
                    $zombies += @{
                        Name = $proc.ProcessName
                        Id = $proc.Id
                        AgeMinutes = [Math]::Round($age, 1)
                        StartTime = $proc.StartTime
                    }
                }
            } catch {
                # Process may have exited or access denied - skip
                Write-Verbose "Failed to get info for process $($proc.ProcessName): $($_.Exception.Message)"
            }
        }
    }

    return $zombies
}

<#
.SYNOPSIS
  Kill zombie UE processes with detailed logging

.PARAMETER MaxAgeMinutes
  Process age threshold in minutes (default: 60)

.PARAMETER Force
  Force kill without confirmation

.EXAMPLE
  Stop-ZombieProcesses -MaxAgeMinutes 60 -Force
#>
function Stop-ZombieProcesses {
    [CmdletBinding()]
    param(
        [Parameter()]
        [int]$MaxAgeMinutes = 60,

        [Parameter()]
        [switch]$Force = $false
    )

    $zombies = Find-ZombieProcesses -MaxAgeMinutes $MaxAgeMinutes

    if ($zombies.Count -eq 0) {
        Write-Verbose "No zombie processes found (age > $MaxAgeMinutes minutes)"
        return @{
            Found = 0
            Killed = 0
            Failed = 0
        }
    }

    Write-Warning "Found $($zombies.Count) zombie process(es) running > $MaxAgeMinutes minutes:"
    foreach ($zombie in $zombies) {
        Write-Warning "  - $($zombie.Name) (PID: $($zombie.Id), age: $($zombie.AgeMinutes) min)"
    }

    $killed = 0
    $failed = 0

    foreach ($zombie in $zombies) {
        try {
            $proc = Get-Process -Id $zombie.Id -ErrorAction Stop

            if ($Force) {
                $proc | Stop-Process -Force -ErrorAction Stop
                Write-Verbose "Killed zombie: $($zombie.Name) (PID: $($zombie.Id))"
                $killed++
            } else {
                $proc | Stop-Process -ErrorAction Stop
                Write-Verbose "Stopped zombie: $($zombie.Name) (PID: $($zombie.Id))"
                $killed++
            }
        } catch {
            Write-Warning "Failed to kill $($zombie.Name) (PID: $($zombie.Id)): $($_.Exception.Message)"
            $failed++
        }
    }

    return @{
        Found = $zombies.Count
        Killed = $killed
        Failed = $failed
    }
}

<#
.SYNOPSIS
  Kill all UE processes (for clean build start)

.PARAMETER IncludeCrashReporter
  Also kill CrashReportClient processes

.EXAMPLE
  Stop-AllUEProcesses -IncludeCrashReporter
#>
function Stop-AllUEProcesses {
    [CmdletBinding()]
    param(
        [Parameter()]
        [switch]$IncludeCrashReporter = $false
    )

    $processNames = @(
        "UnrealEditor*",
        "UnrealEditor-Cmd*",
        "UEBuildWorker*",
        "ShaderCompileWorker*",
        "UnrealLightmass*"
    )

    if ($IncludeCrashReporter) {
        $processNames += "CrashReportClient*"
    }

    $killed = 0
    $failed = 0

    foreach ($pattern in $processNames) {
        $processes = Get-Process | Where-Object { $_.ProcessName -like $pattern }

        foreach ($proc in $processes) {
            try {
                $proc | Stop-Process -Force -ErrorAction Stop
                Write-Verbose "Killed: $($proc.ProcessName) (PID: $($proc.Id))"
                $killed++
            } catch {
                Write-Verbose "Failed to kill $($proc.ProcessName) (PID: $($proc.Id)): $($_.Exception.Message)"
                $failed++
            }
        }
    }

    return @{
        Killed = $killed
        Failed = $failed
    }
}

<#
.SYNOPSIS
  Write structured JSON log entry

.PARAMETER Level
  Log level (INFO, WARN, ERROR, DEBUG)

.PARAMETER Message
  Log message

.PARAMETER Data
  Additional structured data (hashtable)

.PARAMETER LogFile
  Path to JSON log file

.EXAMPLE
  Write-StructuredLog -Level "INFO" -Message "Build started" -Data @{Module="ProjectCore"} -LogFile "build.json"
#>
function Write-StructuredLog {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateSet("INFO", "WARN", "ERROR", "DEBUG")]
        [string]$Level,

        [Parameter(Mandatory)]
        [string]$Message,

        [Parameter()]
        [hashtable]$Data = @{},

        [Parameter()]
        [string]$LogFile
    )

    $logEntry = @{
        timestamp = (Get-Date -Format "o")
        level = $Level
        message = $Message
    }

    # Merge additional data
    foreach ($key in $Data.Keys) {
        $logEntry[$key] = $Data[$key]
    }

    $jsonLine = $logEntry | ConvertTo-Json -Compress

    if ($LogFile) {
        Add-Content -Path $LogFile -Value $jsonLine
    }

    # Also write to verbose/warning streams
    switch ($Level) {
        "ERROR" { Write-Error $Message }
        "WARN" { Write-Warning $Message }
        "DEBUG" { Write-Verbose $Message }
        default { Write-Verbose $Message }
    }
}

<#
.SYNOPSIS
  Record metric to metrics file

.PARAMETER MetricName
  Name of the metric

.PARAMETER Value
  Numeric value

.PARAMETER MetricsFile
  Path to metrics JSON file

.EXAMPLE
  Add-Metric -MetricName "BuildTimeSeconds" -Value 120 -MetricsFile "metrics.json"
#>
function Add-Metric {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$MetricName,

        [Parameter(Mandatory)]
        $Value,

        [Parameter(Mandatory)]
        [string]$MetricsFile
    )

    $metric = @{
        timestamp = (Get-Date -Format "o")
        name = $MetricName
        value = $Value
    }

    $jsonLine = $metric | ConvertTo-Json -Compress
    Add-Content -Path $MetricsFile -Value $jsonLine
}

<#
.SYNOPSIS
  Get metric statistics from metrics file

.PARAMETER MetricName
  Name of the metric to analyze

.PARAMETER MetricsFile
  Path to metrics JSON file

.EXAMPLE
  Get-MetricStats -MetricName "BuildTimeSeconds" -MetricsFile "metrics.json"
#>
function Get-MetricStats {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$MetricName,

        [Parameter(Mandatory)]
        [string]$MetricsFile
    )

    if (-not (Test-Path $MetricsFile)) {
        return $null
    }

    $metrics = Get-Content $MetricsFile | ForEach-Object {
        try { $_ | ConvertFrom-Json } catch { $null }
    } | Where-Object { $_.name -eq $MetricName }

    if ($metrics.Count -eq 0) {
        return $null
    }

    $values = $metrics | ForEach-Object { [double]$_.value }

    return @{
        Count = $values.Count
        Min = ($values | Measure-Object -Minimum).Minimum
        Max = ($values | Measure-Object -Maximum).Maximum
        Average = ($values | Measure-Object -Average).Average
        Total = ($values | Measure-Object -Sum).Sum
    }
}

<#
.SYNOPSIS
  Get current system resource usage

.EXAMPLE
  $resources = Get-SystemResources
#>
function Get-SystemResources {
    [CmdletBinding()]
    param()

    try {
        # CPU usage
        $cpu = Get-WmiObject Win32_Processor | Measure-Object -Property LoadPercentage -Average | Select-Object -ExpandProperty Average

        # RAM usage
        $os = Get-WmiObject Win32_OperatingSystem
        $totalRAM = [Math]::Round($os.TotalVisibleMemorySize / 1MB, 2)
        $freeRAM = [Math]::Round($os.FreePhysicalMemory / 1MB, 2)
        $usedRAM = $totalRAM - $freeRAM
        $ramPercent = [Math]::Round(($usedRAM / $totalRAM) * 100, 1)

        # Disk usage (C: drive)
        $disk = Get-WmiObject Win32_LogicalDisk -Filter "DeviceID='C:'"
        $totalDisk = [Math]::Round($disk.Size / 1GB, 2)
        $freeDisk = [Math]::Round($disk.FreeSpace / 1GB, 2)
        $usedDisk = $totalDisk - $freeDisk
        $diskPercent = [Math]::Round(($usedDisk / $totalDisk) * 100, 1)

        return @{
            CPU = @{
                Percent = $cpu
            }
            RAM = @{
                TotalGB = $totalRAM
                UsedGB = $usedRAM
                FreeGB = $freeRAM
                Percent = $ramPercent
            }
            Disk = @{
                TotalGB = $totalDisk
                UsedGB = $usedDisk
                FreeGB = $freeDisk
                Percent = $diskPercent
            }
            Timestamp = (Get-Date -Format "o")
        }
    } catch {
        Write-Warning "Failed to get system resources: $($_.Exception.Message)"
        return $null
    }
}

Export-ModuleMember -Function @(
    'Invoke-WithExponentialBackoff',
    'Get-CircuitBreakerState',
    'Set-CircuitBreakerState',
    'Test-CircuitBreaker',
    'Update-CircuitBreakerOnSuccess',
    'Update-CircuitBreakerOnFailure',
    'Test-DiskSpace',
    'Get-DiskSpaceInfo',
    'Find-ZombieProcesses',
    'Stop-ZombieProcesses',
    'Stop-AllUEProcesses',
    'Write-StructuredLog',
    'Add-Metric',
    'Get-MetricStats',
    'Get-SystemResources'
)
