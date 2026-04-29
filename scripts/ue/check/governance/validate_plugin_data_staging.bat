@echo off
setlocal

rem Audit plugins reading from GetPluginDataDir(...) at runtime to confirm
rem their .Build.cs stages Plugins/<X>/Data/ via RuntimeDependencies.
rem Pure Python - no editor required, runs in seconds.

set SCRIPT_DIR=%~dp0

where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%validate_plugin_data_staging.py" %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE bundled Python (scripts/ue/check/governance -> scripts/config)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_plugin_data_staging.py" %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
