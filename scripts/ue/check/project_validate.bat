@echo off
setlocal

set SCRIPT_DIR=%~dp0
call "%SCRIPT_DIR%check.bat" --assets
exit /b %ERRORLEVEL%
