@echo off
setlocal

set SCRIPT_DIR=%~dp0
call "%SCRIPT_DIR%validate_blueprints.bat"
exit /b %ERRORLEVEL%
