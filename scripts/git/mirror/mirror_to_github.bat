@echo off
setlocal
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0mirror_to_github.ps1" %*
exit /b %ERRORLEVEL%
