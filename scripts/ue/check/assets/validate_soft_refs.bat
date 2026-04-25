@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================================
rem Validate Soft References and Null Materials
rem ============================================================================
rem Runs the editor to scan all level actors for null material slots and
rem broken soft object references. Catches "Invalid ShaderMap material (None)"
rem before packaging.
rem
rem Usage: validate_soft_refs.bat [--map <map_path>]
rem   Default map: the editor startup map from DefaultEngine.ini
rem ============================================================================

pushd "%~dp0\..\..\..\.." || (echo ERROR: cannot cd to repo root & exit /b 1)

call "scripts\config\resolve_ue_path.bat"
if errorlevel 1 (echo ERROR: UE_PATH not resolved & exit /b 1)

set "PROJECT_FILE=%CD%\Alis.uproject"
set "UE_EDITOR=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "LOG_FILE=%CD%\Saved\Logs\validate_soft_refs.log"

if not exist "%UE_EDITOR%" (
    echo ERROR: UnrealEditor-Cmd.exe not found at: %UE_EDITOR%
    exit /b 1
)

rem Parse optional --map argument
set "MAP_ARG="
:parse_args
if "%~1"=="" goto :done_args
if "%~1"=="--map" (
    set "MAP_ARG=%~2"
    shift
    shift
    goto :parse_args
)
shift
goto :parse_args
:done_args

echo ============================================================================
echo   Soft Reference and Null Material Validation
echo ============================================================================

echo [1/2] Opening map and scanning actors...

if defined MAP_ARG (
    "%UE_EDITOR%" "%PROJECT_FILE%" %MAP_ARG% -run=PythonScript -Script="scripts/ue/check/assets/check_soft_refs.py" -unattended -NullRHI -nosplash -nosound -log="%LOG_FILE%" 2>nul
) else (
    "%UE_EDITOR%" "%PROJECT_FILE%" -run=PythonScript -Script="scripts/ue/check/assets/check_soft_refs.py" -unattended -NullRHI -nosplash -nosound -log="%LOG_FILE%" 2>nul
)

echo [2/2] Checking results...

set CHECK_FAILED=0

findstr /C:"SOFT_REF_CHECK: FAIL" "%LOG_FILE%" >nul 2>&1
if !ERRORLEVEL! equ 0 (
    echo.
    echo   Null material slots found:
    for /f "tokens=*" %%L in ('findstr /C:"NULL_MATERIAL:" "%LOG_FILE%"') do (
        echo     [X] %%L
    )
    set /a CHECK_FAILED+=1
)

findstr /C:"SOFT_REF_CHECK: PASS" "%LOG_FILE%" >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo   [X] Validation script did not produce results - check log: %LOG_FILE%
    set /a CHECK_FAILED+=1
)

echo.
echo ============================================================================
if !CHECK_FAILED! equ 0 (
    echo   Soft reference validation passed.
) else (
    echo   Soft reference validation FAILED
)
echo ============================================================================

popd
if !CHECK_FAILED! neq 0 exit /b 1
exit /b 0
