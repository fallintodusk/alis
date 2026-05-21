@echo off
setlocal

rem Verify that no first-party reusable code uses the forbidden `Alis*` prefix.
rem SOT: docs/architecture/principles.md "Universal Naming Convention".
rem AGENTS.md: "NO Alis* IN REUSABLE CODE (CRITICAL!)".

set SCRIPT_DIR=%~dp0

where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%validate_no_alis_prefix.py" %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE bundled Python (scripts/ue/check/governance -> scripts/config)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_no_alis_prefix.py" %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
