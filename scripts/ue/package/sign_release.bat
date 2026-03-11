@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0sign_release.ps1" %*
exit /b %ERRORLEVEL%
