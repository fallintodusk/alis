@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..\..\..") do set PROJECT_ROOT=%%~fI
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set LOG_DIR=%PROJECT_ROOT%\Saved\Logs
set HELPER_SCRIPT=%PROJECT_ROOT%\scripts\ue\editor\blueprint\character\create_world_body_retarget_wrapper.py

call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo Creating project-owned WorldBody retarget wrapper...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -run=pythonscript ^
    -script="%HELPER_SCRIPT%" ^
    -unattended ^
    -nop4 ^
    -NoSound ^
    -NullRHI ^
    -CrashForUAT ^
    -log="%LOG_DIR%\create_world_body_retarget_wrapper.log" ^
    > "%LOG_DIR%\create_world_body_retarget_wrapper_stdout.log" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [OK] WorldBody retarget wrapper refreshed
exit /b 0
