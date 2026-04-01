@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..\..\..") do set PROJECT_ROOT=%%~fI
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Validation\Reports

if not exist "%REPORTS_DIR%" mkdir "%REPORTS_DIR%"

python "%SCRIPT_DIR%validate_data.py" > "%REPORTS_DIR%\projectmind_data_validation.log" 2>&1
exit /b %ERRORLEVEL%
