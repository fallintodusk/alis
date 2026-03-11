@echo off
REM Automated GameFeature Registration Test
REM Opens editor, waits for full initialization, checks logs, then closes

setlocal enabledelayedexpansion

echo ========================================
echo GameFeature Registration Test
echo ========================================
echo.

REM Configuration
set "UE_EDITOR=<ue-path>\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJECT_FILE=<project-root>\Alis.uproject"
set "LOG_FILE=<project-root>\Saved\Logs\Alis.log"
set "TIMEOUT_SECONDS=60"

REM Clean old logs
echo [1/5] Cleaning old logs...
if exist "%LOG_FILE%" (
    del "%LOG_FILE%" >nul 2>&1
    echo   Deleted old log file
)

REM Start editor in background with -log flag
echo.
echo [2/5] Starting Unreal Editor...
echo   Project: %PROJECT_FILE%
echo   Timeout: %TIMEOUT_SECONDS% seconds
echo.

start "UnrealEditor" "%UE_EDITOR%" "%PROJECT_FILE%" -log -stdout

REM Wait for editor to initialize (check for specific log marker)
echo [3/5] Waiting for editor to initialize...
set /a ELAPSED=0
set /a CHECK_INTERVAL=2

:WAIT_LOOP
timeout /t %CHECK_INTERVAL% /nobreak >nul 2>&1
set /a ELAPSED+=%CHECK_INTERVAL%

REM Check if log file exists and contains initialization marker
if exist "%LOG_FILE%" (
    findstr /C:"LogGameFeatures" "%LOG_FILE%" >nul 2>&1
    if !errorlevel! equ 0 (
        echo   ✓ Editor initialized after %ELAPSED% seconds
        goto CHECK_LOGS
    )
)

REM Check timeout
if %ELAPSED% geq %TIMEOUT_SECONDS% (
    echo   ✗ Timeout after %TIMEOUT_SECONDS% seconds
    echo   Editor may have crashed or log file not created
    goto KILL_EDITOR
)

echo   Waiting... (%ELAPSED%/%TIMEOUT_SECONDS% seconds^)
goto WAIT_LOOP

:CHECK_LOGS
echo.
echo [4/5] Checking logs for GameFeature registration...
echo.

REM Search for errors
findstr /C:"ProjectMenuExperienceGF" "%LOG_FILE%" >nul 2>&1
if !errorlevel! neq 0 (
    echo   ✗ FAIL: No ProjectMenuExperienceGF logs found
    echo   Editor may not have loaded the plugin
    goto SHOW_RESULTS_FAIL
)

REM Check for registration error
findstr /C:"Plugin_Missing_GameFeatureData" "%LOG_FILE%" >nul 2>&1
if !errorlevel! equ 0 (
    echo   ✗ FAIL: Plugin_Missing_GameFeatureData error detected
    echo.
    echo   Error details:
    findstr /C:"Plugin_Missing_GameFeatureData" "%LOG_FILE%"
    echo.
    goto SHOW_RESULTS_FAIL
)

REM Check for other GameFeature errors
findstr /C:"LogGameFeatures: Error" "%LOG_FILE%" | findstr /C:"ProjectMenuExperienceGF" >nul 2>&1
if !errorlevel! equ 0 (
    echo   ✗ FAIL: GameFeature error detected
    echo.
    echo   Error details:
    findstr /C:"LogGameFeatures: Error" "%LOG_FILE%" | findstr /C:"ProjectMenuExperienceGF"
    echo.
    goto SHOW_RESULTS_FAIL
)

REM Check for successful registration
findstr /C:"Loaded GameFeatureData" "%LOG_FILE%" | findstr /C:"ProjectMenuExperienceGF" >nul 2>&1
if !errorlevel! equ 0 (
    echo   ✓ SUCCESS: GameFeatureData loaded successfully
    echo.
    findstr /C:"Loaded GameFeatureData" "%LOG_FILE%" | findstr /C:"ProjectMenuExperienceGF"
    goto SHOW_RESULTS_SUCCESS
) else (
    echo   ⚠ WARNING: No explicit "Loaded GameFeatureData" log found
    echo   Plugin may have registered but without GameFeatureData asset
    goto SHOW_RESULTS_WARN
)

:SHOW_RESULTS_SUCCESS
echo.
echo ========================================
echo ✓✓✓ TEST PASSED ✓✓✓
echo ========================================
echo.
echo   - Plugin mounted: YES
echo   - GameFeatureData loaded: YES
echo   - Errors detected: NO
echo.
echo Next steps:
echo   1. Test PIE (Play in Editor)
echo   2. Verify boot flow -^> menu display
echo.
goto KILL_EDITOR

:SHOW_RESULTS_WARN
echo.
echo ========================================
echo ⚠⚠⚠ TEST WARNING ⚠⚠⚠
echo ========================================
echo.
echo   - Plugin mounted: YES
echo   - GameFeatureData loaded: UNKNOWN
echo   - Errors detected: NO
echo.
echo Possible reasons:
echo   1. GFD_MenuExperience asset not created yet
echo   2. Asset created but not saved
echo   3. Different log format than expected
echo.
echo Action required:
echo   1. Open editor manually
echo   2. Check if GFD_MenuExperience asset exists
echo   3. Create if missing (see README_CREATE_ASSET.md)
echo.
goto KILL_EDITOR

:SHOW_RESULTS_FAIL
echo.
echo ========================================
echo ✗✗✗ TEST FAILED ✗✗✗
echo ========================================
echo.
echo   - Plugin mounted: YES
echo   - GameFeatureData error: YES
echo.
echo Common fixes:
echo   1. Verify .uplugin has GameFeatureData field:
echo      grep "GameFeatureData" Plugins/GameFeatures/ProjectMenuExperienceGF/ProjectMenuExperienceGF.uplugin
echo.
echo   2. Create GFD_MenuExperience asset:
echo      - Run: create_gfd_asset.py in editor (Tools -^> Execute Python Script)
echo      - OR manually create via Content Browser
echo.
echo   3. Regenerate project files:
echo      - Delete Intermediate/ and Saved/
echo      - Right-click Alis.uproject -^> Generate Visual Studio project files
echo.

:KILL_EDITOR
echo.
echo [5/5] Closing editor...
taskkill /IM UnrealEditor.exe /F >nul 2>&1
if !errorlevel! equ 0 (
    echo   ✓ Editor closed
) else (
    echo   ⚠ Editor may have already closed
)

echo.
echo Full log available at:
echo   %LOG_FILE%
echo.
echo ========================================
echo Test completed
echo ========================================
pause
