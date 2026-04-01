@echo off
setlocal

set SCRIPT_DIR=%~dp0

call "%SCRIPT_DIR%validate_uht.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call "%SCRIPT_DIR%validate_syntax.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call "%SCRIPT_DIR%validate_blueprints.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call "%SCRIPT_DIR%validate_assets.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call "%SCRIPT_DIR%gameplay\projectmind\validate_data.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [OK] All fast project validations passed
exit /b 0
