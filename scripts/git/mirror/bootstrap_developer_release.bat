@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_ALIS_DEVELOPER.ps1" %*
exit /b %ERRORLEVEL%
