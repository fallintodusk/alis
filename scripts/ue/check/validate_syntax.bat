@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..") do set PROJECT_ROOT=%%~fI
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Validation\Reports

call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%REPORTS_DIR%" mkdir "%REPORTS_DIR%"

echo Running build validation without compile...
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" ^
    AlisEditor Win64 Development ^
    "%PROJECT_FILE%" ^
    -skipcompile ^
    -NoHotReload ^
    > "%REPORTS_DIR%\skipcompile.log" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [OK] Syntax validation passed
exit /b 0
