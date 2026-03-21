@echo off
REM Validates that spawnClass Blueprint package is present in cooked output.
REM Target: GrandPa spawnClass path (/Game/Project/Resources/Characters/GrandPa/GrandPa_BP)
REM Usage: scripts\ue\test\integration\validate_spawnclass_cook.bat

setlocal

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\..\..\.."
for %%I in ("%PROJECT_ROOT%") do set "PROJECT_ROOT=%%~fI"

set "PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject"
REM Resolve UE_PATH (SOT: resolve_ue_path.bat)
call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1
set "UE_PATH=%UE_PATH:/=\%"
set "EDITOR_CMD=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%EDITOR_CMD%" (
    echo ERROR: UnrealEditor-Cmd.exe not found: %EDITOR_CMD%
    exit /b 1
)

set "LOG_DIR=%PROJECT_ROOT%\Saved\Logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "COOK_LOG=%LOG_DIR%\spawnclass_cook_validation.log"

set "COOKED_BP_WIN=%PROJECT_ROOT%\Saved\Cooked\Windows\Alis\Content\Project\Resources\Characters\GrandPa\GrandPa_BP.uasset"
set "COOKED_BP_WINNO=%PROJECT_ROOT%\Saved\Cooked\WindowsNoEditor\Alis\Content\Project\Resources\Characters\GrandPa\GrandPa_BP.uasset"

echo ========================================
echo SpawnClass Cook Validation
echo ========================================
echo UE:      %EDITOR_CMD%
echo Project: %PROJECT_FILE%
echo Log:     %COOK_LOG%
echo.

echo [1/2] Running project cook (no -CookDir override)...
"%EDITOR_CMD%" "%PROJECT_FILE%" ^
    -run=Cook ^
    -TargetPlatform=Windows ^
    -unattended -nop4 -UTF8Output -NoLogTimes > "%COOK_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: Cook command failed. See log: %COOK_LOG%
    exit /b 1
)

echo [2/2] Verifying cooked Blueprint package exists...
if exist "%COOKED_BP_WIN%" (
    echo OK: Found cooked package: %COOKED_BP_WIN%
    exit /b 0
)
if exist "%COOKED_BP_WINNO%" (
    echo OK: Found cooked package: %COOKED_BP_WINNO%
    exit /b 0
)

echo ERROR: Cook completed but expected Blueprint package was not found.
echo Expected one of:
echo   %COOKED_BP_WIN%
echo   %COOKED_BP_WINNO%
echo Check log: %COOK_LOG%
exit /b 1
