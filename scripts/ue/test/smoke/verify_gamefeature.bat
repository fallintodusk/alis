@echo off
REM ============================================================================
REM GameFeature Verification - Main test orchestrator (SOLID: OCP, DIP)
REM ============================================================================
REM This script orchestrates GameFeature registration verification by
REM composing modular utilities from the lib/ directory.
REM
REM Usage: verify_gamefeature.bat [plugin_name|all]
REM   plugin_name - Test specific plugin (e.g., ProjectMenuExperienceGF)
REM   all         - Test all plugins from config (default)
REM ============================================================================

setlocal enabledelayedexpansion

echo ========================================
echo GameFeature Registration Verification
echo ========================================
echo.

REM Load central config (Dependency Inversion Principle)
call "%~dp0config\test_config.bat"
if errorlevel 1 (
    echo ERROR: Failed to load configuration
    exit /b 1
)

set "TARGET_PLUGIN=%~1"
if "%TARGET_PLUGIN%"=="" set "TARGET_PLUGIN=all"

REM ============================================================================
REM Step 1: Cleanup (Single Responsibility Principle)
REM ============================================================================
echo [1/4] Cleanup
echo ----------------------------------------
call "%~dp0lib\cleanup.bat" all
if errorlevel 1 (
    echo WARNING: Cleanup had issues, continuing anyway...
)
echo.

REM ============================================================================
REM Step 2: Launch Editor (Single Responsibility Principle)
REM ============================================================================
echo [2/4] Launching Editor
echo ----------------------------------------
call "%~dp0lib\launch_editor.bat"
if errorlevel 1 (
    echo.
    echo ✗✗✗ TEST FAILED ✗✗✗
    echo Editor failed to start or initialize
    goto CLEANUP_AND_EXIT
)
echo.

REM ============================================================================
REM Step 3: Verify GameFeature Registration (Single Responsibility Principle)
REM ============================================================================
echo [3/4] Verifying GameFeature Registration
echo ----------------------------------------

set "TEST_FAILED=0"
set "TEST_WARNING=0"
set "TESTS_RUN=0"

if "%TARGET_PLUGIN%"=="all" (
    REM Test all plugins from config
    for %%P in (%GAMEFEATURE_PLUGINS%) do (
        echo.
        echo Testing plugin: %%P
        echo --------------------
        call "%~dp0lib\check_logs.bat" "%%P"
        if !errorlevel! equ 1 (
            set "TEST_FAILED=1"
        ) else if !errorlevel! equ 2 (
            set "TEST_WARNING=1"
        )
        set /a TESTS_RUN+=1
    )
) else (
    REM Test specific plugin
    echo Testing plugin: %TARGET_PLUGIN%
    echo --------------------
    call "%~dp0lib\check_logs.bat" "%TARGET_PLUGIN%"
    if !errorlevel! equ 1 (
        set "TEST_FAILED=1"
    ) else if !errorlevel! equ 2 (
        set "TEST_WARNING=1"
    )
    set /a TESTS_RUN+=1
)

echo.
echo ----------------------------------------
echo Tests run: %TESTS_RUN%
echo.

REM ============================================================================
REM Step 4: Display Results
REM ============================================================================
echo [4/4] Results
echo ========================================

if %TEST_FAILED% equ 1 (
    echo ✗✗✗ TEST FAILED ✗✗✗
    echo.
    echo One or more GameFeature plugins failed to register.
    echo See log output above for details.
    echo.
    echo Log file: %LOG_FILE%
    goto CLEANUP_AND_EXIT
)

if %TEST_WARNING% equ 1 (
    echo ⚠⚠⚠ TEST PASSED WITH WARNINGS ⚠⚠⚠
    echo.
    echo All plugins registered without errors,
    echo but some may not have explicit load confirmation.
    echo.
    echo Log file: %LOG_FILE%
    goto CLEANUP_AND_EXIT
)

echo ✓✓✓ ALL TESTS PASSED ✓✓✓
echo.
echo All GameFeature plugins registered successfully.
echo.
echo Next steps:
echo   1. Test PIE (Play in Editor)
echo   2. Verify boot flow -^> menu display
echo.
echo Log file: %LOG_FILE%

:CLEANUP_AND_EXIT
echo.
echo ========================================
call "%~dp0lib\cleanup.bat" kill_editor
echo.
echo Press any key to exit...
pause >nul
exit /b %TEST_FAILED%
