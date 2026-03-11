@echo off
REM Automated version (no pause) for CI/CD and automated testing
setlocal enabledelayedexpansion

call "%~dp0config\test_config.bat"
call "%~dp0lib\cleanup.bat" all

echo Starting editor test...
call "%~dp0lib\launch_editor.bat"
if errorlevel 1 (
    echo FAIL: Editor launch failed
    call "%~dp0lib\cleanup.bat" kill_editor
    exit /b 1
)

set "PLUGIN=%~1"
if "%PLUGIN%"=="" set "PLUGIN=ProjectMenuExperienceGF"

echo Checking plugin: %PLUGIN%
call "%~dp0lib\check_logs.bat" "%PLUGIN%"
set "RESULT=%errorlevel%"

call "%~dp0lib\cleanup.bat" kill_editor

if %RESULT% equ 0 (
    echo SUCCESS: Plugin registered correctly
    exit /b 0
) else (
    echo FAIL: Plugin registration failed
    exit /b 1
)
