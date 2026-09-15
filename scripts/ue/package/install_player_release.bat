@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_ALIS_PLAYER.ps1" %*
exit /b %ERRORLEVEL%
