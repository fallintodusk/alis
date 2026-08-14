@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALL_ALIS_DEVELOPER_PROJECT.ps1" %*
exit /b %ERRORLEVEL%
