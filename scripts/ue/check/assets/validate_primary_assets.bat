@echo off
setlocal

rem ============================================================================
rem Verify Primary Asset Backing Classes Exist
rem ============================================================================
rem Parses DefaultGame.ini for PrimaryAssetTypesToScan entries and checks that
rem each configured scan directory has .uasset files. Pure Python, no editor.
rem ============================================================================

set SCRIPT_DIR=%~dp0

rem Try system Python first, fall back to UE bundled Python
where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%check_primary_assets.py" %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE Python
rem scripts/ue/check/assets -> scripts/config (4 levels up)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%check_primary_assets.py" %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
