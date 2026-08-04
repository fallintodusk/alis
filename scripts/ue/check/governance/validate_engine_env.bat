@echo off
setlocal

rem Engine-environment governance: identity (UE_PATH Build.version vs
rem Alis.uproject association), repo-wide hardcoded-engine-path scan, and
rem the conditional .uplugin EngineVersion pin invariant.
rem SOT: docs/ue_engine/version_update.md + scripts/config/ue_path.conf.

set SCRIPT_DIR=%~dp0

where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%validate_engine_env.py" --repo-root "%SCRIPT_DIR%..\..\..\.." %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE bundled Python (scripts/ue/check/governance -> scripts/config)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_engine_env.py" --repo-root "%SCRIPT_DIR%..\..\..\.." %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
