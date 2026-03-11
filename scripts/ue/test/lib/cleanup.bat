@echo off
REM ============================================================================
REM Cleanup Utility - Handles editor shutdown and log cleanup (SOLID: SRP)
REM ============================================================================
REM Usage: call cleanup.bat [clean_logs|kill_editor|all]
REM ============================================================================

setlocal

REM Load config
call "%~dp0..\config\test_config.bat"
if errorlevel 1 (
    echo ERROR: Failed to load test_config.bat
    exit /b 1
)

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=all"

if "%ACTION%"=="clean_logs" goto CLEAN_LOGS
if "%ACTION%"=="kill_editor" goto KILL_EDITOR
if "%ACTION%"=="all" goto ALL

echo ERROR: Unknown action '%ACTION%'
echo Usage: cleanup.bat [clean_logs^|kill_editor^|all]
exit /b 1

:CLEAN_LOGS
if exist "%LOG_FILE%" (
    del "%LOG_FILE%" >nul 2>&1
    if errorlevel 1 (
        echo WARNING: Failed to delete log file
        exit /b 1
    )
    echo ✓ Cleaned log file: %LOG_FILE%
)
exit /b 0

:KILL_EDITOR
taskkill /IM UnrealEditor.exe /F >nul 2>&1
if errorlevel 1 (
    echo ⓘ No editor process found (may have already closed)
) else (
    echo ✓ Killed UnrealEditor.exe
)
exit /b 0

:ALL
call :CLEAN_LOGS
call :KILL_EDITOR
exit /b 0
