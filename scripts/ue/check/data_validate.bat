@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..") do set PROJECT_ROOT=%%~fI
set CONFIG_FILE=%PROJECT_ROOT%\scripts\config\ue_path.conf
set LOG_DIR=%PROJECT_ROOT%\Saved\Logs
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set LOG=%LOG_DIR%\data_validate.log

if exist "%CONFIG_FILE%" (
    for /f "usebackq tokens=1,2 delims==" %%a in ("%CONFIG_FILE%") do (
        if "%%a"=="UE_PATH" set UE_PATH=%%b
    )
)

if not defined UE_PATH (
    if exist "<ue-path>" set UE_PATH=<ue-path>
    if exist "<ue-path>" set UE_PATH=<ue-path>
)

if not defined UE_PATH (
    echo ERROR: UE_PATH not found. Check scripts\config\ue_path.conf
    exit /b 1
)

set UE_PATH=%UE_PATH:"=%

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo Running Unreal DataValidation...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_FILE%" -run=DataValidation -unattended -nop4 -NoSound -NullRHI -CrashForUAT > "%LOG%" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Running ProjectMind data schema checks...
python "%SCRIPT_DIR%validate_projectmind_data.py"
exit /b %ERRORLEVEL%
