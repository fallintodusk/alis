@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..") do set PROJECT_ROOT=%%~fI
set LOG_DIR=%PROJECT_ROOT%\Saved\Logs
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set LOG=%LOG_DIR%\data_validate.log

REM Resolve UE_PATH (SOT: resolve_ue_path.bat)
call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo Running Unreal DataValidation...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_FILE%" -run=DataValidation -unattended -nop4 -NoSound -NullRHI -CrashForUAT > "%LOG%" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Running ProjectMind data schema checks...
python "%SCRIPT_DIR%validate_projectmind_data.py"
exit /b %ERRORLEVEL%
