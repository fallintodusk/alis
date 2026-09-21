@echo off
REM resolve_ue_path.bat - UE_PATH resolution for batch scripts
REM
REM Authority: conf (ue_path.local.conf > ue_path.conf, PER KEY) is
REM authoritative. Pre-existing env UE_PATH is a derived cache; a
REM mismatch with the resolved conf value is a HARD FAIL. Numeric-highest
REM launcher fallback (via Resolve-UEConfig.ps1) runs only when no conf
REM declares UE_PATH. Grammar SOT: ue_path.conf header.
REM
REM Usage from any batch script:
REM   call "%~dp0..\..\config\resolve_ue_path.bat"
REM   if errorlevel 1 exit /b 1
REM
REM Sets engine and toolchain paths declared by the config in caller's env.
REM Note: does NOT use setlocal so results propagate to caller.
REM Note: sequential label-based flow on purpose - parenthesized blocks
REM would expand %vars% at parse time and read stale values.

set "_R_DIR=%~dp0"
set "_R_ENV_UEPATH=%UE_PATH%"
set "_R_ERR="
set "UE_PATH="
set "UE_SOURCE_PATH="
set "LINUX_MULTIARCH_ROOT="

if exist "%_R_DIR%ue_path.conf" call :parse_file "%_R_DIR%ue_path.conf"
if defined _R_ERR goto :fail
if exist "%_R_DIR%ue_path.local.conf" call :parse_file "%_R_DIR%ue_path.local.conf"
if defined _R_ERR goto :fail

if defined UE_PATH goto :stale_check

REM No conf UE_PATH -> numeric-highest valid launcher install
for /f "usebackq delims=" %%p in (`powershell -NoProfile -ExecutionPolicy Bypass -Command ". '%_R_DIR%Resolve-UEConfig.ps1'; Resolve-UELauncherFallback"`) do set "UE_PATH=%%p"
if defined UE_PATH echo [resolve_ue_path] No conf UE_PATH; using highest valid install: %UE_PATH%
if defined UE_PATH goto :stale_check
set "_R_ERR=UE_PATH not resolved. Set it in scripts\config\ue_path.conf (or ue_path.local.conf)."
goto :fail

:stale_check
if not defined _R_ENV_UEPATH goto :resolved
set "_R_A=%_R_ENV_UEPATH:/=\%"
set "_R_B=%UE_PATH:/=\%"
if "%_R_A:~-1%"=="\" set "_R_A=%_R_A:~0,-1%"
if "%_R_B:~-1%"=="\" set "_R_B=%_R_B:~0,-1%"
if /I "%_R_A%"=="%_R_B%" goto :resolved
set "_R_ERR=stale UE_PATH env (%_R_ENV_UEPATH%) does not match resolved conf value (%UE_PATH%) - rerun scripts\setup\setup_ue_env.ps1"
goto :fail

:resolved
set "UE_PATH=%UE_PATH:"=%"
goto :cleanup_ok

:parse_file
set "_P_FILE=%~1"
set "_S_UE_PATH="
set "_S_UE_SOURCE_PATH="
set "_S_LINUX_MULTIARCH_ROOT="
set "_S_BUILD_TARGET="
set "_S_BUILD_CONFIG="
set "_S_BUILD_PLATFORM="
for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%_P_FILE%") do call :parse_line "%%a" "%%b"
exit /b 0

:parse_line
if defined _R_ERR exit /b 0
set "_L_KEY=%~1"
set "_L_VAL=%~2"
if "%_L_KEY%"=="" exit /b 0
if "%_L_KEY%"=="UE_PATH"        goto :known_key
if "%_L_KEY%"=="UE_SOURCE_PATH" goto :known_key
if "%_L_KEY%"=="LINUX_MULTIARCH_ROOT" goto :known_key
if "%_L_KEY%"=="BUILD_TARGET"   goto :known_key
if "%_L_KEY%"=="BUILD_CONFIG"   goto :known_key
if "%_L_KEY%"=="BUILD_PLATFORM" goto :known_key
set "_R_ERR=%_P_FILE%: unknown or malformed key '%_L_KEY%' (grammar: KEY=VALUE, no spaces around =)"
exit /b 0

:known_key
if defined _S_%_L_KEY% goto :dup_key
if "%_L_VAL%"=="" goto :empty_val
if not "%_L_VAL%"=="%_L_VAL:#=%" goto :bad_char
if not "%_L_VAL%"=="%_L_VAL:$=%" goto :bad_char
set "_S_%_L_KEY%=1"
set "%_L_KEY%=%_L_VAL%"
exit /b 0

:bad_char
set "_R_ERR=%_P_FILE%: forbidden character in value for '%_L_KEY%' (no inline comments or $)"
exit /b 0

:dup_key
set "_R_ERR=%_P_FILE%: duplicate key '%_L_KEY%'"
exit /b 0

:empty_val
set "_R_ERR=%_P_FILE%: empty value for '%_L_KEY%'"
exit /b 0

:fail
echo ERROR: %_R_ERR% 1>&2
call :cleanup
exit /b 1

:cleanup_ok
call :cleanup
exit /b 0

:cleanup
set "_R_DIR="
set "_R_ENV_UEPATH="
set "_R_ERR="
set "_R_A="
set "_R_B="
set "_P_FILE="
set "_L_KEY="
set "_L_VAL="
set "_S_UE_PATH="
set "_S_UE_SOURCE_PATH="
set "_S_LINUX_MULTIARCH_ROOT="
set "_S_BUILD_TARGET="
set "_S_BUILD_CONFIG="
set "_S_BUILD_PLATFORM="
exit /b 0
