@echo off
REM Direct Windows wrapper for Unreal Engine automation tests
REM Usage: test.bat [--unit|--integration|--all] [--filter <name>]
REM Examples:
REM   test.bat --unit
REM   test.bat --filter ProjectUI

setlocal

REM Resolve UE_PATH (SOT: resolve_ue_path.bat)
call "%~dp0..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

REM Project file
set PROJECT_ROOT=%~dp0..\..\..
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Automation\Reports

REM Parse mode
set MODE=%1
if "%MODE%"=="" set MODE=--unit

REM Determine test filters
set TEST_FILTERS=
if "%MODE%"=="--unit" (
    set TEST_FILTERS=ProjectCore;ProjectBoot;ProjectLoading;ProjectSession;ProjectData;ProjectUI;ProjectMenuUI
) else if "%MODE%"=="--integration" (
    set TEST_FILTERS=ProjectIntegrationTests
) else if "%MODE%"=="--all" (
    set TEST_FILTERS=Project
) else if "%MODE%"=="--filter" (
    set TEST_FILTERS=%2
) else (
    echo Usage: test.bat [--unit^|--integration^|--all] [--filter ^<name^>]
    exit /b 1
)

echo Running tests: %TEST_FILTERS%
echo.

REM Build automation command
set AUTOMATION_CMD=Automation RunTests %TEST_FILTERS%

"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -ExecCmds="%AUTOMATION_CMD%" ^
    -unattended -nopause -nosplash -NullRHI ^
    -testexit="Automation Test Queue Empty" ^
    -ReportOutputPath="%REPORTS_DIR%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Tests PASSED!
    exit /b 0
) else (
    echo.
    echo Tests FAILED!
    exit /b %ERRORLEVEL%
)
