@echo off
REM Autonomous Boot Flow Test with Crash Detection and Auto-Fix
REM
REM This script:
REM   1. Runs standalone boot tests
REM   2. Captures full output
REM   3. Detects crashes (CrashReportClient.exe)
REM   4. Analyzes test results
REM   5. Reports success/failure with actionable details

setlocal enabledelayedexpansion

REM Configuration
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\..\"
set "LOG_DIR=%PROJECT_ROOT%Saved\AutomatedTests\"
set "TIMESTAMP=%date:~-4%%date:~4,2%%date:~7,2%_%time:~0,2%%time:~3,2%%time:~6,2%"
set "TIMESTAMP=!TIMESTAMP: =0!"
set "LOG_FILE=%LOG_DIR%boot_test_!TIMESTAMP!.log"

REM Find UE path
set "UE_PATH=<ue-path>"
if exist "%PROJECT_ROOT%utility\config\ue_path.conf" (
    for /f "tokens=1,* delims==" %%A in (%PROJECT_ROOT%utility\config\ue_path.conf) do (
        if "%%A"=="UE_PATH" set "UE_PATH=%%B"
    )
)

set "PROJECT_FILE=%PROJECT_ROOT%Alis.uproject"
set "UE_EDITOR_CMD=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

REM Create log directory
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo =============================================
echo   Autonomous Boot Flow Test
echo =============================================
echo.
echo Start Time: %date% %time%
echo UE Editor:  %UE_EDITOR_CMD%
echo Project:    %PROJECT_FILE%
echo Log File:   %LOG_FILE%
echo.
echo [STEP 1] Killing any existing UE processes...

REM Kill any existing processes to avoid DLL lock
taskkill //F //IM UnrealEditor-Cmd.exe >nul 2>&1
taskkill //F //IM CrashReportClient.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [STEP 2] Running tests...
echo.

REM Run the test and capture output
"%UE_EDITOR_CMD%" "%PROJECT_FILE%" ^
    -ExecCmds="Automation RunTests ProjectIntegrationTests.Standalone" ^
    -unattended ^
    -nopause ^
    -NullRHI ^
    -log > "%LOG_FILE%" 2>&1

set "TEST_EXITCODE=%ERRORLEVEL%"

echo.
echo [STEP 3] Test execution completed (Exit Code: %TEST_EXITCODE%)
echo.
echo [STEP 4] Analyzing results...
echo.

REM Check for crash
tasklist //FI "IMAGENAME eq CrashReportClient.exe" 2>nul | find /i /n "CrashReportClient.exe" >nul
if "!ERRORLEVEL!"=="0" (
    echo ========================================
    echo   CRASH DETECTED!
    echo ========================================
    echo.
    echo CrashReportClient.exe is running.
    echo.
    echo Killing crash reporter...
    taskkill //F //IM CrashReportClient.exe >nul 2>&1
    echo.
    echo Please check crash dumps in:
    echo   %PROJECT_ROOT%Saved\Crashes\
    echo.
    echo Last log file:
    echo   %LOG_FILE%
    echo.
    goto :FAILED
)

REM Analyze log file for test results
findstr /C:"No automation tests matched" "%LOG_FILE%" >nul
if "!ERRORLEVEL!"=="0" (
    echo ========================================
    echo   TEST NOT FOUND
    echo ========================================
    echo.
    echo The test 'ProjectIntegrationTests.Standalone' was not found.
    echo.
    echo Possible causes:
    echo   1. Tests use wrong context (ClientContext requires standalone game)
    echo   2. Plugin not enabled or not built
    echo   3. Module dependencies missing
    echo.
    echo Fix: Change EAutomationTestFlags::ClientContext to EditorContext
    echo.
    goto :FAILED
)

REM Check for test failures
findstr /C:"BootMap is empty" "%LOG_FILE%" >nul
if "!ERRORLEVEL!"=="0" (
    echo ========================================
    echo   CONFIG NOT LOADED
    echo ========================================
    echo.
    echo BootMap is empty - config file not loaded.
    echo.
    echo Possible causes:
    echo   1. UCLASS has wrong config specifier (should be config=ProjectBoot)
    echo   2. Config file missing: Config/DefaultProjectBoot.ini
    echo   3. Wrong section name in config
    echo.
    echo Checking config file...
    if exist "%PROJECT_ROOT%Config\DefaultProjectBoot.ini" (
        echo   ✓ Config file exists
        type "%PROJECT_ROOT%Config\DefaultProjectBoot.ini"
    ) else (
        echo   ✗ Config file NOT found!
    )
    echo.
    goto :FAILED
)

findstr /C:"still set to engine default" "%LOG_FILE%" >nul
if "!ERRORLEVEL!"=="0" (
    echo ========================================
    echo   CONFIG SPECIFIER WRONG
    echo ========================================
    echo.
    echo Config file not being read - UCLASS specifier is wrong.
    echo.
    echo Fix required in:
    echo   Plugins/Core/ProjectBoot/Source/ProjectBoot/Public/ProjectBootSettings.h
    echo.
    echo Change:
    echo   UCLASS(config=Game, ...)  // WRONG
    echo To:
    echo   UCLASS(config=ProjectBoot, ...)  // CORRECT
    echo.
    goto :FAILED
)

REM Check for success indicators
findstr /C:"BootMap configured correctly" "%LOG_FILE%" >nul
if "!ERRORLEVEL!"=="0" (
    findstr /C:"ProjectBootSubsystem exists" "%LOG_FILE%" >nul
    if "!ERRORLEVEL!"=="0" (
        echo ========================================
        echo   ALL TESTS PASSED!
        echo ========================================
        echo.
        echo Boot flow validation successful:
        echo.

        REM Extract key metrics from log
        findstr /C:"BootMap Package:" "%LOG_FILE%"
        findstr /C:"BootFlowControllerClass:" "%LOG_FILE%"
        findstr /C:"IsBootWorld:" "%LOG_FILE%"
        echo.
        echo Full log: %LOG_FILE%
        echo.
        goto :SUCCESS
    )
)

REM Generic failure
echo ========================================
echo   TEST RESULTS UNCLEAR
echo ========================================
echo.
echo Could not determine test status from output.
echo.
echo Please review the log file:
echo   %LOG_FILE%
echo.
echo Last 50 lines:
echo.
powershell -Command "Get-Content '%LOG_FILE%' | Select-Object -Last 50"
echo.

:FAILED
echo.
echo ========================================
echo   STATUS: FAILED ❌
echo ========================================
echo.
echo End Time: %date% %time%
echo.
exit /b 1

:SUCCESS
echo ========================================
echo   STATUS: SUCCESS ✅
echo ========================================
echo.
echo End Time: %date% %time%
echo.
exit /b 0
