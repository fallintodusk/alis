@echo off
setlocal

REM Package Shipping build using the UE SOURCE engine.
REM Engine root comes from UE_SOURCE_PATH in scripts/config/ue_path.conf
REM (SOT; local override via ue_path.local.conf). Missing UE_SOURCE_PATH
REM fails ONLY this packaging entrypoint, per the resolution contract.
REM Delegates to package_release.ps1 with -EngineRoot override.
REM All additional arguments are forwarded as-is.

set "UE_SOURCE_PATH="
call "%~dp0..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not defined UE_SOURCE_PATH (
    echo ERROR: UE_SOURCE_PATH not set - declare it in scripts\config\ue_path.conf 1>&2
    echo        ^(source-release packaging requires an explicit source engine root^) 1>&2
    exit /b 1
)

set "ENGINE_PYTHON=%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist "%ENGINE_PYTHON%" (
    echo ERROR: launcher engine Python not found: %ENGINE_PYTHON% 1>&2
    exit /b 1
)
"%ENGINE_PYTHON%" "%~dp0..\check\governance\validate_engine_env.py" --repo-root "%~dp0..\..\.." --require-source-identity
if errorlevel 1 exit /b 1

powershell -ExecutionPolicy Bypass -File "%~dp0package_release.ps1" -EngineRoot "%UE_SOURCE_PATH%" -CreateReleaseArchive %*
exit /b %ERRORLEVEL%
