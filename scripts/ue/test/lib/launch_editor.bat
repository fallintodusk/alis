@echo off
REM ============================================================================
REM Editor Launcher - Starts Unreal Editor and waits for initialization (SOLID: SRP)
REM ============================================================================
REM Usage: call launch_editor.bat
REM Returns: 0 on success, 1 on timeout/failure
REM ============================================================================

setlocal enabledelayedexpansion

REM Load config
call "%~dp0..\config\test_config.bat"
if errorlevel 1 (
    echo ERROR: Failed to load test_config.bat
    exit /b 1
)

REM Check if editor is already running
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>NUL | find /I /N "UnrealEditor.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo ERROR: UnrealEditor.exe is already running!
    echo Please close the editor manually before running this test.
    exit /b 1
)

echo Starting Unreal Editor...
echo   Executable: %UE_EDITOR%
echo   Project: %PROJECT_FILE%
echo   Timeout: %EDITOR_TIMEOUT_SECONDS%s

REM Verify editor executable exists
if not exist "%UE_EDITOR%" (
    echo ERROR: UnrealEditor.exe not found at: %UE_EDITOR%
    exit /b 1
)

REM Verify project file exists
if not exist "%PROJECT_FILE%" (
    echo ERROR: Project file not found at: %PROJECT_FILE%
    exit /b 1
)

REM Start editor in background
start "UnrealEditor" "%UE_EDITOR%" "%PROJECT_FILE%" -log -stdout

REM Wait for log file to appear and contain initialization markers
set /a ELAPSED=0

:WAIT_LOOP
timeout /t %LOG_CHECK_INTERVAL_SECONDS% /nobreak >nul 2>&1
set /a ELAPSED+=%LOG_CHECK_INTERVAL_SECONDS%

REM Check if log file exists and contains GameFeatures initialization
if exist "%LOG_FILE%" (
    findstr /C:"LogGameFeatures" "%LOG_FILE%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo ✓ Editor initialized after %ELAPSED%s
        exit /b 0
    )
)

REM Check timeout
if %ELAPSED% geq %EDITOR_TIMEOUT_SECONDS% (
    echo ✗ Timeout after %EDITOR_TIMEOUT_SECONDS%s
    echo ERROR: Editor did not initialize in time
    exit /b 1
)

echo   Waiting for editor... (%ELAPSED%/%EDITOR_TIMEOUT_SECONDS%s^)
goto WAIT_LOOP
