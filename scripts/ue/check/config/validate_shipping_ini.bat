@echo off
setlocal

rem Validate DefaultEngine.ini and DefaultGame.ini for shipping-unsafe settings.
rem Pure Python - no editor required, runs in seconds.

set SCRIPT_DIR=%~dp0

rem Try system Python first, fall back to UE bundled Python
where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%validate_shipping_ini.py" %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE Python
rem scripts/ue/check/config -> scripts/config (4 levels up from check/config)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_shipping_ini.py" %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
