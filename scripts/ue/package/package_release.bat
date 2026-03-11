@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0package_release.ps1" %*
exit /b %ERRORLEVEL%
