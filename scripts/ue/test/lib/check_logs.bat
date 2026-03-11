@echo off
REM ============================================================================
REM Log Checker - Validates GameFeature registration from logs (SOLID: SRP)
REM ============================================================================
REM Usage: call check_logs.bat <plugin_name>
REM Returns: 0 on success, 1 on error, 2 on warning
REM ============================================================================

setlocal enabledelayedexpansion

REM Load config
call "%~dp0..\config\test_config.bat"
if errorlevel 1 (
    echo ERROR: Failed to load test_config.bat
    exit /b 1
)

set "PLUGIN_NAME=%~1"
if "%PLUGIN_NAME%"=="" (
    echo ERROR: Plugin name required
    echo Usage: check_logs.bat ^<plugin_name^>
    exit /b 1
)

REM Verify log file exists
if not exist "%LOG_FILE%" (
    echo ERROR: Log file not found: %LOG_FILE%
    exit /b 1
)

echo Checking logs for plugin: %PLUGIN_NAME%

REM Check 1: Plugin mentioned in logs
findstr /C:"%PLUGIN_NAME%" "%LOG_FILE%" >nul 2>&1
if !errorlevel! neq 0 (
    echo ✗ FAIL: No logs found for %PLUGIN_NAME%
    exit /b 1
)
echo ✓ Plugin logs found

REM Check 2: Check for registration error
findstr /C:"Plugin_Missing_GameFeatureData" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%" >nul 2>&1
if !errorlevel! equ 0 (
    echo ✗ FAIL: Plugin_Missing_GameFeatureData error detected
    echo.
    echo Error details:
    findstr /C:"Plugin_Missing_GameFeatureData" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%"
    exit /b 1
)
echo ✓ No GameFeatureData error

REM Check 3: Check for other errors
findstr /C:"LogGameFeatures: Error" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%" >nul 2>&1
if !errorlevel! equ 0 (
    echo ✗ FAIL: GameFeature error detected
    echo.
    echo Error details:
    findstr /C:"LogGameFeatures: Error" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%"
    exit /b 1
)
echo ✓ No GameFeature errors

REM Check 4: Check for successful load (optional - returns warning if not found)
findstr /C:"Loaded GameFeatureData" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%" >nul 2>&1
if !errorlevel! equ 0 (
    echo ✓ GameFeatureData loaded successfully
    echo.
    findstr /C:"Loaded GameFeatureData" "%LOG_FILE%" | findstr /C:"%PLUGIN_NAME%"
    exit /b 0
) else (
    echo ⚠ WARNING: No explicit "Loaded GameFeatureData" log found
    exit /b 2
)
